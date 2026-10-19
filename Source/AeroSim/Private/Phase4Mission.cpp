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

// Guide points describe a manually flown path, never actor movement or extra score.
TArray<FPhase4GuidancePoint> FPhase4TaskDefinition::BuildGuidanceRoute() const
{
    TArray<FPhase4GuidancePoint> Route;
    if (Checkpoints.Num() != 5) return Route;
    auto Add = [&Route](const TCHAR* Label, const FVector& P, EPhase4GuideKind Kind = EPhase4GuideKind::Transit, int32 Score = INDEX_NONE, float Speed = 1800.0F)
    {
        FPhase4GuidancePoint G;
        G.Id = FName(*FString::Printf(TEXT("coast-guide-%03d"), Route.Num()));
        G.Label = Label; G.Position = P; G.Kind = Kind; G.ScoredCheckpointIndex = Score; G.MaximumPlanarSpeedCm = Speed;
        Route.Add(G);
    };
    const FVector Home = FPacificCoastGrayboxContract::GetRoutePoints()[0];
    Add(TEXT("RISE VERTICALLY TO 3 M"), Home + FVector(0,0,300), EPhase4GuideKind::Takeoff, INDEX_NONE, 100);
    Add(TEXT("CIRCLE 1 / GRASS AND SOIL"), Checkpoints[0].Position, EPhase4GuideKind::Transit, 0);
    const FVector C2 = Checkpoints[1].Position, C3 = Checkpoints[2].Position, C4 = Checkpoints[3].Position, C5 = Checkpoints[4].Position;
    const FVector OutEdge(APacificCoastGraybox::GetCliffEdgeX(C2.Y)-800,C2.Y,0);
    const FVector InEdge(APacificCoastGraybox::GetCliffEdgeX(C3.Y)-800,C3.Y,0);
    // Coherent plateau clearance, not GetLandHeight extrapolated over the ocean.
    auto TransitHeight = [](const FVector& A, const FVector& B)
    {
        float H = 800;
        const int32 Steps = FMath::Max(1,FMath::CeilToInt(FVector::Dist2D(A,B)/100));
        for (int32 I=0; I<=Steps; ++I)
        {
            const FVector P = FMath::Lerp(A,B,I/static_cast<float>(Steps));
            H = FMath::Max(H,APacificCoastGraybox::GetCoastalLandHeight(P.X,P.Y)+300.0F);
        }
        return H; // Deliberately not clamped: safety validation must reject an excessive height.
    };
    const float Hout = TransitHeight(Checkpoints[0].Position,OutEdge);
    const FVector FieldApproach(C4.X+800,C4.Y+1200,APacificCoastGraybox::GetCoastalLandHeight(C4.X+800,C4.Y+1200)+600);
    const float Hin = TransitHeight(InEdge,FieldApproach);
    Add(TEXT("ABOVE CLIFF / KEEP ALTITUDE"), FVector(OutEdge.X,OutEdge.Y,Hout));
    Add(TEXT("OFFSHORE / BRAKE BEFORE DESCENT"), FVector(C2.X,C2.Y,Hout), EPhase4GuideKind::Transit, INDEX_NONE,100);
    Add(TEXT("DESCEND VERTICALLY / DIAMOND 2"), C2,EPhase4GuideKind::VerticalDescent,1,100);
    // Follow actual shore curvature. Close-spaced intermediate nodes bound the chord error;
    // the validator still tests the complete chord, not only these endpoints.
    for (int32 I=1; I<=24; ++I)
    {
        const float U=I/24.0F, Y=FMath::Lerp(static_cast<float>(C2.Y),static_cast<float>(C3.Y),U);
        const FVector P(APacificCoastGraybox::GetCoastalShoreX(Y)+FMath::Lerp(2500.0F,3500.0F,U),Y,FMath::Lerp(C2.Z,C3.Z,U));
        Add(I==24 ? TEXT("SQUARE 3 / LOW WATER") : TEXT("FOLLOW COAST / LOW WATER APPROACH"),P,EPhase4GuideKind::Transit,I==24 ? 2 : INDEX_NONE,600);
    }
    Add(TEXT("CLIMB VERTICALLY BEFORE RETURNING INLAND"),FVector(C3.X,C3.Y,Hin),EPhase4GuideKind::VerticalAscent,INDEX_NONE,100);
    Add(TEXT("CROSS ABOVE CLIFF / KEEP ALTITUDE"),FVector(InEdge.X,InEdge.Y,Hin));
    Add(TEXT("OPEN FIELD / APPROACH FOREST MOUTH"),FieldApproach);
    Add(TEXT("TRIANGLE 4 / FOREST ENTRY"),C4,EPhase4GuideKind::Corridor,3,400);
    TArray<FVector> ShortCorridor;
    float LastDistance=0;
    for (const auto& Sample : CoastFlora::GetLayout().Corridor)
    {
        const FVector P(Sample.Point.X,Sample.Point.Y,APacificCoastGraybox::GetCoastalLandHeight(Sample.Point.X,Sample.Point.Y)+250);
        if (FVector::Dist2D(P,C5)<55) break;
        if (Sample.Distance<LastDistance+150 || Sample.Distance<100) continue;
        LastDistance=Sample.Distance;
        ShortCorridor.Add(P);
        Add(TEXT("FOLLOW FOREST CORRIDOR / KEEP THE TURN"),P,EPhase4GuideKind::Corridor,INDEX_NONE,400);
    }
    Add(TEXT("HEXAGON 5 / FOREST CORRIDOR"),C5,EPhase4GuideKind::Corridor,4,400);
    for (int32 I=ShortCorridor.Num()-1; I>=0; --I)
        Add(TEXT("RETURN ALONG CORRIDOR / DO NOT CUT TREES"),ShortCorridor[I],EPhase4GuideKind::Corridor,INDEX_NONE,400);
    Add(TEXT("EXIT FOREST THROUGH SAME MOUTH"),C4,EPhase4GuideKind::Corridor,INDEX_NONE,400);
    Add(TEXT("OPEN FIELD / RETURN TO MAT"),FieldApproach);
    Add(TEXT("HOME / BRAKE ABOVE MAT"),FVector(Home.X,Home.Y,Home.Z+300),EPhase4GuideKind::Transit,INDEX_NONE,40);
    Add(TEXT("LAND VERTICALLY ON MAT"),Home,EPhase4GuideKind::Landing,INDEX_NONE,40);
    return Route;
}

