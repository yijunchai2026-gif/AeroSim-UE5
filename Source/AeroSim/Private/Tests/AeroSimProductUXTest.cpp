#if WITH_DEV_AUTOMATION_TESTS

#include "AeroSimRuntimeContract.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Phase4Mission.h"
#include "ProductUX.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProductUxFlowTest, "AeroSim.ProductUX.Flow.HangarPreflightTakeoffPrompts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProductUxFlowTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPhase4MissionSession Mission;
    Mission.Reset();
    TestEqual(TEXT("Product starts in visible Hangar"), Mission.Stage, EPhase4MissionStage::Hangar);
    Mission.EnterPreflight();
    TestEqual(TEXT("Primary action enters Preflight"), Mission.Stage, EPhase4MissionStage::Preflight);
    Mission.SetPreflightComplete(true);
    TestTrue(TEXT("Centered preflight starts mission"), Mission.BeginMission());
    TestEqual(TEXT("Flight begins with explicit Takeoff stage"), Mission.Stage, EPhase4MissionStage::Takeoff);
    TestTrue(TEXT("Takeoff prompt names throttle action"), Mission.Feedback.RecoveryAction.Contains(TEXT("throttle")));
    TestTrue(TEXT("Compatible product UX schema is valid"), FAeroSimProductUxContract::IsValid());
    TestEqual(TEXT("Phase7 runtime identity stays compatible"), FString(FAeroSimRuntimeContract::Contract), FString(TEXT("aerosim.runtime.phase7/v1")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProductDisplayTransactionTest, "AeroSim.ProductUX.Display.ResolutionTransactionPersistence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProductDisplayTransactionTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    const TArray<FIntPoint> Resolutions = FProductDisplaySettings::SupportedResolutions();
    TestEqual(TEXT("Five desktop resolutions"), Resolutions.Num(), 5);
    TestTrue(TEXT("1280x720 supported"), Resolutions.Contains(FIntPoint(1280, 720)));
    TestTrue(TEXT("1440x900 supported"), Resolutions.Contains(FIntPoint(1440, 900)));
    TestTrue(TEXT("1920x1080 supported"), Resolutions.Contains(FIntPoint(1920, 1080)));
    TestTrue(TEXT("2560x1440 supported"), Resolutions.Contains(FIntPoint(2560, 1440)));
    TestTrue(TEXT("3840x2160 supported"), Resolutions.Contains(FIntPoint(3840, 2160)));
    bool bHighDpi = false;
    TestTrue(TEXT("High-DPI game mode config exists"), GConfig != nullptr && GConfig->GetBool(
        TEXT("/Script/Engine.UserInterfaceSettings"), TEXT("bAllowHighDPIInGameMode"), bHighDpi, GEngineIni));
    TestTrue(TEXT("High-DPI game mode enabled"), bHighDpi);
    TestEqual(TEXT("Three window modes"), FProductDisplaySettings::SupportedWindowModes().Num(), 3);

    FPhase7Settings Applied;
    FProductDisplaySettings Display;
    TestTrue(TEXT("Settings opens from valid applied state"), Display.Open(Applied));
    Display.ChangeFocusedValue(1);
    TestEqual(TEXT("Resolution is staged, not applied"), Display.Transaction.Applied.ResolutionWidth, 1440);
    TestEqual(TEXT("Next resolution is 1920"), Display.Transaction.Preview.ResolutionWidth, 1920);
    TestTrue(TEXT("Preview is explicit"), Display.BeginPreview());
    TestTrue(TEXT("Apply commits staged values"), Display.Apply());
    TestEqual(TEXT("Applied resolution updated"), Display.Transaction.Applied.ResolutionWidth, 1920);
    TestTrue(TEXT("Rollback restores opening settings"), Display.Rollback());
    TestEqual(TEXT("Rollback returns to 1440"), Display.Transaction.Applied.ResolutionWidth, 1440);

    const FString Encoded = FPhase7SaveCodec::Encode(Display.Transaction.Applied);
    FPhase7Settings Decoded;
    FString Error;
    TestTrue(TEXT("Display fields persist in settings v2"), FPhase7SaveCodec::DecodeAndMigrate(Encoded, Decoded, Error));
    TestEqual(TEXT("Resolution width round-trips"), Decoded.ResolutionWidth, 1440);
    TestEqual(TEXT("Window mode round-trips"), Decoded.WindowMode, EPhase7WindowMode::Windowed);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProductInputPromptTest, "AeroSim.ProductUX.Input.KeyboardPrompts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProductInputPromptTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FProductDisplaySettings Display;
    TestTrue(TEXT("Display settings opens"), Display.Open(FPhase7Settings()));
    Display.MoveFocus(1);
    TestEqual(TEXT("Keyboard down moves focus"), Display.GetFocusedAction(), FString(TEXT("WINDOW MODE")));
    Display.ChangeFocusedValue(1);
    TestEqual(TEXT("Keyboard right selects borderless"), Display.Transaction.Preview.WindowMode, EPhase7WindowMode::Borderless);
    Display.Cancel();
    TestEqual(TEXT("Cancel restores applied mode"), Display.Transaction.Applied.WindowMode, EPhase7WindowMode::Windowed);
    return true;
}

#endif
