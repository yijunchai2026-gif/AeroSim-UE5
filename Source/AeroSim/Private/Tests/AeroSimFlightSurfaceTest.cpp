#if WITH_DEV_AUTOMATION_TESTS
#include "FlightPawn.h"
#include "Phase4Mission.h"
#include "PacificCoastGraybox.h"
#include "PacificCoastFlora.h"
#include "Misc/AutomationTest.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlightNoInvisibleFloorTest,"AeroSim.Flight.Surface.UnsupportedDescentAndGoAround",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFlightNoInvisibleFloorTest::RunTest(const FString& Parameters)
{
    FFlightModel Model;
    Model.Phase=EFlightPhase::Flight;
    Model.Position=FVector(20000,0,400);
    FFlightSurfaceContext Air;
    Air.bHasSupport=false;
    FFlightControlInput Down;
    Down.Throttle=-1;
    for(int32 Index=0;Index<350;++Index) Model.Advance(Down,.05F,Air);
    TestEqual(TEXT("Unsupported descent remains Flight rather than synthetic Landing/Grounded"),Model.Phase,EFlightPhase::Flight);
    TestTrue(TEXT("Semantic throttle descends below the launch plane past low-water altitude"),Model.Position.Z < -6100);
    const double Below=Model.Position.Z;
    Model.Pause();
    Model.Advance(Down,.1F,Air);
    TestEqual(TEXT("Negative altitude does not move during pause"),Model.Position.Z,Below);
    Model.Resume();
    FFlightControlInput Up;
    Up.Throttle=1;
    for(int32 Index=0;Index<80;++Index) Model.Advance(Up,.05F,Air);
    TestTrue(TEXT("Positive throttle climbs from negative launch-relative altitude"),Model.Position.Z > Below);

    FFlightSurfaceContext Ground;
    Ground.SupportedModelZCm=-6200;
    Model.Position=FVector(0,0,-6170);
    Model.Phase=EFlightPhase::Flight;
    Model.Velocity=FVector(0,0,-100);
    Model.Advance(Down,.05F,Ground);
    TestEqual(TEXT("Real nearby support permits landing"),Model.Phase,EFlightPhase::Landing);
    Model.Advance(Up,.05F,Ground);
    TestEqual(TEXT("Positive throttle aborts supported landing"),Model.Phase,EFlightPhase::Flight);
    Model.Phase=EFlightPhase::Landing;
    Model.Position.Z=-6170;
    Model.Advance(Down,.05F,Air);
    TestEqual(TEXT("Lost support cancels landing without snapping to launch altitude"),Model.Phase,EFlightPhase::Flight);
    TestTrue(TEXT("Lost support retains negative altitude"),Model.Position.Z<0);
    Model.Phase=EFlightPhase::Flight;
    Model.Position=FVector(0,0,1);Model.Velocity=FVector(1000,0,-50);
    FFlightControlInput Skid;Skid.Throttle=-.1F;
    FFlightSurfaceContext Flat;
    Model.Advance(Skid,.1F,Flat);
    TestTrue(TEXT("Fast lateral skid with slow descent reports unsafe support contact"),Model.bSupportImpact);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastFlightRouteSafetyTest,"AeroSim.Flight.Surface.WorldRouteSeaAndCorridorSafety",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoastFlightRouteSafetyTest::RunTest(const FString& Parameters)
{
    const FPhase4TaskDefinition Task=FPhase4TaskDefinition::MakePacificCoastV1();
    TestTrue(TEXT("Revised task is valid"),Task.IsValid());
    TestEqual(TEXT("Revised world route has a distinct score/replay version"),Task.Version,2);
    auto Legacy=Task;Legacy.Version=1;
    TestFalse(TEXT("Legacy inland mission cannot masquerade as revised coastal definition"),Legacy.IsValid());
    TestTrue(TEXT("World route includes actual supported home and negative sea targets"),FPacificCoastGrayboxContract::IsGeometryValid());
    const FPhase4Checkpoint* Water=Task.Checkpoints.FindByPredicate([](const auto& P){return P.Label==TEXT("LOW WATER");});
    if(TestNotNull(TEXT("Low water objective is retained"),Water))
    {
        TestEqual(TEXT("Water target is exactly four metres above real sea"),Water->Position.Z,static_cast<double>(APacificCoastGraybox::SeaLevelCm+400));
        TestTrue(TEXT("Water objective is outside the actual cliff foot"),Water->Position.X>APacificCoastGraybox::GetCoastalShoreX(Water->Position.Y)+300);
        TestTrue(TEXT("Negative world height water target is legal"),Task.IsInsideSafetyVolume(Water->Position));
        TestFalse(TEXT("A submerged target is not legal"),Task.IsInsideSafetyVolume(FVector(Water->Position.X,Water->Position.Y,APacificCoastGraybox::SeaLevelCm-100)));
        TestTrue(TEXT("Recovery from water stays inside safe route"),Task.IsInsideSafetyVolume(Task.GetRecoveryPoint(Water->Position+FVector(15000,0,-2000))));
    }
    // Full corridor safety, not just the short first-slice objective segment.
    for(const auto& Sample:CoastFlora::GetLayout().Corridor)
    {
        const FVector P(Sample.Point.X,Sample.Point.Y,APacificCoastGraybox::GetCoastalLandHeight(Sample.Point.X,Sample.Point.Y)+250);
        if(!TestTrue(TEXT("Every existing full-corridor centreline is inside bounded safe route"),Task.IsInsideSafetyVolume(P)))break;
    }
    TestFalse(TEXT("Distant world does not become safe with the sea change"),Task.IsInsideSafetyVolume(FVector(90000,-90000,400)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlightMissionLandingProofTest,"AeroSim.Flight.Surface.HomeNeedsActualSupportAndResolvedPose",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFlightMissionLandingProofTest::RunTest(const FString& Parameters)
{
    FPhase4MissionSession Mission;
    Mission.Reset();Mission.EnterPreflight();Mission.SetPreflightComplete(true);Mission.BeginMission();
    Mission.Observe(.1F,EFlightPhase::Takeoff,FVector(55,79,400),FVector(0,0,100),FRotator::ZeroRotator);
    for(const auto& Point:Mission.Task.Checkpoints)Mission.Observe(.1F,EFlightPhase::Flight,Point.Position,FVector(500,0,0),FRotator::ZeroRotator);
    TestEqual(TEXT("All actual objectives still require return"),Mission.Stage,EPhase4MissionStage::ReturnToHome);
    Mission.Observe(.1F,EFlightPhase::Flight,FVector(55,79,500),FVector(100,0,0),FRotator::ZeroRotator);
    Mission.Observe(.1F,EFlightPhase::Grounded,FVector(55,79,11),FVector::ZeroVector,FRotator::ZeroRotator,false);
    TestEqual(TEXT("Unsupported Grounded flag cannot complete"),Mission.Stage,EPhase4MissionStage::Landing);
    Mission.Observe(.1F,EFlightPhase::Grounded,FVector(500,79,11),FVector::ZeroVector,FRotator::ZeroRotator,true);
    TestEqual(TEXT("Old eighteen-metre home radius cannot count field landing"),Mission.Stage,EPhase4MissionStage::Landing);
    Mission.Observe(.1F,EFlightPhase::Grounded,FVector(55,79,11),FVector(100,0,0),FRotator::ZeroRotator,true);
    TestEqual(TEXT("Fast touchdown cannot complete"),Mission.Stage,EPhase4MissionStage::Landing);
    Mission.ReportWaterContact(FVector(20000,0,-6500));
    TestEqual(TEXT("Water is an unsafe incident, not Grounded or Result"),Mission.Feedback.Incident,EPhase4Incident::UnsafeLanding);
    TestFalse(TEXT("Water never completes the task"),Mission.Score.bComplete);

    FFlightModel Model;
    Model.Position=FVector(500,0,500);Model.Velocity=FVector(900,0,0);
    Model.ResolveMovement(FVector(80,0,500),FVector::ZeroVector);
    TestTrue(TEXT("Resolved wall stop replaces requested model position"),Model.Position.Equals(FVector(80,0,500)));
    TestTrue(TEXT("Resolved blocked velocity stops"),Model.Velocity.IsNearlyZero());
    // This logic fixture is not a claim that an actual Pawn sweep or a human route has run.
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlightWorldSweepProofTest,"AeroSim.Flight.Surface.ActualPawnSweepAndSupport",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFlightWorldSweepProofTest::RunTest(const FString& Parameters)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    if(!TestNotNull(TEXT("Collision fixture world"),World)) return false;
    auto Box=[World](const FVector& Centre,const FVector& Extent)
    {
        AActor* Actor=World->SpawnActor<AActor>();
        UBoxComponent* Component=NewObject<UBoxComponent>(Actor);
        Actor->SetRootComponent(Component);
        Component->SetBoxExtent(Extent);
        Component->SetCollisionObjectType(ECC_WorldStatic);
        Component->SetCollisionResponseToAllChannels(ECR_Block);
        Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Component->RegisterComponent();
        Actor->SetActorLocation(Centre);
        return Actor;
    };
    // Actual query shapes, not a mocked FHitResult or requested model destination.
    Box(FVector(1000,2000,290),FVector(1500,1500,10));
    Box(FVector(1300,2000,800),FVector(10,500,500));
    AFlightPawn* Pawn=World->SpawnActor<AFlightPawn>();
    if(TestNotNull(TEXT("Real flight pawn"),Pawn))
    {
        TestTrue(TEXT("Nonzero world origin initializes supported feet"),Pawn->InitializeGroundedSpawn(FVector(1000,2000,300)));
        Pawn->Tick(.05F);
        TestEqual(TEXT("Actual world-static floor keeps pawn grounded"),Pawn->GetFlightModel().Phase,EFlightPhase::Grounded);
        Pawn->ConfigureReviewFlightState(500,0); // Fixture setup only, never route evidence.
        FFlightControlInput Forward;Forward.Pitch=1;
        for(int32 I=0;I<30;++I){Pawn->SetSemanticInput(Forward);Pawn->Tick(.05F);}
        TestTrue(TEXT("Actual sweep cannot pass through wall"),Pawn->GetActorLocation().X<1290);
        TestTrue(TEXT("Wall was reached; test did not pass by staying still"),Pawn->GetActorLocation().X>1200);
        TestTrue(TEXT("Model position equals actual swept actor at nonzero origin"),Pawn->ModelToWorldPosition(Pawn->GetFlightModel().Position).Equals(Pawn->GetActorLocation(),.01));
        TestTrue(TEXT("Blocked velocity is zero"),Pawn->GetFlightModel().Velocity.IsNearlyZero());
        const auto& Samples=Pawn->GetFlightTrace().Samples;
        TestTrue(TEXT("Trace was recorded"),Samples.Num()>1);
        for(const auto& Sample:Samples)
            if(!TestTrue(TEXT("Recorded trace never reaches behind wall"),Pawn->ModelToWorldPosition(Sample.Position).X<1290))break;
        Pawn->NeutralizeInput();
        const FVector Stopped=Pawn->GetActorLocation();
        Pawn->Tick(.1F);
        TestTrue(TEXT("Neutral input holds collision-resolved pose"),Pawn->GetActorLocation().Equals(Stopped,.01));
    }
    AFlightPawn* WaterPawn=World->SpawnActor<AFlightPawn>();
    if(TestNotNull(TEXT("Actual water contact pawn"),WaterPawn))
    {
        WaterPawn->ConfigureReviewView(FTransform(FRotator::ZeroRotator,FVector(40000,0,-6000)),EFlightCameraMode::Chase);
        WaterPawn->ConfigureReviewFlightState(0,0);
        FFlightControlInput Down;Down.Throttle=-1;
        for(int32 I=0;I<40 && WaterPawn->GetFlightModel().Phase!=EFlightPhase::Paused;++I)
        { WaterPawn->SetSemanticInput(Down);WaterPawn->Tick(.1F); }
        TestEqual(TEXT("Actual descending pawn safe pauses on sea contact"),WaterPawn->GetFlightModel().Phase,EFlightPhase::Paused);
        TestTrue(TEXT("Water contact is explicitly latched in HUD"),WaterPawn->GetHudData().bWaterContact);
        TestTrue(TEXT("Water clears every semantic control"),WaterPawn->GetControlInput().IsNeutral());
        TestTrue(TEXT("Water contact model retains negative launch-relative altitude"),WaterPawn->GetFlightModel().Position.Z<0);
        WaterPawn->SetSafePaused(false);
        TestEqual(TEXT("Water incident cannot resume into underwater flight"),WaterPawn->GetFlightModel().Phase,EFlightPhase::Paused);
    }
    World->DestroyWorld(false);
    return true;
}

#endif
