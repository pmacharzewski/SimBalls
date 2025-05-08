#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "BallsTypes.h"
#include "SimBallsGameState.generated.h"

class AGridManager;
class ABallActor;
class USimulationConfig;
/**
 * 
 */
UCLASS()
class SIMBALLS_API ASimBallsGameState : public AGameState
{
	GENERATED_BODY()

public:
	ASimBallsGameState();

protected:
	// Start Base Class Interface
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	// End Base Class Interface
private:
	/**
	 * Initializes all ball states with random positions and team assignments.
	 * Creates both simulated states and visual actors.
	 */
	void InitializeBalls();
	/**
	 * Manages the simulation time progression.
	 * Processes all pending simulation steps based on elapsed time.
	 */
	void RunSimulation(float DeltaSeconds);
	/**
	 * Advances the simulation by one time step.
	 * @param Timestamp - The current simulation time
	 */
	void AdvanceSimulation(double Timestamp);
	/**
	 * Prepares all ball states for a new simulation step.
	 * Resets temporary flags.
	 */
	void PrepareBallStates(double Timestamp);
	/**
	 * Simulates a single ball's behavior for the current time step.
	 */
	void SimulateBallState(FBallSimulatedState& State);
	/**
	 * Processes combat logic for a ball (attacking and damage).
	 * @return true if combat occurred, false otherwise
	 */
	bool ProcessCombatState(FBallSimulatedState& State);
	/**
	 * Processes movement logic for a ball.
	 * @param State - The ball state to process (will be modified)
	 * @return true if movement occurred, false otherwise
	 */
	bool ProcessMovementState(FBallSimulatedState& State);
	/**
	 * Applies movement to a ball state based on its current path.
	 */
	void ApplyMovement(FBallSimulatedState& State);
	/**
	 * Applies damage from an attacker to a receiver.
	 * @param Attacker - The attacking ball state
	 * @param Receiver - The receiving ball state (will be modified)
	 */
	void ApplyDamage(FBallSimulatedState& Attacker, FBallSimulatedState& Receiver);
	/**
	 * Finds the closest enemy for a given ball state.
	 * @return true if an enemy was found, false otherwise
	 */
	bool FindClosestEnemy(const FBallSimulatedState& State, int32& OutEnemy, int32& OutDistance);
	/**
	 * Creates a new ball state with and appends to BallStates.
	 * @param StateID - Unique identifier for the new ball
	 * @return Reference to the newly created ball state
	 */
	FBallSimulatedState& CreateBallState(int32 StateID);
	/**
	 * Creates and initializes a visual ball actor based on SimulatedState.
	 * @param BallState - The simulated state to visualize
	 * @return Created ball actor
	 */
	ABallActor* CreateBallActor(const FBallSimulatedState& BallState);
	
	// Cached Simulation settings
	UPROPERTY()
	TObjectPtr<const USimulationConfig> Config = nullptr;

	// Grid management system for path finding
	UPROPERTY()
	TWeakObjectPtr<AGridManager> Grid = nullptr;

	// Collection of all ball simulation states
	TArray<FBallSimulatedState> BallStates;

	// Collection of all visual ball actors
	UPROPERTY()
	TArray<TObjectPtr<ABallActor>> BallActors;
	
	// Random number generator for deterministic simulation
	UPROPERTY()
	FRandomStream RandomStream;

	// Track simulation time
	double SimulationTime = 0.0;

private:
	void AdjustCamera(float DeltaSeconds = 0);
};
