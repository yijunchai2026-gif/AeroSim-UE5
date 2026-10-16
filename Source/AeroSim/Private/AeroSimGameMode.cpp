#include "AeroSimGameMode.h"

#include "FlightHUD.h"
#include "FlightPawn.h"
#include "PacificCoastGraybox.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "UnrealClient.h"

AAeroSimGameMode::AAeroSimGameMode()
{
    DefaultPawnClass = AFlightPawn::StaticClass();
    HUDClass = AFlightHUD::StaticClass();
}

void AAeroSimGameMode::StartPlay()
{
    Super::StartPlay();
    Phase4Mission.Reset();

    FString WeatherName;
    if (FParse::Value(FCommandLine::Get(), TEXT("AeroSimWeather="), WeatherName))
    {
        if (WeatherName.Equals(TEXT("Overcast"), ESearchCase::IgnoreCase))
        {
            EnvironmentState.SetWeather(EAeroSimWeather::Overcast);
        }
        else if (WeatherName.Equals(TEXT("Rain"), ESearchCase::IgnoreCase))
        {
            EnvironmentState.SetWeather(EAeroSimWeather::Rain);
        }
    }

    if (UWorld* World = GetWorld())
    {
        Graybox = World->SpawnActor<APacificCoastGraybox>();
        if (Graybox != nullptr)
        {
            Graybox->ApplyEnvironmentState(EnvironmentState);
        }

        FString ReviewName;
        if (FParse::Value(FCommandLine::Get(), TEXT("AeroSimReview="), ReviewName))
        {
            APlayerController* Controller = World->GetFirstPlayerController();
            AFlightPawn* Pawn = Controller != nullptr ? Cast<AFlightPawn>(Controller->GetPawn()) : nullptr;
            if (Pawn != nullptr)
            {
                FTransform View(FRotator::ZeroRotator, FVector(-5000.0F, -5000.0F, 1800.0F));
                EFlightCameraMode CameraMode = EFlightCameraMode::Chase;
                float ReviewArmLength = -1.0F;
                if (ReviewName.Equals(TEXT("Aircraft"), ESearchCase::IgnoreCase))
                {
                    View = FTransform(FRotator(0.0F, 28.0F, 0.0F), FVector(-5100.0F, -4900.0F, 60.0F));
                    ReviewArmLength = 105.0F;
                }
                else if (ReviewName.Equals(TEXT("FPV"), ESearchCase::IgnoreCase))
                {
                    View = FTransform(FRotator(-3.0F, 18.0F, 0.0F), FVector(-3500.0F, -4500.0F, 700.0F));
                    CameraMode = EFlightCameraMode::FPV;
                }
                else if (ReviewName.Equals(TEXT("Gimbal"), ESearchCase::IgnoreCase))
                {
                    View = FTransform(FRotator(0.0F, 22.0F, 0.0F), FVector(-2500.0F, -5200.0F, 1700.0F));
                    CameraMode = EFlightCameraMode::Gimbal;
                    Pawn->SetGimbalTarget(-78.0F, 0.0F);
                }
                else if (ReviewName.Equals(TEXT("NearFollow"), ESearchCase::IgnoreCase))
                {
                    View = FTransform(FRotator(0.0F, 22.0F, 0.0F), FVector(-5000.0F, -5000.0F, 400.0F));
                    ReviewArmLength = 250.0F;
                }
                else if (ReviewName.Equals(TEXT("FarFollow"), ESearchCase::IgnoreCase))
                {
                    View = FTransform(FRotator(0.0F, 22.0F, 0.0F), FVector(-5000.0F, -5000.0F, 800.0F));
                    CameraMode = EFlightCameraMode::FarFollow;
                    ReviewArmLength = 700.0F;
                }
                else if (ReviewName.Equals(TEXT("ScaleSupport"), ESearchCase::IgnoreCase))
                {
                    View = FTransform(FRotator(0.0F, 35.0F, 0.0F), FVector(-6200.0F, -4300.0F, 60.0F));
                    ReviewArmLength = 155.0F;
                }
                else if (ReviewName.Equals(TEXT("Rock"), ESearchCase::IgnoreCase))
                {
                    View = FTransform(FRotator(-6.0F, 8.0F, 0.0F), FVector(1500.0F, -9000.0F, 1700.0F));
                    CameraMode = EFlightCameraMode::FPV;
                }
                else if (ReviewName.Equals(TEXT("Boardwalk"), ESearchCase::IgnoreCase))
                {
                    View = FTransform(FRotator(-7.0F, 20.0F, 0.0F), FVector(-17500.0F, 5000.0F, 1800.0F));
                }
                else if (ReviewName.Equals(TEXT("Overview"), ESearchCase::IgnoreCase))
                {
                    View = FTransform(FRotator(-18.0F, 32.0F, 0.0F), FVector(-22000.0F, -12000.0F, 6000.0F));
                    CameraMode = EFlightCameraMode::FarFollow;
                }
                Pawn->ConfigureReviewView(View, CameraMode);
                if (ReviewArmLength > 0.0F && Pawn->GetFollowSpringArm() != nullptr)
                {
                    Pawn->GetFollowSpringArm()->TargetArmLength = ReviewArmLength;
                }
            }
        }

        FString Phase4View;
        if (FParse::Value(FCommandLine::Get(), TEXT("AeroSimPhase4View="), Phase4View))
        {
            EPhase4MissionStage EvidenceStage = EPhase4MissionStage::Hangar;
            if (Phase4View.Equals(TEXT("Preflight"), ESearchCase::IgnoreCase)) EvidenceStage = EPhase4MissionStage::Preflight;
            else if (Phase4View.Equals(TEXT("FlightHint"), ESearchCase::IgnoreCase)) EvidenceStage = EPhase4MissionStage::Inspection;
            else if (Phase4View.Equals(TEXT("BoundaryRecovery"), ESearchCase::IgnoreCase)) EvidenceStage = EPhase4MissionStage::ReturnToHome;
            else if (Phase4View.Equals(TEXT("Result"), ESearchCase::IgnoreCase)) EvidenceStage = EPhase4MissionStage::Result;
            else if (Phase4View.Equals(TEXT("Replay"), ESearchCase::IgnoreCase)) EvidenceStage = EPhase4MissionStage::Replay;
            Phase4Mission.ConfigureEvidenceState(EvidenceStage);
            bEvidenceStateLocked = true;

            APlayerController* Controller = World->GetFirstPlayerController();
            AFlightPawn* Pawn = Controller != nullptr ? Cast<AFlightPawn>(Controller->GetPawn()) : nullptr;
            if (Pawn != nullptr)
            {
                const bool bFlightView = EvidenceStage == EPhase4MissionStage::Inspection || EvidenceStage == EPhase4MissionStage::ReturnToHome;
                const FTransform EvidenceTransform(
                    FRotator(0.0F, bFlightView ? 22.0F : 28.0F, 0.0F),
                    bFlightView ? FVector(3500.0F, -2100.0F, 850.0F) : FVector(-5100.0F, -4900.0F, 60.0F));
                Pawn->ConfigureReviewView(EvidenceTransform, bFlightView ? EFlightCameraMode::FPV : EFlightCameraMode::Chase);
                if (!bFlightView && Pawn->GetFollowSpringArm() != nullptr) Pawn->GetFollowSpringArm()->TargetArmLength = 115.0F;
            }
        }

        FString ScreenshotPath;
        if (FParse::Value(FCommandLine::Get(), TEXT("AeroSimScreenshot="), ScreenshotPath))
        {
            FTimerHandle ScreenshotTimer;
            World->GetTimerManager().SetTimer(
                ScreenshotTimer,
                FTimerDelegate::CreateLambda([ScreenshotPath]()
                {
                    FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
                }),
                2.0F,
                false);

            FTimerHandle ExitTimer;
            World->GetTimerManager().SetTimer(
                ExitTimer,
                FTimerDelegate::CreateLambda([]()
                {
                    FPlatformMisc::RequestExit(false);
                }),
                4.0F,
                false);
        }
    }
}

