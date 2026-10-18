#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Phase2Systems.h"
#include "Phase3Aircraft.h"
#include "FlightPawn.generated.h"

class UBoxComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class USpringArmComponent;
class UStaticMeshComponent;
class AAeroSimPlayerController;
struct FInputActionValue;

enum class EInputDeviceConnectionState : uint8;

UENUM(BlueprintType)
enum class EFlightPhase : uint8
{
    Grounded,
    Takeoff,
    Flight,
    Landing,
    Paused
};

UENUM(BlueprintType)
enum class EFlightCameraMode : uint8
{
    /** Phase 1 name retained as a serialized/API alias for the Phase 2 nose camera. */
    FPV,
    /** Phase 1 chase mode becomes the near follow preset. */
    Chase,
    FarFollow,
    /** Stabilized payload view with independent clamped pitch/yaw. */
    Gimbal
};

/** Device-independent, normalized semantic controls consumed by the simulation. */
USTRUCT(BlueprintType)
struct AEROSIM_API FFlightControlInput
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Flight")
    float Pitch = 0.0F;

    UPROPERTY(BlueprintReadOnly, Category = "Flight")
    float Roll = 0.0F;

    UPROPERTY(BlueprintReadOnly, Category = "Flight")
    float Yaw = 0.0F;

    UPROPERTY(BlueprintReadOnly, Category = "Flight")
    float Throttle = 0.0F;

    void Clamp();
    void Neutralize();
    bool IsNeutral() const;
};

/** Pure, deterministic Phase 1 motion/state model. Position is relative to the reset point in cm. */
USTRUCT(BlueprintType)
struct AEROSIM_API FFlightModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Flight")
    EFlightPhase Phase = EFlightPhase::Grounded;

    UPROPERTY(BlueprintReadOnly, Category = "Flight")
    FVector Position = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Flight")
    FVector Velocity = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Flight")
    FRotator Rotation = FRotator::ZeroRotator;

    void Advance(const FFlightControlInput& Input, float DeltaSeconds);
    void Pause();
    void Resume();
    void Reset();

private:
    EFlightPhase ResumePhase = EFlightPhase::Grounded;
};

USTRUCT(BlueprintType)
struct AEROSIM_API FFlightHudData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "HUD")
    EFlightPhase Phase = EFlightPhase::Grounded;

    UPROPERTY(BlueprintReadOnly, Category = "HUD")
    EFlightCameraMode CameraMode = EFlightCameraMode::Chase;

    UPROPERTY(BlueprintReadOnly, Category = "HUD")
    float AltitudeMeters = 0.0F;

    UPROPERTY(BlueprintReadOnly, Category = "HUD")
    float SpeedMetersPerSecond = 0.0F;

    UPROPERTY(BlueprintReadOnly, Category = "HUD")
    float Throttle = 0.0F;

    UPROPERTY(BlueprintReadOnly, Category = "HUD")
    bool bInputNeutral = true;

    UPROPERTY(BlueprintReadOnly, Category = "HUD")
    FString AircraftName;

    UPROPERTY(BlueprintReadOnly, Category = "HUD")
    FString AssetStatus;

    UPROPERTY(BlueprintReadOnly, Category = "HUD")
    float GimbalPitchDegrees = 0.0F;

    UPROPERTY(BlueprintReadOnly, Category = "HUD")
    float GimbalYawDegrees = 0.0F;

    UPROPERTY(BlueprintReadOnly, Category = "HUD")
    FString BuildId;
};

/** Mapping context and semantic actions form one atomic override unit. */
USTRUCT(BlueprintType)
struct AEROSIM_API FFlightInputSuite
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<UInputMappingContext> MappingContext;

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<UInputAction> PitchAction;

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<UInputAction> RollAction;

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<UInputAction> YawAction;

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<UInputAction> ThrottleAction;

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<UInputAction> CameraAction;

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<UInputAction> PauseAction;

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<UInputAction> ResetAction;
};

UCLASS()
class AEROSIM_API AFlightPawn final : public APawn
{
    GENERATED_BODY()
    friend class UAeroSimControlBridge;

public:
    AFlightPawn();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void PawnClientRestart() override;

