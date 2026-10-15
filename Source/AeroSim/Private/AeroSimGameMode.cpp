#include "AeroSimGameMode.h"

#include "FlightHUD.h"
#include "FlightPawn.h"
#include "PacificCoastGraybox.h"

AAeroSimGameMode::AAeroSimGameMode()
{
    DefaultPawnClass = AFlightPawn::StaticClass();
    HUDClass = AFlightHUD::StaticClass();
}

void AAeroSimGameMode::StartPlay()
{
    Super::StartPlay();
    if (UWorld* World = GetWorld())
    {
        Graybox = World->SpawnActor<APacificCoastGraybox>();
    }
}
