#pragma once

#include "CoreMinimal.h"
#include "Phase2Systems.generated.h"

UENUM(BlueprintType)
enum class EAeroSimMissionState : uint8
{
    Hangar,
    Preflight,
    Flight,
    Result,
    Replay
};

UENUM(BlueprintType)
enum class EAeroSimWeather : uint8
{
    Clear,
    Overcast,
    Rain
};

USTRUCT(BlueprintType)
struct AEROSIM_API FEnvironmentState
{
    GENERATED_BODY()

    static constexpr int32 CurrentVersion = 1;

    UPROPERTY(BlueprintReadOnly, Category = "Environment")
    int32 Version = CurrentVersion;

    UPROPERTY(BlueprintReadOnly, Category = "Environment")
    EAeroSimWeather Weather = EAeroSimWeather::Clear;

    UPROPERTY(BlueprintReadOnly, Category = "Environment")
    int32 TimeMinutes = 16 * 60 + 30;

    UPROPERTY(BlueprintReadOnly, Category = "Environment")
    int32 Seed = 21630;

    bool IsValid() const;
    void SetWeather(EAeroSimWeather NewWeather);
    float GetWetness() const;
    float GetCloudCover() const;
    float GetLightIntensity() const;
    float GetFogDensity() const;
    float GetExposureBias() const;
    float GetSceneSaturation() const;
};

/** Testable top-level Phase 2 workflow. Invalid transitions fail closed. */
USTRUCT(BlueprintType)
struct AEROSIM_API FMissionFlow
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Mission")
    EAeroSimMissionState State = EAeroSimMissionState::Hangar;

    bool CanTransition(EAeroSimMissionState Next) const;
    bool Transition(EAeroSimMissionState Next);
    bool Advance();
    void Reset();
};

/** Vertical FOV is the only persisted truth. Horizontal FOV is derived from viewport aspect. */
USTRUCT(BlueprintType)
struct AEROSIM_API FVerticalFovTransaction
{
    GENERATED_BODY()

    static constexpr float MinimumVerticalFov = 35.0F;
    static constexpr float MaximumVerticalFov = 100.0F;

    UPROPERTY(BlueprintReadOnly, Category = "Camera")
    float AppliedVerticalFov = 60.0F;

    UPROPERTY(BlueprintReadOnly, Category = "Camera")
    float PreviewVerticalFov = 60.0F;

    UPROPERTY(BlueprintReadOnly, Category = "Camera")
    float UndoVerticalFov = 60.0F;

    UPROPERTY(BlueprintReadOnly, Category = "Camera")
    bool bPreviewActive = false;

    bool Preview(float VerticalDegrees);
    bool Apply();
    void Cancel();
    bool Undo();
    float GetEffectiveVerticalFov() const;

    static float VerticalToHorizontal(float VerticalDegrees, float AspectRatio);
    static float HorizontalToVertical(float HorizontalDegrees, float AspectRatio);
};

USTRUCT(BlueprintType)
struct AEROSIM_API FFlightTraceSample
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Replay")
    float TimeSeconds = 0.0F;

    UPROPERTY(BlueprintReadOnly, Category = "Replay")
    FVector Position = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Replay")
    FRotator Rotation = FRotator::ZeroRotator;

    bool Equals(const FFlightTraceSample& Other, float Tolerance = KINDA_SMALL_NUMBER) const;
};

USTRUCT(BlueprintType)
struct AEROSIM_API FFlightResultMetrics
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    float DurationSeconds = 0.0F;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    float DistanceMeters = 0.0F;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    float MaximumAltitudeMeters = 0.0F;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    int32 Samples = 0;
};

/** Fixed-rate recorder and deterministic interpolation used by Result and Replay. */
USTRUCT(BlueprintType)
struct AEROSIM_API FDeterministicFlightTrace
{
    GENERATED_BODY()

    static constexpr float SampleIntervalSeconds = 0.1F;

    UPROPERTY(BlueprintReadOnly, Category = "Replay")
    TArray<FFlightTraceSample> Samples;

    void Reset();
    void Record(float TimeSeconds, const FVector& Position, const FRotator& Rotation);
    FFlightTraceSample Evaluate(float TimeSeconds) const;
    FFlightResultMetrics BuildResult() const;
};