    const FFlightInputSuite& GetInputSuite() const { return InputSuite; }
    const FFlightControlInput& GetControlInput() const { return ControlInput; }
    const FFlightModel& GetFlightModel() const { return FlightModel; }
    EFlightCameraMode GetCameraMode() const { return CameraMode; }
    bool IsAircraftShellVisible() const;
    bool IsLodProxyVisible() const;
    FFlightHudData GetHudData() const;
    const FAircraftDefinition& GetAircraftDefinition() const { return AircraftDefinition; }
    const FGimbalController& GetGimbalController() const { return GimbalController; }
    int32 GetDetailedVisualPartCount() const { return DetailedVisualParts.Num(); }
    int32 GetMaterialSlotCount() const { return AircraftDefinition.MaterialSlots.Num(); }
    float GetVerticalFov() const { return FovTransaction.GetEffectiveVerticalFov(); }
    float GetHorizontalFov(float AspectRatio) const { return FVerticalFovTransaction::VerticalToHorizontal(GetVerticalFov(), AspectRatio); }
    const FVerticalFovTransaction& GetFovTransaction() const { return FovTransaction; }
    const FDeterministicFlightTrace& GetFlightTrace() const { return FlightTrace; }
    FFlightResultMetrics GetResultMetrics() const { return FlightTrace.BuildResult(); }
    float GetResumeCountdownSeconds() const { return ResumeCountdownSeconds; }
    bool IsResumeCountdownActive() const { return ResumeCountdownSeconds > 0.0F; }
    /** Physical recenter check; independent of semantic input neutralization and UI consumption. */
    UFUNCTION(BlueprintPure, Category = "Input")
    bool IsPhysicalInputNeutral() const;
    USpringArmComponent* GetFollowSpringArm() const { return SpringArm; }
    UCameraComponent* GetFlightCamera() const { return Camera; }

    UInputMappingContext* GetFlightMappingContext() const { return InputSuite.MappingContext; }
    UInputAction* GetPitchAction() const { return InputSuite.PitchAction; }
    UInputAction* GetRollAction() const { return InputSuite.RollAction; }
    UInputAction* GetYawAction() const { return InputSuite.YawAction; }
    UInputAction* GetThrottleAction() const { return InputSuite.ThrottleAction; }
    UInputAction* GetCameraAction() const { return InputSuite.CameraAction; }
    UInputAction* GetPauseAction() const { return InputSuite.PauseAction; }
    UInputAction* GetResetAction() const { return InputSuite.ResetAction; }

    /** Fail-closed validation used before mapping application and action binding. */
    bool IsInputSuiteValid(const FFlightInputSuite& Candidate, bool bLogErrors = true) const;

    /** Explicit safety boundary used by pause, focus loss, disconnect and tests. */
    UFUNCTION(BlueprintCallable, Category = "Input")
    void NeutralizeInput();

    UFUNCTION(BlueprintCallable, Category = "Flight")
    void SetSafePaused(bool bPaused);

    UFUNCTION(BlueprintCallable, Category = "Camera")
    void CycleCamera();

    UFUNCTION(BlueprintCallable, Category = "Camera")
    void SetGimbalTarget(float PitchDegrees, float YawDegrees);

    UFUNCTION(BlueprintCallable, Category = "Camera")
    void SetUseLodProxy(bool bUseProxy);

    UFUNCTION(BlueprintCallable, Category = "Camera")
    bool PreviewVerticalFov(float VerticalDegrees);

    UFUNCTION(BlueprintCallable, Category = "Camera")
    bool ApplyVerticalFov();

    UFUNCTION(BlueprintCallable, Category = "Camera")
    void CancelVerticalFov();

    UFUNCTION(BlueprintCallable, Category = "Camera")
    bool UndoVerticalFov();

    UFUNCTION(BlueprintCallable, Category = "Flight")
    void ResetFlight();

    /** Deterministic fixed view used only by command-line visual review capture. */
    bool InitializeGroundedSpawn(const FVector& SurfacePoint);
    void ConfigureReviewView(const FTransform& PawnTransform, EFlightCameraMode Mode);
    void ConfigureReviewFlightState(float AltitudeCm, float SpeedCmPerSecond);

    /** Presentation-only replay; never resets the model or its recorded trace. */
    void ApplyReplaySample(const FFlightTraceSample& Sample);
    /** Finite product setters; do not reset camera origin, flight state or trace. */
    void SetProductCameraMode(EFlightCameraMode Mode);
    bool ApplyProductVerticalFov(float VerticalDegrees);

