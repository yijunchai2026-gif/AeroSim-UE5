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

// DEV-FLIGHT navigation contract (docs/tasks/AAA-R5-NAVIGATION-GAP.md). Guidance is a
// separate layer: it never scores, never moves the Pawn and never changes the five
// checkpoints' identity, order, count, radius or LOW WATER height window.
enum class EPhase4GuideKind : uint8 { Takeoff, Transit, VerticalDescent, VerticalAscent, Corridor, Landing, Hold };

/** One manually flown navigation leg: an ordered WORLD polyline ending at its guide point. */
struct AEROSIM_API FPhase4GuidanceLeg
{
    FName LegId;                 // stable, e.g. "nav-05-offshore-descent"
    FName GuideId;               // stable id of the leg's end guide point
    FString Label;
    EPhase4GuideKind Kind = EPhase4GuideKind::Transit;
    TArray<FVector> Path;        // Path[0] is the previous leg's end; Path.Last() is the guide point
    float AcceptRadiusCm = 300.0F;          // planar distance to the guide point
    float AcceptBelowCm = 150.0F;           // allowed origin height below the guide point
    float AcceptAboveCm = 150.0F;           // allowed origin height above the guide point
    float MaxAcceptPlanarSpeedCm = 1800.0F; // speed window at the guide point
    float MaxAcceptVerticalSpeedCm = 10000.0F; // vertical speed window at the guide point
    float VerticalBrakeZoneCm = 0.0F;       // remaining height at which the guide asks to brake vertically
    float TrackToleranceCm = 600.0F;        // deviation from the leg that still counts as following it
    float MaxTrackPlanarSpeedCm = 1800.0F;  // progress only counts at or below this planar speed
    float LookaheadCm = 1500.0F;            // steer point ahead of the actual projected progress
    int32 ScoredCheckpointIndex = INDEX_NONE;
    FName ScoredCheckpointId;
    bool bReturnLeg = false;
    /** Transit/corridor: pass-through acceptance (radius/height window or gate-plane crossing, no speed).
        False for safety gates: column tops/bottoms, brake points, C3 and landing keep tight windows. */
    bool bPassThrough = false;

    float GetLength() const;
    FVector Evaluate(float DistanceCm) const;
    /** Closest point within [MinCm,MaxCm] of arc length; returns its arc length and 3D distance. */
    float Project(const FVector& Position, float MinCm, float MaxCm, float& OutDistanceCm) const;
};

/** The single read-only guidance output consumed by HUD arrow, range/height, feedback and recovery. */
struct AEROSIM_API FPhase4GuidanceTarget
{
    FName LegId;
    FName GuideId;
    FString Label;
    EPhase4GuideKind Kind = EPhase4GuideKind::Hold;
    FVector Position = FVector::ZeroVector;   // steer point on the verified leg (or recovery anchor)
    FVector GuidePoint = FVector::ZeroVector; // the leg's end guide point
    float AcceptRadiusCm = 0.0F;
    float AcceptBelowCm = 0.0F;
    float AcceptAboveCm = 0.0F;
    float MaxAcceptPlanarSpeedCm = 0.0F;
    float MaxAcceptVerticalSpeedCm = 0.0F;
    bool bVerticalBrake = false;
    bool bPassThrough = false;
    float RemainingCm = 0.0F;
    int32 LegIndex = INDEX_NONE;
    int32 LegCount = 0;
    int32 ScoredCheckpointIndex = INDEX_NONE;
    FName ScoredCheckpointId;
    bool bRecovery = false;
    bool bHold = false;
    bool IsValid() const { return !LegId.IsNone(); }
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
    /** Deterministic guidance legs between the unchanged five checkpoints (DEV-FLIGHT). */
    TArray<FPhase4GuidanceLeg> BuildGuidanceRoute() const;
    /** Structural/safety-volume check sampled every <=100 cm; real collision is a World Automation test. */
    bool ValidateGuidanceStructure(const TArray<FPhase4GuidanceLeg>& Route, FString& OutFailure) const;
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
    /** The one current guidance output. Read-only; nothing outside Observe advances it. */
    const FPhase4GuidanceTarget& GetCurrentGuidanceTarget() const { return CurrentGuidance; }
    const TArray<FPhase4GuidanceLeg>& GetGuidanceRoute() const { return GuidanceRoute; }
    int32 GetGuidanceLegIndex() const { return GuidanceLegIndex; }
    float GetGuidanceLegProgressCm() const { return GuidanceProgressCm; }

private:
    void ObserveGuidance(const FVector& Position, const FVector& Velocity);
    void AdvanceGuidanceLeg();
    void SetRecoveryGuide(const FVector& Position, bool bWater);
    bool IsOnCurrentLeg(const FVector& Position, float* OutAlongCm = nullptr) const;
    void RefreshGuidance(const FVector& Position);
    void ApplyGuidanceFeedback();
    int32 FindFirstReturnLeg() const;
    void ObserveCheckpoint(const FVector& Position);
    void CompleteMission();
    void BuildScore();
    uint32 BuildDeterministicDigest() const;

    float PrecisionSum = 0.0F;
    float SmoothnessPenalty = 0.0F;
    float LastSpeedCmPerSecond = 0.0F;
    bool bBoundaryLatched = false;
    bool bCollisionLatched = false;
    TArray<FPhase4GuidanceLeg> GuidanceRoute;
    int32 GuidanceLegIndex = 0;
    float GuidanceProgressCm = 0.0F;
    bool bRecoveryGuideActive = false;
    float RecoveryAnchorCm = 0.0F;
    FPhase4GuidanceTarget CurrentGuidance;
    FPhase4GuidanceTarget RecoveryGuidance;
};
