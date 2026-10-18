#if WITH_DEV_AUTOMATION_TESTS

#include "AeroSimRuntimeContract.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "FlightHUD.h"
#include "FlightPawn.h"
#include "GameFramework/SpringArmComponent.h"
#include "Misc/AutomationTest.h"
#include "PacificCoastGraybox.h"
#include "ProceduralMeshComponent.h"
#include "Phase2Systems.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAeroSimPhase2WorkflowTest,
    "AeroSim.Phase2.Flow.HangarToReplay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAeroSimPhase2WorkflowTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FMissionFlow Flow;
    TestEqual(TEXT("Starts in Hangar"), Flow.State, EAeroSimMissionState::Hangar);
    TestFalse(TEXT("Hangar cannot skip to Flight"), Flow.Transition(EAeroSimMissionState::Flight));
    TestTrue(TEXT("Hangar to Preflight"), Flow.Advance());
    TestEqual(TEXT("Preflight state"), Flow.State, EAeroSimMissionState::Preflight);
    TestTrue(TEXT("Preflight to Flight"), Flow.Advance());
    TestTrue(TEXT("Flight to Result"), Flow.Advance());
    TestTrue(TEXT("Result to Replay"), Flow.Advance());
    TestEqual(TEXT("Replay state"), Flow.State, EAeroSimMissionState::Replay);
    TestTrue(TEXT("Runtime contract is current and valid"), FAeroSimRuntimeContract::IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAeroSimPhase2CameraFovTest,
    "AeroSim.Phase2.Camera.VerticalFovTransaction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAeroSimPhase2CameraFovTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FVerticalFovTransaction Fov;
    TestFalse(TEXT("Out of range preview fails closed"), Fov.Preview(120.0F));
    TestTrue(TEXT("Preview accepts valid vFOV"), Fov.Preview(72.0F));
    TestEqual(TEXT("Preview is visible before apply"), Fov.GetEffectiveVerticalFov(), 72.0F);
    Fov.Cancel();
    TestEqual(TEXT("Cancel restores applied value"), Fov.GetEffectiveVerticalFov(), 60.0F);
    TestTrue(TEXT("Second preview"), Fov.Preview(75.0F));
    TestTrue(TEXT("Apply commits preview"), Fov.Apply());
    TestEqual(TEXT("Applied vFOV"), Fov.AppliedVerticalFov, 75.0F);
    TestTrue(TEXT("Undo succeeds"), Fov.Undo());
    TestEqual(TEXT("Undo restores prior value"), Fov.AppliedVerticalFov, 60.0F);
    const float Horizontal = FVerticalFovTransaction::VerticalToHorizontal(60.0F, 16.0F / 9.0F);
    TestTrue(TEXT("Landscape horizontal FOV exceeds vertical"), Horizontal > 60.0F);
    TestTrue(TEXT("Aspect conversion round trips"), FMath::IsNearlyEqual(FVerticalFovTransaction::HorizontalToVertical(Horizontal, 16.0F / 9.0F), 60.0F, 0.001F));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAeroSimPhase2FollowCollisionTest,
    "AeroSim.Phase2.Camera.FollowCollision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAeroSimPhase2FollowCollisionTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    AFlightPawn* Pawn = World != nullptr ? World->SpawnActor<AFlightPawn>() : nullptr;
    TestNotNull(TEXT("Pawn spawns"), Pawn);
    if (Pawn != nullptr)
    {
        TestEqual(TEXT("Default is near follow"), Pawn->GetCameraMode(), EFlightCameraMode::Chase);
        TestTrue(TEXT("Near follow collision enabled"), Pawn->GetFollowSpringArm()->bDoCollisionTest);
        TestTrue(TEXT("Probe has nonzero safety radius"), Pawn->GetFollowSpringArm()->ProbeSize >= 20.0F);
        TestEqual(TEXT("Camera collision channel"), Pawn->GetFollowSpringArm()->ProbeChannel, ECC_Camera);
        Pawn->CycleCamera();
        TestEqual(TEXT("Second mode is nose"), Pawn->GetCameraMode(), EFlightCameraMode::FPV);
        TestFalse(TEXT("Nose has no trailing arm collision"), Pawn->GetFollowSpringArm()->bDoCollisionTest);
        Pawn->CycleCamera();
        TestEqual(TEXT("Third mode is gimbal"), Pawn->GetCameraMode(), EFlightCameraMode::Gimbal);
        TestFalse(TEXT("Gimbal has no trailing arm collision"), Pawn->GetFollowSpringArm()->bDoCollisionTest);
        Pawn->CycleCamera();
        TestEqual(TEXT("Fourth mode is far follow"), Pawn->GetCameraMode(), EFlightCameraMode::FarFollow);
        TestTrue(TEXT("Far follow collision enabled"), Pawn->GetFollowSpringArm()->bDoCollisionTest);
    }
    if (World != nullptr) World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAeroSimPhase2WeatherSyncTest,
    "AeroSim.Phase2.Environment.WeatherSynchronization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAeroSimPhase2WeatherSyncTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FEnvironmentState State;
    TestTrue(TEXT("Clear 16:30 seed baseline valid"), State.IsValid());
    TestEqual(TEXT("Baseline time"), State.TimeMinutes, 990);
    const float ClearLight = State.GetLightIntensity();
    State.SetWeather(EAeroSimWeather::Overcast);
    TestTrue(TEXT("Overcast increases clouds"), State.GetCloudCover() > 0.8F);
    TestTrue(TEXT("Overcast reduces key light"), State.GetLightIntensity() < ClearLight);
    const float OvercastFog = State.GetFogDensity();
    State.SetWeather(EAeroSimWeather::Rain);
    TestEqual(TEXT("Rain wetness synchronized"), State.GetWetness(), 1.0F);
    TestTrue(TEXT("Rain has visibly denser fog than overcast"), State.GetFogDensity() > OvercastFog * 2.0F);
    TestTrue(TEXT("Rain darkens exposure"), State.GetExposureBias() < -0.9F);
    TestTrue(TEXT("Rain desaturates scene"), State.GetSceneSaturation() < 0.5F);

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    APacificCoastGraybox* Coast = World != nullptr ? World->SpawnActor<APacificCoastGraybox>() : nullptr;
    TestNotNull(TEXT("Environment spawns"), Coast);
    if (Coast != nullptr)
    {
        Coast->ApplyEnvironmentState(State);
        TestEqual(TEXT("Visual actor receives same weather"), Coast->GetAppliedEnvironmentState().Weather, EAeroSimWeather::Rain);
        TestEqual(TEXT("Visual actor receives same seed"), Coast->GetAppliedEnvironmentState().Seed, State.Seed);
        TestEqual(TEXT("Rain volume has deterministic streak count"), Coast->GetRainStreakCount(), 120);
        TestTrue(TEXT("Rain curtain is visible only in rain"), Coast->IsRainCurtainVisible());
        TestTrue(TEXT("Applied fog matches rain state"), FMath::IsNearlyEqual(Coast->GetAppliedFogDensity(), State.GetFogDensity()));
        State.SetWeather(EAeroSimWeather::Clear);
        Coast->ApplyEnvironmentState(State);
        TestFalse(TEXT("Clear hides rain curtain"), Coast->IsRainCurtainVisible());
    }
    if (World != nullptr) World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAeroSimPhase2HeroZonesSafeAreaTest,
    "AeroSim.Phase2.Environment.HeroZonesAndSafeArea",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAeroSimPhase2HeroZonesSafeAreaTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    TestEqual(TEXT("Core width is 300m"), FPacificCoastGrayboxContract::CoreWidthCm, 30000.0F);
    TestEqual(TEXT("Core length is 500m"), FPacificCoastGrayboxContract::CoreLengthCm, 50000.0F);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    APacificCoastGraybox* Coast = World != nullptr ? World->SpawnActor<APacificCoastGraybox>() : nullptr;
    TestNotNull(TEXT("Coast spawns"), Coast);
    if (Coast != nullptr)
    {
        TestEqual(TEXT("Exactly three hero zones"), Coast->GetGeneratedHeroZoneCount(), 3);
        // Mesh batching reduces component count without reducing actual scene detail.
        TArray<UProceduralMeshComponent*> ActualMeshes;
        Coast->GetComponents(ActualMeshes);
        int32 ActualTriangles = 0;
        for (UProceduralMeshComponent* Mesh : ActualMeshes)
            if (const FProcMeshSection* Section = Mesh->GetProcMeshSection(0)) ActualTriangles += Section->ProcIndexBuffer.Num() / 3;
        TestTrue(TEXT("Detailed slice contains actual bounded terrain/tree/water geometry"), ActualTriangles > 200000 && ActualTriangles < 4000000);
    }
    const FVector2D PhoneMargin = AFlightHUD::CalculateSafeMargin(1170.0F, 2532.0F);
    TestTrue(TEXT("Safe margin scales beyond minimum"), PhoneMargin.X > 24.0F && PhoneMargin.Y > 24.0F);
    const FVector2D SmallMargin = AFlightHUD::CalculateSafeMargin(320.0F, 240.0F);
    TestEqual(TEXT("Small viewport keeps minimum X"), static_cast<double>(SmallMargin.X), 24.0);
    TestEqual(TEXT("Small viewport keeps minimum Y"), static_cast<double>(SmallMargin.Y), 24.0);
    if (World != nullptr) World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAeroSimPhase2PauseResumeTest,
    "AeroSim.Phase2.Input.PauseResumeRecenter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAeroSimPhase2PauseResumeTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    AFlightPawn* Pawn = World != nullptr ? World->SpawnActor<AFlightPawn>() : nullptr;
    TestNotNull(TEXT("Pawn spawns"), Pawn);
    if (Pawn != nullptr)
    {
        FFlightControlInput Input;
        Input.Throttle = 1.0F;
        Pawn->SetSemanticInput(Input);
        Pawn->SetSafePaused(true);
        TestTrue(TEXT("Pause centers input"), Pawn->GetControlInput().IsNeutral());
        Pawn->SetSafePaused(false);
        TestTrue(TEXT("Resume starts countdown"), Pawn->IsResumeCountdownActive());
        Pawn->SetSemanticInput(Input);
        TestTrue(TEXT("Countdown rejects stale input"), Pawn->GetControlInput().IsNeutral());
        Pawn->Tick(2.0F);
        TestTrue(TEXT("Countdown remains before three seconds"), Pawn->IsResumeCountdownActive());
        Pawn->Tick(1.1F);
        TestFalse(TEXT("Countdown completes"), Pawn->IsResumeCountdownActive());
        TestEqual(TEXT("Model resumes grounded state"), Pawn->GetFlightModel().Phase, EFlightPhase::Grounded);
    }
    if (World != nullptr) World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAeroSimPhase2ResultReplayTest,
    "AeroSim.Phase2.Result.DeterministicReplay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAeroSimPhase2ResultReplayTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FDeterministicFlightTrace Trace;
    Trace.Record(0.0F, FVector::ZeroVector, FRotator::ZeroRotator);
    Trace.Record(0.1F, FVector(100.0F, 0.0F, 200.0F), FRotator(0.0F, 10.0F, 0.0F));
    Trace.Record(0.2F, FVector(200.0F, 0.0F, 100.0F), FRotator(0.0F, 20.0F, 0.0F));
    Trace.Record(0.19F, FVector(999.0F), FRotator::ZeroRotator);
    TestEqual(TEXT("Non-monotonic duplicate is rejected"), Trace.Samples.Num(), 3);
    const FFlightTraceSample First = Trace.Evaluate(0.15F);
    const FFlightTraceSample Second = Trace.Evaluate(0.15F);
    TestTrue(TEXT("Replay evaluation is deterministic"), First.Equals(Second));
    TestTrue(TEXT("Replay interpolates midpoint"), First.Position.Equals(FVector(150.0F, 0.0F, 150.0F), 0.01F));
    const FFlightResultMetrics Result = Trace.BuildResult();
    TestEqual(TEXT("Result sample count"), Result.Samples, 3);
    TestEqual(TEXT("Result duration"), Result.DurationSeconds, 0.2F);
    TestEqual(TEXT("Result maximum altitude"), Result.MaximumAltitudeMeters, 2.0F);
    TestTrue(TEXT("Result distance is measured"), Result.DistanceMeters > 2.0F);
    return true;
}

#endif
