
#pragma once

#include "CoreMinimal.h"
#include "BallsTypes.h"
#include "GameFramework/Actor.h"
#include "BallActor.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

UCLASS()
class SIMBALLS_API ABallActor : public AActor
{
	GENERATED_BODY()

public:
	ABallActor();
	/**
     * Initializes the ball with a given simulated state.
     */
	void InitBall(const FBallSimulatedState& InState);
	/**
     * Applies a new simulated state to the ball, triggering visual effects.
     * @param InState - The new state to apply
     */
	void ApplySimulatedState(const FBallSimulatedState& InState);
	/**
     * Updates visual effects and interpolations each frame.
     * Handles movement lerping, attack flashes, hit reactions and death effects.
     * @param DeltaTime - Time since last frame update
     */
	void UpdateVisuals(float DeltaTime);
	
private:
	// Components
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> BallMesh = nullptr;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BallMaterial = nullptr;

	// Initial health for debug display
	int32 InitialHP = 1;

	// Cache positions to lerp between
	FVector PrevLocation = FVector::ZeroVector;
	FVector DesiredLocation = FVector::ZeroVector;

	// Actions
	FBallTimedAction MovementAction;
	FBallTimedAction AttackAction;
	FBallTimedAction HitAction;
	FBallTimedAction DyingAction;

	// Last cached state
	FBallSimulatedState SimulatedState;
};