bool FPhase4TaskDefinition::ValidateGuidanceSafety(const TArray<FPhase4GuidancePoint>& Route, FString& Failure) const
{
    if (Route.IsEmpty()) { Failure=TEXT("EMPTY_GUIDANCE"); return false; }
    int32 ExpectedScore=0;
    FVector Previous=FPacificCoastGrayboxContract::GetRoutePoints()[0];
    for (int32 Leg=0; Leg<Route.Num(); ++Leg)
    {
        const auto& G=Route[Leg];
        if (G.Id.IsNone() || G.Position.ContainsNaN() || G.HorizontalRadiusCm<=0 || G.HeightToleranceCm<=0)
        { Failure=FString::Printf(TEXT("INVALID_GUIDE leg=%d"),Leg); return false; }
        if (G.ScoredCheckpointIndex!=INDEX_NONE && G.ScoredCheckpointIndex!=ExpectedScore++)
        { Failure=FString::Printf(TEXT("SCORE_ORDER leg=%d"),Leg); return false; }
        const int32 Steps=FMath::Max(1,FMath::CeilToInt(FVector::Distance(Previous,G.Position)/100));
        for (int32 I=0; I<=Steps; ++I)
        {
            const FVector P=FMath::Lerp(Previous,G.Position,I/static_cast<float>(Steps));
            if (!IsInsideSafetyVolume(P))
            { Failure=FString::Printf(TEXT("UNSAFE_GUIDANCE leg=%d sample=%d xyz=%s"),Leg,I,*P.ToString()); return false; }
        }
        Previous=G.Position;
    }
    if (ExpectedScore!=Checkpoints.Num()) { Failure=TEXT("MISSING_SCORED_GUIDE"); return false; }
    Failure.Reset(); return true;
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
    GuidanceRoute = Task.BuildGuidanceRoute();
    GuidanceIndex = 0;
    bGuidanceContactHold = false;
    bRecoveryGuideActive = false;
    ContactPosition = FVector::ZeroVector;
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
    FString GuidanceFailure;
    if (!Task.ValidateGuidanceSafety(GuidanceRoute, GuidanceFailure))
    {
        Feedback.Headline=TEXT("GUIDANCE NOT VALIDATED"); Feedback.RecoveryAction=GuidanceFailure;
        return false;
    }
    Stage = EPhase4MissionStage::Takeoff;
    Feedback = FPhase4Feedback();
    Feedback.Headline = TEXT("TAKE OFF");
    Feedback.RecoveryAction = TEXT("Raise throttle smoothly to 3 m, then follow marker 1.");
    Feedback.ShapeCue = TEXT("UP ARROW");
    SyncGuidanceFeedback();
    return true;
}

