
#include "GridManager.h"
#include "EngineUtils.h"
#include <queue>

#include "SimulationConfig.h"

TWeakObjectPtr<AGridManager> AGridManager::GridManager = nullptr;

static bool bShowDebugGrid = false;
static FAutoConsoleVariableRef CVarShowDebugGrid(
		TEXT("Sim.ShowDebugGrid"),
		bShowDebugGrid,
		TEXT("Enables debug grid drawing."),
		ECVF_Cheat
	);

AGridManager::AGridManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

AGridManager* AGridManager::FindOrSpawnGrid(const UObject* WorldContextObject)
{
	// Already cached - return
	if (AGridManager* FoundGrid = GridManager.Get())
	{
		return FoundGrid;
	}
	
	if (UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr)
	{
		// Try to find GridManager in level
		for (TActorIterator<AGridManager> It(World); It; ++It)
		{
			GridManager = *It;
			return *It;
		}

		// There was no Grid placed in level - spawn new
		FActorSpawnParameters Params;
		AGridManager* NewGrid = World->SpawnActor<AGridManager>(Params);
		GridManager = NewGrid;
		return NewGrid;
		
	}
	
	return nullptr;
}

TArray<FIntPoint> AGridManager::FindPathAStar(const FIntPoint& Start, const FIntPoint& Goal)
{
	struct FPathNode
	{
		FIntPoint Pos;
		int32 G = 0;
		int32 F = 0;
		int32 ParentIndex = INDEX_NONE;

		FPathNode() {}
		FPathNode(FIntPoint InPos, int32 InG, int32 InF, int32 InParent) 
			: Pos(InPos), G(InG), F(InF), ParentIndex(InParent) {}

		// lowest final score gets highest priority
		bool operator<(const FPathNode& Other) const { return F > Other.F; } // min-heap
	};

	TArray<FIntPoint> Path;

	if (Start == Goal)
	{
		return Path;
	}
	
	// Unblock the start and goal so we can generate the path to ball target that by default is not walkable
	TSet<FIntPoint> TempObstacles = Obstacles;
	TempObstacles.Remove(Start);
	TempObstacles.Remove(Goal);
	
	TArray<FPathNode> NodePool;
	TMap<FIntPoint, int32> PosToIndex;
	TSet<FIntPoint> ClosedSet;
	std::priority_queue<FPathNode> OpenQueue;

	// manhatan heuristic (4 directions)
	auto Heuristic = [](const FIntPoint& A, const FIntPoint& B)
	{
		return FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y);
	};

	// Initial node
	NodePool.Add(FPathNode(Start, 0, Heuristic(Start, Goal), INDEX_NONE));
	PosToIndex.Add(Start, 0);
	OpenQueue.push(NodePool[0]);
	
	static const TArray<FIntPoint> Directions = { {1,0}, {-1,0}, {0,1}, {0,-1} };
	
	while (!OpenQueue.empty())
	{
		FPathNode CurrentNode = OpenQueue.top();
		OpenQueue.pop();

		const int32* CurrentIndexPtr = PosToIndex.Find(CurrentNode.Pos);
		// should never happen
		if (!CurrentIndexPtr)
		{
			continue;
		}
		
		const int32 CurrentIndex = *CurrentIndexPtr;
		
		if (CurrentNode.Pos == Goal)
		{
			// Reconstruct path
			int32 TraceIndex = CurrentIndex;
			while (TraceIndex != INDEX_NONE)
			{
				int32 NextIndex = NodePool[TraceIndex].ParentIndex;
				// Skip first element
				if (NextIndex == INDEX_NONE)
				{
					break;
				}
				Path.Insert(NodePool[TraceIndex].Pos, 0);
				TraceIndex = NextIndex;
			}
			return Path;
		}

		if (ClosedSet.Contains(CurrentNode.Pos))
		{
			continue;
		}
		
		ClosedSet.Add(CurrentNode.Pos);

		for (const FIntPoint& Dir : Directions)
		{
			FIntPoint Neighbor = CurrentNode.Pos + Dir;
            
			// Boundary check
			if (Neighbor.X < 0 || Neighbor.Y < 0 || Neighbor.X >= GridSize || Neighbor.Y >= GridSize)
			{
				continue;	
			}
			
			// Obstacle/closed set check
			if (TempObstacles.Contains(Neighbor) || ClosedSet.Contains(Neighbor))
			{
				continue;
			}
			
			const int32 GScore = CurrentNode.G + 1;

			if (const int32* ExistingIndex = PosToIndex.Find(Neighbor))
			{
				FPathNode& ExistingNode = NodePool[*ExistingIndex];
				
				// Existing node - check if this path is better
				if (GScore < ExistingNode.G)
				{
					ExistingNode.G = GScore;
					ExistingNode.F = GScore + Heuristic(Neighbor, Goal);
					ExistingNode.ParentIndex = CurrentIndex;
					OpenQueue.push(ExistingNode);
				}
			}
			else
			{
				// New node
				const int32 NewIndex = NodePool.Add(FPathNode(Neighbor, GScore, GScore + Heuristic(Neighbor, Goal), CurrentIndex));
				PosToIndex.Add(Neighbor, NewIndex);
				OpenQueue.push(NodePool[NewIndex]);
			}
		}
	}

	// No path found
	return Path; 
}

