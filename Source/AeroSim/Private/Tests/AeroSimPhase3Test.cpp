#if WITH_DEV_AUTOMATION_TESTS

#include "AeroSimRuntimeContract.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "FlightPawn.h"
#include "GameFramework/SpringArmComponent.h"
#include "Misc/AutomationTest.h"
#include "Phase3Aircraft.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAeroSimPhase3DefinitionTest,
    "AeroSim.Phase3.Aircraft.DefinitionFailClosed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAeroSimPhase3DefinitionTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FAircraftDefinition Definition = FAircraftDefinition::MakeGenericGolden();
    FString Reason;
    TestTrue(TEXT("Generic definition is structurally production-usable"), Definition.IsStructurallyValid(&Reason));
    TestEqual(TEXT("Generic remains explicitly blocked"), Definition.AssetStatus, EAircraftAssetStatus::BlockedAsset);
    TestEqual(TEXT("Generic uses programmatic source"), Definition.VisualSource, EAircraftVisualSource::ProgrammaticEngineShapes);
    TestFalse(TEXT("Generic cannot enter formal task"), Definition.CanEnterFormalTask(&Reason));
    TestTrue(TEXT("Formal-task rejection is diagnosable"), !Reason.IsEmpty());
    TestEqual(TEXT("Five explicit material slots"), Definition.MaterialSlots.Num(), 5);
    TestEqual(TEXT("Two explicit LOD contracts"), Definition.Lods.Num(), 2);
    TestTrue(TEXT("Runtime contract is phase3/v1"), FAeroSimRuntimeContract::IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAeroSimPhase3GimbalTest,
    "AeroSim.Phase3.Camera.GimbalLimitsSmoothingHorizon",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAeroSimPhase3GimbalTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FGimbalController Gimbal;
    Gimbal.SetTarget(-200.0F, 300.0F);
    TestEqual(TEXT("Pitch target clamps"), Gimbal.TargetPitchDegrees, FGimbalController::MinimumPitchDegrees);
    TestEqual(TEXT("Yaw target clamps"), Gimbal.TargetYawDegrees, FGimbalController::MaximumYawDegrees);
    const float InitialPitch = Gimbal.PitchDegrees;
    Gimbal.Advance(0.016F);
    TestTrue(TEXT("Pitch smooths rather than snaps"), Gimbal.PitchDegrees < InitialPitch && Gimbal.PitchDegrees > Gimbal.TargetPitchDegrees);
    TestTrue(TEXT("Stabilized rotation has zero roll"), FMath::IsNearlyZero(Gimbal.GetStabilizedRelativeRotation().Roll));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAeroSimPhase3CameraVisibilityTest,
    "AeroSim.Phase3.Camera.VisibilityCollisionLodFov",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAeroSimPhase3CameraVisibilityTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    AFlightPawn* Pawn = World != nullptr ? World->SpawnActor<AFlightPawn>() : nullptr;
    TestNotNull(TEXT("Pawn spawns"), Pawn);
    if (Pawn != nullptr)
    {
        TestTrue(TEXT("Detailed aircraft visible in near follow"), Pawn->IsAircraftShellVisible());
        TestTrue(TEXT("Consumer quad has substantial assembly"), Pawn->GetDetailedVisualPartCount() >= 24);
        TestEqual(TEXT("Material-slot contract is exposed"), Pawn->GetMaterialSlotCount(), 5);
        Pawn->SetUseLodProxy(true);
        TestTrue(TEXT("LOD proxy can replace detailed assembly"), Pawn->IsLodProxyVisible());
        TestFalse(TEXT("LOD proxy prevents double rendering"), Pawn->IsAircraftShellVisible());
        Pawn->SetUseLodProxy(false);
        Pawn->CycleCamera();
        TestEqual(TEXT("Near to FPV"), Pawn->GetCameraMode(), EFlightCameraMode::FPV);
        TestFalse(TEXT("FPV hides aircraft"), Pawn->IsAircraftShellVisible());
        TestFalse(TEXT("FPV disables trailing collision"), Pawn->GetFollowSpringArm()->bDoCollisionTest);
        Pawn->CycleCamera();
        TestEqual(TEXT("FPV to gimbal"), Pawn->GetCameraMode(), EFlightCameraMode::Gimbal);
        TestFalse(TEXT("Gimbal hides aircraft"), Pawn->IsAircraftShellVisible());
        TestTrue(TEXT("Gimbal horizon uses absolute rotation"), Pawn->GetFollowSpringArm()->IsUsingAbsoluteRotation());
        TestTrue(TEXT("vFOV transaction reused"), Pawn->PreviewVerticalFov(72.0F));
        TestTrue(TEXT("Apply vFOV"), Pawn->ApplyVerticalFov());
        TestEqual(TEXT("Applied vFOV"), Pawn->GetVerticalFov(), 72.0F);
        Pawn->CycleCamera();
        TestEqual(TEXT("Gimbal to far follow"), Pawn->GetCameraMode(), EFlightCameraMode::FarFollow);
        TestTrue(TEXT("Far follow collision enabled"), Pawn->GetFollowSpringArm()->bDoCollisionTest);
    }
    if (World != nullptr) World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAeroSimPhase3ScaleSupportTest,
    "AeroSim.Phase3.Aircraft.ScaleCenterSupport",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAeroSimPhase3ScaleSupportTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    const FAircraftDefinition Definition = FAircraftDefinition::MakeGenericGolden();
    TestTrue(TEXT("Consumer aircraft is under 60cm long"), Definition.DimensionsCm.X < 60.0F);
    TestTrue(TEXT("Consumer aircraft is over 30cm long"), Definition.DimensionsCm.X > 30.0F);
    TestTrue(TEXT("Center of mass is inside body height"), FMath::Abs(Definition.CenterOfMassCm.Z) < Definition.DimensionsCm.Z * 0.5F);
    TestTrue(TEXT("Support is below center of mass"), Definition.GroundSupportZCm < Definition.CenterOfMassCm.Z);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAeroSimPhase3EvidenceCameraTest,
    "AeroSim.Phase3.Evidence.FixedCameraContracts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAeroSimPhase3EvidenceCameraTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    const TArray<FPhase3EvidenceCameraContract> Cameras = FPhase3EvidenceCameraContract::FixedSet();
    TestEqual(TEXT("Six fixed Phase 3 evidence cameras"), Cameras.Num(), 6);
    TSet<FName> Ids;
    for (const FPhase3EvidenceCameraContract& Camera : Cameras)
    {
        TestTrue(*FString::Printf(TEXT("Camera %s valid"), *Camera.Id.ToString()), Camera.IsValid());
        Ids.Add(Camera.Id);
    }
    TestEqual(TEXT("Evidence camera IDs are unique"), Ids.Num(), Cameras.Num());
    return true;
}

#endif
