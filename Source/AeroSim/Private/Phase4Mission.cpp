#include "Phase4Mission.h"

#include "FlightPawn.h"
#include "PacificCoastGraybox.h"
#include "PacificCoastFlora.h"
#include "Misc/Crc.h"

FPhase4TaskDefinition FPhase4TaskDefinition::MakePacificCoastV1()
{
    FPhase4TaskDefinition Result;
    const TArray<FVector>& Route = FPacificCoastGrayboxContract::GetRoutePoints();
    const TCHAR* Labels[] = { TEXT("GRASS / SOIL"), TEXT("TIDAL ROCK"), TEXT("LOW WATER"), TEXT("FOREST ENTRY"), TEXT("FOREST CORRIDOR") };
    const TCHAR* Shapes[] = { TEXT("CIRCLE 1"), TEXT("DIAMOND 2"), TEXT("SQUARE 3"), TEXT("TRIANGLE 4"), TEXT("HEXAGON 5") };
    for (int32 Index = 0; Index < FPacificCoastGrayboxContract::GateCount; ++Index)
    {
        FPhase4Checkpoint& Checkpoint = Result.Checkpoints.AddDefaulted_GetRef();
        Checkpoint.Id = FName(*FString::Printf(TEXT("checkpoint-%02d"), Index + 1));
        Checkpoint.Label = Labels[Index];
        Checkpoint.ShapeCue = Shapes[Index];
        Checkpoint.Position = Route[Index + 1];
        Checkpoint.RadiusCm = FPacificCoastGrayboxContract::RouteToleranceCm;
        if (Index == 2) Checkpoint.RadiusCm = 500.0F; // sea +4 m cannot count from the old launch-height plane
        if (Index >= 3) Checkpoint.RadiusCm = 400.0F;
    }
    return Result;
}

bool FPhase4TaskDefinition::IsValid() const
{
    if (Version != CurrentVersion || MissionId.IsNone() || Checkpoints.Num() != FPacificCoastGrayboxContract::GateCount)
    {
        return false;
    }
    TSet<FName> Ids;
    for (const FPhase4Checkpoint& Checkpoint : Checkpoints)
    {
        if (Checkpoint.Id.IsNone() || Checkpoint.Label.IsEmpty() || Checkpoint.ShapeCue.IsEmpty()
            || Checkpoint.RadiusCm <= 0.0F || !IsInsideSafetyVolume(Checkpoint.Position))
        {
            return false;
        }
        Ids.Add(Checkpoint.Id);
    }
    return Ids.Num() == Checkpoints.Num() && TargetDurationSeconds > 0.0F;
}

bool FPhase4TaskDefinition::IsInsideSafetyVolume(const FVector& Position) const
{
    if (Position.ContainsNaN() || Position.Z > SafeMaximumAltitudeCm
        || Position.Z < APacificCoastGraybox::SeaLevelCm + 100.0F) return false;
    const bool bCore = FMath::Abs(Position.X) <= SafeHalfLengthCm
        && FMath::Abs(Position.Y) <= SafeHalfWidthCm
        && Position.Z >= 0.0F;
    if (bCore) return true;
    // Narrow bounded coastal inspection band, outside the actual cliff foot, not a giant box.
    if (FMath::Abs(Position.Y) <= 15000.0F)
    {
        const float Shore = APacificCoastGraybox::GetCoastalShoreX(Position.Y);
        if (Position.X >= Shore + 300.0F && Position.X <= Shore + 8500.0F) return true;
    }
    // Existing full forest route, with limited buffer for aircraft/camera recovery.
    const float Ground = APacificCoastGraybox::GetCoastalLandHeight(Position.X,Position.Y);
    return Position.Z >= Ground + 50.0F && Position.Z <= Ground + 1500.0F
        && CoastFlora::CorridorDistance(FVector2D(Position)) <= 1200.0F;
}

FVector FPhase4TaskDefinition::GetRecoveryPoint(const FVector& Position) const
{
    FVector Best(55,79,500); double Distance=BIG_NUMBER;
    for (const FVector& Point : FPacificCoastGrayboxContract::GetRoutePoints())
    {
        FVector Candidate=Point;
        if (Point.Z >= 0) Candidate.Z=FMath::Max(Candidate.Z,APacificCoastGraybox::GetCoastalLandHeight(Point.X,Point.Y)+300.0F);
        if (IsInsideSafetyVolume(Candidate) && FVector::DistSquared(Position,Candidate)<Distance)
        { Best=Candidate; Distance=FVector::DistSquared(Position,Candidate); }
    }
    return Best;
}

