#pragma once

#include "CoreMinimal.h"
#include "Phase2Systems.h"
#include "Phase4Mission.generated.h"

enum class EFlightPhase : uint8;

UENUM(BlueprintType)
enum class EPhase4MissionStage : uint8
{
    Hangar,
    Preflight,
    Takeoff,
    Inspection,
    ReturnToHome,
    Landing,
    Result,
    Replay
};

UENUM(BlueprintType)
enum class EPhase4Incident : uint8
{
    None,
    Boundary,
    Collision,
    MissedCheckpoint,
    UnsafeLanding
};

UENUM(BlueprintType)
enum class EPhase4SemanticCommand : uint8
{
    Confirm,
    ReturnToHome,
    PauseSettings,
    ResumeCentered,
    RetryCheckpoint,
    OpenReplay
};

USTRUCT(BlueprintType)
struct AEROSIM_API FPhase4Checkpoint
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Mission") FName Id;
    UPROPERTY(BlueprintReadOnly, Category = "Mission") FString Label;
    UPROPERTY(BlueprintReadOnly, Category = "Mission") FString ShapeCue;
    UPROPERTY(BlueprintReadOnly, Category = "Mission") FVector Position = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Category = "Mission") float RadiusCm = 2500.0F;
};

USTRUCT(BlueprintType)
struct AEROSIM_API FPhase4TaskDefinition
{
    GENERATED_BODY()

    // v2 replaces inland proxy checkpoints with actual coastal/sea/corridor objectives.
    static constexpr int32 CurrentVersion = 2;
    static constexpr float SafeHalfLengthCm = 25000.0F;
    static constexpr float SafeHalfWidthCm = 15000.0F;
    static constexpr float SafeMaximumAltitudeCm = 2500.0F;

    UPROPERTY(BlueprintReadOnly, Category = "Mission") FName MissionId = TEXT("pacific-coast-inspection");
    UPROPERTY(BlueprintReadOnly, Category = "Mission") int32 Version = CurrentVersion;
    UPROPERTY(BlueprintReadOnly, Category = "Mission") FString Goal = TEXT("Take off, inspect checkpoints 1-5 in order, return home, and land safely.");
    UPROPERTY(BlueprintReadOnly, Category = "Mission") TArray<FPhase4Checkpoint> Checkpoints;
    UPROPERTY(BlueprintReadOnly, Category = "Mission") float TargetDurationSeconds = 75.0F;

    /** Factory name retained for source compatibility; returns CurrentVersion (v2). */
    static FPhase4TaskDefinition MakePacificCoastV1();
    bool IsValid() const;
    bool IsInsideSafetyVolume(const FVector& Position) const;
    FVector GetRecoveryPoint(const FVector& Position) const;
};

USTRUCT(BlueprintType)
struct AEROSIM_API FPhase4PreflightChecklist
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Preflight") bool bInputCentered = false;
    UPROPERTY(BlueprintReadOnly, Category = "Preflight") bool bCameraChecked = false;
    UPROPERTY(BlueprintReadOnly, Category = "Preflight") bool bEnvironmentChecked = false;
    UPROPERTY(BlueprintReadOnly, Category = "Preflight") bool bQualityChecked = false;

    bool IsComplete() const { return bInputCentered && bCameraChecked && bEnvironmentChecked && bQualityChecked; }
};

USTRUCT(BlueprintType)
struct AEROSIM_API FPhase4Score
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Score") int32 Safety = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Score") int32 Precision = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Score") int32 Efficiency = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Score") int32 Smoothness = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Score") int32 Total = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Score") int64 ReplayDigest = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Score") bool bComplete = false;

    bool Equals(const FPhase4Score& Other) const;
};

USTRUCT(BlueprintType)
struct AEROSIM_API FPhase4Feedback
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Feedback") EPhase4Incident Incident = EPhase4Incident::None;
    UPROPERTY(BlueprintReadOnly, Category = "Feedback") FString Headline = TEXT("READY");
    UPROPERTY(BlueprintReadOnly, Category = "Feedback") FString RecoveryAction = TEXT("Follow the numbered route markers.");
    UPROPERTY(BlueprintReadOnly, Category = "Feedback") FString ShapeCue = TEXT("CIRCLE");
    UPROPERTY(BlueprintReadOnly, Category = "Feedback") FVector RecoveryPoint = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Category = "Feedback") bool bRecoverable = true;
};

/** Deterministic Phase 4 training contract. It owns task order, recoverable incidents and score inputs. */
USTRUCT(BlueprintType)
struct AEROSIM_API FPhase4MissionSession
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Mission") FPhase4TaskDefinition Task;
    UPROPERTY(BlueprintReadOnly, Category = "Mission") EPhase4MissionStage Stage = EPhase4MissionStage::Hangar;
    UPROPERTY(BlueprintReadOnly, Category = "Mission") FPhase4PreflightChecklist Preflight;
    UPROPERTY(BlueprintReadOnly, Category = "Mission") FPhase4Feedback Feedback;
    UPROPERTY(BlueprintReadOnly, Category = "Mission") FPhase4Score Score;
    UPROPERTY(BlueprintReadOnly, Category = "Mission") int32 NextCheckpointIndex = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Mission") int32 BoundaryViolations = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Mission") int32 Collisions = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Mission") int32 MissedCheckpoints = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Mission") float ElapsedSeconds = 0.0F;

    void Reset();
    void EnterPreflight();
    void SetPreflightComplete(bool bInputCentered);
    bool BeginMission();
    /** Positions/velocities are resolved WORLD coordinates; completion needs independently queried home support. */
    void Observe(float DeltaSeconds, EFlightPhase FlightPhase, const FVector& Position, const FVector& Velocity, const FRotator& Rotation, bool bConfirmedHomeSupport = false);
    void ReportCollision(const FVector& Position);
    void ReleaseCollisionContact() { bCollisionLatched=false; }
    void ReportWaterContact(const FVector& Position);
    void RequestReturnToHome();
    bool OpenReplay();
    void ConfigureEvidenceState(EPhase4MissionStage EvidenceStage);
    FString GetObjectiveText() const;
    FString GetProgressText() const;
    FString GetProgressiveHint() const;
    bool IsRecovering() const { return Feedback.Incident != EPhase4Incident::None; }

private:
    void ObserveCheckpoint(const FVector& Position);
    void CompleteMission();
    void BuildScore();
    uint32 BuildDeterministicDigest() const;

    float PrecisionSum = 0.0F;
    float SmoothnessPenalty = 0.0F;
    float LastSpeedCmPerSecond = 0.0F;
    bool bBoundaryLatched = false;
    bool bCollisionLatched = false;
};
