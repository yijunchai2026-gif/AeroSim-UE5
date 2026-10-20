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

// ---------------------------------------------------------------------------------------
// DEV-FLIGHT guidance layer (AAA-R5-NAVIGATION-GAP). Pure deterministic data and state; it
// never writes the environment, never moves the Pawn and never awards score.
// ---------------------------------------------------------------------------------------
namespace Phase4Guidance
{
constexpr float TakeoffClimbCm = 320.0F;      // origin above the mat: ~3 m foot clearance
constexpr float TransitClearanceCm = 300.0F;  // AAA-R5: transit terrain/structure clearance
constexpr float FootAllowanceCm = 50.0F;      // support feet + tilt allowance below the origin
constexpr float CorridorHeightCm = 250.0F;    // existing low-flight corridor band
constexpr float CliffInsetCm = 800.0F;        // above-cliff guide sits inland of the actual edge
constexpr float ForestMouthApproachCm = 1400.0F;
constexpr float MinimumCruiseCm = 350.0F;

/** Coherent cruise height for a land leg from analytic terrain; validation proves it on real collision. */
float CruiseHeight(const FVector2D& A, const FVector2D& B)
{
    float Height = MinimumCruiseCm;
    const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector2D::Distance(A, B) / 100.0F));
    for (int32 Index = 0; Index <= Steps; ++Index)
    {
        const FVector2D P = FMath::Lerp(A, B, Index / static_cast<float>(Steps));
        // Land only: GetCoastalLandHeight is not extended over the sea (callers keep legs inland of Edge).
        Height = FMath::Max(Height, APacificCoastGraybox::GetCoastalLandHeight(P.X, P.Y) + TransitClearanceCm + FootAllowanceCm);
    }
    return FMath::CeilToFloat(Height / 10.0F) * 10.0F;
}

/** Conservative analytic surface for recovery-link checks: cliff top persists until past the shore. */
float ConservativeSurface(const float X, const float Y)
{
    const float Edge = APacificCoastGraybox::GetCliffEdgeX(Y);
    if (X <= Edge) return APacificCoastGraybox::GetCoastalLandHeight(X, Y);
    if (X <= APacificCoastGraybox::GetCoastalShoreX(Y) + 300.0F)
        return FMath::Max(APacificCoastGraybox::GetCoastalLandHeight(Edge, Y), APacificCoastGraybox::SeaLevelCm);
    return APacificCoastGraybox::SeaLevelCm;
}
}

float FPhase4GuidanceLeg::GetLength() const
{
    float Length = 0.0F;
    for (int32 Index = 1; Index < Path.Num(); ++Index) Length += FVector::Distance(Path[Index - 1], Path[Index]);
    return Length;
}

FVector FPhase4GuidanceLeg::Evaluate(float DistanceCm) const
{
    if (Path.IsEmpty()) return FVector::ZeroVector;
    DistanceCm = FMath::Max(0.0F, DistanceCm);
    for (int32 Index = 1; Index < Path.Num(); ++Index)
    {
        const float Segment = FVector::Distance(Path[Index - 1], Path[Index]);
        if (DistanceCm <= Segment && Segment > KINDA_SMALL_NUMBER)
            return FMath::Lerp(Path[Index - 1], Path[Index], DistanceCm / Segment);
        DistanceCm -= Segment;
    }
    return Path.Last();
}

float FPhase4GuidanceLeg::Project(const FVector& Position, const float MinCm, const float MaxCm, float& OutDistanceCm) const
{
    OutDistanceCm = BIG_NUMBER;
    float Best = FMath::Max(0.0F, MinCm), Start = 0.0F;
    for (int32 Index = 1; Index < Path.Num(); ++Index)
    {
        const FVector A = Path[Index - 1], B = Path[Index];
        const float Segment = FVector::Distance(A, B);
        const float End = Start + Segment;
        if (End >= MinCm && Start <= MaxCm && Segment > KINDA_SMALL_NUMBER)
        {
            const float Lo = FMath::Max(MinCm, Start) - Start, Hi = FMath::Min(MaxCm, End) - Start;
            const float Along = FMath::Clamp(static_cast<float>(FVector::DotProduct(Position - A, (B - A) / Segment)), Lo, Hi);
            const float Distance = FVector::Distance(Position, A + (B - A) * (Along / Segment));
            if (Distance < OutDistanceCm) { OutDistanceCm = Distance; Best = Start + Along; }
        }
        Start = End;
    }
    if (OutDistanceCm == BIG_NUMBER && !Path.IsEmpty()) OutDistanceCm = FVector::Distance(Position, Path.Last());
    return Best;
}

float FPhase4GuidanceLeg::ProjectCorridor(const FVector& Position, const float MinCm, const float MaxCm, float& OutLateralCm, float& OutVerticalCm) const
{
    OutLateralCm = BIG_NUMBER; OutVerticalCm = 0.0F;
    float Best = FMath::Max(0.0F, MinCm), Start = 0.0F, BestScore = BIG_NUMBER;
    for (int32 Index = 1; Index < Path.Num(); ++Index)
    {
        const FVector A = Path[Index - 1], B = Path[Index];
        const float Segment = FVector::Distance(A, B);
        const float End = Start + Segment;
        if (End >= MinCm && Start <= MaxCm && Segment > KINDA_SMALL_NUMBER)
        {
            const float Lo = (FMath::Max(MinCm, Start) - Start) / Segment, Hi = (FMath::Min(MaxCm, End) - Start) / Segment;
            const double Planar = FVector::Dist2D(A, B);
            float T;
            if (Planar > 1.0) T = static_cast<float>(FVector2D::DotProduct(FVector2D(Position - A), FVector2D(B - A)) / (Planar * Planar));
            else T = static_cast<float>((Position.Z - A.Z) / (B.Z - A.Z));
            T = FMath::Clamp(T, Lo, Hi);
            const FVector Point = FMath::Lerp(A, B, static_cast<double>(T));
            const float Lateral = static_cast<float>(FVector::Dist2D(Position, Point));
            const float Vertical = static_cast<float>(Position.Z - Point.Z);
            const float Score = Lateral * Lateral + Vertical * Vertical;
            if (Score < BestScore) { BestScore = Score; Best = Start + T * Segment; OutLateralCm = Lateral; OutVerticalCm = Vertical; }
        }
        Start = End;
    }
    return Best;
}