TArray<FIntPoint> AGridManager::FindPathSimple(const FIntPoint& Start, const FIntPoint& Goal)
{
	TArray<FIntPoint> Path;
	
	FIntPoint NextPathPoint = Start;
	
	Path.Add(Start);
	
	while (NextPathPoint != Goal)
	{
		if (NextPathPoint.X < Goal.X)
		{
			Path.Add(++NextPathPoint.X);
		}
		else if (NextPathPoint.X > Goal.X)
		{
			Path.Add(--NextPathPoint.X);
		}
		else if (NextPathPoint.Y < Goal.Y)
		{
			Path.Add(++NextPathPoint.Y);
		}
		else if (NextPathPoint.Y > Goal.Y)
		{
			Path.Add(--NextPathPoint.Y);
		}
	}

	return Path;
}

bool AGridManager::ShouldRegeneratePath(const FIntPoint& Start, const FIntPoint& Goal, const TArray<FIntPoint>& InPath) const
{
	if (InPath.IsEmpty())
	{
		return true;
	}

	// Goal changed - other ball moved away
	if (Goal != InPath.Last())
	{
		return true;
	}

	// Reached end already
	if (Start == Goal)
	{
		return true;
	}
	
	//Ignore Start/End for obstacle testing
	TSet<FIntPoint> TempObstacles = Obstacles;
	TempObstacles.Remove(Start);
	TempObstacles.Remove(Goal);

	bool bFoundStart = false;
	
	for (const FIntPoint& Pos : InPath)
	{
		if (!bFoundStart)
		{
			bFoundStart = Start == Pos;
			continue;
		}
		
		// Boundary check
		if (Pos.X < 0 || Pos.Y < 0 || Pos.X >= GridSize || Pos.Y >= GridSize)
		{
			return true;
		}
			
		// Obstacle/closed set check
		if (TempObstacles.Contains(Pos))
		{
			return true;
		}
	}

	return false;

}

void AGridManager::SetObstacles(const TSet<FIntPoint>& InObstacles)
{
	Obstacles = InObstacles;
}

void AGridManager::UpdateObstacle(const FIntPoint& PrevObstacle, const FIntPoint& NewObstacle)
{
	if (PrevObstacle == NewObstacle)
	{
		return;
	}

	Obstacles.Remove(PrevObstacle);
	Obstacles.Add(NewObstacle);
}

void AGridManager::BeginPlay()
{
	Super::BeginPlay();

	GridSize = USimulationConfig::Get()->GridSize;
	CellSize = USimulationConfig::Get()->CellSize;
}

void AGridManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	GridManager.Reset();
}

// Called every frame
void AGridManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bShowDebugGrid)
	{
		DebugDrawGrid(DeltaTime);	
	}
}

void AGridManager::DebugDrawGrid(float DeltaTime)
{
	const float HalfGridSize = GridSize * CellSize * 0.5f;
	const float GridZ = -80.f;
	
	for (int32 X = 0; X <= GridSize; ++X)
	{
		const float XPos = -HalfGridSize + X * CellSize;
		const FVector Start = FVector(XPos, -HalfGridSize, GridZ);
		const FVector End = FVector(XPos, HalfGridSize, GridZ);
		DrawDebugLine(GetWorld(), Start, End, FColor::Green, false, 0, 0, 2);
	}
	
	for (int32 Y = 0; Y <= GridSize; ++Y)
	{
		const float YPos = -HalfGridSize + Y * CellSize;
		const FVector Start = FVector(-HalfGridSize, YPos, GridZ);
		const FVector End = FVector(HalfGridSize, YPos, GridZ);
		DrawDebugLine(GetWorld(), Start, End, FColor::Green, false, 0, 0, 2);
	}
}
