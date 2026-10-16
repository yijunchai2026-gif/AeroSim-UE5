#pragma once

#include "CoreMinimal.h"
#include "Phase7Product.generated.h"

UENUM(BlueprintType)
enum class EPhase7QualityTier : uint8
{
    DesktopHigh,
    DesktopLow,
    Tablet
};

/** Versioned budgets are acceptance thresholds, never measurements. */
USTRUCT(BlueprintType)
struct AEROSIM_API FPhase7PerformanceBudget
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Phase7|Performance")
    FString ProfileId;

    UPROPERTY(BlueprintReadOnly, Category = "Phase7|Performance")
    EPhase7QualityTier Tier = EPhase7QualityTier::DesktopHigh;

    double FrameP95Ms = 16.67;
    double GameThreadP95Ms = 8.0;
    double RenderThreadP95Ms = 8.0;
    double GpuP95Ms = 14.0;
    int32 ResidentMemoryMiB = 4096;
    int32 DrawCalls = 2000;
    int32 StreamingPoolMiB = 2048;
    double StreamingSpikeP95Ms = 4.0;

    bool bWorldPartition = true;
    bool bHlod = true;
    bool bNanite = false;

    static FPhase7PerformanceBudget MacDevelopmentHigh();
    static FPhase7PerformanceBudget WindowsHighCandidate();
    static FPhase7PerformanceBudget IPadCandidate();
    bool IsValid() const;
};

USTRUCT(BlueprintType)
struct AEROSIM_API FPhase7PerformanceEvidence
{
    GENERATED_BODY()

    FString BuildId;
    FString Platform;
    FString HardwareId;
    FString Rhi;
    FString QualityProfileId;
    int32 WarmupSeconds = 0;
    int32 DurationSeconds = 0;
    int32 ColdStarts = 0;
    bool bNonDevelopmentBuild = false;
    bool bTargetHardware = false;
    bool bCompleteReplay = false;
    double FrameP95Ms = 0.0;
    double GameThreadP95Ms = 0.0;
    double RenderThreadP95Ms = 0.0;
    double GpuP95Ms = 0.0;
    int32 ResidentMemoryMiB = 0;
    int32 DrawCalls = 0;
    int32 StreamingPoolMiB = 0;
    double StreamingSpikeP95Ms = 0.0;
};

USTRUCT(BlueprintType)
struct AEROSIM_API FPhase7GateResult
{
    GENERATED_BODY()

    bool bPass = false;
    FString Status = TEXT("BLOCKED_MISSING_EVIDENCE");
    TArray<FString> Reasons;
};

/** Fail-closed: missing provenance or any exceeded budget is not a pass. */
struct AEROSIM_API FPhase7PerformanceGate final
{
    static FPhase7GateResult Evaluate(const FPhase7PerformanceBudget& Budget, const FPhase7PerformanceEvidence& Evidence);
};

UENUM(BlueprintType)
enum class EPhase7WindowMode : uint8
{
    Windowed,
    Borderless,
    Fullscreen
};

USTRUCT(BlueprintType)
struct AEROSIM_API FPhase7Settings
{
    GENERATED_BODY()

    static constexpr int32 CurrentVersion = 2;

    int32 Version = CurrentVersion;
    float VerticalFov = 60.0F;
    EPhase7QualityTier QualityTier = EPhase7QualityTier::DesktopHigh;
    float MasterVolume = 1.0F;
    bool bTelemetryEnabled = false;
    int32 ResolutionWidth = 1440;
    int32 ResolutionHeight = 900;
    EPhase7WindowMode WindowMode = EPhase7WindowMode::Windowed;

    bool IsValid() const;
};

/** Preview/apply/cancel plus one-level rollback; persistence only consumes Applied. */
struct AEROSIM_API FPhase7SettingsTransaction final
{
    FPhase7Settings Applied;
    FPhase7Settings Opening;
    FPhase7Settings Preview;
    bool bPreviewActive = false;
    bool bCanRollback = false;

    bool Begin(const FPhase7Settings& Candidate);
    bool Apply();
    void Cancel();
    bool Rollback();
};

/** Save DTO migration. Newer versions and invalid values fail closed. */
struct AEROSIM_API FPhase7SaveCodec final
{
    static FString Encode(const FPhase7Settings& Settings);
    static bool DecodeAndMigrate(const FString& Json, FPhase7Settings& OutSettings, FString& OutError);
    static bool WriteCrashSafe(const FString& FinalPath, const FString& Payload, FString& OutError);
};

/** Removes common credential, email, bearer and user-home forms before structured logging. */
struct AEROSIM_API FPhase7LogSanitizer final
{
    static FString Redact(const FString& Input);
};

/** Fixed-step replay harness. Digest must match across repeated runs. */
struct AEROSIM_API FPhase7DeterministicSoak final
{
    static constexpr int32 StepsPerSecond = 60;
    static uint64 Run(int32 DurationSeconds, int32 Seed, int32& OutSteps);
};
