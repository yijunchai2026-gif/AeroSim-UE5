#if WITH_DEV_AUTOMATION_TESTS

#include "AeroSimRuntimeContract.h"
#include "FlightPawn.h"
#include "Misc/AutomationTest.h"
#include "Phase4Mission.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhase4DefinitionTest, "AeroSim.Phase4.Mission.DefinitionVersionGoalsSafety", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPhase4DefinitionTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    const FPhase4TaskDefinition Task = FPhase4TaskDefinition::MakePacificCoastV1();
    TestTrue(TEXT("Versioned task is valid"), Task.IsValid());
    TestEqual(TEXT("Five ordered checkpoints"), Task.Checkpoints.Num(), 5);
    TestTrue(TEXT("Center is safe"), Task.IsInsideSafetyVolume(FVector(0.0F, 0.0F, 700.0F)));
    TestFalse(TEXT("Out of bounds is unsafe"), Task.IsInsideSafetyVolume(FVector(90000.0F, 0.0F, 700.0F)));
    TestTrue(TEXT("Recovery point is safe"), Task.IsInsideSafetyVolume(Task.GetRecoveryPoint(FVector(40000.0F, 20000.0F, 4000.0F))));
    TestTrue(TEXT("Runtime is phase4/v1"), FAeroSimRuntimeContract::IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhase4MissionLoopTest, "AeroSim.Phase4.Mission.TakeoffInspectReturnLandResult", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPhase4MissionLoopTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPhase4MissionSession Mission;
    Mission.Reset();
    Mission.EnterPreflight();
    Mission.SetPreflightComplete(true);
    TestTrue(TEXT("Complete preflight can start"), Mission.BeginMission());
    Mission.Observe(0.1F, EFlightPhase::Takeoff, FVector(0, 0, 300), FVector(0, 0, 300), FRotator::ZeroRotator);
    // State-machine fixture follows every guidance leg in 50 cm steps; this is not flown-route evidence.
    auto WalkLegsUntil = [&Mission](TFunctionRef<bool()> Done)
    {
        for (int32 Guard = 0; Guard < 64 && !Done(); ++Guard)
        {
            const int32 LegIndex = Mission.GetGuidanceLegIndex();
            const FPhase4GuidanceLeg Leg = Mission.GetGuidanceRoute()[LegIndex];
            for (float Along = 0.0F; Along <= Leg.GetLength() + 50.0F && Mission.GetGuidanceLegIndex() == LegIndex && !Done(); Along += 50.0F)
                Mission.Observe(0.05F, EFlightPhase::Flight, Leg.Evaluate(Along), FVector::ZeroVector, FRotator::ZeroRotator);
        }
    };
    WalkLegsUntil([&Mission] { return Mission.NextCheckpointIndex >= 5; });
    TestEqual(TEXT("All gates trigger RTH"), Mission.Stage, EPhase4MissionStage::ReturnToHome);
    Mission.Observe(0.1F, EFlightPhase::Flight, FVector(0, 0, 500), FVector(300, 0, 0), FRotator::ZeroRotator);
    TestEqual(TEXT("Arriving above home without the verified return legs does not start landing"), Mission.Stage, EPhase4MissionStage::ReturnToHome);
    WalkLegsUntil([&Mission] { return Mission.Stage == EPhase4MissionStage::Landing; });
    TestEqual(TEXT("Verified return guides lead to landing"), Mission.Stage, EPhase4MissionStage::Landing);
    // A model Grounded flag alone is no longer evidence of landing on the real mat.
    Mission.Observe(0.1F, EFlightPhase::Grounded, FVector(55,79,10.8682F), FVector::ZeroVector, FRotator::ZeroRotator, true);
    TestEqual(TEXT("Landing produces Result"), Mission.Stage, EPhase4MissionStage::Result);
    TestTrue(TEXT("Result is complete"), Mission.Score.bComplete);
    TestTrue(TEXT("Four dimensions contribute"), Mission.Score.Safety > 0 && Mission.Score.Precision > 0 && Mission.Score.Efficiency > 0 && Mission.Score.Smoothness > 0);
    TestTrue(TEXT("Aligned result opens Replay"), Mission.OpenReplay());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhase4RecoveryTest, "AeroSim.Phase4.Mission.RecoverBoundaryCollisionMissedGate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPhase4RecoveryTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPhase4MissionSession Mission;
    Mission.Reset(); Mission.EnterPreflight(); Mission.SetPreflightComplete(true); Mission.BeginMission();
    Mission.Observe(0.1F, EFlightPhase::Flight, FVector(-5000, 0, 500), FVector::ZeroVector, FRotator::ZeroRotator);
    Mission.Observe(0.1F, EFlightPhase::Flight, FVector(90000, 0, 500), FVector::ZeroVector, FRotator::ZeroRotator);
    TestEqual(TEXT("Boundary incident recorded once"), Mission.BoundaryViolations, 1);
    TestTrue(TEXT("Boundary feedback is recoverable"), Mission.Feedback.bRecoverable);
    TestEqual(TEXT("Boundary has non-color shape cue"), Mission.Feedback.ShapeCue, FString(TEXT("INWARD CHEVRONS")));
    // Recovery is the shared guidance output: an anchor on the verified current leg.
    TestTrue(TEXT("Boundary selects one shared recovery guide"), Mission.GetCurrentGuidanceTarget().bRecovery
        && Mission.Feedback.RecoveryPoint.Equals(Mission.GetCurrentGuidanceTarget().Position));
    Mission.Observe(0.1F, EFlightPhase::Flight, FVector(-5000, 0, 500), FVector::ZeroVector, FRotator::ZeroRotator);
    TestEqual(TEXT("Inside again but off the verified leg keeps recovering"), Mission.Feedback.Incident, EPhase4Incident::Boundary);
    Mission.Observe(0.1F, EFlightPhase::Flight, Mission.GetCurrentGuidanceTarget().Position, FVector::ZeroVector, FRotator::ZeroRotator);
    TestEqual(TEXT("Rejoining the verified leg clears incident"), Mission.Feedback.Incident, EPhase4Incident::None);
    Mission.ReportCollision(FVector::ZeroVector);
    TestEqual(TEXT("Collision tracked"), Mission.Collisions, 1);
    Mission.ReleaseCollisionContact(); // fixture: the actual contact has ended
    Mission.Observe(0.1F, EFlightPhase::Flight, Mission.Task.Checkpoints[2].Position, FVector::ZeroVector, FRotator::ZeroRotator);
    TestTrue(TEXT("Skipping forward gate reports miss"), Mission.MissedCheckpoints > 0);
    TestEqual(TEXT("Progress preserved at expected gate"), Mission.NextCheckpointIndex, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhase4ReplayScoreTest, "AeroSim.Phase4.Result.ScoreReplayDeterminism", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPhase4ReplayScoreTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPhase4MissionSession A; A.ConfigureEvidenceState(EPhase4MissionStage::Result);
    FPhase4MissionSession B; B.ConfigureEvidenceState(EPhase4MissionStage::Result);
    TestTrue(TEXT("Identical telemetry inputs yield identical score and digest"), A.Score.Equals(B.Score));
    const FPhase4Score Frozen = A.Score;
    TestTrue(TEXT("Replay transition validates digest"), A.OpenReplay());
    TestTrue(TEXT("Replay retains exact Result score"), A.Score.Equals(Frozen));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhase4AccessibilityTest, "AeroSim.Phase4.HUD.AccessibleCuesAndProgressiveHints", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPhase4AccessibilityTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPhase4MissionSession Mission; Mission.ConfigureEvidenceState(EPhase4MissionStage::Inspection);
    TestTrue(TEXT("Objective is textual"), !Mission.GetObjectiveText().IsEmpty());
    TestTrue(TEXT("Progress is textual"), Mission.GetProgressText().Contains(TEXT("CHECKPOINTS")));
    TestTrue(TEXT("Gate cue includes shape and number"), Mission.Feedback.ShapeCue.Contains(TEXT("3")));
    TestTrue(TEXT("Hint is progressive"), !Mission.GetProgressiveHint().IsEmpty());
    return true;
}

#endif
