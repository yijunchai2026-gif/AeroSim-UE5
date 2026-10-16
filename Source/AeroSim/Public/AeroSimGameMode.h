#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Phase2Systems.h"
#include "Phase4Mission.h"
#include "ProductUX.h"
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

    UFUNCTION(BlueprintCallable, Category = "Mission")
    void RequestReturnToHome();

    void ObserveMission(float DeltaSeconds, EFlightPhase FlightPhase, const FVector& Position, const FVector& Velocity, const FRotator& Rotation, bool bCollision);

    const FMissionFlow& GetMissionFlow() const { return MissionFlow; }
    const FPhase4MissionSession& GetPhase4Mission() const { return Phase4Mission; }
    const FEnvironmentState& GetEnvironmentState() const { return EnvironmentState; }
    APacificCoastGraybox* GetEnvironmentActor() const { return Graybox; }

    const FProductDisplaySettings& GetDisplaySettings() const { return DisplaySettings; }
    const FPhase7Settings& GetAppliedSettings() const { return AppliedSettings; }
    EAeroSimProductSurface GetProductSurface() const;
    bool IsDisplaySettingsOpen() const { return DisplaySettings.bOpen; }
    void ToggleDisplaySettings();
    void NavigateDisplaySettings(int32 Delta);
    void ChangeDisplaySetting(int32 Delta);
    void ActivateDisplaySetting();
    void CancelDisplaySettings();

private:
    void ApplyDisplayConfiguration(const FPhase7Settings& Settings, bool bSaveEngineSettings);
    bool SaveProductSettings();
    void LoadProductSettings();

private:
    UPROPERTY(VisibleInstanceOnly, Category = "Mission")
    FMissionFlow MissionFlow;

    UPROPERTY(VisibleInstanceOnly, Category = "Environment")
    FEnvironmentState EnvironmentState;

    UPROPERTY(VisibleInstanceOnly, Category = "Mission")
    FPhase4MissionSession Phase4Mission;

    UPROPERTY(Transient)
    TObjectPtr<APacificCoastGraybox> Graybox;

    FPhase7Settings AppliedSettings;
    FProductDisplaySettings DisplaySettings;
    bool bEvidenceStateLocked = false;
};
