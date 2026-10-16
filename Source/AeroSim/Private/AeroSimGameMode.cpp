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
    return MissionFlow.Advance();
}

void AAeroSimGameMode::SetWeather(const EAeroSimWeather Weather)
{
    EnvironmentState.SetWeather(Weather);
    if (Graybox != nullptr)
    {
        Graybox->ApplyEnvironmentState(EnvironmentState);
    }
}