bool FPhase4Score::Equals(const FPhase4Score& Other) const
{
    return Safety == Other.Safety && Precision == Other.Precision && Efficiency == Other.Efficiency
        && Smoothness == Other.Smoothness && Total == Other.Total && ReplayDigest == Other.ReplayDigest
        && bComplete == Other.bComplete;
}

void FPhase4MissionSession::Reset()
{
    Task = FPhase4TaskDefinition::MakePacificCoastV1();
    Stage = EPhase4MissionStage::Hangar;
    Preflight = FPhase4PreflightChecklist();
    Feedback = FPhase4Feedback();
    Score = FPhase4Score();
    NextCheckpointIndex = 0;
    BoundaryViolations = 0;
    Collisions = 0;
    MissedCheckpoints = 0;
    ElapsedSeconds = 0.0F;
    PrecisionSum = 0.0F;
    SmoothnessPenalty = 0.0F;
    LastSpeedCmPerSecond = 0.0F;
    bBoundaryLatched = false;
    bCollisionLatched = false;
}

void FPhase4MissionSession::EnterPreflight()
{
    if (Stage == EPhase4MissionStage::Hangar)
    {
        Stage = EPhase4MissionStage::Preflight;
        Feedback.Headline = TEXT("PREFLIGHT / 4 CHECKS");
        Feedback.RecoveryAction = TEXT("Center input, verify camera, environment, and quality.");
        Feedback.ShapeCue = TEXT("CHECKLIST");
    }
}

void FPhase4MissionSession::SetPreflightComplete(const bool bInputCentered)
{
    if (Stage != EPhase4MissionStage::Preflight)
    {
        return;
    }
    Preflight.bInputCentered = bInputCentered;
    Preflight.bCameraChecked = true;
    Preflight.bEnvironmentChecked = true;
    Preflight.bQualityChecked = true;
    Feedback.Headline = Preflight.IsComplete() ? TEXT("PREFLIGHT COMPLETE") : TEXT("CENTER ALL INPUTS");
    Feedback.RecoveryAction = Preflight.IsComplete() ? TEXT("Confirm to arm the training mission.") : TEXT("Release all flight controls.");
}

bool FPhase4MissionSession::BeginMission()
{
    if (Stage != EPhase4MissionStage::Preflight || !Preflight.IsComplete() || !Task.IsValid())
    {
        return false;
    }
    Stage = EPhase4MissionStage::Takeoff;
    Feedback = FPhase4Feedback();
    Feedback.Headline = TEXT("TAKE OFF");
    Feedback.RecoveryAction = TEXT("Raise throttle smoothly to 3 m, then follow marker 1.");
    Feedback.ShapeCue = TEXT("UP ARROW");
    return true;
}

void FPhase4MissionSession::Observe(const float DeltaSeconds, const EFlightPhase FlightPhase, const FVector& Position, const FVector& Velocity, const FRotator& Rotation, const bool bConfirmedHomeSupport)
{
    if (Stage < EPhase4MissionStage::Takeoff || Stage > EPhase4MissionStage::Landing || DeltaSeconds <= 0.0F)
    {
        return;
    }
    ElapsedSeconds += FMath::Min(DeltaSeconds, 0.1F);
    const float Speed = Velocity.Size();
    SmoothnessPenalty += FMath::Abs(Speed - LastSpeedCmPerSecond) / 100.0F;
    SmoothnessPenalty += (FMath::Abs(Rotation.Roll) + FMath::Abs(Rotation.Pitch)) * DeltaSeconds * 0.015F;
    LastSpeedCmPerSecond = Speed;

    const bool bInside = Task.IsInsideSafetyVolume(Position);
    if (!bInside && !bBoundaryLatched)
    {
        ++BoundaryViolations;
        bBoundaryLatched = true;
        Feedback.Incident = EPhase4Incident::Boundary;
        Feedback.Headline = TEXT("OUTSIDE SAFE ZONE");
        Feedback.RecoveryAction = TEXT("Slow down and follow the inward chevrons; progress is preserved.");
        Feedback.ShapeCue = TEXT("INWARD CHEVRONS");
        Feedback.RecoveryPoint = Task.GetRecoveryPoint(Position);
    }
    else if (bInside && bBoundaryLatched)
    {
        bBoundaryLatched = false;
        Feedback.Incident = EPhase4Incident::None;
        Feedback.Headline = TEXT("SAFE ZONE RECOVERED");
        Feedback.RecoveryAction = TEXT("Continue to the next numbered checkpoint.");
        Feedback.ShapeCue = TEXT("CHECKMARK");
    }

    if (Stage == EPhase4MissionStage::Takeoff && (FlightPhase == EFlightPhase::Takeoff || FlightPhase == EFlightPhase::Flight))
    {
        Stage = EPhase4MissionStage::Inspection;
    }
    if (Stage == EPhase4MissionStage::Inspection && bInside)
    {
        ObserveCheckpoint(Position);
    }
    if (Stage == EPhase4MissionStage::ReturnToHome && FVector::Dist2D(Position, FVector(55,79,0)) <= 1800.0F)
    {
        Stage = EPhase4MissionStage::Landing;
        Feedback.Headline = TEXT("LAND ON PAD");
        Feedback.RecoveryAction = TEXT("Reduce speed, descend vertically, and keep the horizon level.");
        Feedback.ShapeCue = TEXT("DOWN ARROW + PAD");
    }
    if (Stage == EPhase4MissionStage::Landing && FlightPhase == EFlightPhase::Grounded && bConfirmedHomeSupport
        && FVector::Dist2D(Position,FVector(55,79,0)) <= FPacificCoastGrayboxContract::TakeoffPadRadiusCm
        && Velocity.Size()<40.0F)
    {
        CompleteMission();
    }
}

