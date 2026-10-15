#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Phase2Systems.h"
#include "AeroSimGameMode.generated.h"

class APacificCoastGraybox;

UCLASS()
class AEROSIM_API AAeroSimGameMode final : public AGameModeBase
{
    GENERATED_BODY()

public:
    AAeroSimGameMode();
    virtual void StartPlay() override;

    UFUNCTION(BlueprintCallable, Category = "Mission")
    bool AdvanceMission();

    UFUNCTION(BlueprintCallable, Category = "Environment")
    void SetWeather(EAeroSimWeather Weather);

    const FMissionFlow& GetMissionFlow() const { return MissionFlow; }
    const FEnvironmentState& GetEnvironmentState() const { return EnvironmentState; }
    APacificCoastGraybox* GetEnvironmentActor() const { return Graybox; }

private:
    UPROPERTY(VisibleInstanceOnly, Category = "Mission")
    FMissionFlow MissionFlow;

    UPROPERTY(VisibleInstanceOnly, Category = "Environment")
    FEnvironmentState EnvironmentState;

    UPROPERTY(Transient)
    TObjectPtr<APacificCoastGraybox> Graybox;
};
