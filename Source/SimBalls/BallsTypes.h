
#pragma once

#include "CoreMinimal.h"

enum class EBallTeamColor : uint8
{
	Red,
	Blue,
	
	Max_None,
};

struct FBallSimulatedState
{
	int32 ID = INDEX_NONE;
	int32 TargetID = INDEX_NONE;
	int32 HP = INDEX_NONE;
	int32 StepsToAttack = INDEX_NONE;
	int32 PathIndex = 0;
	int32 Damage = 0;
	
	FIntPoint GridPosition = FIntPoint::ZeroValue;
	EBallTeamColor Team = EBallTeamColor::Max_None;
	TArray<FIntPoint> GridPath;
	
	bool bIsDead = false;
	double Timestamp = 0.0;
	
	FBallSimulatedState() = default;
	FBallSimulatedState(int32 InID, int32 InTargetID, int32 InHP, int32 InStepsToAttack, const FIntPoint& InGridPosition, const EBallTeamColor InTeam)
	: ID(InID)
	, TargetID(InTargetID)
	, HP(InHP)
	, StepsToAttack(InStepsToAttack)
	, GridPosition(InGridPosition)
	, Team(InTeam)
	{}

	bool IsTargetValid() const
	{
		return TargetID != INDEX_NONE && ID != TargetID;
	}
	
	bool IsValid() const
	{
		return !(ID == INDEX_NONE || HP == INDEX_NONE || StepsToAttack == INDEX_NONE || Team == EBallTeamColor::Max_None);
	}
};

struct FBallTimedAction
{
	float Duration = 1.0f;
	float Time = 0.0f;
	bool bPlaying = false;
	
	bool Update(float DeltaTime, float& OutAlpha)
	{
		if (bPlaying)
		{
			Time = FMath::Min(Time + DeltaTime, Duration);
			
			OutAlpha = Time / Duration;;

			if (OutAlpha >= 1.0f)
			{
				bPlaying = false;
			}

			return true;
		}
		
		return false;
	}

	void Play(float InDuration)
	{
		Duration = InDuration;
		Time = 0;
		bPlaying = true;
	}
};