void FPhase4MissionSession::ReportWaterContact(const FVector& Position)
{
    if (Stage < EPhase4MissionStage::Takeoff || Stage > EPhase4MissionStage::Landing) return;
    Feedback.Incident=EPhase4Incident::UnsafeLanding;
    Feedback.Headline=TEXT("WATER CONTACT / SAFE PAUSE");
    Feedback.ShapeCue=TEXT("STOP OCTAGON");
    Feedback.RecoveryAction=TEXT("Water is not a landing surface. Reset to the launch mat to retry.");
    Feedback.RecoveryPoint=Task.GetRecoveryPoint(Position);
}

void FPhase4MissionSession::ObserveCheckpoint(const FVector& Position)
{
    if (!Task.Checkpoints.IsValidIndex(NextCheckpointIndex))
    {
        RequestReturnToHome();
        return;
    }
    const FPhase4Checkpoint& Expected = Task.Checkpoints[NextCheckpointIndex];
    const float Distance = FVector::Distance(Position, Expected.Position);
    for (int32 Future = NextCheckpointIndex + 1; Future < Task.Checkpoints.Num(); ++Future)
    {
        if (FVector::Distance(Position, Task.Checkpoints[Future].Position) <= Task.Checkpoints[Future].RadiusCm)
        {
            ++MissedCheckpoints;
            Feedback.Incident = EPhase4Incident::MissedCheckpoint;
            Feedback.Headline = FString::Printf(TEXT("MISSED %s"), *Expected.Id.ToString().ToUpper());
            Feedback.RecoveryAction = TEXT("Turn back to the highlighted numbered gate; completed gates stay saved.");
            Feedback.ShapeCue = Expected.ShapeCue;
            Feedback.RecoveryPoint = Expected.Position;
            return;
        }
    }
    const bool bLowWaterHeight = Expected.Label != TEXT("LOW WATER")
        || (Position.Z >= APacificCoastGraybox::SeaLevelCm+300 && Position.Z <= APacificCoastGraybox::SeaLevelCm+500);
    if (Distance <= Expected.RadiusCm && bLowWaterHeight)
    {
        PrecisionSum += FMath::Clamp(1.0F - Distance / Expected.RadiusCm, 0.0F, 1.0F);
        ++NextCheckpointIndex;
        Feedback.Incident = EPhase4Incident::None;
        Feedback.Headline = FString::Printf(TEXT("CHECKPOINT %d COMPLETE"), NextCheckpointIndex);
        Feedback.ShapeCue = TEXT("CHECKMARK");
        if (NextCheckpointIndex >= Task.Checkpoints.Num())
        {
            RequestReturnToHome();
        }
        else
        {
            Feedback.RecoveryAction = FString::Printf(TEXT("Continue to %s."), *Task.Checkpoints[NextCheckpointIndex].ShapeCue);
        }
    }
}

void FPhase4MissionSession::ReportCollision(const FVector& Position)
{
    if (Stage < EPhase4MissionStage::Takeoff || Stage > EPhase4MissionStage::Landing || bCollisionLatched)
    {
        return;
    }
    ++Collisions;
    bCollisionLatched = true;
    Feedback.Incident = EPhase4Incident::Collision;
    Feedback.Headline = TEXT("COLLISION / HOLD POSITION");
    Feedback.RecoveryAction = TEXT("Inputs neutralized. Stabilize, then retry the current checkpoint.");
    Feedback.ShapeCue = TEXT("STOP OCTAGON");
    Feedback.RecoveryPoint = Task.GetRecoveryPoint(Position);
}

