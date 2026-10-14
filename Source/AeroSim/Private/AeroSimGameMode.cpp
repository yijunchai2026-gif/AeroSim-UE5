#include "AeroSimGameMode.h"
#include "FlightPawn.h"

AAeroSimGameMode::AAeroSimGameMode()
{
    DefaultPawnClass = AFlightPawn::StaticClass();
}
