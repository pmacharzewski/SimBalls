#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SimulationConfig.generated.h"

UCLASS(config=Game, defaultconfig, meta=(DisplayName="Simulation Configuration"))
class SIMBALLS_API USimulationConfig : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/**
	 * Time interval (in seconds) between simulation updates
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="General", meta=(ClampMin="0.001"))
	float SimulationTimeStep = 0.1f;
	/**
	 * Seed value for random number generation in the simulation
	 * Note: This should come from server but just for testing
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="General")
	int32 Seed = 100;
	/**
	 * Number of cells along axis in movement grid
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="General", meta=(ClampMin="1"))
	int32 GridSize = 100;
	/** 
	 * World size of a single grid cell
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="General", meta=(ClampMin="1"))
	int32 CellSize = 100;
	/** 
	* Minimum health points for balls
	*/
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Balls", meta=(ClampMin="1"))
	int32 MinHP = 2;
	/**
	 * Maximum health points for balls
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Balls", meta=(ClampMin="1"))
	int32 MaxHP = 5;
	/** 
	 * How many grid cells a ball can move per simulation step
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Balls", meta=(ClampMin="1"))
	int32 MoveRate = 1;
	/**
	 * Attack range in grid cells
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Balls", meta=(ClampMin="1"))
	int32 AttackRange = 2;
	/** 
	 * Number of simulation steps between attack attempts
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Balls", meta=(ClampMin="1"))
	int32 AttackInterval = 10;
	/** 
	 * Number of balls to spawn in the simulation
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Balls", meta=(ClampMin="1"))
	int32 NumBalls = 4;
	/** 
	 * Duration of an attack action
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Visuals", meta=(ClampMin="0.1"))
	float AttackDuration = 0.5f;
	/**
	 * Duration of hit reaction
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Visuals", meta=(ClampMin="0.1"))
	float HitDuration = 0.25f;
	/** 
	 * Duration of dying action
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Visuals", meta=(ClampMin="0.1"))
	float DyingDuration = 2.0f;

	// Helper function to easily access these settings
	static const USimulationConfig* Get()
	{
		return GetDefault<USimulationConfig>();
	}
};
