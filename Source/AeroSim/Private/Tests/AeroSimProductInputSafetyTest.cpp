#if WITH_DEV_AUTOMATION_TESTS

#include "AeroSimGameMode.h"
#include "AeroSimControlBridge.h"
#include "Dom/JsonObject.h"
#include "AeroSimPlayerController.h"
#include "Engine/GameInstance.h"
#include "InputKeyEventArgs.h"
#include "Engine/World.h"
#include "FlightPawn.h"
#include "FlightTestWorldSupport.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/WorldSettings.h"
#include "InputMappingContext.h"
#include "InputActionValue.h"
#include "Misc/AutomationTest.h"

namespace AeroSimInputSafetyTests
{
struct FTestWorld final
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    AAeroSimGameMode* GameMode = nullptr;
    AAeroSimPlayerController* Controller = nullptr;
    AFlightPawn* Pawn = nullptr;

    explicit FTestWorld(const bool bWithGameMode = false)
    {
        if (World == nullptr) return;
        if (bWithGameMode)
        {
            World->SetGameInstance(NewObject<UGameInstance>());
            World->GetWorldSettings()->DefaultGameMode = AAeroSimGameMode::StaticClass();
            World->SetGameMode(FURL());
            GameMode = World->GetAuthGameMode<AAeroSimGameMode>();
        }
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Controller = World->SpawnActor<AAeroSimPlayerController>(Spawn);
        Pawn = World->SpawnActor<AFlightPawn>(Spawn);
        AeroSimTestSupport::AddFloorAndGroundPawn(World,Pawn);
        if (Controller != nullptr && Pawn != nullptr)
        {
            // CreateWorld is not begun, so Controller PreInitializeComponents has not registered it.
            World->AddController(Controller);
            Controller->PlayerInput = NewObject<UPlayerInput>(Controller);
            Controller->Possess(Pawn);
        }
    }
    ~FTestWorld() { if (World != nullptr) World->DestroyWorld(false); }
    bool IsValid() const { return World != nullptr && Controller != nullptr && Pawn != nullptr; }