    /** Testable semantic injection; values are clamped and ignored while paused. */
    void SetSemanticInput(const FFlightControlInput& NewInput);

private:
    friend class AAeroSimPlayerController;
    friend class FAeroSimProductInputLifecycleTest;
    bool IsProductFlightPage() const;
    bool IsGameplayInputAllowed() const;
    void HandlePhysicalInputActivity();
    bool ValidateConfiguredInputSuite() const;
    void ApplyMappingContext() const;
    void ApplyCameraMode();
    void ApplyFov();
    void AdvanceResumeCountdown(float DeltaSeconds);
    void RecordTrace(float DeltaSeconds);
    void HandlePitch(const FInputActionValue& Value);
    void HandleRoll(const FInputActionValue& Value);
    void HandleYaw(const FInputActionValue& Value);
    void HandleThrottle(const FInputActionValue& Value);
    void HandlePitchReleased(const FInputActionValue& Value);
    void HandleRollReleased(const FInputActionValue& Value);
    void HandleYawReleased(const FInputActionValue& Value);
    void HandleThrottleReleased(const FInputActionValue& Value);
    void HandleCamera(const FInputActionValue& Value);
    void HandlePause(const FInputActionValue& Value);
    void HandleReset(const FInputActionValue& Value);
    void HandleMissionAdvance();
    void HandleReturnToHome();
    void HandleSettingsToggle();
    void HandleUiUp();
    void HandleUiDown();
    void HandleUiLeft();
    void HandleUiRight();
    void HandleUiActivate();
    void HandleWeatherCycle();
    void HandleGimbalPitchUp();
    void HandleGimbalPitchDown();
    void HandleGimbalYawLeft();
    void HandleGimbalYawRight();
    void HandleApplicationDeactivated();
    void HandleInputDeviceConnectionChanged(EInputDeviceConnectionState ConnectionState, FPlatformUserId PlatformUserId, FInputDeviceId InputDeviceId);

    UPROPERTY(VisibleAnywhere, Category = "Flight")
    TObjectPtr<UBoxComponent> Body;

    UPROPERTY(VisibleAnywhere, Category = "Aircraft")
    TObjectPtr<UStaticMeshComponent> Fuselage;

    UPROPERTY(VisibleAnywhere, Category = "Aircraft")
    TArray<TObjectPtr<UStaticMeshComponent>> ShellPanels;

    UPROPERTY(VisibleAnywhere, Category = "Aircraft")
    TArray<TObjectPtr<UStaticMeshComponent>> Arms;

    UPROPERTY(VisibleAnywhere, Category = "Aircraft")
    TArray<TObjectPtr<UStaticMeshComponent>> Motors;

    UPROPERTY(VisibleAnywhere, Category = "Aircraft")
    TArray<TObjectPtr<UStaticMeshComponent>> Propellers;

    UPROPERTY(VisibleAnywhere, Category = "Aircraft")
    TArray<TObjectPtr<UStaticMeshComponent>> LandingSupports;

    UPROPERTY(VisibleAnywhere, Category = "Aircraft")
    TObjectPtr<UStaticMeshComponent> GimbalMount;

    UPROPERTY(VisibleAnywhere, Category = "Aircraft")
    TObjectPtr<UStaticMeshComponent> GimbalCamera;

    UPROPERTY(VisibleAnywhere, Category = "Aircraft")
    TObjectPtr<UStaticMeshComponent> CameraLens;

    UPROPERTY(VisibleAnywhere, Category = "Aircraft")
    TArray<TObjectPtr<UStaticMeshComponent>> NavigationLights;

    UPROPERTY(VisibleAnywhere, Category = "Aircraft")
    TObjectPtr<UStaticMeshComponent> LodProxy;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> DetailedVisualParts;

    UPROPERTY(VisibleAnywhere, Category = "Camera")
    TObjectPtr<USpringArmComponent> SpringArm;

    UPROPERTY(VisibleAnywhere, Category = "Camera")
    TObjectPtr<UCameraComponent> Camera;

    /** Override the complete suite. Partial or mismatched overrides are rejected and never applied. */
    UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (ShowOnlyInnerProperties))
    FFlightInputSuite InputSuite;

    UPROPERTY(VisibleInstanceOnly, Category = "Flight")
    FFlightControlInput ControlInput;

    UPROPERTY(VisibleInstanceOnly, Category = "Flight")
    FFlightModel FlightModel;

    UPROPERTY(VisibleInstanceOnly, Category = "Camera")
    EFlightCameraMode CameraMode = EFlightCameraMode::Chase;

    UPROPERTY(VisibleInstanceOnly, Category = "Camera")
    FVerticalFovTransaction FovTransaction;

    UPROPERTY(VisibleInstanceOnly, Category = "Aircraft")
    FAircraftDefinition AircraftDefinition;

    UPROPERTY(VisibleInstanceOnly, Category = "Camera")
    FGimbalController GimbalController;

    UPROPERTY(VisibleInstanceOnly, Category = "Aircraft")
    bool bUseLodProxy = false;

    UPROPERTY(VisibleInstanceOnly, Category = "Replay")
    FDeterministicFlightTrace FlightTrace;

    UPROPERTY(VisibleInstanceOnly, Category = "Input")
    float ResumeCountdownSeconds = 0.0F;

    float FlightElapsedSeconds = 0.0F;
    float TraceAccumulatorSeconds = 0.0F;
    bool bResumeRequested = false;
    bool bReplayVisualActive = false;
    FTransform InitialTransform = FTransform::Identity;
};
