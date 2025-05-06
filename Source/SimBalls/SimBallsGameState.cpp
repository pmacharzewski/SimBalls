#include "SimBallsGameState.h"

#include "BallActor.h"
#include "GridManager.h"

#include "SimulationConfig.h"

ASimBallsGameState::ASimBallsGameState()
{
	PrimaryActorTick.bCanEverTick = true;
}

FBallSimulatedState& ASimBallsGameState::CreateBallState(int32 StateID)
{
	const int32 GridMax = Config->GridSize - 1;
	const int32 HP = RandomStream.RandRange(Config->MinHP, Config->MaxHP);
	const int32 X = RandomStream.RandRange(0, GridMax);
	const int32 Y = RandomStream.RandRange(0, GridMax);
	const EBallTeamColor Team = static_cast<EBallTeamColor>(StateID % static_cast<int32>(EBallTeamColor::Max));
		
	FBallSimulatedState State(StateID, INDEX_NONE, HP, Config->AttackInterval, FIntPoint(X, Y), Team);
	BallStates.Add(MoveTemp(State));

	return BallStates.Last();
}

ABallActor* ASimBallsGameState::CreateBallActor(const FBallSimulatedState& BallState)
{
	FActorSpawnParameters ASP;
	ASP.Owner = this;
	ASP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABallActor* NewBall = GetWorld()->SpawnActor<ABallActor>(ASP);
	NewBall->InitBall(BallState);

	BallActors.Add(NewBall);

	return NewBall;
}

void ASimBallsGameState::BeginPlay()
{
	Super::BeginPlay();

	Config = USimulationConfig::Get();
	Grid = AGridManager::FindOrSpawnGrid(this);
	//Note: setting the Seed from config, but this should come from server
	RandomStream.Initialize(Config->Seed);
		
	InitializeBalls();

	// Hack - Make player look at the balls.
	if (!GetWorld()->IsNetMode(NM_DedicatedServer))
	{
		FTimerHandle LookAtBallsHandle;
		GetWorldTimerManager().SetTimer(LookAtBallsHandle,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (auto PC = GetGameInstance()->GetFirstLocalPlayerController())
				{
					FVector BallsMiddlePoint = FVector::ZeroVector;
					for (const auto Ball : BallActors)
					{
						BallsMiddlePoint += Ball->GetActorLocation() / BallActors.Num();
					}

					const FVector CameraLoc = PC->PlayerCameraManager->GetCameraLocation();
					const FVector LookDir = (BallsMiddlePoint - CameraLoc).GetSafeNormal();
			
					PC->SetControlRotation(LookDir.ToOrientationRotator());
				}

			}), 0.5f, false);
	}
}

void ASimBallsGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	RunSimulation(DeltaSeconds);

	// no need to update visual actors on DS
	//if (!GetWorld()->IsNetMode(NM_DedicatedServer))
	{
		for (ABallActor* BallActor : BallActors)
		{
			BallActor->UpdateVisuals(DeltaSeconds);
		}
	}
}

void ASimBallsGameState::InitializeBalls()
{
	BallStates.Reserve(Config->NumBalls);
	BallActors.Reserve(Config->NumBalls);
	
	// Initialize all the states based on random seed value
	for (int32 Index = 0; Index < Config->NumBalls; ++Index)
	{
		const FBallSimulatedState& State = CreateBallState(Index);
		
		CreateBallActor(State);
	}
}

void ASimBallsGameState::RunSimulation(float DeltaSeconds)
{
	bool bStateDirty = false;
	const double CurrentTime = HasAuthority() ? GetWorld()->GetTimeSeconds() : GetServerWorldTimeSeconds();
	const double TimeStep = Config->SimulationTimeStep;
	
	// Try to process all missing steps for late joiners so everyone can stay at the same time frame.
	// Note: this may be too heavy if client joins very late - better to conditional replicate initial state or figure something else
	while (CurrentTime > SimulationTime)
	{
		AdvanceSimulation(SimulationTime);
		SimulationTime += TimeStep;
		bStateDirty = true;
	}

	// Apply updated simulated states to the Ball Actors.
	if (bStateDirty)
	{
		for (const FBallSimulatedState& State : BallStates)
		{
			BallActors[State.ID]->ApplySimulatedState(State);
		}
	}
}

