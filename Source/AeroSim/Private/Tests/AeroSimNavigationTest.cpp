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
        AddInfo(FString::Printf(TEXT("%s guide=%s kind=%d score=%s len=%.0fcm end=(%.0f,%.0f,%.0f) R=%.0f H=-%.0f/+%.0f Vacc<=%.0f track<=%.0f@%.0f"),
            *Leg.LegId.ToString(), *Leg.GuideId.ToString(), static_cast<int32>(Leg.Kind), *Leg.ScoredCheckpointId.ToString(), Leg.GetLength(),
            Leg.Path.Last().X, Leg.Path.Last().Y, Leg.Path.Last().Z, Leg.AcceptRadiusCm, Leg.AcceptBelowCm, Leg.AcceptAboveCm,
            Leg.MaxAcceptPlanarSpeedCm, Leg.TrackToleranceCm, Leg.MaxTrackPlanarSpeedCm));
    }
    TestEqual(TEXT("Five scored legs; intermediate guides never score"), Scored, 5);
    const FPhase4GuidanceLeg& Descent = Route[LegIndexOf(Mission, TEXT("nav-05-offshore-descent"))];
    TestTrue(TEXT("Descent column locks XY at C2"), FVector::Dist2D(Descent.Path[0], Mission.Task.Checkpoints[1].Position) < 1.0);
    TestTrue(TEXT("Descent column is slow (<=100 cm/s)"), Descent.MaxTrackPlanarSpeedCm <= 100.0F && Descent.TrackToleranceCm <= 100.0F);
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
            VerticalSpeed = FMath::Clamp(static_cast<float>(Guide.GuidePoint.Z - P.Z) * 1.2F, -380.0F, 380.0F);
        }
        else
        {
            const float Cruise = Leg.Kind == EPhase4GuideKind::Corridor ? 450.0F : 1750.0F;
            const float Allowed = FMath::Min(Cruise, 1.1F * static_cast<float>(ToEnd.Size()) + .6F * Leg.MaxAcceptPlanarSpeedCm);
            const FVector2D Aim = ToEnd.Size() < Leg.LookaheadCm ? ToEnd : ToSteer;
            Desired = Aim.GetSafeNormal() * FMath::Min(Allowed, static_cast<float>(Aim.Size()) * 1.2F);
            VerticalSpeed = FMath::Clamp(static_cast<float>(Guide.Position.Z - P.Z) * 1.2F, -380.0F, 380.0F);
        }
        Input.Pitch = Desired.X / 1800.0F; Input.Roll = Desired.Y / 1800.0F; Input.Throttle = VerticalSpeed / 400.0F;
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
    }
    if (LastLeg >= 0) Timeline += FString::Printf(TEXT(" %s=%.1fs"), *Mission.GetGuidanceRoute()[LastLeg].LegId.ToString(), Time - LegStart);
    AddInfo(FString::Printf(TEXT("scripted traversal sim time %.1f s (mission elapsed %.1f s); stage=%d next=%d collisions=%d boundary=%d missed=%d incident=%d at (%s)"),
        Time, Mission.ElapsedSeconds, static_cast<int32>(Mission.Stage), Mission.NextCheckpointIndex, Mission.Collisions, Mission.BoundaryViolations,
        Mission.MissedCheckpoints, static_cast<int32>(Mission.Feedback.Incident), *Pawn->GetActorLocation().ToString()));
    AddInfo(TEXT("leg durations:") + Timeline);
    TestEqual(TEXT("Scripted semantic-input traversal reaches Result"), Mission.Stage, EPhase4MissionStage::Result);
    TestEqual(TEXT("All five checkpoints scored"), Mission.NextCheckpointIndex, 5);
    TestEqual(TEXT("No real collision along the guided route"), Mission.Collisions, 0);
    TestEqual(TEXT("Never left the finite safety volume"), Mission.BoundaryViolations, 0);
    TestEqual(TEXT("No shortcut diagnostics"), Mission.MissedCheckpoints, 0);
    TestFalse(TEXT("Never touched water"), Pawn->HasWaterIncident());
    return true;
}

#endif
