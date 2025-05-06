#include "SimBallsGameMode.h"
#include "SimBallsGameState.h"

ASimBallsGameMode::ASimBallsGameMode()
{
	GameStateClass = ASimBallsGameState::StaticClass();
}