bool AAeroSimGameMode::AdvanceMission()
{
    AFlightPawn* Pawn = GetWorld() != nullptr ? Cast<AFlightPawn>(GetWorld()->GetFirstPlayerController() != nullptr ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr) : nullptr;
    if (Phase4Mission.Stage == EPhase4MissionStage::Hangar)
    {
        Phase4Mission.EnterPreflight();
        return MissionFlow.Transition(EAeroSimMissionState::Preflight);
    }
    if (Phase4Mission.Stage == EPhase4MissionStage::Preflight)
    {
        Phase4Mission.SetPreflightComplete(Pawn == nullptr || Pawn->GetControlInput().IsNeutral());
        if (!Phase4Mission.BeginMission()) return false;
        return MissionFlow.Transition(EAeroSimMissionState::Flight);
    }
    if (Phase4Mission.Stage == EPhase4MissionStage::Result)
    {
        if (!Phase4Mission.OpenReplay()) return false;
        return MissionFlow.Transition(EAeroSimMissionState::Replay);
    }
    return false;
}

void AAeroSimGameMode::ObserveMission(
    const float DeltaSeconds,
    const EFlightPhase FlightPhase,
    const FVector& Position,
    const FVector& Velocity,
    const FRotator& Rotation,
    const bool bCollision)
{
    if (bEvidenceStateLocked)
    {
        return;
    }
    if (bCollision)
    {
        Phase4Mission.ReportCollision(Position);
    }
    Phase4Mission.Observe(DeltaSeconds, FlightPhase, Position, Velocity, Rotation);
    if (Phase4Mission.Stage == EPhase4MissionStage::Result && MissionFlow.State == EAeroSimMissionState::Flight)
    {
        MissionFlow.Transition(EAeroSimMissionState::Result);
    }
}

void AAeroSimGameMode::RequestReturnToHome()
{
    Phase4Mission.RequestReturnToHome();
}

void AAeroSimGameMode::SetWeather(const EAeroSimWeather Weather)
{
    EnvironmentState.SetWeather(Weather);
    if (Graybox != nullptr)
    {
        Graybox->ApplyEnvironmentState(EnvironmentState);
    }
}