namespace Phase4Guidance
{
/** Conservative analytic link check used before offering a straight return arrow. */
bool LinkClear(const FVector& From, const FVector& To)
{
    const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector::Distance(From, To) / 100.0F));
    for (int32 Step = 0; Step <= Steps; ++Step)
    {
        const FVector P = FMath::Lerp(From, To, Step / static_cast<double>(Steps));
        if (FMath::Abs(P.X) > FPacificCoastGrayboxContract::WorldHalfExtentCm || FMath::Abs(P.Y) > FPacificCoastGrayboxContract::WorldHalfExtentCm
            || P.Z < ConservativeSurface(P.X, P.Y) + TransitClearanceCm) return false;
    }
    return true;
}
}

TArray<FPhase4GuidanceLeg> FPhase4TaskDefinition::BuildGuidanceRoute() const
{
    using namespace Phase4Guidance;
    TArray<FPhase4GuidanceLeg> Route;
    if (Checkpoints.Num() != FPacificCoastGrayboxContract::GateCount) return Route;
    const FVector Home = FPacificCoastGrayboxContract::GetRoutePoints()[0];
    const FVector C1 = Checkpoints[0].Position, C2 = Checkpoints[1].Position, C3 = Checkpoints[2].Position;
    const FVector C4 = Checkpoints[3].Position, C5 = Checkpoints[4].Position;
    FVector Cursor = Home;
    auto Add = [&](const TCHAR* LegId, const TCHAR* GuideId, const TCHAR* Label, const EPhase4GuideKind Kind,
        const TArray<FVector>& Points, const int32 Score = INDEX_NONE) -> FPhase4GuidanceLeg&
    {
        FPhase4GuidanceLeg& Leg = Route.AddDefaulted_GetRef();
        Leg.LegId = LegId; Leg.Label = Label; Leg.Kind = Kind;
        Leg.ScoredCheckpointIndex = Score;
        Leg.ScoredCheckpointId = Checkpoints.IsValidIndex(Score) ? Checkpoints[Score].Id : NAME_None;
        Leg.GuideId = Leg.ScoredCheckpointId.IsNone() ? FName(GuideId) : Leg.ScoredCheckpointId;
        Leg.Path.Add(Cursor);
        for (const FVector& Point : Points) if (!Point.Equals(Leg.Path.Last(), 1.0)) Leg.Path.Add(Point);
        if (Leg.Path.Num() == 1) Leg.Path.Add(Cursor);
        Cursor = Leg.Path.Last();
        if (Kind == EPhase4GuideKind::VerticalDescent || Kind == EPhase4GuideKind::VerticalAscent
            || Kind == EPhase4GuideKind::Takeoff || Kind == EPhase4GuideKind::Landing)
        {
            // Columns lock XY: <=100 cm off axis and <=100 cm/s planar while progressing.
            Leg.AcceptRadiusCm = 100.0F; Leg.AcceptBelowCm = 75.0F; Leg.AcceptAboveCm = 75.0F;
            Leg.SafetyHalfWidthCm = 100.0F; Leg.SafetyBelowCm = 150.0F; Leg.SafetyAboveCm = 150.0F;
            Leg.MaxTrackPlanarSpeedCm = 100.0F; Leg.LookaheadCm = 300.0F;
            Leg.MaxAcceptPlanarSpeedCm = 100.0F;
            // Vertical brake window: ~2x the 6-8 m/s envelope's stopping distance (3.3-5.3 m).
            Leg.MaxAcceptVerticalSpeedCm = 150.0F; Leg.VerticalBrakeZoneCm = 1000.0F;
        }
        else if (Kind == EPhase4GuideKind::Corridor)
        {
            // Pass-through along the polyline (R6a real-input finding): reach the end of the verified
            // polyline inside the corridor tube; no speed window.
            Leg.AcceptRadiusCm = 150.0F; Leg.AcceptBelowCm = 150.0F; Leg.AcceptAboveCm = 150.0F;
            // Forest: measured clear half-width >=205 cm along the corridor polyline (gates included).
            Leg.SafetyHalfWidthCm = 180.0F; Leg.SafetyBelowCm = 150.0F; Leg.SafetyAboveCm = 150.0F;
            Leg.LookaheadCm = 600.0F; Leg.MaxAcceptPlanarSpeedCm = 10000.0F;
            Leg.bPassThrough = true;
        }
        else if (Kind == EPhase4GuideKind::Transit)
        {
            // Mid-air transit/turn points are pass-through gates (>=6 m planar, +-2 m, or crossing the
            // gate plane), no speed requirement. Safety brake points override this below.
            Leg.AcceptRadiusCm = 600.0F; Leg.AcceptBelowCm = 200.0F; Leg.AcceptAboveCm = 200.0F;
            Leg.MaxAcceptPlanarSpeedCm = 10000.0F; Leg.bPassThrough = true;
        }
        return Leg;
    };

    // 1. Vertical takeoff from the actual mat to ~3 m foot clearance.
    {
        auto& Leg = Add(TEXT("nav-01-takeoff-column"), TEXT("guide-takeoff-top"), TEXT("RISE VERTICALLY TO 3 M ABOVE THE MAT"),
            EPhase4GuideKind::Takeoff, {Home + FVector(0, 0, TakeoffClimbCm)});
        // The takeoff phase overshoots its 3 m hand-over; accept the actual climb-out band.
        Leg.AcceptBelowCm = 40.0F; Leg.AcceptAboveCm = 700.0F; Leg.MaxAcceptPlanarSpeedCm = 200.0F;
        Leg.MaxAcceptVerticalSpeedCm = 10000.0F; Leg.VerticalBrakeZoneCm = 0.0F;
        Leg.SafetyHalfWidthCm = 150.0F; Leg.SafetyBelowCm = 100.0F; Leg.SafetyAboveCm = 700.0F; Leg.MaxTrackPlanarSpeedCm = 200.0F;
    }
    // 2. C1 over the grass/soil worksite (unchanged checkpoint and radius).
    Add(TEXT("nav-02-grass-soil"), TEXT("checkpoint-01"), TEXT("CIRCLE 1 / GRASS AND SOIL"), EPhase4GuideKind::Transit, {C1}, 0);
    // 3. Inland to an above-cliff point; altitude is held across the whole cliff crossing.
    const FVector2D OutEdge(APacificCoastGraybox::GetCliffEdgeX(C2.Y) - CliffInsetCm, C2.Y);
    const float Hout = FMath::Max(CruiseHeight(FVector2D(C1), OutEdge), MinimumCruiseCm);
    Add(TEXT("nav-03-inland-to-cliff"), TEXT("guide-above-cliff-out"), TEXT("ABOVE THE CLIFF / HOLD ALTITUDE"),
        EPhase4GuideKind::Transit, {FVector(OutEdge, Hout)});
    {
        auto& Leg = Add(TEXT("nav-04-cross-cliff-out"), TEXT("guide-offshore-descent-top"), TEXT("CROSS THE CLIFF / BRAKE OFFSHORE BEFORE DESCENDING"),
            EPhase4GuideKind::Transit, {FVector(C2.X, C2.Y, Hout)});
        Leg.AcceptRadiusCm = 150.0F; Leg.AcceptBelowCm = 75.0F; Leg.AcceptAboveCm = 75.0F; Leg.MaxAcceptPlanarSpeedCm = 100.0F;
        Leg.MaxAcceptVerticalSpeedCm = 150.0F; Leg.bPassThrough = false; // safety gate: descent column top
    }
    // 4. Offshore vertical descent column to C2 (XY locked, slow).
    Add(TEXT("nav-05-offshore-descent"), TEXT("checkpoint-02"), TEXT("DESCEND VERTICALLY / DIAMOND 2 TIDAL ROCK"),
        EPhase4GuideKind::VerticalDescent, {C2}, 1);
    // 5. Along-shore curve C2 -> C3 inside the near-shore band (not the chord).
    {
        TArray<FVector> Curve;
        constexpr int32 Nodes = 24;
        for (int32 Index = 1; Index <= Nodes; ++Index)
        {
            const float U = Index / static_cast<float>(Nodes);
            const float Y = FMath::Lerp(static_cast<float>(C2.Y), static_cast<float>(C3.Y), U);
            const float Offset = FMath::Lerp(static_cast<float>(C2.X - APacificCoastGraybox::GetCoastalShoreX(C2.Y)),
                static_cast<float>(C3.X - APacificCoastGraybox::GetCoastalShoreX(C3.Y)), U);
            Curve.Add(Index == Nodes ? C3 : FVector(APacificCoastGraybox::GetCoastalShoreX(Y) + Offset, Y, FMath::Lerp(C2.Z, C3.Z, static_cast<double>(U))));
        }
        auto& Leg = Add(TEXT("nav-06-alongshore-low-water"), TEXT("checkpoint-03"), TEXT("FOLLOW THE SHORE LOW / SQUARE 3 LOW WATER"),
            EPhase4GuideKind::Transit, Curve, 2);
        Leg.AcceptRadiusCm = 150.0F; Leg.AcceptBelowCm = 75.0F; Leg.AcceptAboveCm = 75.0F; Leg.bPassThrough = false; // C3 = climb column bottom
        Leg.MaxAcceptPlanarSpeedCm = 100.0F;
        Leg.SafetyHalfWidthCm = 1000.0F; Leg.SafetyBelowCm = 250.0F; Leg.SafetyAboveCm = 300.0F; // low water: foot stays >1 m above sea
        // C3 is 4 m above the sea: brake the descent before it (no overshoot into the water).
        Leg.MaxAcceptVerticalSpeedCm = 150.0F; Leg.VerticalBrakeZoneCm = 1000.0F;
    }
    // 6. Vertical climb at C3 before any inland heading.
    const FVector2D InEdge(APacificCoastGraybox::GetCliffEdgeX(C3.Y) - CliffInsetCm, C3.Y);
    const FVector2D MouthTangent = CoastFlora::GetLayout().Corridor.Num() > 1
        ? (CoastFlora::GetLayout().Corridor[1].Point - CoastFlora::GetLayout().Corridor[0].Point).GetSafeNormal() : FVector2D(0, -1);
    const FVector2D Mouth = FVector2D(C4) - MouthTangent * ForestMouthApproachCm;
    const float Hin = FMath::Max(CruiseHeight(InEdge, Mouth), MinimumCruiseCm);
    {
        auto& Leg = Add(TEXT("nav-07-sea-climb"), TEXT("guide-sea-climb-top"), TEXT("CLIMB VERTICALLY BEFORE TURNING INLAND"),
            EPhase4GuideKind::VerticalAscent, {FVector(C3.X, C3.Y, Hin)});
    }
    // 7. Back over the cliff, then across the open field to the forest mouth.
    Add(TEXT("nav-08-cross-cliff-in"), TEXT("guide-above-cliff-in"), TEXT("CROSS ABOVE THE CLIFF / HOLD ALTITUDE"),
        EPhase4GuideKind::Transit, {FVector(InEdge, Hin)});
    const FVector MouthApproach(Mouth, Hin);
    {
        auto& Leg = Add(TEXT("nav-09-field-to-forest-mouth"), TEXT("guide-forest-mouth-approach"), TEXT("OPEN FIELD / SLOW DOWN AT THE FOREST MOUTH"),
            EPhase4GuideKind::Transit, {MouthApproach});
    }
    // 8. Corridor: entry (C4), polyline follow to C5 along CoastFlora::GetLayout().Corridor.
    Add(TEXT("nav-10-forest-entry"), TEXT("checkpoint-04"), TEXT("TRIANGLE 4 / FOREST ENTRY / LOW AND SLOW"),
        EPhase4GuideKind::Corridor, {C4}, 3);
    TArray<FVector> Corridor;
    for (const CoastFlora::FCorridorSample& Sample : CoastFlora::GetLayout().Corridor)
    {
        if (Sample.Distance <= 0.0F) continue;
        if (FVector2D::Distance(Sample.Point, FVector2D(C5)) < 30.0F) break;
        Corridor.Add(FVector(Sample.Point, APacificCoastGraybox::GetCoastalLandHeight(Sample.Point.X, Sample.Point.Y) + CorridorHeightCm));
    }
    Corridor.Add(C5);
    {
        auto& Leg = Add(TEXT("nav-11-corridor-follow"), TEXT("checkpoint-05"), TEXT("FOLLOW THE FOREST CORRIDOR / HEXAGON 5"),
            EPhase4GuideKind::Corridor, Corridor, 4);
        Leg.MaxAcceptPlanarSpeedCm = 300.0F;
    }
    // 9. Return: the verified corridor in reverse, the same mouth, field, brake above the mat, land.
    TArray<FVector> Reverse;
    for (int32 Index = Corridor.Num() - 2; Index >= 0; --Index) Reverse.Add(Corridor[Index]);
    Reverse.Add(C4);
    Add(TEXT("nav-12-corridor-reverse"), TEXT("guide-forest-mouth"), TEXT("RETURN ALONG THE CORRIDOR / DO NOT CUT THE TURN"),
        EPhase4GuideKind::Corridor, Reverse).bReturnLeg = true;
    Add(TEXT("nav-13-forest-exit"), TEXT("guide-forest-exit"), TEXT("EXIT THROUGH THE SAME MOUTH"),
        EPhase4GuideKind::Corridor, {MouthApproach}).bReturnLeg = true;
    const float Hhome = FMath::Max(CruiseHeight(Mouth, FVector2D(Home)), MinimumCruiseCm);
    const FVector HomeAbove(Home.X, Home.Y, FMath::Max(Hhome, static_cast<float>(Home.Z) + MinimumCruiseCm));
    {
        auto& Leg = Add(TEXT("nav-14-field-return"), TEXT("guide-home-above-mat"), TEXT("RETURN HOME / BRAKE ABOVE THE MAT"),
            EPhase4GuideKind::Transit, {FVector(Mouth, Hhome), HomeAbove});
        Leg.bReturnLeg = true; Leg.bPassThrough = false; // safety gate: landing column top
        // Open field is wide; launch-site props near the mat narrow it (funnel to 3 m at the brake point).
        Leg.EndFunnelHalfWidthCm = 300.0F; Leg.EndFunnelSlope = 0.1F; // measured: props ~4 m off the line 5 m before the mat
        Leg.AcceptRadiusCm = 100.0F; Leg.AcceptBelowCm = 75.0F; Leg.AcceptAboveCm = 75.0F;
        Leg.MaxAcceptPlanarSpeedCm = 100.0F; Leg.MaxAcceptVerticalSpeedCm = 150.0F;
    }
    {
        auto& Leg = Add(TEXT("nav-15-land-on-mat"), TEXT("guide-home-mat"), TEXT("LAND VERTICALLY ON THE MAT / BELOW 0.4 M/S"),
            EPhase4GuideKind::Landing, {Home});
        Leg.bReturnLeg = true; Leg.AcceptRadiusCm = FPacificCoastGrayboxContract::TakeoffPadRadiusCm; Leg.MaxAcceptPlanarSpeedCm = 40.0F;
    }
    return Route;
}

