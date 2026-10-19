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
    case EFlightPhase::Paused: return TEXT("PAUSED / SETTINGS");
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

const TCHAR* MissionStageText(const EPhase4MissionStage Stage)
{
    switch (Stage)
    {
    case EPhase4MissionStage::Hangar: return TEXT("HANGAR");
    case EPhase4MissionStage::Preflight: return TEXT("PREFLIGHT");
    case EPhase4MissionStage::Takeoff: return TEXT("TAKEOFF");
    case EPhase4MissionStage::Inspection: return TEXT("SEQUENTIAL INSPECTION");
    case EPhase4MissionStage::ReturnToHome: return TEXT("RETURN TO HOME");
    case EPhase4MissionStage::Landing: return TEXT("LANDING");
    case EPhase4MissionStage::Result: return TEXT("RESULT");
    case EPhase4MissionStage::Replay: return TEXT("REPLAY");
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

const TCHAR* WindowModeText(const EPhase7WindowMode Mode)
{
    switch (Mode)
    {
    case EPhase7WindowMode::Windowed: return TEXT("WINDOWED");
    case EPhase7WindowMode::Borderless: return TEXT("BORDERLESS");
    case EPhase7WindowMode::Fullscreen: return TEXT("FULLSCREEN");
    default: return TEXT("UNKNOWN");
    }
}
}

FVector2D AFlightHUD::CalculateSafeMargin(const float ViewportWidth, const float ViewportHeight)
{
    return FVector2D(FMath::Max(24.0F, ViewportWidth * 0.04F), FMath::Max(24.0F, ViewportHeight * 0.04F));
}

void AFlightHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!bShowHUD) return;
    const AFlightPawn* Pawn = PlayerOwner != nullptr ? Cast<AFlightPawn>(PlayerOwner->GetPawn()) : nullptr;
    if (Pawn == nullptr || Canvas == nullptr) return;

    const AAeroSimGameMode* GameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<AAeroSimGameMode>() : nullptr;
    const FFlightHudData Data = Pawn->GetHudData();
    const FEnvironmentState Environment = GameMode != nullptr ? GameMode->GetEnvironmentState() : FEnvironmentState();
    // Native CommonUI owns all product menu pages; Canvas is only a flight perimeter layer.
    if (GameMode != nullptr && GameMode->GetProductPage() != EProductPage::Flight) return;
    FPhase4MissionSession FallbackMission;
    FallbackMission.Reset();
    const FPhase4MissionSession& Mission = GameMode != nullptr ? GameMode->GetPhase4Mission() : FallbackMission;

    if (Environment.Weather == EAeroSimWeather::Rain)
    {
        const float TimeOffset = GetWorld() != nullptr ? FMath::Fmod(GetWorld()->GetTimeSeconds() * 760.0F, Canvas->ClipY + 180.0F) : 0.0F;
        DrawRect(FLinearColor(0.025F, 0.07F, 0.12F, 0.17F), 0.0F, 0.0F, Canvas->ClipX, Canvas->ClipY);
        for (int32 Index = 0; Index < 72; ++Index)
        {
            const float X = FMath::Fmod(static_cast<float>(Index * 173), Canvas->ClipX + 120.0F) - 60.0F;
            const float BaseY = FMath::Fmod(static_cast<float>(Index * 97) + TimeOffset, Canvas->ClipY + 180.0F) - 180.0F;
            const float Length = 58.0F + static_cast<float>((Index * 31) % 72);
            DrawLine(X, BaseY, X - Length * 0.22F, BaseY + Length, FLinearColor(0.62F, 0.78F, 0.90F, 0.52F), 1.2F);
        }
    }

    const FVector2D Margin = CalculateSafeMargin(Canvas->ClipX, Canvas->ClipY);
    const float Left = Margin.X;
    const float Top = Margin.Y;
    const float RightWidth = 390.0F;
    const float Right = Canvas->ClipX - Margin.X - RightWidth;

    if (Data.Phase == EFlightPhase::Paused && !Pawn->IsResumeCountdownActive())
    {
        DrawRect(FLinearColor(0.04F, 0.11F, 0.15F, 0.94F), Left, Canvas->ClipY - Margin.Y - 120.0F, 660.0F, 54.0F);
        DrawText(Data.bWaterContact ? TEXT("WATER CONTACT / SAFE PAUSE\nWATER IS NOT LANDABLE. PRESS R TO RESTART FROM THE MAT.") : TEXT("SAFE PAUSE / INPUT NEUTRAL\nRELEASE CONTROLS, THEN ENTER / SPACE TO RESUME"),
            FLinearColor(1.0F, 0.72F, 0.29F), Left + 12.0F, Canvas->ClipY - Margin.Y - 108.0F, GEngine->GetSmallFont(), 1.0F, false);
    }

    if (GameMode != nullptr && GameMode->IsDisplaySettingsOpen())
    {
        const FProductDisplaySettings& Display = GameMode->GetDisplaySettings();
        const FPhase7Settings& Visible = Display.GetVisible();
        const float PanelWidth = FMath::Min(520.0F, Canvas->ClipX * 0.43F);
        const float PanelLeft = Canvas->ClipX - Margin.X - PanelWidth;
        DrawRect(FLinearColor(0.015F, 0.035F, 0.05F, 0.97F), PanelLeft, Top, PanelWidth, Canvas->ClipY - Margin.Y * 2.0F);
        DrawText(TEXT("DISPLAY SETTINGS\nSAFE PAUSE / INPUT NEUTRAL"), FLinearColor(0.29F, 0.84F, 1.0F), PanelLeft + 24.0F, Top + 22.0F, GEngine->GetMediumFont(), 1.0F, false);
        DrawText(FString::Printf(TEXT("%s  RESOLUTION     <  %d x %d  >\n%s  WINDOW MODE    <  %s  >\n\n%s  PREVIEW\n%s  APPLY + SAVE\n%s  CANCEL\n%s  ROLLBACK"),
            Display.FocusedRow == 0 ? TEXT(">") : TEXT(" "), Visible.ResolutionWidth, Visible.ResolutionHeight,
            Display.FocusedRow == 1 ? TEXT(">") : TEXT(" "), WindowModeText(Visible.WindowMode),
            Display.FocusedRow == 2 ? TEXT(">") : TEXT(" "), Display.FocusedRow == 3 ? TEXT(">") : TEXT(" "),
            Display.FocusedRow == 4 ? TEXT(">") : TEXT(" "), Display.FocusedRow == 5 ? TEXT(">") : TEXT(" ")),
            FLinearColor::White, PanelLeft + 28.0F, Top + 110.0F, GEngine->GetMediumFont(), 1.0F, false);
        DrawRect(FLinearColor(0.04F, 0.15F, 0.19F, 0.94F), PanelLeft + 20.0F, Top + 390.0F, PanelWidth - 40.0F, 72.0F);
        DrawText(Display.Status, FLinearColor(0.95F, 0.82F, 0.35F), PanelLeft + 32.0F, Top + 414.0F, GEngine->GetSmallFont(), 1.0F, false);
        DrawText(TEXT("UP/DOWN Navigate   LEFT/RIGHT Change\nENTER/SPACE Select   ESC/F10 Cancel & close\nPreview never saves. Apply uses settings v2 crash-safe save."),
            FLinearColor(0.82F, 0.9F, 0.94F), PanelLeft + 24.0F, Top + 490.0F, GEngine->GetSmallFont(), 1.0F, false);
        DrawText(TEXT("FLIGHT VIEW KEPT CLEAR"), FLinearColor(0.29F, 0.84F, 1.0F), Margin.X, Canvas->ClipY - Margin.Y - 28.0F, GEngine->GetSmallFont(), 1.0F, false);
        return;
    }

    // Objective and telemetry stay on the perimeter; the central flight corridor is always unobstructed.
    DrawRect(FLinearColor(0.01F, 0.035F, 0.055F, 0.88F), Left, Top, 500.0F, 170.0F);
    DrawText(
        FString::Printf(TEXT("AEROSIM / %s\n%s\n%s\n%s  |  %s  |  vFOV %.0f\nLAUNCH ALT %.1f m  SPD %.1f m/s  THR %+.2f\nSURFACE %s  /  %s  /  SEED %d"),
            MissionStageText(Mission.Stage), *Mission.GetObjectiveText(), *Mission.GetProgressText(),
            FlightPhaseText(Data.Phase), CameraModeText(Data.CameraMode), Pawn->GetVerticalFov(),
            Data.AltitudeMeters, Data.SpeedMetersPerSecond, Data.Throttle,
            *(Data.bSurfaceClearanceValid ? FString::Printf(TEXT("%.1f m"),Data.SurfaceClearanceMeters) : FString(TEXT("NOT MEASURED"))), WeatherText(Environment.Weather), Environment.Seed),
        FLinearColor(0.92F, 0.97F, 1.0F), Left + 14.0F, Top + 12.0F, GEngine->GetSmallFont(), 1.0F, false);

    DrawRect(FLinearColor(0.025F, 0.045F, 0.055F, 0.88F), Right, Top, RightWidth, 116.0F);
    DrawText(
        FString::Printf(TEXT("TRAINING AIRCRAFT\n%s\n%s\nBUILD %s"), *Data.AircraftName, *Data.AssetStatus, *Data.BuildId),
        FLinearColor::White, Right + 14.0F, Top + 12.0F, GEngine->GetSmallFont(), 1.0F, false);

    if (Mission.Stage == EPhase4MissionStage::Hangar)
    {
        DrawRect(FLinearColor(0.04F, 0.11F, 0.15F, 0.96F), Right, Top + 132.0F, RightWidth, 280.0F);
        DrawText(TEXT("01  HANGAR / AIRCRAFT BAY\n\nAEROSIM GENERIC\nGOLDEN TRAINING QUADCOPTER\nPROTOTYPE / UNVERIFIED\n\nPACIFIC COAST INSPECTION\n60-90 s / 5 ordered checkpoints\nCLEAR 16:30 / HIGH\n\n[ ENTER / SPACE ]  CONTINUE TO PREFLIGHT\nESC / F10  DISPLAY SETTINGS\n\nM  debug shortcut only"),
            FLinearColor(0.94F, 0.98F, 1.0F), Right + 14.0F, Top + 146.0F, GEngine->GetSmallFont(), 1.0F, false);
    }
    else if (Mission.Stage == EPhase4MissionStage::Preflight)
    {
        DrawRect(FLinearColor(0.04F, 0.11F, 0.15F, 0.96F), Right, Top + 132.0F, RightWidth, 292.0F);
        DrawText(
            FString::Printf(TEXT("02  PREFLIGHT / READY TO FLY\n\n[%s] INPUT CENTERED\n[%s] CAMERA  %s / vFOV %.0f\n[%s] ENVIRONMENT  %s 16:30\n[%s] DISPLAY  %d x %d / %s\n\n[ ENTER / SPACE ]  ARM & START\nNEXT: HOLD LEFT SHIFT TO TAKE OFF\nW/S Forward/back  A/D Sideways  Q/E Turn\nShift/Ctrl Rise/descend  Release to hover\nC Camera  H Home guidance  ESC/F10 Settings  R Reset"),
                Mission.Preflight.bInputCentered ? TEXT("OK") : TEXT("READY"), Mission.Preflight.bCameraChecked ? TEXT("OK") : TEXT("READY"), CameraModeText(Data.CameraMode), Pawn->GetVerticalFov(),
                Mission.Preflight.bEnvironmentChecked ? TEXT("OK") : TEXT("READY"), WeatherText(Environment.Weather), Mission.Preflight.bQualityChecked ? TEXT("OK") : TEXT("READY"),
                GameMode->GetAppliedSettings().ResolutionWidth, GameMode->GetAppliedSettings().ResolutionHeight, WindowModeText(GameMode->GetAppliedSettings().WindowMode)),
            FLinearColor::White, Right + 14.0F, Top + 146.0F, GEngine->GetSmallFont(), 1.0F, false);
    }

    if (Mission.IsRecovering())
    {
        DrawRect(FLinearColor(0.78F, 0.22F, 0.02F, 0.94F), Right, Top + 132.0F, RightWidth, 142.0F);
        DrawText(FString::Printf(TEXT("! %s\nSHAPE: %s\n%s"), *Mission.Feedback.Headline, *Mission.Feedback.ShapeCue, *Mission.Feedback.RecoveryAction),
            FLinearColor::White, Right + 14.0F, Top + 146.0F, GEngine->GetSmallFont(), 1.0F, false);
    }

    if (Mission.Stage == EPhase4MissionStage::Result || Mission.Stage == EPhase4MissionStage::Replay)
    {
        DrawRect(FLinearColor(0.01F, 0.06F, 0.075F, 0.94F), Right, Top + 132.0F, RightWidth, 220.0F);
        DrawText(
            FString::Printf(TEXT("%s\nTOTAL %03d / 100\nSHIELD  SAFETY      %03d\nTARGET  PRECISION   %03d\nCLOCK   EFFICIENCY  %03d\nWAVE    SMOOTHNESS  %03d\n\nREPLAY DIGEST %08llX\n%s"),
                Mission.Stage == EPhase4MissionStage::Replay ? TEXT("PLAYBACK TRIANGLE / REPLAY") : TEXT("RESULT CARD"),
                Mission.Score.Total, Mission.Score.Safety, Mission.Score.Precision, Mission.Score.Efficiency,
                Mission.Score.Smoothness, static_cast<unsigned long long>(Mission.Score.ReplayDigest),
                Mission.Stage == EPhase4MissionStage::Replay ? TEXT("ALIGNED WITH RESULT") : TEXT("[ ENTER / SPACE ] OPEN REPLAY")),
            FLinearColor(0.94F, 1.0F, 0.96F), Right + 14.0F, Top + 146.0F, GEngine->GetSmallFont(), 1.0F, false);
    }

    const float HintWidth = FMath::Min(720.0F, Canvas->ClipX - Margin.X * 2.0F);
    DrawRect(FLinearColor(0.0F, 0.02F, 0.03F, 0.84F), Margin.X, Canvas->ClipY - Margin.Y - 52.0F, HintWidth, 52.0F);
    FString NextAction = Mission.GetProgressiveHint();
    if (Mission.Stage == EPhase4MissionStage::Takeoff && Data.Phase == EFlightPhase::Grounded)
    {
        NextAction = TEXT("NEXT ACTION: HOLD LEFT SHIFT TO RAISE THROTTLE AND TAKE OFF");
    }
    else if (Mission.Stage == EPhase4MissionStage::Inspection)
    {
        NextAction = FString::Printf(TEXT("NEXT ACTION: %s"), *Mission.GetProgressiveHint());
    }
    DrawText(FString::Printf(TEXT("%s   |   H RTH   ESC/F10 SETTINGS   C CAMERA   R RESET"), *NextAction),
        FLinearColor::White, Margin.X + 12.0F, Canvas->ClipY - Margin.Y - 37.0F, GEngine->GetSmallFont(), 1.0F, false);

    if (Pawn->IsResumeCountdownActive())
    {
        DrawRect(FLinearColor(0.84F, 0.27F, 0.02F, 0.94F), Right, Canvas->ClipY - Margin.Y - 86.0F, RightWidth, 74.0F);
        DrawText(FString::Printf(TEXT("CENTER ICON / INPUTS NEUTRAL\nRESUME IN %d"), FMath::CeilToInt(Pawn->GetResumeCountdownSeconds())),
            FLinearColor::White, Right + 14.0F, Canvas->ClipY - Margin.Y - 70.0F, GEngine->GetSmallFont(), 1.0F, false);
    }
}
