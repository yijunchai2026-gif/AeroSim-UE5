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
    TestFalse(TEXT("Out of bounds is unsafe"), Task.IsInsideSafetyVolume(FVector(30000.0F, 0.0F, 700.0F)));
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
    for (const FPhase4Checkpoint& Checkpoint : Mission.Task.Checkpoints)
    {
        Mission.Observe(0.1F, EFlightPhase::Flight, Checkpoint.Position, FVector(900, 0, 0), FRotator::ZeroRotator);
    }
    TestEqual(TEXT("All gates trigger RTH"), Mission.Stage, EPhase4MissionStage::ReturnToHome);
    Mission.Observe(0.1F, EFlightPhase::Flight, FVector(0, 0, 500), FVector(300, 0, 0), FRotator::ZeroRotator);
    TestEqual(TEXT("Home triggers landing"), Mission.Stage, EPhase4MissionStage::Landing);
    Mission.Observe(0.1F, EFlightPhase::Grounded, FVector::ZeroVector, FVector::ZeroVector, FRotator::ZeroRotator);
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
    Mission.Observe(0.1F, EFlightPhase::Flight, FVector(30000, 0, 500), FVector::ZeroVector, FRotator::ZeroRotator);
    TestEqual(TEXT("Boundary incident recorded once"), Mission.BoundaryViolations, 1);
    TestTrue(TEXT("Boundary feedback is recoverable"), Mission.Feedback.bRecoverable);
    TestEqual(TEXT("Boundary has non-color shape cue"), Mission.Feedback.ShapeCue, FString(TEXT("INWARD CHEVRONS")));
    Mission.Observe(0.1F, EFlightPhase::Flight, FVector(-5000, 0, 500), FVector::ZeroVector, FRotator::ZeroRotator);
    TestEqual(TEXT("Recovery clears incident"), Mission.Feedback.Incident, EPhase4Incident::None);
    Mission.ReportCollision(FVector::ZeroVector);
    TestEqual(TEXT("Collision tracked"), Mission.Collisions, 1);
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