bool FPhase4TaskDefinition::ValidateGuidanceStructure(const TArray<FPhase4GuidanceLeg>& Route, FString& OutFailure) const
{
    auto Fail = [&OutFailure](const int32 Leg, const FName Id, const float Distance, const FVector& P, const TCHAR* Reason)
    {
        OutFailure = FString::Printf(TEXT("leg=%d id=%s distance_cm=%.0f xyz=(%.1f,%.1f,%.1f) reason=%s"),
            Leg, *Id.ToString(), Distance, P.X, P.Y, P.Z, Reason);
        return false;
    };
    if (Route.IsEmpty()) return Fail(INDEX_NONE, NAME_None, 0, FVector::ZeroVector, TEXT("EMPTY_ROUTE"));
    const FVector Home = FPacificCoastGrayboxContract::GetRoutePoints()[0];
    TSet<FName> LegIds, GuideIds;
    int32 ExpectedScore = 0;
    bool bReturn = false;
    FVector Previous = Home;
    for (int32 LegIndex = 0; LegIndex < Route.Num(); ++LegIndex)
    {
        const FPhase4GuidanceLeg& Leg = Route[LegIndex];
        if (Leg.LegId.IsNone() || Leg.GuideId.IsNone() || Leg.Label.IsEmpty() || Leg.Path.Num() < 2)
            return Fail(LegIndex, Leg.LegId, 0, Previous, TEXT("INVALID_LEG"));
        if (LegIds.Contains(Leg.LegId) || GuideIds.Contains(Leg.GuideId)) return Fail(LegIndex, Leg.LegId, 0, Previous, TEXT("DUPLICATE_ID"));
        LegIds.Add(Leg.LegId); GuideIds.Add(Leg.GuideId);
        if (!Leg.Path[0].Equals(Previous, 1.0)) return Fail(LegIndex, Leg.LegId, 0, Leg.Path[0], TEXT("DISCONTINUOUS"));
        if (Leg.AcceptRadiusCm <= 0 || Leg.AcceptBelowCm <= 0 || Leg.AcceptAboveCm <= 0 || Leg.SafetyHalfWidthCm <= 0 || Leg.SafetyBelowCm < 0 || Leg.SafetyAboveCm <= 0
            || Leg.MaxAcceptPlanarSpeedCm <= 0 || Leg.MaxAcceptVerticalSpeedCm <= 0 || Leg.MaxTrackPlanarSpeedCm <= 0 || Leg.LookaheadCm <= 0)
            return Fail(LegIndex, Leg.LegId, 0, Previous, TEXT("INVALID_WINDOW"));
        if (Leg.ScoredCheckpointIndex != INDEX_NONE)
        {
            if (Leg.ScoredCheckpointIndex != ExpectedScore || !Checkpoints.IsValidIndex(Leg.ScoredCheckpointIndex)
                || Leg.ScoredCheckpointId != Checkpoints[Leg.ScoredCheckpointIndex].Id
                || !Leg.Path.Last().Equals(Checkpoints[Leg.ScoredCheckpointIndex].Position, 1.0))
                return Fail(LegIndex, Leg.LegId, 0, Leg.Path.Last(), TEXT("SCORE_ORDER_OR_IDENTITY"));
            ++ExpectedScore;
        }
        if (Leg.bReturnLeg && ExpectedScore != Checkpoints.Num()) return Fail(LegIndex, Leg.LegId, 0, Previous, TEXT("RETURN_BEFORE_ALL_CHECKPOINTS"));
        if (bReturn && !Leg.bReturnLeg) return Fail(LegIndex, Leg.LegId, 0, Previous, TEXT("RETURN_NOT_CONTIGUOUS"));
        bReturn |= Leg.bReturnLeg;
        const bool bColumn = Leg.Kind == EPhase4GuideKind::VerticalDescent || Leg.Kind == EPhase4GuideKind::VerticalAscent
            || Leg.Kind == EPhase4GuideKind::Takeoff || Leg.Kind == EPhase4GuideKind::Landing;
        float Along = 0.0F;
        for (int32 Index = 1; Index < Leg.Path.Num(); ++Index)
        {
            const FVector A = Leg.Path[Index - 1], B = Leg.Path[Index];
            if (bColumn && FVector::Dist2D(A, B) > 1.0) return Fail(LegIndex, Leg.LegId, Along, B, TEXT("COLUMN_NOT_VERTICAL"));
            const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector::Distance(A, B) / 100.0F));
            for (int32 Step = 0; Step <= Steps; ++Step)
            {
                const FVector P = FMath::Lerp(A, B, Step / static_cast<double>(Steps));
                if (P.ContainsNaN() || !IsInsideSafetyVolume(P))
                    return Fail(LegIndex, Leg.LegId, Along + FVector::Distance(A, P), P, TEXT("OUTSIDE_SAFETY_VOLUME"));
            }
            Along += FVector::Distance(A, B);
        }
        Previous = Leg.Path.Last();
    }
    if (ExpectedScore != Checkpoints.Num()) return Fail(INDEX_NONE, NAME_None, 0, Previous, TEXT("MISSING_SCORED_LEG"));
    if (Route.Last().Kind != EPhase4GuideKind::Landing || !Previous.Equals(Home, 1.0))
        return Fail(Route.Num() - 1, Route.Last().LegId, 0, Previous, TEXT("ROUTE_DOES_NOT_END_ON_MAT"));
    OutFailure.Reset();
    return true;
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
    // Navigation is rebuilt per mission start; no stale leg index survives a legal reset.
    // (Not built here: Reset also runs in CDO constructors, before flora data should be touched.)
    GuidanceRoute.Reset();
    GuidanceLegIndex = 0;
    GuidanceProgressCm = 0.0F;
    bRecoveryGuideActive = false;
    bOffCorridor = false;
    RecoveryAnchorCm = 0.0F;
    RecoveryGuidance = FPhase4GuidanceTarget();
    CurrentGuidance = FPhase4GuidanceTarget();
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
    GuidanceRoute = Task.BuildGuidanceRoute();
    GuidanceLegIndex = 0;
    GuidanceProgressCm = 0.0F;
    if (!Task.ValidateGuidanceStructure(GuidanceRoute, GuidanceFailure))
    {
        Feedback.Headline = TEXT("GUIDANCE NOT VALIDATED");
        Feedback.RecoveryAction = GuidanceFailure;
        return false;
    }
    Stage = EPhase4MissionStage::Takeoff;
    Feedback = FPhase4Feedback();
    Feedback.Headline = TEXT("TAKE OFF");
    Feedback.RecoveryAction = TEXT("Raise throttle smoothly to 3 m, then follow marker 1.");
    Feedback.ShapeCue = TEXT("UP ARROW");
    RefreshGuidance(FPacificCoastGrayboxContract::GetRoutePoints()[0]);
    return true;
}

