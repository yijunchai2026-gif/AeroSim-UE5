#if WITH_DEV_AUTOMATION_TESTS
// DEV-FLIGHT navigation contract tests (docs/tasks/AAA-R5-NAVIGATION-GAP.md).
// Logic fixtures feed deterministic resolved poses; the world tests use the real coast
// collision and the real Pawn. None of this is player/real-input route evidence.
#include "AeroSimGameMode.h"
#include "AeroSimPlayerController.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FlightPawn.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "PacificCoastFlora.h"
#include "PacificCoastGraybox.h"
#include "Phase4Mission.h"

namespace AeroSimNavigationTests
{
FPhase4MissionSession StartMission()
{
    FPhase4MissionSession Mission;
    Mission.Reset(); Mission.EnterPreflight(); Mission.SetPreflightComplete(true); Mission.BeginMission();
    return Mission;
}

int32 LegIndexOf(const FPhase4MissionSession& Mission, const TCHAR* LegId)
{
    return Mission.GetGuidanceRoute().IndexOfByPredicate([LegId](const FPhase4GuidanceLeg& Leg) { return Leg.LegId == FName(LegId); });
}

/** Feeds hovering poses every 50 cm along the current leg; returns false if the leg did not complete. */
bool WalkCurrentLeg(FPhase4MissionSession& Mission)
{
    const int32 Start = Mission.GetGuidanceLegIndex();
    if (!Mission.GetGuidanceRoute().IsValidIndex(Start)) return false;
    const FPhase4GuidanceLeg Leg = Mission.GetGuidanceRoute()[Start];
    const EFlightPhase Phase = Mission.Stage == EPhase4MissionStage::Takeoff ? EFlightPhase::Takeoff : EFlightPhase::Flight;
    const float Length = Leg.GetLength();
    for (float Along = 0.0F; Along <= Length + 50.0F && Mission.GetGuidanceLegIndex() == Start; Along += 50.0F)
        Mission.Observe(.05F, Mission.Stage == EPhase4MissionStage::Takeoff ? Phase : EFlightPhase::Flight, Leg.Evaluate(Along), FVector::ZeroVector, FRotator::ZeroRotator);
    return Mission.GetGuidanceLegIndex() != Start;
}

/** Walks to the landing leg and lands with independently confirmed support. */
bool WalkWholeRoute(FPhase4MissionSession& Mission, FAutomationTestBase& Test)
{
    for (int32 Guard = 0; Guard < 64; ++Guard)
    {
        const FPhase4GuidanceLeg& Leg = Mission.GetGuidanceRoute()[Mission.GetGuidanceLegIndex()];
        if (Leg.Kind == EPhase4GuideKind::Landing)
        {
            for (float Along = 0.0F; Along <= Leg.GetLength(); Along += 50.0F)
                Mission.Observe(.05F, EFlightPhase::Flight, Leg.Evaluate(Along), FVector::ZeroVector, FRotator::ZeroRotator);
            Mission.Observe(.05F, EFlightPhase::Grounded, Leg.Path.Last() + FVector(0, 0, 9), FVector::ZeroVector, FRotator::ZeroRotator, true);
            return Mission.Stage == EPhase4MissionStage::Result;
        }
        if (!WalkCurrentLeg(Mission))
        {
            Test.AddError(FString::Printf(TEXT("Leg %s did not complete when followed at 50 cm steps"), *Leg.LegId.ToString()));
            return false;
        }
    }
    return false;
}

/** Walks until the given leg is the current one. */
bool WalkUntil(FPhase4MissionSession& Mission, const TCHAR* LegId)
{
    const int32 Target = LegIndexOf(Mission, LegId);
    for (int32 Guard = 0; Guard < 64 && Mission.GetGuidanceLegIndex() < Target; ++Guard)
        if (!WalkCurrentLeg(Mission)) return false;
    return Mission.GetGuidanceLegIndex() == Target;
}

bool SingleSource(const FPhase4MissionSession& Mission)
{
    return Mission.GetCurrentGuidanceTarget().IsValid() && Mission.Feedback.RecoveryPoint.Equals(Mission.GetCurrentGuidanceTarget().Position, .01);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAeroSimNavigationContractTest, "AeroSim.Flight.Navigation.GuidanceContractKeepsFiveCheckpoints",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAeroSimNavigationContractTest::RunTest(const FString& Parameters)
{
    using namespace AeroSimNavigationTests;
    FPhase4MissionSession Mission = StartMission();
    TestEqual(TEXT("Mission starts"), Mission.Stage, EPhase4MissionStage::Takeoff);
    const FPhase4TaskDefinition Fresh = FPhase4TaskDefinition::MakePacificCoastV1();
    TestEqual(TEXT("Exactly five scored checkpoints"), Mission.Task.Checkpoints.Num(), 5);
    for (int32 Index = 0; Index < 5; ++Index)
    {
        TestEqual(TEXT("Checkpoint identity unchanged"), Mission.Task.Checkpoints[Index].Id, Fresh.Checkpoints[Index].Id);
        TestTrue(TEXT("Checkpoint position unchanged"), Mission.Task.Checkpoints[Index].Position.Equals(Fresh.Checkpoints[Index].Position));
        TestEqual(TEXT("Checkpoint radius unchanged"), Mission.Task.Checkpoints[Index].RadiusCm, Fresh.Checkpoints[Index].RadiusCm);
    }
    const TArray<FPhase4GuidanceLeg>& Route = Mission.GetGuidanceRoute();
    FString Failure;
    TestTrue(FString::Printf(TEXT("Guidance structure valid (%s)"), *Failure), Mission.Task.ValidateGuidanceStructure(Route, Failure));
    const TCHAR* Expected[] = {TEXT("nav-01-takeoff-column"), TEXT("nav-02-grass-soil"), TEXT("nav-03-inland-to-cliff"), TEXT("nav-04-cross-cliff-out"),
        TEXT("nav-05-offshore-descent"), TEXT("nav-06-alongshore-low-water"), TEXT("nav-07-sea-climb"), TEXT("nav-08-cross-cliff-in"),
        TEXT("nav-09-field-to-forest-mouth"), TEXT("nav-10-forest-entry"), TEXT("nav-11-corridor-follow"), TEXT("nav-12-corridor-reverse"),
        TEXT("nav-13-forest-exit"), TEXT("nav-14-field-return"), TEXT("nav-15-land-on-mat")};
    if (TestEqual(TEXT("Stable leg count"), Route.Num(), static_cast<int32>(UE_ARRAY_COUNT(Expected))))
        for (int32 Index = 0; Index < Route.Num(); ++Index) TestEqual(TEXT("Stable leg id"), Route[Index].LegId, FName(Expected[Index]));
    int32 Scored = 0;
    for (const FPhase4GuidanceLeg& Leg : Route)
    {
        if (Leg.ScoredCheckpointIndex != INDEX_NONE) TestEqual(TEXT("Scored legs keep checkpoint order"), Leg.ScoredCheckpointIndex, Scored++);
        AddInfo(FString::Printf(TEXT("%s guide=%s kind=%d score=%s len=%.0fcm end=(%.0f,%.0f,%.0f) R=%.0f H=-%.0f/+%.0f Vacc<=%.0f corridor+-%.0f@%.0f band-%.0f/+%.0f %s"),
            *Leg.LegId.ToString(), *Leg.GuideId.ToString(), static_cast<int32>(Leg.Kind), *Leg.ScoredCheckpointId.ToString(), Leg.GetLength(),
            Leg.Path.Last().X, Leg.Path.Last().Y, Leg.Path.Last().Z, Leg.AcceptRadiusCm, Leg.AcceptBelowCm, Leg.AcceptAboveCm,
            Leg.MaxAcceptPlanarSpeedCm, Leg.SafetyHalfWidthCm, Leg.MaxTrackPlanarSpeedCm, Leg.SafetyBelowCm, Leg.SafetyAboveCm,
            Leg.bPassThrough ? TEXT("PASS") : TEXT("STOP")));
    }
    TestEqual(TEXT("Five scored legs; intermediate guides never score"), Scored, 5);
    const FPhase4GuidanceLeg& Descent = Route[LegIndexOf(Mission, TEXT("nav-05-offshore-descent"))];
    TestTrue(TEXT("Descent column locks XY at C2"), FVector::Dist2D(Descent.Path[0], Mission.Task.Checkpoints[1].Position) < 1.0);
    TestTrue(TEXT("Descent column is slow (<=100 cm/s)"), Descent.MaxTrackPlanarSpeedCm <= 100.0F && Descent.SafetyHalfWidthCm <= 100.0F);
    const FPhase4GuidanceLeg& Brake = Route[LegIndexOf(Mission, TEXT("nav-04-cross-cliff-out"))];
    TestTrue(TEXT("Brake to <=100 cm/s before descending"), Brake.MaxAcceptPlanarSpeedCm <= 100.0F);
    TestTrue(TEXT("Cliff is crossed at a positive altitude, never on a descending chord"), Brake.Path[0].Z >= 0 && FMath::IsNearlyEqual(Brake.Path[0].Z, Brake.Path.Last().Z));
    const FPhase4GuidanceLeg& Climb = Route[LegIndexOf(Mission, TEXT("nav-07-sea-climb"))];
    TestTrue(TEXT("Climb column sits on C3 XY"), FVector::Dist2D(Climb.Path[0], Mission.Task.Checkpoints[2].Position) < 1.0 && FVector::Dist2D(Climb.Path.Last(), Climb.Path[0]) < 1.0);
    const FPhase4GuidanceLeg& Corridor = Route[LegIndexOf(Mission, TEXT("nav-11-corridor-follow"))];
    TestTrue(TEXT("Corridor follows the flora corridor polyline, not a chord"), Corridor.Path.Num() > 20);
    for (const FVector& Point : Corridor.Path)
        if (!TestTrue(TEXT("Corridor guide stays on CoastFlora corridor centreline"), CoastFlora::CorridorDistance(FVector2D(Point)) < 5.0F)) break;
    TestTrue(TEXT("Return starts with the corridor in reverse"), Route[LegIndexOf(Mission, TEXT("nav-12-corridor-reverse"))].bReturnLeg);
    // Logic-only: walking every leg at 50 cm steps completes the five checkpoints and lands.
    TestTrue(TEXT("Leg-following state fixture reaches Result"), WalkWholeRoute(Mission, *this));
    TestEqual(TEXT("All five checkpoints counted once"), Mission.NextCheckpointIndex, 5);
    TestEqual(TEXT("No missed checkpoint on the verified route"), Mission.MissedCheckpoints, 0);
    TestTrue(TEXT("Result complete"), Mission.Score.bComplete);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAeroSimNavigationCounterexampleTest, "AeroSim.Flight.Navigation.CounterexamplesNeverSkipSafetyGates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAeroSimNavigationCounterexampleTest::RunTest(const FString& Parameters)
{
    using namespace AeroSimNavigationTests;
    {
        // Future checkpoint radius right after takeoff: diagnosed, never scored, guidance unmoved.
        FPhase4MissionSession Mission = StartMission();
        WalkCurrentLeg(Mission);
        const int32 Leg = Mission.GetGuidanceLegIndex();
        Mission.Observe(.05F, EFlightPhase::Flight, Mission.Task.Checkpoints[2].Position, FVector::ZeroVector, FRotator::ZeroRotator);
        TestEqual(TEXT("Future checkpoint does not score"), Mission.NextCheckpointIndex, 0);
        TestTrue(TEXT("Shortcut is diagnosed"), Mission.MissedCheckpoints > 0);
        TestEqual(TEXT("Shortcut does not move guidance"), Mission.GetGuidanceLegIndex(), Leg);
        TestTrue(TEXT("HUD/feedback/recovery share one target"), SingleSource(Mission));
    }
    {
        // Fast drift at the offshore brake point and on the descent column never progresses.
        FPhase4MissionSession Mission = StartMission();
        TestTrue(TEXT("Reach the offshore brake leg"), WalkUntil(Mission, TEXT("nav-04-cross-cliff-out")));
        const FPhase4GuidanceLeg Brake = Mission.GetGuidanceRoute()[Mission.GetGuidanceLegIndex()];
        for (float Along = 0; Along <= Brake.GetLength(); Along += 50)
            Mission.Observe(.05F, EFlightPhase::Flight, Brake.Evaluate(Along), FVector(900, 0, 0), FRotator::ZeroRotator);
        TestEqual(TEXT("Still drifting at 9 m/s: no descent yet"), Mission.GetCurrentGuidanceTarget().LegId, FName(TEXT("nav-04-cross-cliff-out")));
        Mission.Observe(.05F, EFlightPhase::Flight, Brake.Path.Last(), FVector(60, 0, 0), FRotator::ZeroRotator);
        TestEqual(TEXT("Braked: descent column active"), Mission.GetCurrentGuidanceTarget().LegId, FName(TEXT("nav-05-offshore-descent")));
        const FPhase4GuidanceLeg Descent = Mission.GetGuidanceRoute()[Mission.GetGuidanceLegIndex()];
        for (float Along = 0; Along <= Descent.GetLength(); Along += 50)
            Mission.Observe(.05F, EFlightPhase::Flight, Descent.Evaluate(Along), FVector(400, 0, -300), FRotator::ZeroRotator);
        TestEqual(TEXT("Fast planar drift on the column records no progress"), Mission.GetGuidanceLegProgressCm(), 0.0F);
        TestEqual(TEXT("Scored C2 by radius does not complete the column"), Mission.GetCurrentGuidanceTarget().LegId, FName(TEXT("nav-05-offshore-descent")));
        TestEqual(TEXT("C2 original radius still scores once its leg is active"), Mission.NextCheckpointIndex, 2);
        for (float Along = 0; Along <= Descent.GetLength(); Along += 50)
            Mission.Observe(.05F, EFlightPhase::Flight, Descent.Evaluate(Along), FVector(0, 0, -500), FRotator::ZeroRotator);
        TestEqual(TEXT("Arriving at C2 still descending at 5 m/s is not accepted (vertical brake window)"), Mission.GetCurrentGuidanceTarget().LegId, FName(TEXT("nav-05-offshore-descent")));
        TestTrue(TEXT("Brake cue shown inside the vertical brake zone"), Mission.GetCurrentGuidanceTarget().bVerticalBrake);
        Mission.Observe(.05F, EFlightPhase::Flight, Descent.Path.Last(), FVector(0, 0, -100), FRotator::ZeroRotator);
        TestEqual(TEXT("Braked vertical arrival completes the column"), Mission.GetCurrentGuidanceTarget().LegId, FName(TEXT("nav-06-alongshore-low-water")));
    }
    {
        // R6a real-input finding: transit points are pass-through gates, and a pilot who strayed from
        // the transit tube and rejoins it near the end is not stranded behind the progress window.
        FPhase4MissionSession Mission = StartMission();
        TestTrue(TEXT("Reach the inland-to-cliff transit"), WalkUntil(Mission, TEXT("nav-03-inland-to-cliff")));
        const FPhase4GuidanceLeg Transit = Mission.GetGuidanceRoute()[Mission.GetGuidanceLegIndex()];
        TestTrue(TEXT("Transit guide is pass-through (>=6 m, +-2 m, no speed window)"), Transit.bPassThrough
            && Transit.AcceptRadiusCm >= 600.0F && Transit.AcceptBelowCm >= 200.0F && Transit.AcceptAboveCm >= 200.0F);
        const FVector Side = FVector::CrossProduct(Transit.Path.Last() - Transit.Path[0], FVector::UpVector).GetSafeNormal() * (Transit.SafetyHalfWidthCm + 1000.0F);
        for (float Along = 0; Along <= Transit.GetLength() - 2000.0F; Along += 50)
            Mission.Observe(.05F, EFlightPhase::Flight, Transit.Evaluate(Along) + Side, FVector(1200, 0, 0), FRotator::ZeroRotator);
        TestEqual(TEXT("Outside the safety corridor records no progress"), Mission.GetGuidanceLegProgressCm(), 0.0F);
        TestTrue(TEXT("Off-corridor is visible guidance, not a silent stall"), Mission.GetCurrentGuidanceTarget().bOffCorridor
            && Mission.GetCurrentGuidanceTarget().bRecovery && SingleSource(Mission));
        // Orbit 1-2 m around the guide point at 3 m/s, like the R6a driver.
        for (int32 Step = 0; Step < 40 && Mission.GetCurrentGuidanceTarget().LegId == Transit.LegId; ++Step)
        {
            const float Angle = Step * .4F;
            Mission.Observe(.05F, EFlightPhase::Flight, Transit.Path.Last() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * 150.0F,
                FVector(-FMath::Sin(Angle), FMath::Cos(Angle), 0) * 300.0F, FRotator::ZeroRotator);
        }
        TestEqual(TEXT("Rejoining near the end at 3 m/s completes the pass-through transit"), Mission.GetCurrentGuidanceTarget().LegId, FName(TEXT("nav-04-cross-cliff-out")));
        const FPhase4GuidanceLeg Brake = Mission.GetGuidanceRoute()[Mission.GetGuidanceLegIndex()];
        TestFalse(TEXT("Descent-column top stays a tight safety gate"), Brake.bPassThrough);
        Mission.Observe(.05F, EFlightPhase::Flight, Brake.Path.Last(), FVector(500, 0, 0), FRotator::ZeroRotator);
        TestEqual(TEXT("Column top is never passed through at speed"), Mission.GetCurrentGuidanceTarget().LegId, FName(TEXT("nav-04-cross-cliff-out")));
        // Gate-plane crossing at cruise speed completes a transit (fresh mission, nav-02 C1).
        FPhase4MissionSession Cross = StartMission();
        WalkCurrentLeg(Cross);
        const FPhase4GuidanceLeg C1Leg = Cross.GetGuidanceRoute()[Cross.GetGuidanceLegIndex()];
        for (float Along = 0; Along <= C1Leg.GetLength() + 900.0F; Along += 60)
        {
            const FVector Dir = (C1Leg.Path.Last() - C1Leg.Path[0]).GetSafeNormal();
            Cross.Observe(.05F, EFlightPhase::Flight, C1Leg.Path[0] + Dir * Along + FVector(0, 400, 0), Dir * 1700.0F, FRotator::ZeroRotator);
        }
        TestEqual(TEXT("Flying through C1 at 17 m/s, 4 m to the side, passes the gate"), Cross.GetCurrentGuidanceTarget().LegId, FName(TEXT("nav-03-inland-to-cliff")));
        TestEqual(TEXT("C1 scored by its original radius"), Cross.NextCheckpointIndex, 1);
    }
    {
        // R6a redesign: a real-input-like L-shaped and zigzag path inside the open-air safety corridor
        // completes transit legs; nobody has to hug the centre line.
        for (int32 Shape = 0; Shape < 2; ++Shape)
        {
            FPhase4MissionSession Mission = StartMission();
            TestTrue(TEXT("Reach the inland-to-cliff transit"), WalkUntil(Mission, TEXT("nav-03-inland-to-cliff")));
            const FPhase4GuidanceLeg Leg = Mission.GetGuidanceRoute()[Mission.GetGuidanceLegIndex()];
            const FVector Normal = FVector::CrossProduct(Leg.Path.Last() - Leg.Path[0], FVector::UpVector).GetSafeNormal();
            const float W = Leg.SafetyHalfWidthCm * .8F, Length = Leg.GetLength();
            for (float Along = 0; Along <= Length + 200.0F && Mission.GetGuidanceLegIndex() == LegIndexOf(Mission, TEXT("nav-03-inland-to-cliff")); Along += 60)
            {
                const float U = Along / Length;
                // L: drift straight out to 80 % width over the first quarter, run parallel, cut back late.
                // Zigzag: +-80 % width weave every ~30 m with diagonal corrections and +-1.5 m height.
                const float Offset = Shape == 0 ? W * FMath::Clamp(FMath::Min(U * 4.0F, (1.0F - U) * 6.0F), 0.0F, 1.0F)
                    : W * FMath::Sin(Along / 3000.0F * PI);
                const float Height = Shape == 0 ? 0.0F : 150.0F * FMath::Sin(Along / 1700.0F);
                Mission.Observe(.05F, EFlightPhase::Flight, Leg.Evaluate(Along) + Normal * Offset + FVector(0, 0, Height),
                    FVector(1500, 0, 0), FRotator::ZeroRotator);
            }
            TestEqual(Shape == 0 ? TEXT("L-shaped path inside the corridor completes the transit") : TEXT("Zigzag path inside the corridor completes the transit"),
                Mission.GetCurrentGuidanceTarget().LegId, FName(TEXT("nav-04-cross-cliff-out")));
        }
        {
            // Exact PMO R6a packaged case: progress 501 cm, then parked at (16823,-12026,339), speed 0.
            FPhase4MissionSession Parked = StartMission();
            TestTrue(TEXT("Reach the inland-to-cliff transit"), WalkUntil(Parked, TEXT("nav-03-inland-to-cliff")));
            const FPhase4GuidanceLeg Leg = Parked.GetGuidanceRoute()[Parked.GetGuidanceLegIndex()];
            Parked.Observe(.05F, EFlightPhase::Flight, Leg.Evaluate(501.0F), FVector(1500, 0, 0), FRotator::ZeroRotator);
            Parked.Observe(.05F, EFlightPhase::Flight, FVector(16823, -12026, 339), FVector::ZeroVector, FRotator::ZeroRotator);
            TestEqual(TEXT("PMO R6a parked pose 17/26/11 cm from the guide completes leg 3"), Parked.GetCurrentGuidanceTarget().LegId, FName(TEXT("nav-04-cross-cliff-out")));
        }
        // Cutting across the cliff edge: dropping below the transit band over the cliff, or flying the
        // chord from C1 straight at the offshore descent point, is rejected (no progress, no skip).
        FPhase4MissionSession Cut = StartMission();
        TestTrue(TEXT("Reach the cliff crossing"), WalkUntil(Cut, TEXT("nav-04-cross-cliff-out")));
        const FPhase4GuidanceLeg Cross = Cut.GetGuidanceRoute()[Cut.GetGuidanceLegIndex()];
        for (float Along = 0; Along <= Cross.GetLength(); Along += 50)
            Cut.Observe(.05F, EFlightPhase::Flight, Cross.Evaluate(Along) - FVector(0, 0, 1500.0F * Along / Cross.GetLength()), FVector(800, 0, -200), FRotator::ZeroRotator);
        Cut.Observe(.05F, EFlightPhase::Flight, Cross.Path.Last() - FVector(0, 0, 1500), FVector::ZeroVector, FRotator::ZeroRotator);
        TestEqual(TEXT("Descending across the cliff edge below the band does not complete the crossing"), Cut.GetCurrentGuidanceTarget().LegId, FName(TEXT("nav-04-cross-cliff-out")));
        TestTrue(TEXT("Below-band cliff crossing shows recovery guidance"), Cut.GetCurrentGuidanceTarget().bOffCorridor || Cut.GetCurrentGuidanceTarget().bRecovery);
        FPhase4MissionSession Chord = StartMission();
        TestTrue(TEXT("Reach the inland-to-cliff transit"), WalkUntil(Chord, TEXT("nav-03-inland-to-cliff")));
        const FVector From = Chord.GetGuidanceRoute()[Chord.GetGuidanceLegIndex()].Path[0];
        const FVector OffshoreTop = Chord.GetGuidanceRoute()[LegIndexOf(Chord, TEXT("nav-04-cross-cliff-out"))].Path.Last();
        for (float U = 0; U <= 1.0F; U += .002F)
            Chord.Observe(.05F, EFlightPhase::Flight, FMath::Lerp(From, OffshoreTop, static_cast<double>(U)), FVector(1500, 0, 0), FRotator::ZeroRotator);
        for (int32 I = 0; I < 5; ++I) Chord.Observe(.05F, EFlightPhase::Flight, OffshoreTop, FVector::ZeroVector, FRotator::ZeroRotator);
        TestEqual(TEXT("The C1-to-offshore chord never completes the above-cliff gate"), Chord.GetCurrentGuidanceTarget().LegId, FName(TEXT("nav-03-inland-to-cliff")));
        TestEqual(TEXT("C2 is not scored from a shortcut"), Chord.NextCheckpointIndex, 1);
    }
    {
        // Heading inland before the climb column is complete never advances to the cliff crossing.
        FPhase4MissionSession Mission = StartMission();
        TestTrue(TEXT("Reach the sea climb"), WalkUntil(Mission, TEXT("nav-07-sea-climb")));
        const FVector C3 = Mission.Task.Checkpoints[2].Position;
        const FPhase4GuidanceLeg InEdge = Mission.GetGuidanceRoute()[LegIndexOf(Mission, TEXT("nav-08-cross-cliff-in"))];
        for (float X = C3.X; X > InEdge.Path.Last().X; X -= 50)
            Mission.Observe(.05F, EFlightPhase::Flight, FVector(X, C3.Y, C3.Z), FVector::ZeroVector, FRotator::ZeroRotator);
        Mission.Observe(.05F, EFlightPhase::Flight, InEdge.Path.Last(), FVector::ZeroVector, FRotator::ZeroRotator);
        TestEqual(TEXT("No climb, no inland guide"), Mission.GetCurrentGuidanceTarget().LegId, FName(TEXT("nav-07-sea-climb")));
        TestTrue(TEXT("Target is the C3 climb column or a controlled stop, never inland"),
            Mission.GetCurrentGuidanceTarget().bHold || FVector::Dist2D(Mission.GetCurrentGuidanceTarget().Position, C3) < 1.0);
        TestEqual(TEXT("C4 is not scored from the cliff"), Mission.NextCheckpointIndex, 3);
    }
    {
        // Rejoining the corridor further along cannot jump progress past the turn.
        FPhase4MissionSession Mission = StartMission();
        TestTrue(TEXT("Reach the corridor reverse"), WalkUntil(Mission, TEXT("nav-12-corridor-reverse")));
        TestEqual(TEXT("Five checkpoints done switches to return (RTH leg switch)"), Mission.Stage, EPhase4MissionStage::ReturnToHome);
        const FPhase4GuidanceLeg Reverse = Mission.GetGuidanceRoute()[Mission.GetGuidanceLegIndex()];
        Mission.Observe(.05F, EFlightPhase::Flight, Reverse.Path.Last(), FVector::ZeroVector, FRotator::ZeroRotator);
        TestEqual(TEXT("Entering the mouth from the field does not finish the reverse leg"), Mission.GetCurrentGuidanceTarget().LegId, FName(TEXT("nav-12-corridor-reverse")));
        TestTrue(TEXT("Progress stays near the reverse start"), Mission.GetGuidanceLegProgressCm() < 300.0F);
    }
    {
        // Pause, collision freeze and water never advance navigation; water needs explicit Reset.
        FPhase4MissionSession Mission = StartMission();
        WalkCurrentLeg(Mission);
        const int32 Leg = Mission.GetGuidanceLegIndex();
        const FPhase4GuidanceLeg Current = Mission.GetGuidanceRoute()[Leg];
        Mission.Observe(.05F, EFlightPhase::Paused, Current.Path.Last(), FVector::ZeroVector, FRotator::ZeroRotator);
        TestEqual(TEXT("Paused pose does not advance"), Mission.GetGuidanceLegIndex(), Leg);
        TestEqual(TEXT("Paused pose does not score"), Mission.NextCheckpointIndex, 0);
        Mission.ReportCollision(Current.Evaluate(100));
        Mission.Observe(.05F, EFlightPhase::Flight, Current.Evaluate(200), FVector::ZeroVector, FRotator::ZeroRotator);
        TestEqual(TEXT("Collision frame records no progress"), Mission.GetGuidanceLegProgressCm(), 0.0F);
        TestTrue(TEXT("Collision recovery reads the current guide"), SingleSource(Mission));
        Mission.ReleaseCollisionContact();
        Mission.Observe(.05F, EFlightPhase::Flight, Current.Evaluate(200), FVector::ZeroVector, FRotator::ZeroRotator);
        TestTrue(TEXT("After release the same leg continues from the actual pose"), Mission.GetGuidanceLegProgressCm() > 0.0F && Mission.GetGuidanceLegIndex() == Leg);
        Mission.ReportWaterContact(FVector(25000, -6000, -6500));
        TestTrue(TEXT("Water shows a hold, not an arrow"), Mission.GetCurrentGuidanceTarget().bHold);
        TestTrue(TEXT("Water hold is the single recovery output"), SingleSource(Mission));
        const float Progress = Mission.GetGuidanceLegProgressCm();
        Mission.Observe(.05F, EFlightPhase::Flight, Current.Path.Last(), FVector::ZeroVector, FRotator::ZeroRotator);
        TestEqual(TEXT("Water incident never resumes navigation"), Mission.GetGuidanceLegProgressCm(), Progress);
        Mission.Reset();
        TestFalse(TEXT("Legal reset clears the old guidance index"), Mission.GetCurrentGuidanceTarget().IsValid());
        Mission.EnterPreflight(); Mission.SetPreflightComplete(true); Mission.BeginMission();
        TestEqual(TEXT("Rebuilt navigation starts at takeoff"), Mission.GetCurrentGuidanceTarget().LegId, FName(TEXT("nav-01-takeoff-column")));
    }
    {
        // Boundary: recovery anchor on the verified leg; cleared only after rejoining that leg.
        FPhase4MissionSession Mission = StartMission();
        WalkCurrentLeg(Mission);
        const FPhase4GuidanceLeg Current = Mission.GetGuidanceRoute()[Mission.GetGuidanceLegIndex()];
        Mission.Observe(.05F, EFlightPhase::Flight, FVector(2000, -16000, 600), FVector::ZeroVector, FRotator::ZeroRotator);
        TestEqual(TEXT("Boundary recorded"), Mission.BoundaryViolations, 1);
        TestTrue(TEXT("Recovery guide active"), Mission.GetCurrentGuidanceTarget().bRecovery);
        TestTrue(TEXT("Recovery feedback = HUD target"), SingleSource(Mission));
        float Distance = 0;
        Current.Project(Mission.GetCurrentGuidanceTarget().Position, 0, Current.GetLength(), Distance);
        TestTrue(TEXT("Recovery anchor lies on the verified current leg"), Distance < 1.0F);
        Mission.Observe(.05F, EFlightPhase::Flight, FVector(2000, -12000, 600), FVector::ZeroVector, FRotator::ZeroRotator);
        TestEqual(TEXT("Back inside but off the leg: still recovering"), Mission.Feedback.Incident, EPhase4Incident::Boundary);
        Mission.Observe(.05F, EFlightPhase::Flight, Mission.GetCurrentGuidanceTarget().Position, FVector::ZeroVector, FRotator::ZeroRotator);
        TestEqual(TEXT("Rejoining the leg clears the incident"), Mission.Feedback.Incident, EPhase4Incident::None);
        TestFalse(TEXT("Normal guide restored"), Mission.GetCurrentGuidanceTarget().bRecovery);
        // An unsafe link (below the cliff top near the face) is a controlled stop, not a straight arrow.
        FPhase4MissionSession Deep = StartMission();
        TestTrue(TEXT("Reach the climb"), WalkUntil(Deep, TEXT("nav-07-sea-climb")));
        Deep.Observe(.05F, EFlightPhase::Flight, FVector(APacificCoastGraybox::GetCoastalShoreX(-6000) + 150, -6000, -6000), FVector::ZeroVector, FRotator::ZeroRotator);
        TestTrue(TEXT("Under-cliff incident pose with no proven link holds"), Deep.GetCurrentGuidanceTarget().bHold);
    }
    {
        // RTH before the fifth checkpoint is refused and does not switch legs.
        FPhase4MissionSession Mission = StartMission();
        WalkCurrentLeg(Mission); WalkCurrentLeg(Mission);
        const int32 Leg = Mission.GetGuidanceLegIndex();
        Mission.RequestReturnToHome();
        TestEqual(TEXT("Incomplete RTH keeps the leg"), Mission.GetGuidanceLegIndex(), Leg);
        TestEqual(TEXT("Incomplete RTH reports route incomplete"), Mission.Feedback.Incident, EPhase4Incident::MissedCheckpoint);
        TestTrue(TEXT("Incomplete RTH recovery is the current guide, not a far checkpoint"), SingleSource(Mission));
    }
    return true;
}

namespace AeroSimNavigationTests
{
struct FCoastWorld
{
    UWorld* World = nullptr;
    APacificCoastGraybox* Coast = nullptr;
    AFlightPawn* Pawn = nullptr;
    AAeroSimGameMode* GameMode = nullptr;
    AAeroSimPlayerController* Controller = nullptr;
    explicit FCoastWorld(const bool bWithGameMode)
    {
        World = UWorld::CreateWorld(EWorldType::Game, false);
        if (!World) return;
        if (bWithGameMode)
        {
            World->SetGameInstance(NewObject<UGameInstance>());
            World->GetWorldSettings()->DefaultGameMode = AAeroSimGameMode::StaticClass();
            World->SetGameMode(FURL());
            GameMode = World->GetAuthGameMode<AAeroSimGameMode>();
        }
        Coast = World->SpawnActor<APacificCoastGraybox>();
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Pawn = World->SpawnActor<AFlightPawn>(Spawn);
        if (bWithGameMode && Pawn)
        {
            Controller = World->SpawnActor<AAeroSimPlayerController>(Spawn);
            World->AddController(Controller);
            Controller->PlayerInput = NewObject<UPlayerInput>(Controller);
            Controller->Possess(Pawn);
        }
    }
    ~FCoastWorld() { if (World) World->DestroyWorld(false); }
};

float FootDepth(const AFlightPawn* Pawn)
{
    TArray<UStaticMeshComponent*> Parts;
    Pawn->GetComponents(Parts);
    double Lowest = 0;
    for (const UStaticMeshComponent* Part : Parts)
        if (Part->GetStaticMesh() && Part->GetName().StartsWith(TEXT("Support_")))
            Lowest = FMath::Min(Lowest, Part->CalcBounds(Part->GetComponentTransform()).GetBox().Min.Z - Pawn->GetActorLocation().Z);
    return static_cast<float>(-Lowest + .3);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAeroSimNavigationCollisionRouteTest, "AeroSim.Flight.Navigation.PerMetreRouteClearsRealCollision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAeroSimNavigationCollisionRouteTest::RunTest(const FString& Parameters)
{
    using namespace AeroSimNavigationTests;
    FCoastWorld Fixture(false);
    if (!TestNotNull(TEXT("Coast world"), Fixture.Coast) || !TestNotNull(TEXT("Real pawn"), Fixture.Pawn)) return false;
    AFlightPawn* Pawn = Fixture.Pawn;
    UWorld* World = Fixture.World;
    const UBoxComponent* Body = Cast<UBoxComponent>(Pawn->GetRootComponent());
    if (!TestNotNull(TEXT("Actual pawn collision box"), Body)) return false;
    const FVector Box = Body->GetScaledBoxExtent();
    // Yaw-invariant superset of the real box, plus the 18 deg commanded tilt (rotated occupancy).
    const float Planar = static_cast<float>(FMath::Sqrt(Box.X * Box.X + Box.Y * Box.Y));
    const FVector Shape(Planar, Planar, Box.Z + Box.X * FMath::Sin(FMath::DegreesToRadians(18.0F)));
    const float Foot = FootDepth(Pawn);
    AddInfo(FString::Printf(TEXT("pawn box=(%.1f,%.1f,%.1f) sweep shape=(%.1f,%.1f,%.1f) foot depth=%.2f cm"), Box.X, Box.Y, Box.Z, Shape.X, Shape.Y, Shape.Z, Foot));
    TestTrue(TEXT("Foot depth measured from actual supports"), Foot > 1.0F && Foot < 60.0F);

    FPhase4MissionSession Mission = StartMission();
    TArray<FPhase4GuidanceLeg> Legs = Mission.GetGuidanceRoute();
    // Overshoot buffer: a pilot braking at C5 drifts on along the corridor; that stretch is verified too.
    {
        FPhase4GuidanceLeg Buffer;
        Buffer.LegId = TEXT("nav-11b-c5-overshoot-buffer"); Buffer.Kind = EPhase4GuideKind::Corridor;
        Buffer.SafetyHalfWidthCm = 180.0F; Buffer.SafetyBelowCm = 150.0F; Buffer.SafetyAboveCm = 150.0F;
        const FVector C5 = Mission.Task.Checkpoints[4].Position;
        Buffer.Path.Add(C5);
        float StartDistance = -1;
        for (const CoastFlora::FCorridorSample& Sample : CoastFlora::GetLayout().Corridor)
        {
            if (StartDistance < 0 && FVector2D::Distance(Sample.Point, FVector2D(C5)) < 30.0F) StartDistance = Sample.Distance;
            if (StartDistance >= 0 && Sample.Distance > StartDistance && Sample.Distance <= StartDistance + 1000.0F)
                Buffer.Path.Add(FVector(Sample.Point, APacificCoastGraybox::GetCoastalLandHeight(Sample.Point.X, Sample.Point.Y) + 250.0F));
        }
        if (Buffer.Path.Num() > 1) Legs.Insert(Buffer, LegIndexOf(Mission, TEXT("nav-12-corridor-reverse")));
    }

    const FCollisionObjectQueryParams Objects(ECC_WorldStatic);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(AeroSimNavigationRoute), true, Pawn);
    auto Overlaps = [&](const FVector& P, const FVector& Inflate)
    {
        return World->OverlapAnyTestByObjectType(P, FQuat::Identity, Objects, FCollisionShape::MakeBox(Shape + Inflate), Query);
    };
    int32 Failures = 0, TotalSamples = 0;
    float RouteLength = 0;
    for (const FPhase4GuidanceLeg& Leg : Legs)
    {
        const bool bSupport = Leg.Kind == EPhase4GuideKind::Takeoff || Leg.Kind == EPhase4GuideKind::Landing;
        const bool bCorridor = Leg.Kind == EPhase4GuideKind::Corridor;
        // Transit/vertical: 300 cm in every direction (and to the water). Corridor: separate low-flight
        // band (foot 150..400 cm) and >=50 cm lateral trunk margin. Takeoff/landing: swept + real support.
        const float Required = bSupport ? 0.0F : bCorridor ? 50.0F : 300.0F;
        const FVector Inflate = bCorridor ? FVector(Required, Required, 0) : FVector(Required);
        const float Length = Leg.GetLength();
        const int32 Steps = FMath::Max(1, FMath::CeilToInt(Length / 100.0F));
        float MinFoot = BIG_NUMBER, MinWater = BIG_NUMBER, MinClear = BIG_NUMBER;
        // Visible crowns are not collision; report (do not hide) samples inside a placed crown volume
        // so the visual/G1 review can check them in the real view. Informational, not a PASS gate.
        TArray<const CoastFlora::FPlacedTree*> NearbyTrees;
        {
            FBox Bounds(Leg.Path);
            Bounds = Bounds.ExpandBy(FVector(2500, 2500, 5000));
            for (const CoastFlora::FPlacedTree& Tree : CoastFlora::GetLayout().Trees)
                if (Bounds.IsInsideXY(Tree.Base)) NearbyTrees.Add(&Tree);
        }
        int32 CrownSamples = 0;
        FString FirstCrown;
        // Declared safety corridor (progress envelope) must itself be clear: every pose a pilot may hold
        // while progressing keeps a margin from WorldStatic, stays in the safety volume and above water.
        const float Margin = bSupport ? 0.0F : bCorridor ? 30.0F : 100.0F;
        const double MinOrigin = FPacificCoastGrayboxContract::GetRoutePoints()[0].Z + Foot + .5;
        auto EnvelopeClear = [&](const FVector& P, const FVector& Direction, const float HalfWidth, FString* OutReason)
        {
            const double Bottom = bSupport ? FMath::Max(P.Z - Leg.SafetyBelowCm, MinOrigin) : P.Z - Leg.SafetyBelowCm;
            const double Top = P.Z + Leg.SafetyAboveCm;
            const FVector Centre(P.X, P.Y, (Bottom + Top) * .5);
            const FVector2D Dir2 = FVector2D(Direction).GetSafeNormal();
            const FQuat Rotation = Dir2.IsNearlyZero() ? FQuat::Identity : FRotator(0, FMath::RadiansToDegrees(FMath::Atan2(Dir2.Y, Dir2.X)), 0).Quaternion();
            const FVector Half(Shape.X + 60.0F + Margin, Shape.Y + HalfWidth + Margin, (Top - Bottom) * .5 + Shape.Z + Margin);
            if (World->OverlapAnyTestByObjectType(Centre, Rotation, Objects, FCollisionShape::MakeBox(Half), Query))
            { if (OutReason) *OutReason = TEXT("ENVELOPE_HITS_WORLDSTATIC"); return false; }
            const FVector Across = Rotation.RotateVector(FVector(0, HalfWidth, 0));
            for (const FVector& Corner : {FVector(P.X, P.Y, Bottom) + Across, FVector(P.X, P.Y, Bottom) - Across, FVector(P.X, P.Y, Top) + Across, FVector(P.X, P.Y, Top) - Across})
            {
                if (!Mission.Task.IsInsideSafetyVolume(Corner)) { if (OutReason) *OutReason = TEXT("ENVELOPE_OUTSIDE_SAFETY_VOLUME"); return false; }
                if (Corner.X > APacificCoastGraybox::GetCoastalShoreX(Corner.Y) + 30.0F && Corner.Z - Foot - APacificCoastGraybox::SeaLevelCm < 100.0F)
                { if (OutReason) *OutReason = TEXT("ENVELOPE_FOOT_WITHIN_1M_OF_WATER"); return false; }
            }
            return true;
        };
        float MaxClearWidth = BIG_NUMBER, MinMargin = BIG_NUMBER;
        FString First;
        FVector Previous = Leg.Evaluate(0);
        if (bSupport) Previous.Z = FMath::Max(Previous.Z, FPacificCoastGrayboxContract::GetRoutePoints()[0].Z + Foot + .5);
        for (int32 Step = 0; Step <= Steps; ++Step)
        {
            const float Along = Length * Step / Steps;
            FVector P = Leg.Evaluate(Along);
            // Mat legs: the guide is the mat surface; the actual origin can never sit below the grounded origin.
            if (bSupport) P.Z = FMath::Max(P.Z, FPacificCoastGrayboxContract::GetRoutePoints()[0].Z + Foot + .5);
            ++TotalSamples;
            FHitResult Down;
            const bool bGround = World->LineTraceSingleByObjectType(Down, P, P - FVector(0, 0, 20000), Objects, Query);
            const bool bOverWater = P.X > APacificCoastGraybox::GetCoastalShoreX(P.Y) + 30.0F;
            const float Surface = bGround ? static_cast<float>(Down.ImpactPoint.Z) : -BIG_NUMBER;
            const float FootClear = static_cast<float>(P.Z) - Foot - FMath::Max(Surface, bOverWater ? APacificCoastGraybox::SeaLevelCm : -BIG_NUMBER);
            const float WaterClear = bOverWater ? static_cast<float>(P.Z) - Foot - APacificCoastGraybox::SeaLevelCm : BIG_NUMBER;
            MinFoot = FMath::Min(MinFoot, FootClear); MinWater = FMath::Min(MinWater, WaterClear);
            FString Reason;
            float Clear = Required;
            if (P.ContainsNaN() || !Mission.Task.IsInsideSafetyVolume(P)) Reason = TEXT("OUTSIDE_SAFETY_VOLUME");
            else if (bOverWater && WaterClear <= 2.0F) Reason = TEXT("FOOT_TOUCHES_WATER");
            else if (!bSupport && !bCorridor && FootClear < 300.0F) Reason = TEXT("FOOT_CLEARANCE_BELOW_300");
            else if (bCorridor && (FootClear < 150.0F || FootClear > 400.0F)) Reason = TEXT("CORRIDOR_BAND_150_400");
            else if (Required > 0.0F && Overlaps(P, Inflate))
            {
                float Lo = 0, Hi = Required;
                for (int32 Iteration = 0; Iteration < 10; ++Iteration)
                {
                    const float Mid = (Lo + Hi) * .5F;
                    (Overlaps(P, bCorridor ? FVector(Mid, Mid, 0) : FVector(Mid)) ? Hi : Lo) = Mid;
                }
                Clear = Lo;
                Reason = bCorridor ? TEXT("LATERAL_MARGIN_BELOW_50") : TEXT("CLEARANCE_BELOW_300");
            }
            const FVector Direction = Leg.Evaluate(FMath::Min(Along + 50.0F, Length)) - Leg.Evaluate(FMath::Max(Along - 50.0F, 0.0F));
            if (Reason.IsEmpty())
            {
                FString Envelope;
                if (!EnvelopeClear(P, Direction, Leg.GetHalfWidthAt(Along), &Envelope)) Reason = FString::Printf(TEXT("SAFETY_CORRIDOR +-%.0f %s"), Leg.GetHalfWidthAt(Along), *Envelope);
            }
            {
                // Measured maximum clear half-width at this sample (for deriving the per-leg corridor).
                float Lo = 0, Hi = 3000;
                if (!EnvelopeClear(P, Direction, Lo, nullptr)) Hi = 0;
                for (int32 Iteration = 0; Iteration < 9 && Hi > 0; ++Iteration)
                { const float Mid = (Lo + Hi) * .5F; (EnvelopeClear(P, Direction, Mid, nullptr) ? Lo : Hi) = Mid; }
                MaxClearWidth = FMath::Min(MaxClearWidth, Lo);
                MinMargin = FMath::Min(MinMargin, Lo - Leg.GetHalfWidthAt(Along));
            }
            if (Reason.IsEmpty() && Step > 0)
            {
                FHitResult Hit;
                if (World->SweepSingleByObjectType(Hit, Previous, P, FQuat::Identity, Objects, FCollisionShape::MakeBox(Shape), Query))
                    Reason = FString::Printf(TEXT("SWEEP_HIT %s"), Hit.GetComponent() ? *Hit.GetComponent()->GetName() : TEXT("?")), Clear = 0;
            }
            if (Required > 0.0F) MinClear = FMath::Min(MinClear, Clear);
            for (const CoastFlora::FPlacedTree* Tree : NearbyTrees)
            {
                const CoastFlora::FArchetype& A = CoastFlora::GetArchetypes()[Tree->Archetype];
                const float Bottom = Tree->Base.Z + A.CrownBase * Tree->Scale, Top = Tree->Base.Z + A.Height * Tree->Scale;
                if (P.Z >= Bottom && P.Z <= Top && FVector::Dist2D(P, Tree->Base) <= A.CrownRadius * Tree->Scale)
                {
                    if (CrownSamples++ == 0) FirstCrown = FString::Printf(TEXT(" first_crown d=%.0f xyz=(%.0f,%.0f,%.0f) tree=(%.0f,%.0f) %s"),
                        Along, P.X, P.Y, P.Z, Tree->Base.X, Tree->Base.Y, *A.Name.ToString());
                    break;
                }
            }
            if (!Reason.IsEmpty() && First.IsEmpty())
            {
                First = FString::Printf(TEXT("FIRST FAIL leg=%s d=%.0fcm xyz=(%.1f,%.1f,%.1f) surface=%s sea=%.0f normal=%s foot_clear=%.1f clearance=%.1f reason=%s"),
                    *Leg.LegId.ToString(), Along, P.X, P.Y, P.Z, bGround ? *FString::Printf(TEXT("%.1f(%s)"), Down.ImpactPoint.Z, Down.GetComponent() ? *Down.GetComponent()->GetName() : TEXT("?")) : TEXT("none"),
                    APacificCoastGraybox::SeaLevelCm, bGround ? *Down.ImpactNormal.ToCompactString() : TEXT("none"), FootClear, Clear, *Reason);
            }
            Previous = P;
        }
        RouteLength += Length;
        if (bSupport)
        {
            // Final contact band: actual mat support under the takeoff/landing column.
            FHitResult Support;
            const FVector Home = FPacificCoastGrayboxContract::GetRoutePoints()[0];
            const bool bHit = World->LineTraceSingleByObjectType(Support, Home + FVector(0, 0, 50), Home - FVector(0, 0, 100), Objects, Query);
            if (!bHit || Support.ImpactNormal.Z < .8 || FMath::Abs(Support.ImpactPoint.Z - Home.Z) > 5.0)
                First += FString::Printf(TEXT(" NO_REAL_SUPPORT_AT_MAT hit=%d z=%.1f"), bHit, bHit ? Support.ImpactPoint.Z : 0.0);
        }
        AddInfo(FString::Printf(TEXT("LEG %s corridor+-%.0f%s band-%.0f/+%.0f measured_min_clear_halfwidth=%.0f min_spare=%.0f"), *Leg.LegId.ToString(),
            Leg.SafetyHalfWidthCm, Leg.EndFunnelHalfWidthCm > 0 ? *FString::Printf(TEXT(" (funnel to +-%.0f)"), Leg.EndFunnelHalfWidthCm) : TEXT(""),
            Leg.SafetyBelowCm, Leg.SafetyAboveCm, MaxClearWidth, MinMargin));
        AddInfo(FString::Printf(TEXT("LEG %s kind=%d length=%.0fcm samples=%d min_foot_clear=%.0f min_water_clear=%s min_clearance=%s required=%.0f %s visible_crown_samples=%d%s"),
            *Leg.LegId.ToString(), static_cast<int32>(Leg.Kind), Length, Steps + 1, MinFoot,
            MinWater < BIG_NUMBER ? *FString::Printf(TEXT("%.0f"), MinWater) : TEXT("n/a"),
            MinClear < BIG_NUMBER ? *FString::Printf(TEXT(">=%.0f"), MinClear) : TEXT("n/a"), Required, First.IsEmpty() ? TEXT("PASS") : TEXT("FAIL"),
            CrownSamples, *FirstCrown));
        if (!First.IsEmpty()) { ++Failures; AddError(First); }
    }
    AddInfo(FString::Printf(TEXT("route length %.0f m, %d samples (<=100 cm), %d failing legs"), RouteLength * .01F, TotalSamples, Failures));
    // Support proof with the actual pawn shape at the mat.
    TestTrue(TEXT("Actual pawn grounds on the real mat start"), Pawn->InitializeGroundedSpawn(FPacificCoastGrayboxContract::GetRoutePoints()[0]));
    return Failures == 0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAeroSimNavigationScriptedTraversalTest, "AeroSim.Flight.Navigation.ScriptedSemanticInputTraversal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAeroSimNavigationScriptedTraversalTest::RunTest(const FString& Parameters)
{
    using namespace AeroSimNavigationTests;
    // A scripted controller writes ordinary semantic inputs (the same SetSemanticInput the keyboard
    // feeds) into the real Pawn in the real coast with the real GameMode mission. It is an engineering
    // reachability fixture with a simple control law, not a real-input/player route PASS.
    FCoastWorld Fixture(true);
    if (!TestNotNull(TEXT("GameMode"), Fixture.GameMode) || !TestNotNull(TEXT("Pawn"), Fixture.Pawn) || !Fixture.Controller) return false;
    AFlightPawn* Pawn = Fixture.Pawn;
    AAeroSimGameMode* Mode = Fixture.GameMode;
    TestTrue(TEXT("Pawn grounds on the actual mat"), Pawn->InitializeGroundedSpawn(FPacificCoastGrayboxContract::GetRoutePoints()[0]));
    Mode->NavigateProductPage(EProductPage::Mission);
    Mode->ActivatePrimaryAction(); Mode->ActivatePrimaryAction();
    if (!TestEqual(TEXT("Normal menu commands start Flight"), Mode->GetProductPage(), EProductPage::Flight)) return false;
    const FPhase4MissionSession& Mission = Mode->GetPhase4Mission();
    if (!TestEqual(TEXT("Mission armed"), Mission.Stage, EPhase4MissionStage::Takeoff)) return false;
    constexpr float Dt = 1.0F / 30.0F;
    // Commanded vertical speed: proportional near the target, braking-limited (a=500 cm/s^2) far away.
    auto VerticalCommand = [](const float Dz)
    {
        const float Magnitude = FMath::Min3(FMath::Abs(Dz) * 1.5F, FMath::Sqrt(2.0F * 500.0F * FMath::Abs(Dz)), Dz >= 0.0F ? 780.0F : 580.0F);
        return Dz >= 0.0F ? Magnitude : -Magnitude;
    };
    TMap<FName, float> PeakVertical;
    float Time = 0.0F;
    int32 LastLeg = -1;
    float LegStart = 0.0F;
    FString Timeline;
    while (Time < 400.0F && Mission.Stage != EPhase4MissionStage::Result)
    {
        const FPhase4GuidanceTarget& Guide = Mission.GetCurrentGuidanceTarget();
        if (Guide.LegIndex != LastLeg)
        {
            if (LastLeg >= 0) Timeline += FString::Printf(TEXT(" %s=%.1fs"), *Mission.GetGuidanceRoute()[LastLeg].LegId.ToString(), Time - LegStart);
            LastLeg = Guide.LegIndex; LegStart = Time;
        }
        if (Pawn->GetFlightModel().Phase == EFlightPhase::Paused || Mission.IsRecovering() || !Guide.IsValid() || Guide.bHold) break;
        const FPhase4GuidanceLeg& Leg = Mission.GetGuidanceRoute()[Guide.LegIndex];
        const FVector P = Pawn->GetActorLocation();
        const FVector V = Pawn->GetFlightModel().Velocity;
        const bool bColumn = Leg.Kind == EPhase4GuideKind::Takeoff || Leg.Kind == EPhase4GuideKind::VerticalAscent
            || Leg.Kind == EPhase4GuideKind::VerticalDescent || Leg.Kind == EPhase4GuideKind::Landing;
        FFlightControlInput Input;
        const FVector2D ToSteer(Guide.Position - P), ToEnd(Guide.GuidePoint - P);
        FVector2D Desired;
        float VerticalSpeed;
        if (bColumn)
        {
            Desired = (ToEnd * 1.2F).GetClampedToMaxSize(60.0F);
            VerticalSpeed = VerticalCommand(static_cast<float>(Guide.GuidePoint.Z - P.Z));
        }
        else
        {
            const float Cruise = Leg.Kind == EPhase4GuideKind::Corridor ? 450.0F : 1750.0F;
            float Allowed = FMath::Min(Cruise, 1.1F * static_cast<float>(ToEnd.Size()) + .6F * FMath::Min(Leg.MaxAcceptPlanarSpeedCm, 1800.0F));
            // Pass-through ends need no stop, but slow down before a corridor follows.
            const TArray<FPhase4GuidanceLeg>& AllLegs = Mission.GetGuidanceRoute();
            if (AllLegs.IsValidIndex(Guide.LegIndex + 1) && AllLegs[Guide.LegIndex + 1].Kind == EPhase4GuideKind::Corridor)
                Allowed = FMath::Min(Allowed, 1.1F * static_cast<float>(ToEnd.Size()) + 450.0F);
            const FVector2D Aim = ToEnd.Size() < Leg.LookaheadCm ? ToEnd : ToSteer;
            Desired = Aim.GetSafeNormal() * FMath::Min(Allowed, static_cast<float>(Aim.Size()) * 1.2F);
            VerticalSpeed = VerticalCommand(static_cast<float>(Guide.Position.Z - P.Z));
        }
        Input.Pitch = Desired.X / 1800.0F; Input.Roll = Desired.Y / 1800.0F;
        Input.Throttle = VerticalSpeed / (VerticalSpeed >= 0.0F ? 800.0F : 600.0F);
        if (Pawn->GetFlightModel().Phase == EFlightPhase::Grounded && Leg.Kind == EPhase4GuideKind::Takeoff) Input = FFlightControlInput(), Input.Throttle = 1.0F;
        if (Leg.Kind == EPhase4GuideKind::Landing)
        {
            Input.Pitch = FMath::Clamp(Input.Pitch, -.2F, .2F); Input.Roll = FMath::Clamp(Input.Roll, -.2F, .2F);
            if (ToEnd.Size() < 40.0F && V.Size2D() < 30.0F) Input.Throttle = -1.0F;
            if (Pawn->GetFlightModel().Phase == EFlightPhase::Grounded || Pawn->GetFlightModel().Phase == EFlightPhase::Landing) Input = FFlightControlInput(), Input.Throttle = Pawn->GetFlightModel().Phase == EFlightPhase::Landing ? -1.0F : 0.0F;
        }
        Pawn->SetSemanticInput(Input);
        Pawn->Tick(Dt);
        Time += Dt;
        float& Peak = PeakVertical.FindOrAdd(Leg.LegId);
        Peak = FMath::Max(Peak, FMath::Abs(static_cast<float>(Pawn->GetFlightModel().Velocity.Z)));
    }
    if (LastLeg >= 0) Timeline += FString::Printf(TEXT(" %s=%.1fs"), *Mission.GetGuidanceRoute()[LastLeg].LegId.ToString(), Time - LegStart);
    AddInfo(FString::Printf(TEXT("scripted traversal sim time %.1f s (mission elapsed %.1f s); stage=%d next=%d collisions=%d boundary=%d missed=%d incident=%d at (%s)"),
        Time, Mission.ElapsedSeconds, static_cast<int32>(Mission.Stage), Mission.NextCheckpointIndex, Mission.Collisions, Mission.BoundaryViolations,
        Mission.MissedCheckpoints, static_cast<int32>(Mission.Feedback.Incident), *Pawn->GetActorLocation().ToString()));
    AddInfo(TEXT("leg durations:") + Timeline);
    FString Peaks;
    for (const auto& Pair : PeakVertical) Peaks += FString::Printf(TEXT(" %s=%.0f"), *Pair.Key.ToString(), Pair.Value);
    AddInfo(TEXT("peak |vertical speed| cm/s:") + Peaks);
    TestEqual(TEXT("Scripted semantic-input traversal reaches Result"), Mission.Stage, EPhase4MissionStage::Result);
    TestEqual(TEXT("All five checkpoints scored"), Mission.NextCheckpointIndex, 5);
    TestEqual(TEXT("No real collision along the guided route"), Mission.Collisions, 0);
    TestEqual(TEXT("Never left the finite safety volume"), Mission.BoundaryViolations, 0);
    TestEqual(TEXT("No shortcut diagnostics"), Mission.MissedCheckpoints, 0);
    TestFalse(TEXT("Never touched water"), Pawn->HasWaterIncident());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAeroSimVerticalEnvelopeTest, "AeroSim.Flight.Navigation.VerticalEnvelopeSmoothAndSoft",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAeroSimVerticalEnvelopeTest::RunTest(const FString& Parameters)
{
    // Pure model (R5 user decision): climb ~8 m/s, descent ~6 m/s, bounded vertical acceleration.
    FFlightSurfaceContext Air; Air.bHasSupport = false;
    auto Hover = [] { FFlightModel M; M.Phase = EFlightPhase::Flight; M.Position = FVector(0, 0, 300); return M; };
    FFlightControlInput Up; Up.Throttle = 1; FFlightControlInput Down; Down.Throttle = -1; const FFlightControlInput Neutral;
    FFlightModel Climb = Hover(); float PeakAccel = 0, Last = 0;
    for (int32 I = 0; I < 120; ++I) { Climb.Advance(Up, 1.0F / 30.0F, Air); PeakAccel = FMath::Max(PeakAccel, FMath::Abs(static_cast<float>(Climb.Velocity.Z) - Last) * 30.0F); Last = Climb.Velocity.Z; }
    AddInfo(FString::Printf(TEXT("full climb after 4 s: %.0f cm/s, peak accel %.0f cm/s^2"), Climb.Velocity.Z, PeakAccel));
    TestTrue(TEXT("Climb reaches ~8 m/s and never exceeds it"), Climb.Velocity.Z > 760 && Climb.Velocity.Z <= 800.5);
    TestTrue(TEXT("Vertical acceleration is bounded (smooth ramp)"), PeakAccel <= 701.0F);
    FFlightModel Descend = Hover(); Descend.Position.Z = 10000;
    for (int32 I = 0; I < 120; ++I) Descend.Advance(Down, 1.0F / 30.0F, Air);
    AddInfo(FString::Printf(TEXT("full descent after 4 s: %.0f cm/s"), Descend.Velocity.Z));
    TestTrue(TEXT("Descent reaches ~6 m/s, slower than climb (vortex-ring margin)"), Descend.Velocity.Z < -560 && Descend.Velocity.Z >= -600.5);
    const double BrakeFrom = Descend.Position.Z;
    int32 Frames = 0;
    while (Descend.Velocity.Z < -100 && Frames < 300) { Descend.Advance(Neutral, 1.0F / 30.0F, Air); ++Frames; }
    AddInfo(FString::Printf(TEXT("release at 6 m/s: <=1 m/s after %.2f s and %.0f cm"), Frames / 30.0F, BrakeFrom - Descend.Position.Z));
    TestTrue(TEXT("Released descent brakes below 1 m/s within the 10 m guide brake zone"), BrakeFrom - Descend.Position.Z < 500.0);
    double MinVz = 0;
    for (int32 I = 0; I < 60; ++I) { Descend.Advance(Neutral, 1.0F / 30.0F, Air); MinVz = FMath::Min(MinVz, Descend.Velocity.Z); }
    TestTrue(TEXT("Braking does not overshoot into a climb"), Descend.Velocity.Z <= 0.5 && Descend.Velocity.Z > -50);
    // A short Ctrl tap near the ground is a gentle nudge, not a twitch.
    FFlightModel Tap = Hover();
    for (int32 I = 0; I < 3; ++I) Tap.Advance(Down, 1.0F / 30.0F, Air);
    float TapPeak = 0;
    for (int32 I = 0; I < 90; ++I) { TapPeak = FMath::Max(TapPeak, FMath::Abs(static_cast<float>(Tap.Velocity.Z))); Tap.Advance(Neutral, 1.0F / 30.0F, Air); }
    AddInfo(FString::Printf(TEXT("0.1 s Ctrl tap: peak %.0f cm/s, total drop %.1f cm"), TapPeak, 300.0 - Tap.Position.Z));
    TestTrue(TEXT("0.1 s tap moves less than 50 cm"), 300.0 - Tap.Position.Z < 50.0 && TapPeak < 100.0F);
    // Supported landing stays soft: Landing phase descends at its fixed 2.5 m/s, not the 6 m/s envelope.
    FFlightModel Land = Hover(); Land.Position.Z = 200;
    FFlightSurfaceContext Ground; Ground.bHasSupport = true; Ground.SupportedModelZCm = 0;
    float Touchdown = 0; bool bImpact = false;
    for (int32 I = 0; I < 200 && Land.Phase != EFlightPhase::Grounded; ++I)
    { Touchdown = Land.Velocity.Z; Land.Advance(Down, 1.0F / 30.0F, Ground); bImpact |= Land.bSupportImpact; }
    AddInfo(FString::Printf(TEXT("full-Ctrl landing from 2 m: phase %d, last vz %.0f cm/s, impact %d"), static_cast<int32>(Land.Phase), Touchdown, bImpact));
    TestEqual(TEXT("Full-Ctrl descent onto support becomes a soft Landing, then Grounded"), Land.Phase, EFlightPhase::Grounded);
    TestFalse(TEXT("No unsafe support impact"), bImpact);
    TestTrue(TEXT("Touchdown at the soft landing rate"), Touchdown >= -260.0F);
    return true;
}

#endif