void ASimBallsGameState::AdvanceSimulation(double Timestamp)
{
	// Reset and prepare states for new simulation step (e.g. reset Damage)
	PrepareBallStates(Timestamp);
	
	for (FBallSimulatedState& State : BallStates)
	{
		SimulateBallState(State);
	}

	// Resolve attack/damage
	for (FBallSimulatedState& State : BallStates)
	{
		State.HP = FMath::Max(0, State.HP - State.Damage);
		State.bIsDead = State.HP <= 0;
	}
}

void ASimBallsGameState::PrepareBallStates(double Timestamp)
{
	TSet<FIntPoint> Obstacles;
	Obstacles.Reserve(BallStates.Num());
	
	for (FBallSimulatedState& State : BallStates)
	{
		State.Timestamp = Timestamp;
		State.Damage = 0; //<< Reset accumulated damage
		
		if (State.StepsToAttack == 0)
		{
			State.StepsToAttack = Config->AttackInterval;	
		}
		 
		Obstacles.Add(State.GridPosition);
	}
	
	Grid->SetObstacles(Obstacles);
}

void ASimBallsGameState::SimulateBallState(FBallSimulatedState& State)
{
	if (State.bIsDead)
	{
		return;
	}

	if (!ProcessCombatState(State))
	{
		ProcessMovementState(State);

		// reset attack timer when no longer in combat
		State.StepsToAttack = Config->AttackInterval;
	}
}

bool ASimBallsGameState::ProcessCombatState(FBallSimulatedState& State)
{
	int32 EnemyDistance = 0;
	if (!FindClosestEnemy(State, State.TargetID, EnemyDistance))
	{
		return false;	
	}

	// Enter fighting mode at range - this will stop movement
	if (EnemyDistance <= Config->AttackRange)
	{
		// Apply damage according to expected time step
		if (--State.StepsToAttack == 0)
		{
			ApplyDamage(State, BallStates[State.TargetID]);
		}
		
		return true;
	}
	
	return false;
}

bool ASimBallsGameState::ProcessMovementState(FBallSimulatedState& State)
{
	if (!State.IsTargetValid())
	{
		return false;
	}
	
	const FIntPoint& TargetPosition = BallStates[State.TargetID].GridPosition;

	// We cache the path so try to generate when anything changed only
	// Note: should be done in Async task
	if (Grid->ShouldRegeneratePath(State.GridPosition, TargetPosition, State.GridPath))
	{
		State.PathIndex = 0;
		State.GridPath = Grid->FindPathAStar(State.GridPosition, TargetPosition);
	}

	ApplyMovement(State);

	return true;
}

void ASimBallsGameState::ApplyMovement(FBallSimulatedState& State)
{
	const FIntPoint PrevPosition = State.GridPosition;
    int32 MoveStep = 0;
	while (MoveStep++ < Config->MoveRate && State.GridPath.Num() > State.PathIndex)
	{
		State.GridPosition = State.GridPath[State.PathIndex++];
	}

	// Not sure about this, perhaps not needed to update every time but just once at simulation start (PrepareSimulation)
	//Grid->UpdateObstacle(PrevPosition, State.GridPosition);
}

void ASimBallsGameState::ApplyDamage(FBallSimulatedState& Attacker, FBallSimulatedState& Receiver)
{
	// accumulate damage and set at the end of simulation
	//TODO: Attacker should provide 'AttackPower'
	Receiver.Damage++;
}

bool ASimBallsGameState::FindClosestEnemy(const FBallSimulatedState& State, int32& OutEnemy, int32& OutDistance)
{
	OutEnemy = INDEX_NONE;
	OutDistance = TNumericLimits<int32>::Max();

	for (const FBallSimulatedState& OtherState : BallStates)
	{
		// interested in other team states only
		if (OtherState.bIsDead || OtherState.Team == State.Team || State.ID == OtherState.ID)
		{
			continue;
		}
		
		const int32 Dist = FMath::Abs(OtherState.GridPosition.X - State.GridPosition.X) + FMath::Abs(OtherState.GridPosition.Y - State.GridPosition.Y);
		
		if (Dist < OutDistance)
		{
			OutDistance = Dist;
			OutEnemy = OtherState.ID;
		}
	}
		
	return OutEnemy != INDEX_NONE;
}