void FPhase4MissionSession::Observe(const float DeltaSeconds, const EFlightPhase FlightPhase, const FVector& Position, const FVector& Velocity, const FRotator& Rotation, const bool bConfirmedHomeSupport)
{
    // A paused vehicle (input pause, collision/support or water safe pause) never advances anything.
    if (Stage < EPhase4MissionStage::Takeoff || Stage > EPhase4MissionStage::Landing || DeltaSeconds <= 0.0F
        || FlightPhase == EFlightPhase::Paused || Feedback.Incident == EPhase4Incident::UnsafeLanding)
    {
        return; // water contact keeps its explicit Reset semantics; nothing resumes underwater
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
    }
    else if (float Rejoined = 0.0F; bInside && bBoundaryLatched && IsOnCurrentLeg(Position, &Rejoined))
    {
        // Recovery ends only when the actual pose has rejoined the verified current leg.
        GuidanceProgressCm = FMath::Max(GuidanceProgressCm, Rejoined);
        bBoundaryLatched = false;
        bRecoveryGuideActive = false;
        Feedback.Incident = EPhase4Incident::None;
        Feedback.Headline = TEXT("SAFE ZONE RECOVERED");
        Feedback.RecoveryAction = TEXT("Back on the verified route; continue to the current guide.");
        Feedback.ShapeCue = TEXT("CHECKMARK");
    }
    if (bBoundaryLatched) SetRecoveryGuide(Position, false);

    if (Stage == EPhase4MissionStage::Takeoff && (FlightPhase == EFlightPhase::Takeoff || FlightPhase == EFlightPhase::Flight))
    {
        Stage = EPhase4MissionStage::Inspection;
    }
    if (bInside && !bBoundaryLatched && !bRecoveryGuideActive && Stage != EPhase4MissionStage::Takeoff)
    {
        ObserveGuidance(Position, Velocity);
    }
    const bool bLandingLeg = GuidanceRoute.IsValidIndex(GuidanceLegIndex) && GuidanceRoute[GuidanceLegIndex].Kind == EPhase4GuideKind::Landing;
    if (Stage == EPhase4MissionStage::ReturnToHome && bLandingLeg)
    {
        Stage = EPhase4MissionStage::Landing;
        Feedback.Headline = TEXT("LAND ON PAD");
        Feedback.RecoveryAction = TEXT("Reduce speed, descend vertically, and keep the horizon level.");
        Feedback.ShapeCue = TEXT("DOWN ARROW + PAD");
    }
    if (Stage == EPhase4MissionStage::Landing && bLandingLeg && FlightPhase == EFlightPhase::Grounded && bConfirmedHomeSupport
        && FVector::Dist2D(Position,FVector(55,79,0)) <= FPacificCoastGrayboxContract::TakeoffPadRadiusCm
        && Velocity.Size()<40.0F)
    {
        CompleteMission();
    }
    RefreshGuidance(Position);
}