void FPhase4MissionSession::Observe(const float DeltaSeconds, const EFlightPhase FlightPhase, const FVector& Position, const FVector& Velocity, const FRotator& Rotation, const bool bConfirmedHomeSupport)
{
    if (Stage < EPhase4MissionStage::Takeoff || Stage > EPhase4MissionStage::Landing || DeltaSeconds <= 0.0F || FlightPhase == EFlightPhase::Paused)
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
        const FPhase4GuidancePoint* Current=GetCurrentGuidanceTarget();
        if (Current)
        {
            const FVector From=GuidanceIndex>0 ? GuidanceRoute[GuidanceIndex-1].Position : FPacificCoastGrayboxContract::GetRoutePoints()[0];
            RecoveryGuide=*Current;
            RecoveryGuide.Id=TEXT("coast-recovery-anchor");
            RecoveryGuide.Position=FMath::ClosestPointOnSegment(Position,From,Current->Position);
            RecoveryGuide.Label=TEXT("HOLD / RECOVERY LINK UNVERIFIED / RETURN TO VERIFIED LEG");
            RecoveryGuide.ScoredCheckpointIndex=INDEX_NONE;
            RecoveryGuide.MaximumPlanarSpeedCm=100;
            bRecoveryGuideActive=true;
        }
        SyncGuidanceFeedback();
    }
    else if (bInside && bBoundaryLatched && !bRecoveryGuideActive)
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
    if (bInside && (Stage == EPhase4MissionStage::Inspection || Stage == EPhase4MissionStage::ReturnToHome || Stage == EPhase4MissionStage::Landing))
        ObserveGuidance(Position,Velocity);
    if (Stage == EPhase4MissionStage::ReturnToHome && GuidanceIndex==GuidanceRoute.Num()-1
        && FVector::Dist2D(Position, FVector(55,79,0)) <= 1800.0F)
    {
        Stage = EPhase4MissionStage::Landing;
        Feedback.Headline = TEXT("LAND ON PAD");
        Feedback.RecoveryAction = TEXT("Reduce speed, descend vertically, and keep the horizon level.");
        Feedback.ShapeCue = TEXT("DOWN ARROW + PAD");
    }
    if (Stage == EPhase4MissionStage::Landing && GuidanceIndex==GuidanceRoute.Num()-1
        && FlightPhase == EFlightPhase::Grounded && bConfirmedHomeSupport
        && FVector::Dist2D(Position,FVector(55,79,0)) <= FPacificCoastGrayboxContract::TakeoffPadRadiusCm
        && Velocity.Size()<40.0F)
    {
        CompleteMission();
    }
}

const FPhase4GuidancePoint* FPhase4MissionSession::GetCurrentGuidanceTarget() const
{
    if (bRecoveryGuideActive) return &RecoveryGuide;
    return GuidanceRoute.IsValidIndex(GuidanceIndex) ? &GuidanceRoute[GuidanceIndex] : nullptr;
}

void FPhase4MissionSession::SyncGuidanceFeedback()
{
    if (const auto* G=GetCurrentGuidanceTarget())
    {
        Feedback.RecoveryPoint=G->Position;
        if (!IsRecovering())
        {
            Feedback.Headline=FString::Printf(TEXT("GUIDE / %s"),*G->Label);
            Feedback.RecoveryAction=G->Label;
            Feedback.ShapeCue=Task.Checkpoints.IsValidIndex(G->ScoredCheckpointIndex)
                ? Task.Checkpoints[G->ScoredCheckpointIndex].ShapeCue : TEXT("GUIDE ARROW");
        }
    }
}

