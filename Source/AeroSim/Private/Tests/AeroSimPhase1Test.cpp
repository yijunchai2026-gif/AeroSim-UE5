#if WITH_DEV_AUTOMATION_TESTS

#include "AeroSimGameMode.h"
#include "AeroSimRuntimeContract.h"
#include "Engine/World.h"
#include "FlightHUD.h"
#include "FlightPawn.h"
#include "Misc/AutomationTest.h"
#include "PacificCoastGraybox.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAeroSimPhase1StateMachineTest,
    "AeroSim.Phase1.Flight.StateMachine",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAeroSimPhase1StateMachineTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FFlightModel Model;
    FFlightControlInput Input;
    TestEqual(TEXT("Initial state is grounded"), Model.Phase, EFlightPhase::Grounded);

    Input.Throttle = 1.0F;
    Model.Advance(Input, 0.05F);
    TestEqual(TEXT("Positive throttle starts takeoff"), Model.Phase, EFlightPhase::Takeoff);
    for (int32 Step = 0; Step < 200 && Model.Phase != EFlightPhase::Flight; ++Step)
    {
        Model.Advance(Input, 0.05F);
    }
    TestEqual(TEXT("Takeoff reaches flight"), Model.Phase, EFlightPhase::Flight);
    TestTrue(TEXT("Flight has positive altitude"), Model.Position.Z > 0.0F);
    TestTrue(TEXT("Flight has inertia"), Model.Velocity.SizeSquared() > 0.0F);

    Input.Throttle = -1.0F;
    for (int32 Step = 0; Step < 200 && Model.Phase != EFlightPhase::Grounded; ++Step)
    {
        Model.Advance(Input, 0.05F);
    }
    TestEqual(TEXT("Landing returns to grounded"), Model.Phase, EFlightPhase::Grounded);
    TestTrue(TEXT("Landing clamps ground altitude"), FMath::IsNearlyZero(Model.Position.Z));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAeroSimPhase1InputSafetyTest,
    "AeroSim.Phase1.Input.NeutralizeAndPause",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAeroSimPhase1InputSafetyTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    AFlightPawn* Pawn = World != nullptr ? World->SpawnActor<AFlightPawn>() : nullptr;
    TestNotNull(TEXT("Test pawn spawned"), Pawn);
    if (Pawn != nullptr)
    {
        FFlightControlInput Input;
        Input.Pitch = 2.0F;
        Input.Roll = -2.0F;
        Input.Yaw = 0.5F;
        Input.Throttle = 1.0F;
        Pawn->SetSemanticInput(Input);
        TestEqual(TEXT("Semantic pitch clamps"), Pawn->GetControlInput().Pitch, 1.0F);
        TestEqual(TEXT("Semantic roll clamps"), Pawn->GetControlInput().Roll, -1.0F);

        Pawn->SetSafePaused(true);
        TestEqual(TEXT("Safe pause enters paused state"), Pawn->GetFlightModel().Phase, EFlightPhase::Paused);
        TestTrue(TEXT("Safe pause neutralizes every axis"), Pawn->GetControlInput().IsNeutral());

        Pawn->SetSemanticInput(Input);
        TestTrue(TEXT("Paused input remains neutral"), Pawn->GetControlInput().IsNeutral());
        Pawn->SetSafePaused(false);
        TestTrue(TEXT("Resume starts neutral"), Pawn->GetControlInput().IsNeutral());
    }
    if (World != nullptr)
    {
        World->DestroyWorld(false);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAeroSimPhase1CameraResetHudTest,
    "AeroSim.Phase1.Runtime.CameraResetHud",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAeroSimPhase1CameraResetHudTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    TestTrue(TEXT("Runtime contract is valid"), FAeroSimRuntimeContract::IsValid());

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    AFlightPawn* Pawn = World != nullptr ? World->SpawnActor<AFlightPawn>() : nullptr;
    TestNotNull(TEXT("Test pawn spawned"), Pawn);
    if (Pawn != nullptr)
    {
        TestEqual(TEXT("Default camera is chase"), Pawn->GetCameraMode(), EFlightCameraMode::Chase);
        TestTrue(TEXT("Chase camera shows aircraft shell"), Pawn->IsAircraftShellVisible());
        Pawn->CycleCamera();
        TestEqual(TEXT("Camera cycles to FPV"), Pawn->GetCameraMode(), EFlightCameraMode::FPV);
        TestFalse(TEXT("FPV hides aircraft shell"), Pawn->IsAircraftShellVisible());
        Pawn->CycleCamera();
        TestEqual(TEXT("Camera cycles to gimbal"), Pawn->GetCameraMode(), EFlightCameraMode::Gimbal);
        TestFalse(TEXT("Gimbal hides aircraft shell"), Pawn->IsAircraftShellVisible());
        Pawn->CycleCamera();
        TestEqual(TEXT("Camera cycles to far follow"), Pawn->GetCameraMode(), EFlightCameraMode::FarFollow);
        TestTrue(TEXT("Far follow shows aircraft shell"), Pawn->IsAircraftShellVisible());
        Pawn->CycleCamera();
        TestEqual(TEXT("Camera cycles back to chase"), Pawn->GetCameraMode(), EFlightCameraMode::Chase);
        TestTrue(TEXT("Returning to chase shows aircraft shell"), Pawn->IsAircraftShellVisible());

        FFlightControlInput Input;
        Input.Throttle = 1.0F;
        Pawn->SetSemanticInput(Input);
        Pawn->Tick(0.05F);
        TestEqual(TEXT("Pawn leaves grounded state"), Pawn->GetFlightModel().Phase, EFlightPhase::Takeoff);
        Pawn->ResetFlight();
        TestEqual(TEXT("Reset returns grounded"), Pawn->GetFlightModel().Phase, EFlightPhase::Grounded);
        TestTrue(TEXT("Reset neutralizes input"), Pawn->GetControlInput().IsNeutral());
        TestTrue(TEXT("Reset clears relative position"), Pawn->GetFlightModel().Position.IsNearlyZero());

        const FFlightHudData Hud = Pawn->GetHudData();
        TestEqual(TEXT("HUD exposes Build ID"), Hud.BuildId, FString(FAeroSimRuntimeContract::BuildId));
        TestEqual(TEXT("HUD exposes camera"), Hud.CameraMode, EFlightCameraMode::Chase);
        TestTrue(TEXT("HUD exposes neutral state"), Hud.bInputNeutral);
    }
    if (World != nullptr)
    {
        World->DestroyWorld(false);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAeroSimPhase1PureFunctionsTest,
    "AeroSim.Phase1.PureFunctions.BoundsAndReset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAeroSimPhase1PureFunctionsTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FFlightControlInput Input;
    Input.Pitch = 4.0F;
    Input.Roll = -4.0F;
    Input.Yaw = 2.0F;
    Input.Throttle = -2.0F;
    Input.Clamp();
    TestEqual(TEXT("Pitch upper bound"), Input.Pitch, 1.0F);
    TestEqual(TEXT("Roll lower bound"), Input.Roll, -1.0F);
    TestEqual(TEXT("Yaw upper bound"), Input.Yaw, 1.0F);
    TestEqual(TEXT("Throttle lower bound"), Input.Throttle, -1.0F);
    Input.Neutralize();
    TestTrue(TEXT("Neutralize is complete"), Input.IsNeutral());

    FFlightModel Model;
    Model.Pause();
    const FVector Before = Model.Position;
    Model.Advance(FFlightControlInput(), 1.0F);
    TestTrue(TEXT("Paused model does not move"), Model.Position.Equals(Before));
    Model.Reset();
    TestEqual(TEXT("Reset phase"), Model.Phase, EFlightPhase::Grounded);
    TestTrue(TEXT("Reset velocity"), Model.Velocity.IsNearlyZero());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAeroSimPhase1GrayboxContractTest,
    "AeroSim.Phase1.Graybox.GeometryContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAeroSimPhase1GrayboxContractTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    TestEqual(TEXT("Training area is 1 km"), FPacificCoastGrayboxContract::WorldSizeCm, 100000.0F);
    TestEqual(TEXT("Five route gates are defined"), FPacificCoastGrayboxContract::GateCount, 5);
    TestTrue(TEXT("Route and bounds contract is valid"), FPacificCoastGrayboxContract::IsGeometryValid());

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    APacificCoastGraybox* Graybox = World != nullptr ? World->SpawnActor<APacificCoastGraybox>() : nullptr;
    TestNotNull(TEXT("Programmatic graybox spawns"), Graybox);
    if (Graybox != nullptr)
    {
        TestEqual(TEXT("All gates generated"), Graybox->GetGeneratedGateCount(), FPacificCoastGrayboxContract::GateCount);
        TestEqual(TEXT("All route direction markers generated"), Graybox->GetGeneratedRouteArrowCount(), FPacificCoastGrayboxContract::RouteArrowCount);
        TestEqual(TEXT("Four standard scale references generated"), Graybox->GetGeneratedScaleReferenceCount(), 4);
        TestTrue(TEXT("Tiled terrain, supports, route and landmarks generated"), Graybox->GetGeneratedPrimitiveCount() >= 150);
    }
    if (World != nullptr)
    {
        World->DestroyWorld(false);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAeroSimPhase1RouteSmokeTest,
    "AeroSim.Phase1.Graybox.RouteSmoke",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAeroSimPhase1RouteSmokeTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPacificCoastRouteProgress Progress;
    const TArray<FVector>& Points = FPacificCoastGrayboxContract::GetRoutePoints();
    Progress.Observe(EFlightPhase::Grounded, Points[0]);
    TestFalse(TEXT("Grounded start is not complete"), Progress.IsComplete());
    Progress.Observe(EFlightPhase::Takeoff, Points[0]);
    TestTrue(TEXT("Takeoff is observed"), Progress.bTookOff);
    for (int32 Index = 1; Index < Points.Num() - 1; ++Index)
    {
        Progress.Observe(EFlightPhase::Flight, Points[Index]);
    }
    TestFalse(TEXT("Passing gates still requires return and landing"), Progress.IsComplete());
    Progress.Observe(EFlightPhase::Grounded, Points.Last());
    TestTrue(TEXT("Takeoff, all gates and return complete the route"), Progress.IsComplete());

    const AAeroSimGameMode* GameMode = GetDefault<AAeroSimGameMode>();
    TestTrue(TEXT("Visible Canvas HUD is configured"),
        GameMode != nullptr && GameMode->HUDClass == AFlightHUD::StaticClass());
    return true;
}

#endif