bool FPhase4MissionSession::IsOnCurrentLeg(const FVector& Position, float* OutAlongCm) const
{
    if (!GuidanceRoute.IsValidIndex(GuidanceLegIndex)) return false;
    const FPhase4GuidanceLeg& Leg = GuidanceRoute[GuidanceLegIndex];
    float Lateral = 0.0F, Vertical = 0.0F;
    // While recovering, the offered anchor (possibly further along the verified leg) also counts.
    const float Max = FMath::Max(GuidanceProgressCm + Leg.LookaheadCm, bRecoveryGuideActive ? RecoveryAnchorCm + 200.0F : 0.0F);
    const float Along = Leg.ProjectCorridor(Position, GuidanceProgressCm - 200.0F, Max, Lateral, Vertical);
    if (OutAlongCm) *OutAlongCm = Along;
    return Leg.IsInsideCorridor(Along, Lateral, Vertical);
}

void FPhase4MissionSession::ObserveGuidance(const FVector& Position, const FVector& Velocity)
{
    // Contact frames are frozen collision-resolved poses: no navigation or score progress.
    if (bCollisionLatched || !GuidanceRoute.IsValidIndex(GuidanceLegIndex)) return;
    const int32 LegAtStart = GuidanceLegIndex;
    bShortcutThisFrame = false;
    const FPhase4GuidanceLeg& Leg = GuidanceRoute[GuidanceLegIndex];
    const float Planar = static_cast<float>(Velocity.Size2D());
    // Progress = furthest projection of the sweep-resolved actual pose onto the leg while that pose is
    // inside the leg's validated safety corridor (wide in open air, tight at columns/forest). Straight
    // transit/column legs project over the whole remaining leg; the curved forest polyline keeps a
    // bounded window so a later stretch of the same corridor is never reached by a jump.
    float Lateral = 0.0F, Vertical = 0.0F;
    const float MaxAlong = Leg.Kind == EPhase4GuideKind::Corridor ? GuidanceProgressCm + 1500.0F : Leg.GetLength();
    const float Along = Leg.ProjectCorridor(Position, GuidanceProgressCm - 200.0F, MaxAlong, Lateral, Vertical);
    bOffCorridor = !Leg.IsInsideCorridor(Along, Lateral, Vertical);
    if (!bOffCorridor && Planar <= Leg.MaxTrackPlanarSpeedCm)
        GuidanceProgressCm = FMath::Max(GuidanceProgressCm, Along);

    if (Stage == EPhase4MissionStage::Inspection)
    {
        // Entering a future checkpoint is diagnosed, never scored, and never moves guidance.
        bool bShortcut = false;
        for (int32 Future = NextCheckpointIndex + 1; Future < Task.Checkpoints.Num() && !bShortcut; ++Future)
            if (FVector::Distance(Position, Task.Checkpoints[Future].Position) <= Task.Checkpoints[Future].RadiusCm)
            {
                bShortcut = true;
                ++MissedCheckpoints;
                Feedback.Incident = EPhase4Incident::MissedCheckpoint;
                Feedback.Headline = FString::Printf(TEXT("MISSED %s / DO NOT CUT THE ROUTE"),
                    Task.Checkpoints.IsValidIndex(NextCheckpointIndex) ? *Task.Checkpoints[NextCheckpointIndex].Id.ToString().ToUpper() : TEXT("ROUTE"));
                Feedback.RecoveryAction = TEXT("Return to the current guide; completed checkpoints stay saved.");
                Feedback.ShapeCue = Task.Checkpoints.IsValidIndex(NextCheckpointIndex) ? Task.Checkpoints[NextCheckpointIndex].ShapeCue : TEXT("GUIDE ARROW");
            }
        bShortcutThisFrame = bShortcut;
    }
    if (Leg.Kind == EPhase4GuideKind::Landing) return; // Completion is the supported-landing rule.
    const FVector End = Leg.Path.Last();
    const float Rise = static_cast<float>(Position.Z - End.Z);
    const float Length = Leg.GetLength();
    const bool bHeight = Rise >= -Leg.AcceptBelowCm && Rise <= Leg.AcceptAboveCm;
    const bool bProgress = GuidanceProgressCm >= Length - FMath::Max3(Leg.AcceptRadiusCm, Leg.AcceptBelowCm, Leg.AcceptAboveCm) - 50.0F;
    bool bAccepted = false;
    if (Leg.bPassThrough)
    {
        // Inside the window, or across the gate plane through the guide point (perpendicular to the
        // leg's last planar direction) within 2x the radius; no speed requirement.
        const FVector2D Last = Leg.Path.Num() > 1 ? FVector2D(End - Leg.Path[Leg.Path.Num() - 2]) : FVector2D::ZeroVector;
        const bool bCrossed = Last.SizeSquared() > 1.0 && FVector2D::DotProduct(FVector2D(Position - End), Last.GetSafeNormal()) >= 0.0
            && FVector::Dist2D(Position, End) <= 2.0F * Leg.AcceptRadiusCm;
        bAccepted = bProgress && bHeight && (FVector::Dist2D(Position, End) <= Leg.AcceptRadiusCm || bCrossed);
    }
    else
    {
        bAccepted = bProgress && bHeight
            && FVector::Dist2D(Position, End) <= Leg.AcceptRadiusCm
            && Planar <= Leg.MaxAcceptPlanarSpeedCm
            && FMath::Abs(static_cast<float>(Velocity.Z)) <= Leg.MaxAcceptVerticalSpeedCm;
    }
    if (!bAccepted) return;
    if (Leg.ScoredCheckpointIndex != INDEX_NONE)
    {
        // R6b: a scored checkpoint is scored by its original radius/height rules at the moment its own
        // leg's end gate is accepted, so score and guidance can never diverge (e.g. C2 scored by its
        // 25 m radius while the descent column still pointed back up).
        if (Stage != EPhase4MissionStage::Inspection || bShortcutThisFrame || Leg.ScoredCheckpointIndex != NextCheckpointIndex) return;
        ObserveCheckpoint(Position);
        if (NextCheckpointIndex <= Leg.ScoredCheckpointIndex) return;
        if (GuidanceLegIndex != LegAtStart) return; // fifth checkpoint: RTH already switched to the return legs
    }
    AdvanceGuidanceLeg();
}