void FPhase4MissionSession::RequestReturnToHome()
{
    if (Stage == EPhase4MissionStage::Inspection && NextCheckpointIndex < Task.Checkpoints.Num())
    {
        Feedback.Incident = EPhase4Incident::MissedCheckpoint;
        Feedback.Headline = TEXT("ROUTE INCOMPLETE");
        Feedback.RecoveryAction = TEXT("Return to the highlighted checkpoint before RTH.");
        Feedback.ShapeCue = Task.Checkpoints[NextCheckpointIndex].ShapeCue;
        Feedback.RecoveryPoint = Task.Checkpoints[NextCheckpointIndex].Position;
        return;
    }
    if (Stage == EPhase4MissionStage::Inspection || Stage == EPhase4MissionStage::ReturnToHome)
    {
        Stage = EPhase4MissionStage::ReturnToHome;
        Feedback.Incident = EPhase4Incident::None;
        Feedback.Headline = TEXT("RETURN TO HOME");
        Feedback.RecoveryAction = TEXT("Follow the HOME diamond to the takeoff pad.");
        Feedback.ShapeCue = TEXT("HOME DIAMOND");
        Feedback.RecoveryPoint = FPacificCoastGrayboxContract::GetRoutePoints()[0];
    }
}

void FPhase4MissionSession::CompleteMission()
{
    Stage = EPhase4MissionStage::Result;
    Feedback.Incident = EPhase4Incident::None;
    Feedback.Headline = TEXT("MISSION COMPLETE");
    Feedback.RecoveryAction = TEXT("Review the four score dimensions or open deterministic Replay.");
    Feedback.ShapeCue = TEXT("RESULT CARD");
    BuildScore();
}

void FPhase4MissionSession::BuildScore()
{
    Score.Safety = FMath::Clamp(100 - BoundaryViolations * 12 - Collisions * 25 - MissedCheckpoints * 5, 0, 100);
    Score.Precision = Task.Checkpoints.IsEmpty() ? 0 : FMath::Clamp(FMath::RoundToInt(100.0F * PrecisionSum / Task.Checkpoints.Num()), 0, 100);
    const float TimeOver = FMath::Max(0.0F, ElapsedSeconds - Task.TargetDurationSeconds);
    Score.Efficiency = FMath::Clamp(FMath::RoundToInt(100.0F - TimeOver * 1.5F - MissedCheckpoints * 6.0F), 0, 100);
    Score.Smoothness = FMath::Clamp(FMath::RoundToInt(100.0F - SmoothnessPenalty * 0.25F - Collisions * 10.0F), 0, 100);
    Score.Total = FMath::RoundToInt(Score.Safety * 0.35F + Score.Precision * 0.30F + Score.Efficiency * 0.20F + Score.Smoothness * 0.15F);
    Score.bComplete = NextCheckpointIndex == Task.Checkpoints.Num();
    Score.ReplayDigest = BuildDeterministicDigest();
}

uint32 FPhase4MissionSession::BuildDeterministicDigest() const
{
    uint32 Hash = FCrc::StrCrc32(*Task.MissionId.ToString());
    Hash = HashCombine(Hash, GetTypeHash(Task.Version));
    Hash = HashCombine(Hash, GetTypeHash(NextCheckpointIndex));
    Hash = HashCombine(Hash, GetTypeHash(BoundaryViolations));
    Hash = HashCombine(Hash, GetTypeHash(Collisions));
    Hash = HashCombine(Hash, GetTypeHash(MissedCheckpoints));
    Hash = HashCombine(Hash, GetTypeHash(FMath::RoundToInt(ElapsedSeconds * 10.0F)));
    Hash = HashCombine(Hash, GetTypeHash(FMath::RoundToInt(PrecisionSum * 1000.0F)));
    return Hash;
}

bool FPhase4MissionSession::OpenReplay()
{
    if (Stage != EPhase4MissionStage::Result || !Score.bComplete || Score.ReplayDigest != BuildDeterministicDigest())
    {
        return false;
    }
    Stage = EPhase4MissionStage::Replay;
    Feedback.Headline = TEXT("DETERMINISTIC REPLAY");
    Feedback.RecoveryAction = FString::Printf(TEXT("Result aligned / digest %08llX"), static_cast<unsigned long long>(Score.ReplayDigest));
    Feedback.ShapeCue = TEXT("PLAYBACK TRIANGLE");
    return true;
}

