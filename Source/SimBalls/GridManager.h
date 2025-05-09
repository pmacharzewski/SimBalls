// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GridManager.generated.h"

UCLASS()
class SIMBALLS_API AGridManager : public AActor
{
	GENERATED_BODY()

public:
	AGridManager();

	static AGridManager* FindOrSpawnGrid(const UObject* WorldContextObject);
	
	TArray<FIntPoint> FindPathAStar(const FIntPoint& Start, const FIntPoint& Goal);
	TArray<FIntPoint> FindPathSimple(const FIntPoint& Start, const FIntPoint& Goal);
	
	bool ShouldRegeneratePath(const FIntPoint& Start, const FIntPoint& Goal, const TArray<FIntPoint>& InPath, int32 Range) const;
	
	void SetObstacles(const TSet<FIntPoint>& InObstacles);
	void UpdateObstacle(const FIntPoint& PrevObstacle, const FIntPoint& NewObstacle);
	
	// Helper methods
	inline int32 GridPositionToIndex(const FIntPoint& GridPos) const;
	inline FIntPoint IndexToGridPosition(int32 Index) const;
	inline FVector GridToWorld(const FIntPoint& GridPos) const;
	inline bool IsAtRange(const FIntPoint& A, const FIntPoint& B, int32 Range) const;

protected:
	// Begin Base class Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true;};
	// End Base class Interface
private:
	
	static TWeakObjectPtr<AGridManager> GridManager;

	TSet<FIntPoint> Obstacles;
	
	UPROPERTY(EditAnywhere)
	int32 GridSize = 100;

	UPROPERTY(EditAnywhere)
	int32 CellSize = 100;
	
	void DebugDrawGrid(float DeltaTime);
};

int32 AGridManager::GridPositionToIndex(const FIntPoint& GridPos) const
{
	return FMath::Clamp(GridPos.X, 0, GridSize - 1) * GridSize + FMath::Clamp(GridPos.Y, 0, GridSize - 1);
}

FIntPoint AGridManager::IndexToGridPosition(int32 Index) const
{
	return FIntPoint(Index / GridSize, Index % GridSize);
}

FVector AGridManager::GridToWorld(const FIntPoint& GridPos) const
{
	const float HalfSize = GridSize * CellSize * 0.5;
	return GetActorLocation() + FVector(GridPos.X * CellSize + CellSize * 0.5 - HalfSize, GridPos.Y * CellSize + CellSize * 0.5 - HalfSize, 0.f);
}

bool AGridManager::IsAtRange(const FIntPoint& A, const FIntPoint& B, int32 Range) const
{
	return FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y) <= Range;
}