void FPhase4MissionSession::AdvanceGuidanceLeg()
{
    if (GuidanceLegIndex + 1 < GuidanceRoute.Num())
    {
        ++GuidanceLegIndex;
        GuidanceProgressCm = 0.0F;
        bOffCorridor = false;
    }
}

bool FPhase4MissionSession::IsScoreGuidanceConsistent() const
{
    if (GuidanceRoute.IsEmpty()) return true;
    int32 ScoredLegsDone = 0;
    for (int32 Index = 0; Index < GuidanceLegIndex && Index < GuidanceRoute.Num(); ++Index)
        if (GuidanceRoute[Index].ScoredCheckpointIndex != INDEX_NONE) ++ScoredLegsDone;
    return ScoredLegsDone == NextCheckpointIndex;
}

int32 FPhase4MissionSession::FindFirstReturnLeg() const
{
    return GuidanceRoute.IndexOfByPredicate([](const FPhase4GuidanceLeg& Leg) { return Leg.bReturnLeg; });
}

void FPhase4MissionSession::SetRecoveryGuide(const FVector& Position, const bool bWater)
{
    bRecoveryGuideActive = true;
    FPhase4GuidanceTarget Guide = CurrentGuidance;
    Guide.bRecovery = true;
    Guide.ScoredCheckpointIndex = INDEX_NONE;
    Guide.ScoredCheckpointId = NAME_None;
    Guide.Kind = EPhase4GuideKind::Hold;
    Guide.GuideId = TEXT("guide-recovery-hold");
    Guide.Position = Position;
    Guide.bHold = true;
    if (bWater)
    {
        // Water keeps its explicit user Reset: no arrow, no automatic transfer, no underwater flight.
        Guide.Label = TEXT("WATER CONTACT / PRESS R TO RESTART FROM THE MAT");
    }
    else if (GuidanceRoute.IsValidIndex(GuidanceLegIndex))
    {
        const FPhase4GuidanceLeg& Leg = GuidanceRoute[GuidanceLegIndex];
        // Anchors lie on the verified current leg; the link to one is only offered when a conservative
        // analytic terrain check passes (cliff top assumed to extend to the shore). Otherwise hold.
        auto LinkClear = [&Position](const FVector& Anchor) { return Phase4Guidance::LinkClear(Position, Anchor); };
        float Distance = 0.0F;
        const float Candidates[] = {Leg.Project(Position, 0.0F, Leg.GetLength(), Distance), GuidanceProgressCm, Leg.GetLength()};
        bool bLinkClear = false;
        FVector Anchor = Position;
        for (const float Along : Candidates)
            if (LinkClear(Leg.Evaluate(Along))) { bLinkClear = true; Anchor = Leg.Evaluate(Along); RecoveryAnchorCm = Along; break; }
        if (bLinkClear)
        {
            Guide.GuideId = TEXT("guide-recovery-anchor");
            Guide.Kind = Leg.Kind;
            Guide.Position = Anchor;
            Guide.bHold = false;
            Guide.Label = TEXT("RETURN TO THE VERIFIED ROUTE / FOLLOW THE INWARD CHEVRONS");
        }
        else
        {
            Guide.Label = TEXT("CONTROLLED STOP / HOVER, THEN CLIMB SLOWLY UNTIL A SAFE LINK APPEARS");
        }
    }
    RecoveryGuidance = Guide;
}

