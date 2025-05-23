#include "SimBallsGameState.h"

#include "BallActor.h"
#include "GridManager.h"

#include "SimulationConfig.h"

DEFINE_LOG_CATEGORY_STATIC(LogSim, Log, All)

static bool bAutoCameraAdjust = true;
static FAutoConsoleVariableRef CVarAutoCameraAdjust(
		TEXT("Sim.AutoCameraAdjust"),
		bAutoCameraAdjust,
		TEXT("Enables camera adjustment."),
		ECVF_Cheat
	);

namespace
{
	// limit number of simulation steps per tick
	constexpr int32 MAX_SIMULATIONS_PER_TICK = 50;
}

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
	const EBallTeamColor Team = static_cast<EBallTeamColor>(StateID % static_cast<int32>(EBallTeamColor::Max_None));

	FBallSimulatedState State(StateID, INDEX_NONE, HP, Config->AttackInterval, FIntPoint(X, Y), Team);
	
	if (!BallStates.IsValidIndex(StateID))
	{
		BallStates.Add(MoveTemp(State));
	}
	else
	{
		BallStates[StateID] = State;
	}

	return BallStates[StateID];
}

ABallActor* ASimBallsGameState::CreateBallActor(const FBallSimulatedState& BallState)
{
	ABallActor* NewBall = BallActors.IsValidIndex(BallState.ID) ? BallActors[BallState.ID] : nullptr;
	
	if (!NewBall)
	{
		FActorSpawnParameters ASP;
		ASP.Owner = this;
		ASP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		NewBall = GetWorld()->SpawnActor<ABallActor>(ASP);
		
		BallActors.Add(NewBall);
	}
	
	NewBall->InitBall(BallState);
	
	return NewBall;
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
		GetWorldTimerManager().SetTimer(LookAtBallsHandle, FTimerDelegate::CreateWeakLambda(this, [this](){ AdjustCamera(0);}), 0.25f, false);
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

	// Debug camera adjustment - press space
	if (!GetWorld()->IsNetMode(NM_DedicatedServer))
	{
		if (auto PC = GetGameInstance()->GetFirstLocalPlayerController(); PC && PC->WasInputKeyJustPressed(EKeys::SpaceBar))
		{
			AdjustCamera();
		}

		if (bAutoCameraAdjust)
		{
			AdjustCamera(DeltaSeconds);
		}
		
	}
}

void ASimBallsGameState::RunSimulation(float DeltaSeconds)
{
	const double CurrentTime = HasAuthority() ? GetWorld()->GetTimeSeconds() : GetServerWorldTimeSeconds();
	const double TimeStep = Config->SimulationTimeStep;

	int32 Cycle = 0;
	
	// Try to process all missing steps for late joiners so everyone can stay at the same time frame.
	// Note: this may be too heavy if client joins very late - better to conditional replicate initial state?
	while (CurrentTime > SimulationTime)
	{
		AdvanceSimulation(SimulationTime);
		SimulationTime += TimeStep;

		// Prevent too many cycles per single frame
		if (Cycle++ > MAX_SIMULATIONS_PER_TICK)
		{
			return;
		}
	}

	// Apply updated simulated states to the Ball Actors.
	if (Cycle > 0)
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
		if (State.bIsDead)
		{
			// Respawn after death
			if (Timestamp - State.Timestamp > Config->DyingDuration)
			{
				CreateBallActor(CreateBallState(State.ID));
				State.Timestamp = Timestamp;
			}
		}
		else
		{
			State.Timestamp = Timestamp;	
		}

		// Reset trackers before entering next simulation step
		State.Damage = 0;
		State.MoveSteps = 0;
		// Reset attack if reached attack interval
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

	// We cache the path and generate when anything changed only
	// Note: should be done in Async task
	if (Grid->ShouldRegeneratePath(State.GridPosition, TargetPosition, State.GridPath, Config->AttackRange))
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
	
	while (State.MoveSteps < Config->MoveRate && State.PathIndex < State.GridPath.Num() - 1 - Config->AttackRange)
	{
		State.MoveSteps++;
		// start from the next grid position and move until MoveStep or Goal is reached
		State.GridPosition = State.GridPath[++State.PathIndex];
	}
	
	// prevent other state finding the same goal position
	Grid->UpdateObstacle(PrevPosition, State.GridPosition);
}

void ASimBallsGameState::ApplyDamage(FBallSimulatedState& Attacker, FBallSimulatedState& Receiver)
{
	// accumulate damage and set at the end of simulation
	//Note: Attacker could provide Damage size
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

void ASimBallsGameState::AdjustCamera(float DeltaSeconds)
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
		
		if (DeltaSeconds <= 0.0f)
		{
			PC->SetControlRotation(LookDir.ToOrientationRotator());
		}
		else
		{
			const float HalfFOVRad = FMath::DegreesToRadians(PC->PlayerCameraManager->GetFOVAngle() * 0.5f);
			float MinCameraDist = 500;
			for (const auto Ball : BallActors)
			{
				FVector Origin, BoxExtent;
				Ball->GetActorBounds(false, Origin, BoxExtent);
				
				const float DistToMiddlePoint = (Origin - BallsMiddlePoint).Size();
				const float DistanceForThisObject = BoxExtent.Size() / FMath::Tan(HalfFOVRad);
				const float MinCamDistThisObject = DistToMiddlePoint + DistanceForThisObject;
				
				MinCameraDist = FMath::Max(MinCameraDist, MinCamDistThisObject);
			}

			constexpr float MaxCameraDist = 1000.0f;
			const FVector CameraPivot = Grid->GetActorLocation() + FVector::UpVector * MaxCameraDist;
			const FVector CameraOffset = (CameraPivot - BallsMiddlePoint).GetSafeNormal() * FMath::Max(MinCameraDist, MaxCameraDist);
			const FVector DesiredCameraLoc = BallsMiddlePoint + CameraOffset;
			
			PC->SetControlRotation(FMath::RInterpTo(PC->GetControlRotation(), LookDir.ToOrientationRotator(), DeltaSeconds, 0.5));
			PC->GetPawn()->SetActorLocation(FMath::VInterpTo(PC->GetPawn()->GetActorLocation(), DesiredCameraLoc, DeltaSeconds, 0.5));
		}
	}
}