    void Observe(const FKey& Key, const EInputEvent Event, const float Value = 1.0F)
    {
        // Same raw observation entry as Slate; deliberately not forwarded to PlayerInput.
        Controller->ObservePhysicalInput(FInputKeyEventArgs(nullptr, INPUTDEVICEID_NONE, Key, Event, Value, false, 0u));
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAeroSimProductMenuInputTest,
    "AeroSim.ProductUX.Input.MenuRejectsFlightAndPreflightChecksPhysical",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAeroSimProductMenuInputTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    AeroSimInputSafetyTests::FTestWorld TestWorld(true);
    if (!TestNotNull(TEXT("Product GameMode installed"), TestWorld.GameMode) || !TestWorld.IsValid()) return false;
    AFlightPawn* Pawn = TestWorld.Pawn;
    AAeroSimGameMode* GameMode = TestWorld.GameMode;
    FFlightControlInput Input;
    Input.Throttle = 1.0F;
    Input.Pitch = 0.8F;
    const EProductPage MenuPages[] = { EProductPage::Home, EProductPage::Hangar, EProductPage::Mission };
    for (const EProductPage Page : MenuPages)
    {
        TestTrue(TEXT("Menu can open"), GameMode->NavigateProductPage(Page));
        Pawn->SetSemanticInput(Input);
        TestTrue(TEXT("Menu rejects axes"), Pawn->GetControlInput().IsNeutral());
        const FTransform Before = Pawn->GetActorTransform();
        const int32 TraceCount = Pawn->GetFlightTrace().Samples.Num();
        Pawn->Tick(0.5F);
        TestTrue(TEXT("Menu has no actor drift"), Pawn->GetActorTransform().Equals(Before));
        TestEqual(TEXT("Menu records no trace"), Pawn->GetFlightTrace().Samples.Num(), TraceCount);
    }
    GameMode->ActivatePrimaryAction();
    TestEqual(TEXT("Mission primary enters Preflight"), GameMode->GetProductPage(), EProductPage::Preflight);
    TestWorld.Observe(EKeys::LeftShift, IE_Pressed);
    Pawn->NeutralizeInput();
    TestFalse(TEXT("Held physical throttle is not centered"), Pawn->IsPhysicalInputNeutral());
    TestFalse(TEXT("HUD reports physical non-neutral"), Pawn->GetHudData().bInputNeutral);
    GameMode->ActivatePrimaryAction();
    TestEqual(TEXT("Held control prevents arming"), GameMode->GetProductPage(), EProductPage::Preflight);
    TestWorld.Observe(EKeys::LeftShift, IE_Released);
    TestTrue(TEXT("Released raw throttle is physically centered"), Pawn->IsPhysicalInputNeutral());
    TestNotNull(TEXT("Product command has first player controller"), TestWorld.World->GetFirstPlayerController());
    TestTrue(TEXT("Preflight task structurally valid"), GameMode->GetPhase4Mission().Task.IsValid());
    TestEqual(TEXT("Mission flow is Preflight before start"), GameMode->GetMissionFlow().State, EAeroSimMissionState::Preflight);
    GameMode->ActivatePrimaryAction();
    TestEqual(TEXT("Explicit centered primary enters Flight"), GameMode->GetProductPage(), EProductPage::Flight);
    Pawn->SetSemanticInput(Input);
    TestFalse(TEXT("Flight accepts axes"), Pawn->GetControlInput().IsNeutral());
    GameMode->NavigateProductPage(EProductPage::Settings);
    const EFlightCameraMode Camera = Pawn->GetCameraMode();
    Pawn->CycleCamera();
    TestEqual(TEXT("Menu rejects gameplay camera"), Pawn->GetCameraMode(), Camera);
    Pawn->SetSemanticInput(Input);
    TestTrue(TEXT("Settings rejects axes"), Pawn->GetControlInput().IsNeutral());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAeroSimProductPhysicalResumeTest,
    "AeroSim.ProductUX.Input.HeldControlsAndConsumedEventsRestartResume",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAeroSimProductPhysicalResumeTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    AeroSimInputSafetyTests::FTestWorld TestWorld;
    if (!TestWorld.IsValid()) return false;
    AFlightPawn* Pawn = TestWorld.Pawn;
    const FKey Controls[] = { EKeys::W, EKeys::A, EKeys::Q, EKeys::LeftShift, EKeys::LeftControl,
        EKeys::Gamepad_LeftX, EKeys::Gamepad_LeftY, EKeys::Gamepad_RightX, EKeys::Gamepad_RightY,
        EKeys::Gamepad_LeftTriggerAxis, EKeys::Gamepad_RightTriggerAxis };
    for (const FKey& Key : Controls)
    {
        Pawn->SetSafePaused(true);
        TestWorld.Observe(Key, Key.IsAnalog() ? IE_Axis : IE_Pressed, 0.8F);
        TestWorld.Controller->FlushPressedKeys();
        Pawn->NeutralizeInput();
        Pawn->SetSafePaused(false);
        Pawn->Tick(4.0F);
        TestEqual(TEXT("Held control keeps model paused despite PlayerInput flush"), Pawn->GetFlightModel().Phase, EFlightPhase::Paused);
        TestEqual(TEXT("Held control restarts full interval"), Pawn->GetResumeCountdownSeconds(), 3.0F);
        TestWorld.Observe(Key, Key.IsAnalog() ? IE_Axis : IE_Released, 0.0F);
        Pawn->Tick(2.0F);
        TestEqual(TEXT("Two neutral seconds still paused"), Pawn->GetFlightModel().Phase, EFlightPhase::Paused);
        // A press/release fully consumed between ticks must also restart the countdown.
        TestWorld.Observe(EKeys::W, IE_Pressed);
        TestWorld.Observe(EKeys::W, IE_Released);
        Pawn->Tick(1.1F);
        TestEqual(TEXT("Brief activity cannot hide between ticks"), Pawn->GetFlightModel().Phase, EFlightPhase::Paused);
        Pawn->Tick(2.0F);
        TestEqual(TEXT("Uninterrupted three seconds resume"), Pawn->GetFlightModel().Phase, EFlightPhase::Grounded);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAeroSimProductInputLifecycleTest,
    "AeroSim.ProductUX.Input.DeactivationAndDisconnectNeverAutoResume",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAeroSimProductInputLifecycleTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    AeroSimInputSafetyTests::FTestWorld TestWorld;
    if (!TestWorld.IsValid()) return false;
    AFlightPawn* Pawn = TestWorld.Pawn;
    TestWorld.Observe(EKeys::W, IE_Pressed);
    Pawn->HandleApplicationDeactivated();
    TestWorld.Controller->NotifyApplicationActivationChanged(false);
    TestWorld.Controller->NotifyApplicationActivationChanged(true);
    Pawn->Tick(5.0F);
    TestEqual(TEXT("Reactivation never resumes"), Pawn->GetFlightModel().Phase, EFlightPhase::Paused);
    TestFalse(TEXT("Focus change cannot erase held key"), Pawn->IsPhysicalInputNeutral());
    TestWorld.Observe(EKeys::W, IE_Released);
    Pawn->Tick(5.0F);
    TestEqual(TEXT("Release alone cannot resume"), Pawn->GetFlightModel().Phase, EFlightPhase::Paused);
    Pawn->SetSafePaused(false);
    Pawn->Tick(3.1F);
    TestEqual(TEXT("Explicit resume after focus recovery"), Pawn->GetFlightModel().Phase, EFlightPhase::Grounded);
    TestWorld.Observe(EKeys::Gamepad_RightX, IE_Axis, 0.9F);
    Pawn->HandleInputDeviceConnectionChanged(EInputDeviceConnectionState::Disconnected, PLATFORMUSERID_NONE, INPUTDEVICEID_NONE);
    TestWorld.Controller->NotifyInputDeviceConnectionChanged(EInputDeviceConnectionState::Disconnected, PLATFORMUSERID_NONE, INPUTDEVICEID_NONE);
    TestWorld.Controller->NotifyInputDeviceConnectionChanged(EInputDeviceConnectionState::Connected, PLATFORMUSERID_NONE, INPUTDEVICEID_NONE);
    Pawn->Tick(5.0F);
    TestEqual(TEXT("Reconnection never resumes"), Pawn->GetFlightModel().Phase, EFlightPhase::Paused);
    TestFalse(TEXT("Reconnection is not a neutral sample"), Pawn->IsPhysicalInputNeutral());
    TestWorld.Observe(EKeys::Gamepad_RightX, IE_Axis, 0.0F);
    Pawn->Tick(5.0F);
    TestEqual(TEXT("Neutral reconnect event still needs user action"), Pawn->GetFlightModel().Phase, EFlightPhase::Paused);
    Pawn->SetSafePaused(false);
    Pawn->Tick(3.1F);
    TestEqual(TEXT("Explicit reconnect resume succeeds after recenter"), Pawn->GetFlightModel().Phase, EFlightPhase::Grounded);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAeroSimProductInputCompatibilityTest,
    "AeroSim.ProductUX.Input.PurePawnCompatibilityAndNoPassiveMouseFlight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAeroSimProductInputCompatibilityTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    AFlightPawn* Pawn = World != nullptr ? World->SpawnActor<AFlightPawn>() : nullptr;
    AeroSimTestSupport::AddFloorAndGroundPawn(World,Pawn);
    if (Pawn != nullptr)
    {
        FFlightControlInput Input;
        Input.Throttle = 1.0F;
        Pawn->SetSemanticInput(Input);
        Pawn->SetSafePaused(true);
        Pawn->SetSafePaused(false);
        Pawn->SetSemanticInput(Input);
        Pawn->Tick(2.0F);
        TestTrue(TEXT("Existing pure-pawn countdown remains compatible"), Pawn->IsResumeCountdownActive());
        Pawn->Tick(1.1F);
        TestEqual(TEXT("Pure-pawn resumes grounded"), Pawn->GetFlightModel().Phase, EFlightPhase::Grounded);
        for (const FEnhancedActionKeyMapping& Mapping : Pawn->GetFlightMappingContext()->GetMappings())
        {
            TestFalse(TEXT("Mouse movement never maps to flight"), Mapping.Key == EKeys::MouseX || Mapping.Key == EKeys::MouseY);
        }
    }
    if (World != nullptr) World->DestroyWorld(false);
    return Pawn != nullptr;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAeroSimProductReplayPresentationTest,
    "AeroSim.ProductUX.Input.ReplayAndProductCameraPreserveModelAndTrace",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAeroSimProductReplayPresentationTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    AeroSimInputSafetyTests::FTestWorld TestWorld;
    if (!TestWorld.IsValid()) return false;
    AFlightPawn* Pawn = TestWorld.Pawn;
    const FTransform Origin(FRotator(0.0F, 35.0F, 0.0F), FVector(1000.0F, 2000.0F, 100.0F));
    Pawn->ConfigureReviewView(Origin, EFlightCameraMode::Chase);
    FFlightControlInput Input;
    Input.Throttle = 1.0F;
    Pawn->SetSemanticInput(Input);
    Pawn->Tick(0.1F);
    const FFlightModel Model = Pawn->GetFlightModel();
    const int32 Samples = Pawn->GetFlightTrace().Samples.Num();
    FFlightTraceSample Sample;
    Sample.TimeSeconds = 1.0F;
    Sample.Position = FVector(200.0F, 300.0F, 400.0F);
    Sample.Rotation = FRotator(-10.0F, 15.0F, 5.0F);
    Pawn->ApplyReplaySample(Sample);
    const FTransform Expected(Origin.TransformRotation(Sample.Rotation.Quaternion()), Origin.TransformPosition(Sample.Position));
    TestTrue(TEXT("Replay uses reset-origin transform"), Pawn->GetActorTransform().Equals(Expected));
    Pawn->Tick(1.0F);
    TestTrue(TEXT("Tick cannot overwrite replay pose"), Pawn->GetActorTransform().Equals(Expected));
    TestTrue(TEXT("Replay preserves model position"), Pawn->GetFlightModel().Position.Equals(Model.Position));
    TestEqual(TEXT("Replay preserves model phase"), Pawn->GetFlightModel().Phase, Model.Phase);
    TestEqual(TEXT("Replay does not append trace"), Pawn->GetFlightTrace().Samples.Num(), Samples);
    Pawn->SetProductCameraMode(EFlightCameraMode::Gimbal);
    TestTrue(TEXT("Product camera setter does not reset origin"), Pawn->GetActorTransform().Equals(Expected));
    TestEqual(TEXT("Product camera setter retains trace"), Pawn->GetFlightTrace().Samples.Num(), Samples);
    TestTrue(TEXT("Product FOV accepts valid value"), Pawn->ApplyProductVerticalFov(75.0F));
    TestFalse(TEXT("Product FOV rejects invalid value"), Pawn->ApplyProductVerticalFov(200.0F));
    TestEqual(TEXT("Rejected FOV preserves applied value"), Pawn->GetVerticalFov(), 75.0F);
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAeroSimProductWaterResetTest,
    "AeroSim.ProductUX.Input.WaterIncidentExplicitReset",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAeroSimProductWaterResetTest::RunTest(const FString& Parameters)
{
    AeroSimInputSafetyTests::FTestWorld Fixture(true);
    if(!Fixture.IsValid() || !TestNotNull(TEXT("Actual product GameMode"),Fixture.GameMode))return false;
    AFlightPawn* Pawn=Fixture.Pawn;
    AAeroSimGameMode* Mode=Fixture.GameMode;
    Mode->NavigateProductPage(EProductPage::Mission);
    Mode->ActivatePrimaryAction();Mode->ActivatePrimaryAction();
    TestEqual(TEXT("Normal menu commands start Flight"),Mode->GetProductPage(),EProductPage::Flight);
    Mode->RestartProductMission();
    TestEqual(TEXT("Ordinary active mission restart guard is retained"),Mode->GetProductPage(),EProductPage::Flight);
    const FTransform Home=Pawn->GetActorTransform();
    // Fixture setup puts a real pawn over water; this is not route traversal evidence.
    Pawn->FlightModel.Phase=EFlightPhase::Flight;
    Pawn->FlightModel.Position=Pawn->WorldToModelPosition(FVector(40000,0,-6400));
    Pawn->SetActorLocation(Pawn->ModelToWorldPosition(Pawn->FlightModel.Position));
    FFlightControlInput Down;Down.Throttle=-1;
    for(int32 I=0;I<40 && !Pawn->HasWaterIncident();++I)
    {Pawn->SetSemanticInput(Down);Pawn->Tick(.1F);}
    TestTrue(TEXT("Active mission real pawn detects water"),Pawn->HasWaterIncident());
    TestEqual(TEXT("Mission carries unsafe-water feedback"),Mode->GetPhase4Mission().Feedback.Incident,EPhase4Incident::UnsafeLanding);
    TestEqual(TEXT("Incident model is paused"),Pawn->GetFlightModel().Phase,EFlightPhase::Paused);
    Pawn->HandleReset(FInputActionValue(true)); // Exact ResetAction consumer, not a direct ResetFlight shortcut.
    TestEqual(TEXT("Explicit water retry returns to Hangar"),Mode->GetProductPage(),EProductPage::Hangar);
    TestFalse(TEXT("Explicit retry clears water latch"),Pawn->HasWaterIncident());
    TestTrue(TEXT("Retry returns to original supported mat pose"),Pawn->GetActorTransform().Equals(Home,.01));
    TestEqual(TEXT("Retry resets task rather than retaining completed gates"),Mode->GetPhase4Mission().Stage,EPhase4MissionStage::Hangar);
    TestEqual(TEXT("Retry is grounded"),Pawn->GetFlightModel().Phase,EFlightPhase::Grounded);
    TestFalse(TEXT("Retry clears world pause"),Fixture.World->IsPaused());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAeroSimControlFlightRollbackTest,
    "AeroSim.ProductUX.Input.ControlTransactionRestoresFlightIncidentFlags",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAeroSimControlFlightRollbackTest::RunTest(const FString& Parameters)
{
    AeroSimInputSafetyTests::FTestWorld Fixture(true);
    if(!Fixture.IsValid() || !Fixture.GameMode) return false;
    UAeroSimControlBridge* Bridge=NewObject<UAeroSimControlBridge>(Fixture.GameMode);
    AFlightPawn* Pawn=Fixture.Pawn;
    for(bool Initial:{false,true})
    {
        Pawn->bWaterIncidentLatched=Initial;Pawn->bCollisionHold=Initial;
        const FFlightModel Before=Pawn->FlightModel;
        const FTransform Pose=Pawn->GetActorTransform();
        auto Request=MakeShared<FJsonObject>();
        Request->SetStringField(TEXT("request_id"),FGuid::NewGuid().ToString(EGuidFormats::Digits));
        Request->SetStringField(TEXT("session"),TEXT(""));
        Request->SetNumberField(TEXT("expires_unix"),FDateTime::UtcNow().ToUnixTimestamp()+10);
        Request->SetStringField(TEXT("target"),TEXT("player:0"));
        Request->SetStringField(TEXT("operation"),TEXT("release"));
        Request->SetStringField(TEXT("mode"),TEXT("apply"));
        Request->SetStringField(TEXT("expected_page"),TEXT("Home"));
        Bridge->Dispatch(Request);
        if(!TestTrue(TEXT("Production command creates rollback snapshot"),static_cast<bool>(Bridge->Restore)))break;
        Pawn->bWaterIncidentLatched=!Initial;Pawn->bCollisionHold=!Initial;
        Pawn->FlightModel.Position+=FVector(100,200,300);
        Pawn->SetActorLocation(Pose.GetLocation()+FVector(100,200,300));
        Bridge->Restore(); // Exact production rollback callback; no mailbox writes in this fixture.
        TestEqual(TEXT("Rollback restores water latch in both directions"),Pawn->bWaterIncidentLatched,Initial);
        TestEqual(TEXT("Rollback restores collision hold in both directions"),Pawn->bCollisionHold,Initial);
        TestTrue(TEXT("Rollback restores actual actor pose"),Pawn->GetActorTransform().Equals(Pose,.01));
        TestTrue(TEXT("Rollback restores model pose"),Pawn->FlightModel.Position.Equals(Before.Position));
        Bridge->Restore=nullptr;Bridge->Pending.Reset();Bridge->Before.Reset();
    }
    return true;
}
#endif