void FPhase4MissionSession::RefreshGuidance(const FVector& Position)
{
    FPhase4GuidanceTarget Target;
    if (bRecoveryGuideActive) Target = RecoveryGuidance;
    else if (GuidanceRoute.IsValidIndex(GuidanceLegIndex))
    {
        const FPhase4GuidanceLeg& Leg = GuidanceRoute[GuidanceLegIndex];
        Target.LegId = Leg.LegId; Target.GuideId = Leg.GuideId; Target.Label = Leg.Label; Target.Kind = Leg.Kind;
        Target.GuidePoint = Leg.Path.Last();
        Target.AcceptRadiusCm = Leg.AcceptRadiusCm; Target.AcceptBelowCm = Leg.AcceptBelowCm; Target.AcceptAboveCm = Leg.AcceptAboveCm;
        Target.MaxAcceptPlanarSpeedCm = Leg.MaxAcceptPlanarSpeedCm;
        Target.MaxAcceptVerticalSpeedCm = Leg.MaxAcceptVerticalSpeedCm;
        Target.bPassThrough = Leg.bPassThrough;
        const float HeightToGo = FMath::Abs(static_cast<float>(Leg.Path.Last().Z - Position.Z));
        Target.bVerticalBrake = Leg.VerticalBrakeZoneCm > 0.0F && HeightToGo <= Leg.VerticalBrakeZoneCm
            && FVector::Dist2D(Position, Leg.Path.Last()) <= FMath::Max(Leg.SafetyHalfWidthCm, 300.0F);
        if (Target.bVerticalBrake) Target.Label = FString::Printf(TEXT("BRAKE VERTICAL SPEED / %s"), *Leg.Label);
        Target.ScoredCheckpointIndex = Leg.ScoredCheckpointIndex; Target.ScoredCheckpointId = Leg.ScoredCheckpointId;
        Target.SafetyHalfWidthCm = Leg.GetHalfWidthAt(GuidanceProgressCm);
        Target.Position = Leg.Evaluate(GuidanceProgressCm + Leg.LookaheadCm);
        Target.RemainingCm = FMath::Max(0.0F, Leg.GetLength() - GuidanceProgressCm);
        if (bOffCorridor)
        {
            // Visible recovery guidance (never a silent stall): steer back onto the verified leg near the
            // actual progress when a conservative straight link exists, otherwise a controlled stop.
            float Distance = 0.0F, AnchorLateral = 0.0F, AnchorVertical = 0.0F;
            // Columns re-centre horizontally at the pilot's own height (never back up to the column top).
            const float AnchorAlong = Leg.IsColumn()
                ? Leg.ProjectCorridor(Position, 0.0F, Leg.GetLength(), AnchorLateral, AnchorVertical)
                : Leg.Project(Position, FMath::Max(0.0F, GuidanceProgressCm - 200.0F), GuidanceProgressCm + Leg.LookaheadCm, Distance);
            const FVector Anchor = Leg.Evaluate(AnchorAlong);
            Target.bOffCorridor = true;
            Target.bRecovery = true;
            // Columns and the forest corridor: a short local re-centre (the analytic 3 m link check cannot
            // hold at 2.5 m forest height); open-air transit: only with a conservative clear link.
            if (Leg.IsColumn() || Leg.Kind == EPhase4GuideKind::Corridor || Phase4Guidance::LinkClear(Position, Anchor))
            {
                Target.Position = Anchor;
                Target.Label = Leg.IsColumn() ? FString::Printf(TEXT("RE-CENTER OVER THE COLUMN / %s"), *Leg.Label)
                    : Leg.Kind == EPhase4GuideKind::Corridor ? FString::Printf(TEXT("RE-CENTER IN THE FOREST CORRIDOR / %s"), *Leg.Label)
                    : FString::Printf(TEXT("OFF THE VERIFIED CORRIDOR / RETURN TO THE ARROW / %s"), *Leg.Label);
            }
            else
            {
                Target.Position = Position;
                Target.bHold = true;
                Target.Label = TEXT("OFF CORRIDOR / CONTROLLED STOP / HOVER, THEN CLIMB SLOWLY UNTIL THE ARROW RETURNS");
            }
        }
    }
    Target.LegIndex = GuidanceLegIndex;
    Target.LegCount = GuidanceRoute.Num();
    CurrentGuidance = Target;
    ApplyGuidanceFeedback();
}