void FPhase4MissionSession::ConfigureEvidenceState(const EPhase4MissionStage EvidenceStage)
{
    Reset();
    Stage = EvidenceStage;
    if (EvidenceStage == EPhase4MissionStage::Preflight)
    {
        SetPreflightComplete(true);
    }
    else if (EvidenceStage == EPhase4MissionStage::Inspection)
    {
        NextCheckpointIndex = 2;
        Feedback.Headline = TEXT("CHECKPOINT 3 / BOARDWALK");
        Feedback.RecoveryAction = TEXT("Fly through SQUARE 3; keep the center view clear.");
        Feedback.ShapeCue = TEXT("SQUARE 3");
    }
    else if (EvidenceStage == EPhase4MissionStage::ReturnToHome)
    {
        NextCheckpointIndex = Task.Checkpoints.Num();
        BoundaryViolations = 1;
        Feedback.Incident = EPhase4Incident::Boundary;
        Feedback.Headline = TEXT("OUTSIDE SAFE ZONE");
        Feedback.RecoveryAction = TEXT("Follow inward chevrons to recover; progress is preserved.");
        Feedback.ShapeCue = TEXT("INWARD CHEVRONS");
        Feedback.RecoveryPoint = FVector(12000.0F, 8000.0F, 700.0F);
    }
    else if (EvidenceStage == EPhase4MissionStage::Result || EvidenceStage == EPhase4MissionStage::Replay)
    {
        NextCheckpointIndex = Task.Checkpoints.Num();
        PrecisionSum = 4.35F;
        ElapsedSeconds = 78.4F;
        SmoothnessPenalty = 26.0F;
        BuildScore();
        Stage = EvidenceStage;
        Feedback.Headline = EvidenceStage == EPhase4MissionStage::Replay ? TEXT("DETERMINISTIC REPLAY") : TEXT("MISSION COMPLETE");
        Feedback.RecoveryAction = FString::Printf(TEXT("Result aligned / digest %08llX"), static_cast<unsigned long long>(Score.ReplayDigest));
        Feedback.ShapeCue = EvidenceStage == EPhase4MissionStage::Replay ? TEXT("PLAYBACK TRIANGLE") : TEXT("RESULT CARD");
    }
}

FString FPhase4MissionSession::GetObjectiveText() const
{
    switch (Stage)
    {
    case EPhase4MissionStage::Hangar: return TEXT("Select TRAINING PROXY and review BLOCKED_ASSET status");
    case EPhase4MissionStage::Preflight: return TEXT("Complete 4/4 preflight safety checks");
    case EPhase4MissionStage::Takeoff: return TEXT("Take off smoothly to route altitude");
    case EPhase4MissionStage::Inspection: return Task.Checkpoints.IsValidIndex(NextCheckpointIndex)
        ? FString::Printf(TEXT("Inspect %s / %s"), *Task.Checkpoints[NextCheckpointIndex].Id.ToString().ToUpper(), *Task.Checkpoints[NextCheckpointIndex].Label)
        : TEXT("Inspection complete");
    case EPhase4MissionStage::ReturnToHome: return TEXT("Return to HOME diamond");
    case EPhase4MissionStage::Landing: return TEXT("Land inside the marked pad");
    case EPhase4MissionStage::Result: return TEXT("Review score and incident breakdown");
    case EPhase4MissionStage::Replay: return TEXT("Replay the exact scored telemetry");
    default: return TEXT("Mission");
    }
}

FString FPhase4MissionSession::GetProgressText() const
{
    return FString::Printf(TEXT("%d/%d CHECKPOINTS  |  %s"), NextCheckpointIndex, Task.Checkpoints.Num(), Score.bComplete ? TEXT("COMPLETE") : TEXT("IN PROGRESS"));
}

FString FPhase4MissionSession::GetProgressiveHint() const
{
    if (IsRecovering()) return Feedback.RecoveryAction;
    if (Stage == EPhase4MissionStage::Hangar) return TEXT("[ENTER / SPACE] Open Preflight");
    if (Stage == EPhase4MissionStage::Preflight) return Preflight.IsComplete() ? TEXT("[ENTER / SPACE] Start mission") : TEXT("Center controls to finish checks");
    if (Stage == EPhase4MissionStage::Inspection && ElapsedSeconds < 18.0F) return TEXT("Number + shape identify the next gate; color is secondary.");
    if (Stage == EPhase4MissionStage::Inspection && ElapsedSeconds < 35.0F) return TEXT("Use RTH only after all five checkpoints.");
    return Feedback.RecoveryAction;
}
