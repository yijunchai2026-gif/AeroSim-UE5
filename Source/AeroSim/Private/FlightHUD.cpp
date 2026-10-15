#include "FlightHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "FlightPawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
const TCHAR* FlightPhaseText(const EFlightPhase Phase)
{
    switch (Phase)
    {
    case EFlightPhase::Grounded: return TEXT("GROUNDED");
    case EFlightPhase::Takeoff: return TEXT("TAKEOFF");
    case EFlightPhase::Flight: return TEXT("FLIGHT");
    case EFlightPhase::Landing: return TEXT("LANDING");
    case EFlightPhase::Paused: return TEXT("PAUSED");
    default: return TEXT("UNKNOWN");
    }
}

const TCHAR* CameraModeText(const EFlightCameraMode Mode)
{
    return Mode == EFlightCameraMode::FPV ? TEXT("FPV") : TEXT("CHASE");
}
}

void AFlightHUD::DrawHUD()
{
    Super::DrawHUD();
    const AFlightPawn* Pawn = PlayerOwner != nullptr ? Cast<AFlightPawn>(PlayerOwner->GetPawn()) : nullptr;
    if (Pawn == nullptr || Canvas == nullptr)
    {
        return;
    }

    const FFlightHudData Data = Pawn->GetHudData();
    const FString Text = FString::Printf(
        TEXT("%s  |  %s\nALT %6.1f m   SPD %5.1f m/s\nTHR %+4.2f%s"),
        FlightPhaseText(Data.Phase),
        CameraModeText(Data.CameraMode),
        Data.AltitudeMeters,
        Data.SpeedMetersPerSecond,
        Data.Throttle,
        Data.Phase == EFlightPhase::Paused ? TEXT("   PAUSED / INPUT SAFE") : TEXT(""));

    constexpr float Margin = 24.0F;
    constexpr float PanelWidth = 330.0F;
    constexpr float PanelHeight = 82.0F;
    DrawRect(FLinearColor(0.0F, 0.0F, 0.0F, 0.42F), Margin, Margin, PanelWidth, PanelHeight);
    DrawText(Text, FLinearColor::White, Margin + 12.0F, Margin + 9.0F, GEngine->GetSmallFont(), 1.0F, false);
}