void FPhase4MissionSession::ApplyGuidanceFeedback()
{
    // Feedback, HUD and recovery all read the same guidance output.
    if (CurrentGuidance.IsValid()) Feedback.RecoveryPoint = CurrentGuidance.Position;
}

void FPhase4MissionSession::ReportWaterContact(const FVector& Position)
{
    if (Stage < EPhase4MissionStage::Takeoff || Stage > EPhase4MissionStage::Landing) return;
    Feedback.Incident=EPhase4Incident::UnsafeLanding;
    Feedback.Headline=TEXT("WATER CONTACT / SAFE PAUSE");
    Feedback.ShapeCue=TEXT("STOP OCTAGON");
    Feedback.RecoveryAction=TEXT("Water is not a landing surface. Reset to the launch mat to retry.");
    SetRecoveryGuide(Position, true);
    RefreshGuidance(Position);
}

void FPhase4MissionSession::ObserveCheckpoint(const FVector& Position)
{
    if (!Task.Checkpoints.IsValidIndex(NextCheckpointIndex))
    {
        RequestReturnToHome();
        return;
    }
    // Only called while this checkpoint's own guidance leg is active; the scoring rules
    // (radius, LOW WATER height window, precision) are unchanged.
    const FPhase4Checkpoint& Expected = Task.Checkpoints[NextCheckpointIndex];
    const float Distance = FVector::Distance(Position, Expected.Position);
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
            Feedback.RecoveryAction = FString::Printf(TEXT("Continue to %s along the guide."), *Task.Checkpoints[NextCheckpointIndex].ShapeCue);
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
    Feedback.RecoveryAction = TEXT("Inputs neutralized. Stabilize, then continue the current guide leg.");
    Feedback.ShapeCue = TEXT("STOP OCTAGON");
    ApplyGuidanceFeedback();
}

void FPhase4MissionSession::RequestReturnToHome()
{
    if (Stage == EPhase4MissionStage::Inspection && NextCheckpointIndex < Task.Checkpoints.Num())
    {
        Feedback.Incident = EPhase4Incident::MissedCheckpoint;
        Feedback.Headline = TEXT("ROUTE INCOMPLETE");
        Feedback.RecoveryAction = TEXT("Return to the highlighted checkpoint before RTH.");
        Feedback.ShapeCue = Task.Checkpoints[NextCheckpointIndex].ShapeCue;
        ApplyGuidanceFeedback(); // the current guide, never a straight line to a far checkpoint
        return;
    }
    if (Stage == EPhase4MissionStage::Inspection || Stage == EPhase4MissionStage::ReturnToHome)
    {
        Stage = EPhase4MissionStage::ReturnToHome;
        Feedback.Incident = EPhase4Incident::None;
        Feedback.Headline = TEXT("RETURN TO HOME");
        Feedback.RecoveryAction = TEXT("Follow the return guides back through the corridor to the pad.");
        Feedback.ShapeCue = TEXT("HOME DIAMOND");
        // RTH is a guidance leg switch onto the verified return legs, not autopilot.
        const int32 FirstReturn = FindFirstReturnLeg();
        if (FirstReturn != INDEX_NONE && GuidanceLegIndex < FirstReturn)
        {
            GuidanceLegIndex = FirstReturn;
            GuidanceProgressCm = 0.0F;
        }
        if (GuidanceRoute.IsValidIndex(GuidanceLegIndex)) RefreshGuidance(GuidanceRoute[GuidanceLegIndex].Path[0]);
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
    if (EvidenceStage >= EPhase4MissionStage::Takeoff && EvidenceStage <= EPhase4MissionStage::Landing)
        GuidanceRoute = Task.BuildGuidanceRoute();
    if (EvidenceStage == EPhase4MissionStage::Preflight)
    {
        SetPreflightComplete(true);
    }
    else if (EvidenceStage == EPhase4MissionStage::Inspection)
    {
        NextCheckpointIndex = 2;
        GuidanceLegIndex = FMath::Max(0, GuidanceRoute.IndexOfByPredicate([](const FPhase4GuidanceLeg& Leg) { return Leg.ScoredCheckpointIndex == 2; }));
        Feedback.Headline = TEXT("CHECKPOINT 3 / BOARDWALK");
        Feedback.RecoveryAction = TEXT("Fly through SQUARE 3; keep the center view clear.");
        Feedback.ShapeCue = TEXT("SQUARE 3");
        if (GuidanceRoute.IsValidIndex(GuidanceLegIndex)) RefreshGuidance(GuidanceRoute[GuidanceLegIndex].Path[0]);
    }
    else if (EvidenceStage == EPhase4MissionStage::ReturnToHome)
    {
        NextCheckpointIndex = Task.Checkpoints.Num();
        BoundaryViolations = 1;
        Feedback.Incident = EPhase4Incident::Boundary;
        Feedback.Headline = TEXT("OUTSIDE SAFE ZONE");
        Feedback.RecoveryAction = TEXT("Follow inward chevrons to recover; progress is preserved.");
        Feedback.ShapeCue = TEXT("INWARD CHEVRONS");
        GuidanceLegIndex = FMath::Max(0, FindFirstReturnLeg());
        if (GuidanceRoute.IsValidIndex(GuidanceLegIndex)) RefreshGuidance(GuidanceRoute[GuidanceLegIndex].Path[0]);
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
    if (Stage >= EPhase4MissionStage::Takeoff && Stage <= EPhase4MissionStage::Landing && CurrentGuidance.IsValid())
        return CurrentGuidance.Label;
    return Feedback.RecoveryAction;
}