void FPhase4MissionSession::ObserveGuidance(const FVector& Position, const FVector& Velocity)
{
    // Contact release does not alone prove the frozen vehicle has moved again.
    if (bGuidanceContactHold)
    {
        if (bCollisionLatched || FVector::Distance(Position,ContactPosition)<25) return;
        bGuidanceContactHold=false;
    }
    const FPhase4GuidancePoint* G=GetCurrentGuidanceTarget();
    if (!G) return;
    const bool bReached=FVector::Dist2D(Position,G->Position)<=G->HorizontalRadiusCm
        && FMath::Abs(Position.Z-G->Position.Z)<=G->HeightToleranceCm
        && Velocity.Size2D()<=G->MaximumPlanarSpeedCm;
    if (bRecoveryGuideActive)
    {
        if (bReached)
        {
            bRecoveryGuideActive=false; bBoundaryLatched=false;
            Feedback.Incident=EPhase4Incident::None;
        }
        SyncGuidanceFeedback(); return;
    }
    if (Stage==EPhase4MissionStage::Inspection && G->ScoredCheckpointIndex==NextCheckpointIndex)
    {
        // Original checkpoint radius/low-water height remain the score rules;
        // speed control belongs to the active guide, never a future gate shortcut.
        const int32 Before=NextCheckpointIndex;
        if (Velocity.Size2D()<=G->MaximumPlanarSpeedCm) ObserveCheckpoint(Position);
        if (NextCheckpointIndex>Before && bReached) ++GuidanceIndex;
    }
    else if (bReached && G->Kind!=EPhase4GuideKind::Landing)
    {
        ++GuidanceIndex;
    }
    else if (Stage==EPhase4MissionStage::Inspection)
    {
        // Diagnose a shortcut without changing either index or rewarding it.
        for (int32 Future=NextCheckpointIndex+1; Future<Task.Checkpoints.Num(); ++Future)
            if (FVector::Distance(Position,Task.Checkpoints[Future].Position)<=Task.Checkpoints[Future].RadiusCm)
            {
                ++MissedCheckpoints; Feedback.Incident=EPhase4Incident::MissedCheckpoint;
                Feedback.Headline=TEXT("GUIDANCE INCOMPLETE / DO NOT CUT THE ROUTE");
                Feedback.ShapeCue=TEXT("GUIDE ARROW");
                Feedback.RecoveryAction=TEXT("Return to the current guidance point; completed checkpoints are preserved.");
                break;
            }
    }
    SyncGuidanceFeedback();
}

void FPhase4MissionSession::ReportWaterContact(const FVector& Position)
{
    if (Stage < EPhase4MissionStage::Takeoff || Stage > EPhase4MissionStage::Landing) return;
    Feedback.Incident=EPhase4Incident::UnsafeLanding;
    Feedback.Headline=TEXT("WATER CONTACT / SAFE PAUSE");
    Feedback.ShapeCue=TEXT("STOP OCTAGON");
    Feedback.RecoveryAction=TEXT("Water is not a landing surface. Reset to the launch mat to retry.");
    SyncGuidanceFeedback();
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
            SyncGuidanceFeedback();
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
    bGuidanceContactHold = true;
    ContactPosition = Position;
    Feedback.Incident = EPhase4Incident::Collision;
    Feedback.Headline = TEXT("COLLISION / HOLD POSITION");
    Feedback.RecoveryAction = TEXT("Inputs neutralized. Stabilize, then retry the current checkpoint.");
    Feedback.ShapeCue = TEXT("STOP OCTAGON");
    SyncGuidanceFeedback();
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
        SyncGuidanceFeedback();
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
        GuidanceIndex=GuidanceRoute.IndexOfByPredicate([](const auto& G){return G.ScoredCheckpointIndex==2;});
        Feedback.Headline = TEXT("CHECKPOINT 3 / BOARDWALK");
        Feedback.RecoveryAction = TEXT("Fly through SQUARE 3; keep the center view clear.");
        Feedback.ShapeCue = TEXT("SQUARE 3");
    }
    else if (EvidenceStage == EPhase4MissionStage::ReturnToHome)
    {
        NextCheckpointIndex = Task.Checkpoints.Num();
        GuidanceIndex=GuidanceRoute.IndexOfByPredicate([](const auto& G){return G.ScoredCheckpointIndex==4;})+1;
        BoundaryViolations = 1;
        Feedback.Incident = EPhase4Incident::Boundary;
        Feedback.Headline = TEXT("OUTSIDE SAFE ZONE");
        Feedback.RecoveryAction = TEXT("Follow inward chevrons to recover; progress is preserved.");
        Feedback.ShapeCue = TEXT("INWARD CHEVRONS");
        SyncGuidanceFeedback();
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
    if (Stage>=EPhase4MissionStage::Takeoff && Stage<=EPhase4MissionStage::Landing)
        if (const auto* G=GetCurrentGuidanceTarget()) return G->Label;
    return Feedback.RecoveryAction;
}
