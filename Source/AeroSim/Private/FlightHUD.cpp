#include "FlightHUD.h"

#include "AeroSimGameMode.h"
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
    switch (Mode)
    {
    case EFlightCameraMode::FPV: return TEXT("FPV / NOSE");
    case EFlightCameraMode::Gimbal: return TEXT("GIMBAL");
    case EFlightCameraMode::Chase: return TEXT("NEAR FOLLOW");
    case EFlightCameraMode::FarFollow: return TEXT("FAR FOLLOW");
    default: return TEXT("UNKNOWN");
    }
}

const TCHAR* MissionStateText(const EAeroSimMissionState State)
{
    switch (State)
    {
    case EAeroSimMissionState::Hangar: return TEXT("HANGAR");
    case EAeroSimMissionState::Preflight: return TEXT("PREFLIGHT");
    case EAeroSimMissionState::Flight: return TEXT("FLIGHT");
    case EAeroSimMissionState::Result: return TEXT("RESULT");
    case EAeroSimMissionState::Replay: return TEXT("REPLAY");
    default: return TEXT("UNKNOWN");
    }
}

const TCHAR* WeatherText(const EAeroSimWeather Weather)
{
    switch (Weather)
    {
    case EAeroSimWeather::Clear: return TEXT("CLEAR");
    case EAeroSimWeather::Overcast: return TEXT("OVERCAST");
    case EAeroSimWeather::Rain: return TEXT("RAIN");
    default: return TEXT("UNKNOWN");
    }
}
}

FVector2D AFlightHUD::CalculateSafeMargin(const float ViewportWidth, const float ViewportHeight)
{
    return FVector2D(
        FMath::Max(24.0F, ViewportWidth * 0.04F),
        FMath::Max(24.0F, ViewportHeight * 0.04F));
}

void AFlightHUD::DrawHUD()
{
    Super::DrawHUD();
    const AFlightPawn* Pawn = PlayerOwner != nullptr ? Cast<AFlightPawn>(PlayerOwner->GetPawn()) : nullptr;
    if (Pawn == nullptr || Canvas == nullptr)
    {
        return;
    }

    const AAeroSimGameMode* GameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<AAeroSimGameMode>() : nullptr;
    const FFlightHudData Data = Pawn->GetHudData();
    const EAeroSimMissionState MissionState = GameMode != nullptr
        ? GameMode->GetMissionFlow().State
        : EAeroSimMissionState::Flight;
    const FEnvironmentState Environment = GameMode != nullptr
        ? GameMode->GetEnvironmentState()
        : FEnvironmentState();
    const FFlightResultMetrics Result = Pawn->GetResultMetrics();

    // Lightweight camera-space rain curtain. World streaks and wet surfaces provide depth cues;
    // these deterministic foreground lines keep Nose/Follow views unmistakably rainy.
    if (Environment.Weather == EAeroSimWeather::Rain)
    {
        const float TimeOffset = GetWorld() != nullptr ? FMath::Fmod(GetWorld()->GetTimeSeconds() * 760.0F, Canvas->ClipY + 180.0F) : 0.0F;
        DrawRect(FLinearColor(0.025F, 0.07F, 0.12F, 0.17F), 0.0F, 0.0F, Canvas->ClipX, Canvas->ClipY);
        for (int32 Index = 0; Index < 72; ++Index)
        {
            const float X = FMath::Fmod(static_cast<float>(Index * 173), Canvas->ClipX + 120.0F) - 60.0F;
            const float BaseY = FMath::Fmod(static_cast<float>(Index * 97) + TimeOffset, Canvas->ClipY + 180.0F) - 180.0F;
            const float Length = 58.0F + static_cast<float>((Index * 31) % 72);
            const float Thickness = Index % 5 == 0 ? 2.2F : 1.15F;
            DrawLine(X, BaseY, X - Length * 0.22F, BaseY + Length, FLinearColor(0.62F, 0.78F, 0.90F, Index % 5 == 0 ? 0.72F : 0.46F), Thickness);
        }
    }

    const FVector2D SafeMargin = CalculateSafeMargin(Canvas->ClipX, Canvas->ClipY);
    const float SafeX = SafeMargin.X;
    const float SafeY = SafeMargin.Y;
    constexpr float PanelWidth = 520.0F;
    constexpr float PanelHeight = 172.0F;
    DrawRect(FLinearColor(0.0F, 0.015F, 0.025F, 0.72F), SafeX, SafeY, PanelWidth, PanelHeight);

    FString Status = FString::Printf(
        TEXT("%s  |  %s  |  vFOV %.1f\n%s\n%s  |  GIMBAL P%+.1f Y%+.1f\nALT %6.1f m   SPD %5.1f m/s   THR %+4.2f\n%s 16:30  SEED %d\n[M] NEXT [T] WEATHER [C] CAMERA [ARROWS] GIMBAL [P] PAUSE"),
        FlightPhaseText(Data.Phase),
        CameraModeText(Data.CameraMode),
        Pawn->GetVerticalFov(),
        *Data.AircraftName,
        *Data.AssetStatus,
        Data.GimbalPitchDegrees,
        Data.GimbalYawDegrees,
        Data.AltitudeMeters,
        Data.SpeedMetersPerSecond,
        Data.Throttle,
        WeatherText(Environment.Weather),
        Environment.Seed);
    DrawText(Status, FLinearColor::White, SafeX + 12.0F, SafeY + 9.0F, GEngine->GetSmallFont(), 1.0F, false);

    const float WorkflowWidth = 350.0F;
    const float WorkflowX = (Canvas->ClipX - WorkflowWidth) * 0.5F;
    DrawRect(FLinearColor(0.04F, 0.14F, 0.19F, 0.82F), WorkflowX, SafeY, WorkflowWidth, 34.0F);
    DrawText(
        FString::Printf(TEXT("PHASE 3  /  %s"), MissionStateText(MissionState)),
        FLinearColor(0.90F, 0.96F, 1.0F),
        WorkflowX + 18.0F,
        SafeY + 8.0F,
        GEngine->GetSmallFont(),
        1.0F,
        false);

    if (Pawn->IsResumeCountdownActive())
    {
        const FString Countdown = FString::Printf(TEXT("INPUT CENTERED  /  RESUME IN %d"), FMath::CeilToInt(Pawn->GetResumeCountdownSeconds()));
        DrawRect(FLinearColor(0.85F, 0.28F, 0.02F, 0.88F), WorkflowX, SafeY + 46.0F, WorkflowWidth, 42.0F);
        DrawText(Countdown, FLinearColor::White, WorkflowX + 20.0F, SafeY + 58.0F, GEngine->GetSmallFont(), 1.0F, false);
    }

    if (MissionState == EAeroSimMissionState::Result || MissionState == EAeroSimMissionState::Replay)
    {
        const float ResultX = Canvas->ClipX - SafeX - 320.0F;
        DrawRect(FLinearColor(0.02F, 0.08F, 0.10F, 0.78F), ResultX, SafeY, 320.0F, 94.0F);
        DrawText(
            FString::Printf(TEXT("RESULT\nTIME %.1f s   DIST %.1f m\nMAX ALT %.1f m   SAMPLES %d"), Result.DurationSeconds, Result.DistanceMeters, Result.MaximumAltitudeMeters, Result.Samples),
            FLinearColor::White,
            ResultX + 12.0F,
            SafeY + 8.0F,
            GEngine->GetSmallFont(),
            1.0F,
            false);
    }
}
