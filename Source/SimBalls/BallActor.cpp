
#include "BallActor.h"
#include "BallsTypes.h"
#include "GridManager.h"
#include "SimulationConfig.h"

namespace
{
	constexpr float BallScale = 0.5f;
	
	constexpr float HitShakeIntensity = 10.0f;
	constexpr float HitShakeSpeed = 25.0f;
	constexpr float Flashes = 3.0f;
	
	constexpr TCHAR Param_Color[] = TEXT("Color");
	constexpr TCHAR Param_Flash[] = TEXT("Flash");
	constexpr TCHAR Param_Dissolve[] = TEXT("Dissolve");
}

ABallActor::ABallActor()
{
	PrimaryActorTick.bCanEverTick = false;
	
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh;
		ConstructorHelpers::FObjectFinder<UMaterial> Material;
		
		FConstructorStatics()
			: SphereMesh(TEXT("/Engine/EngineMeshes/Sphere"))
			, Material(TEXT("/Game/Assets/M_SimBall")){}
	};

	static FConstructorStatics ConstructorStatics;

	BallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BallMesh"));
	BallMesh->SetStaticMesh(ConstructorStatics.SphereMesh.Object);
	BallMesh->SetupAttachment(RootComponent);
	BallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BallMesh->SetRelativeScale3D(FVector::OneVector * BallScale);
	BallMesh->SetMaterial(0, ConstructorStatics.Material.Object);
	BallMesh->SetGenerateOverlapEvents(false);
	
}
void ABallActor::InitBall(const FBallSimulatedState& InState)
{
	InitialHP = InState.HP;
	SimulatedState = InState;
	
	auto Grid = AGridManager::FindOrSpawnGrid(this);
	DesiredLocation = Grid->GridToWorld(InState.GridPosition);
	PrevLocation = DesiredLocation;
	
	const FLinearColor TeamColor = InState.Team == EBallTeamColor::Red ? FLinearColor::Red : FLinearColor::Blue;
	
	BallMaterial = BallMesh->CreateDynamicMaterialInstance(0);
	BallMaterial->SetVectorParameterValue(Param_Color, TeamColor);
	SetActorLocation(DesiredLocation);
	SetActorHiddenInGame(false);

	BallMaterial->SetScalarParameterValue(Param_Flash, 0);
	BallMaterial->SetScalarParameterValue(Param_Dissolve, 0);

	MovementAction.bPlaying = false;
	AttackAction.bPlaying = false;
	HitAction.bPlaying = false;
	DyingAction.bPlaying = false;
}

void ABallActor::ApplySimulatedState(const FBallSimulatedState& InState)
{
	auto Config = USimulationConfig::Get();
	auto Grid = AGridManager::FindOrSpawnGrid(this);
	
	// Took damage - Start Hit reaction
	if (InState.HP < SimulatedState.HP)
	{
		HitAction.Play(Config->HitDuration);
	}

	// Just died - Start dying sequence
	if (!SimulatedState.bIsDead && InState.bIsDead)
	{
		DyingAction.Play(Config->DyingDuration);
	}

	// Just attacked - Start Attack action
	if (InState.StepsToAttack != SimulatedState.StepsToAttack && InState.StepsToAttack == 0)
	{
		AttackAction.Play(Config->AttackDuration);
	}

	// Changed position - move
	if (SimulatedState.GridPosition != InState.GridPosition)
	{
		DesiredLocation = Grid->GridToWorld(InState.GridPosition);
		PrevLocation = Grid->GridToWorld(SimulatedState.GridPosition);

		// Make sure we reached previous target spot
		SetActorLocation(PrevLocation);
		
		const double PhaseDuration = SimulatedState.Timestamp > 0.0 ? InState.Timestamp - SimulatedState.Timestamp : Config->SimulationTimeStep;
		const double MoveDuration = PhaseDuration;// / Config->MoveRate; //<< When queue works

		MovementAction.Play(MoveDuration);

	}

	// Cache last simulated state
	SimulatedState = InState;
}

void ABallActor::UpdateVisuals(float DeltaTime)
{
	FString DebugState;
	
	{ // Process movement
		float Alpha = 0.0f;
		if (MovementAction.Update(DeltaTime, Alpha))
		{
			SetActorLocation(FMath::Lerp(PrevLocation, DesiredLocation, Alpha));
			DebugState.Append("\nMove");
		}
	}
	
	{ // Attack flashes
		float Alpha = 0.0f;
		if (AttackAction.Update(DeltaTime, Alpha))
		{
			// Blink few times
			float FlashAlpha = FMath::Frac(Alpha * Flashes);
			BallMaterial->SetScalarParameterValue(Param_Flash, FlashAlpha);
			DebugState.Append("\nAttack");
		}
	}
	
	{ // Hit Reaction
		float Alpha = 0.0f;
		if (HitAction.Update(DeltaTime, Alpha))
		{
			const float Time = GetWorld()->GetTimeSeconds();
			
			// Position shake
			const FVector ShakeOffset = FVector(
				FMath::Sin(Time * HitShakeSpeed) * HitShakeIntensity * (1.0f - Alpha),
				FMath::Cos(Time * HitShakeSpeed) * HitShakeIntensity * (1.0f - Alpha),
				0.0f
			);
			
			SetActorLocation(DesiredLocation + ShakeOffset);
			DebugState.Append("\nHit");
		}
	}

	{ // Dying (Dissolve)
		float Alpha = 0.0f;
		if (DyingAction.Update(DeltaTime, Alpha))
		{
			BallMaterial->SetScalarParameterValue(Param_Dissolve, Alpha);

			if (!DyingAction.bPlaying)
			{
				SetActorHiddenInGame(true);
			}
		}
	}

	if (SimulatedState.bIsDead)
	{
		DebugState.Append("\nDead");
	}

	DrawDebugString(GetWorld(), FVector::UpVector * 100.0, FString::Printf(TEXT("HP: %i / %i%s"),
		SimulatedState.HP, InitialHP, *DebugState), this, FColor::White, 0);
}