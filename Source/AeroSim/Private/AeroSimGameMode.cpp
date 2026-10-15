#include "AeroSimGameMode.h"

#include "FlightHUD.h"
#include "FlightPawn.h"
#include "PacificCoastGraybox.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

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
                FTransform View(FRotator(-8.0F, 35.0F, 0.0F), FVector(-5000.0F, -5000.0F, 1800.0F));
                EFlightCameraMode CameraMode = EFlightCameraMode::Chase;
                if (ReviewName.Equals(TEXT("Rock"), ESearchCase::IgnoreCase))
                {
                    View = FTransform(FRotator(-6.0F, 8.0F, 0.0F), FVector(1500.0F, -9000.0F, 1700.0F));
                    CameraMode = EFlightCameraMode::FPV;
                }
                else if (ReviewName.Equals(TEXT("Boardwalk"), ESearchCase::IgnoreCase))
                {
                    View = FTransform(FRotator(-7.0F, 20.0F, 0.0F), FVector(-17500.0F, 5000.0F, 1800.0F));
                    CameraMode = EFlightCameraMode::Chase;
                }
                else if (ReviewName.Equals(TEXT("Overview"), ESearchCase::IgnoreCase))
                {
                    View = FTransform(FRotator(-18.0F, 32.0F, 0.0F), FVector(-22000.0F, -12000.0F, 6000.0F));
                    CameraMode = EFlightCameraMode::FarFollow;
                }
                Pawn->ConfigureReviewView(View, CameraMode);
            }
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
