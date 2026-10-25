#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Phase2Systems.h"
#include "Phase4Mission.h"
#include "ProductUX.h"
#include "ProductShell.h"
#include "AeroSimGameMode.generated.h"

class UAeroSimControlBridge;
class APacificCoastGraybox;
class UAeroSimProductWidget;
class ACameraActor;

UCLASS()
class AEROSIM_API AAeroSimGameMode final : public AGameModeBase
{
    GENERATED_BODY()
    friend class UAeroSimControlBridge;

public:
    AAeroSimGameMode();
    virtual void StartPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    EProductPage GetProductPage() const { return Shell.Page; }
    EProductSettingsCategory GetSettingsCategory() const { return Shell.SettingsCategory; }
    EProductInputKind GetProductInputKind() const { return Shell.InputKind; }
    void SetProductInputKind(EProductInputKind Kind);
    bool NavigateProductPage(EProductPage Page);
    void ActivatePrimaryAction();
    void HandleProductBack();
    void SetSettingsCategory(EProductSettingsCategory Category);
    bool StageProductSetting(EProductSettingField Field, int32 Delta);
    void ExecuteSettingsAction(EProductSettingsAction Action);
    const FPhase7Settings& GetEditableSettings() const { return DisplaySettings.GetVisible(); }
    const FString& GetProductStatus() const { return Shell.Status; }
    const FProductLabState& GetLabState() const { return Lab; }
    const FProductDiagnosticSample& GetDiagnostics() const { return Diagnostics; }
    void SetLabWeather(EAeroSimWeather Weather);
    void LockLabReference();
    void ToggleLabComparison();
    void CaptureProductEvidence();
    /** Fixed review-camera presets shared by temporal sampling and the control bridge; Step 0-8 follows hold-move-hold. */
    static bool ResolveReviewView(const FString& Name, int32 Step, FVector& OutLocation, FRotator& OutRotation);
    void RestartProductMission();
    void SetReplayPlaying(bool bPlaying);
    bool IsReplayPlaying() const { return bReplayPlaying; }
    void SeekReplay(float NormalizedTime);
    float GetReplayNormalizedTime() const;
    void RefreshProductWidget();

    UFUNCTION(BlueprintCallable, Category = "Mission")
    bool AdvanceMission();

    UFUNCTION(BlueprintCallable, Category = "Environment")
    void SetWeather(EAeroSimWeather Weather);

    UFUNCTION(BlueprintCallable, Category = "Mission")
    void RequestReturnToHome();

    void ObserveMission(float DeltaSeconds, EFlightPhase FlightPhase, const FVector& WorldPosition, const FVector& WorldVelocity, const FRotator& WorldRotation, bool bCollision, bool bConfirmedHomeSupport = false, bool bWaterContact = false);

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
    void ApplyDisplayConfiguration(const FPhase7Settings& Settings, bool bSaveEngineSettings,
        bool bCheckForCommandLineOverrides = false);
    bool SaveProductSettings();
    void LoadProductSettings();
    void ApplyProductConfiguration(const FPhase7Settings& Settings, bool bSaveEngineSettings,
        bool bCheckForCommandLineOverrides = false);
    void ConfigureProductInputMode();
    bool IsMissionActive() const;
    void CloseLabWorkspace();
    void TickTemporalCapture(double Now);

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
    FString LastProductEvidenceOutput;
    FString EvidenceReviewView;
    FString EvidenceViewMode;
    int32 EvidenceSequenceIndex = 0;
    int32 EvidenceSequenceFrames = 0;
    double EvidenceSequenceStart = 0.0;
    bool bEvidenceStateLocked = false;
    UPROPERTY(Transient)
    TObjectPtr<UAeroSimControlBridge> ControlBridge;
    FProductShellState Shell;
    FProductLabState Lab;
    FProductDiagnosticSample Diagnostics;
    TArray<double> DiagnosticFrameTimes;
    double DiagnosticElapsed = 0.0;
    bool bReplayPlaying = false;
    float ReplayTimeSeconds = 0.0F;
    double LastProductTickSeconds = 0.0;
    double EvidenceCaptureDeadline = 0.0;
    double EvidenceExitDeadline = 0.0;
    bool bTemporalCapture = false;
    bool bCoastalOverviewCapture = false;
    int32 TemporalCaptureIndex = 0;
    double TemporalCaptureNextTime = 0.0;
    double TemporalCaptureStartTime = 0.0;
    FString TemporalCaptureRoot;
    FString TemporalCaptureOutput;
    UPROPERTY(Transient)
    TObjectPtr<ACameraActor> TemporalCaptureCamera;
    FTransform LabOpeningTransform;
    uint8 LabOpeningCamera = 0;
    FEnvironmentState LabOpeningEnvironment;
    UPROPERTY(Transient)
    TObjectPtr<UAeroSimProductWidget> ProductWidget;
    UPROPERTY(Transient)
    TObjectPtr<ACameraActor> LabCamera;
};
