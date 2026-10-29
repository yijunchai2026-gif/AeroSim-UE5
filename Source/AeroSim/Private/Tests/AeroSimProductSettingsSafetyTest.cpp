#if WITH_DEV_AUTOMATION_TESTS

#include "AeroSimRuntimeContract.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProductUX.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include <limits>

namespace
{
constexpr const TCHAR* LegacyV2 = TEXT("{\"version\":2,\"vertical_fov\":65,\"quality_tier\":1,\"master_volume\":0.5,\"telemetry_enabled\":false}");

// A null literal pointer removes a required field; TEXT("null") tests an explicit JSON null.
FString WithV2Field(const TCHAR* Field, const TCHAR* Literal)
{
    TSharedPtr<FJsonObject> Root;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(LegacyV2), Root) || !Root.IsValid()) return FString();
    Root->RemoveField(Field);
    FString Json;
    FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json));
    Json.TrimEndInline();
    Json.LeftChopInline(1);
    if (Literal != nullptr) Json += FString::Printf(TEXT(",\"%s\":%s"), Field, Literal);
    return Json + TEXT("}");
}

TArray<FString> ReadProductSaveSnapshot()
{
    const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Config/AeroSim/settings-v2.json"));
    TArray<FString> Snapshot;
    IPlatformFile& Files = FPlatformFileManager::Get().GetPlatformFile();
    for (const FString& Suffix : { FString(), FString(TEXT(".bak")), FString(TEXT(".tmp")) })
    {
        const FString File = Path + Suffix;
        FString Payload;
        const bool bExists = Files.FileExists(*File);
        const bool bReadable = bExists && FFileHelper::LoadFileToString(Payload, *File);
        Snapshot.Add(FString::Printf(TEXT("%d:%d:%lld:%s:%s"), bExists, bReadable,
            static_cast<long long>(Files.FileSize(*File)), *Files.GetTimeStamp(*File).ToString(), *Payload));
    }
    return Snapshot;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProductSettingsFiniteValidationTest, "AeroSim.ProductUX.SettingsSafety.FiniteRangesAndEnums", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProductSettingsFiniteValidationTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    const FPhase7Settings Defaults;
    TestTrue(TEXT("Defaults are valid"), Defaults.IsValid());
    TestEqual(TEXT("HUD defaults to unscaled"), Defaults.HudScale, 1.0F);
    TestEqual(TEXT("Text defaults to unscaled"), Defaults.TextScale, 1.0F);
    TestFalse(TEXT("Reduced motion defaults off"), Defaults.bReducedMotion);

    struct FFloatCase { const TCHAR* Name; float FPhase7Settings::* Member; float Minimum; float Maximum; };
    const FFloatCase Fields[] = {
        { TEXT("vertical_fov"), &FPhase7Settings::VerticalFov, 35.0F, 100.0F },
        { TEXT("master_volume"), &FPhase7Settings::MasterVolume, 0.0F, 1.0F },
        { TEXT("hud_scale"), &FPhase7Settings::HudScale, 0.8F, 1.4F },
        { TEXT("text_scale"), &FPhase7Settings::TextScale, 1.0F, 2.0F }
    };
    for (const FFloatCase& Field : Fields)
    {
        for (const float Boundary : { Field.Minimum, Field.Maximum })
        {
            FPhase7Settings Candidate = Defaults;
            Candidate.*(Field.Member) = Boundary;
            TestTrue(FString::Printf(TEXT("%s inclusive boundary"), Field.Name), Candidate.IsValid());
            FPhase7Settings Decoded;
            FString Error;
            TestTrue(TEXT("Every float boundary round-trips"), FPhase7SaveCodec::DecodeAndMigrate(FPhase7SaveCodec::Encode(Candidate), Decoded, Error));
            TestEqual(TEXT("Boundary value survives codec"), Decoded.*(Field.Member), Boundary);
        }
        const float InvalidValues[] = {
            std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
            -std::numeric_limits<float>::infinity(), Field.Minimum - 0.01F, Field.Maximum + 0.01F
        };
        for (const float Value : InvalidValues)
        {
            FPhase7Settings Candidate = Defaults;
            Candidate.*(Field.Member) = Value;
            TestFalse(FString::Printf(TEXT("%s invalid number fails validation"), Field.Name), Candidate.IsValid());
            TestTrue(TEXT("Invalid settings cannot encode"), FPhase7SaveCodec::Encode(Candidate).IsEmpty());
            FPhase7SettingsTransaction Transaction;
            TestFalse(TEXT("Invalid preview cannot begin"), Transaction.Begin(Candidate));
            TestFalse(TEXT("Rejected preview leaves no active transaction"), Transaction.bPreviewActive);
        }
    }
    for (const uint8 InvalidEnum : { uint8(3), uint8(255) })
    {
        FPhase7Settings Candidate;
        Candidate.QualityTier = static_cast<EPhase7QualityTier>(InvalidEnum);
        TestFalse(TEXT("Unknown quality tier fails"), Candidate.IsValid());
        Candidate = Defaults;
        Candidate.WindowMode = static_cast<EPhase7WindowMode>(InvalidEnum);
        TestFalse(TEXT("Unknown window mode fails"), Candidate.IsValid());
    }
    for (const FIntPoint Resolution : { FIntPoint(2560, 1440), FIntPoint(3840, 2160) })
    {
        FPhase7Settings HighResolution = Defaults;
        HighResolution.ResolutionWidth = Resolution.X;
        HighResolution.ResolutionHeight = Resolution.Y;
        TestTrue(FString::Printf(TEXT("%dx%d is a valid desktop resolution"), Resolution.X, Resolution.Y),
            HighResolution.IsValid());
    }
    FPhase7Settings Invalid = Defaults;
    Invalid.ResolutionHeight = 1080;
    TestFalse(TEXT("Resolution components must be a supported pair"), Invalid.IsValid());
    Invalid = Defaults;
    Invalid.Version = 1;
    TestFalse(TEXT("Runtime DTO must be migrated before use"), Invalid.IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProductSettingsCodecCompatibilityTest, "AeroSim.ProductUX.SettingsSafety.BackwardCompatibility", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProductSettingsCodecCompatibilityTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    TestEqual(TEXT("Runtime contract remains phase7/v1"), FString(FAeroSimRuntimeContract::Contract), FString(TEXT("aerosim.runtime.phase7/v1")));
    TestEqual(TEXT("Settings version remains v2"), FPhase7Settings::CurrentVersion, 2);
    const TCHAR* OldSaves[] = {
        TEXT("{\"version\":1,\"vertical_fov\":65}"),
        TEXT("{\"schema\":\"aerosim.settings/v1\",\"version\":1,\"vertical_fov\":65}"),
        LegacyV2,
        TEXT("{\"schema\":\"aerosim.settings/v2\",\"version\":2,\"vertical_fov\":65,\"quality_tier\":1,\"master_volume\":0.5,\"telemetry_enabled\":false}"),
        TEXT("{\"version\":2,\"vertical_fov\":65,\"quality_tier\":1,\"master_volume\":0.5,\"telemetry_enabled\":false,\"resolution_width\":1920,\"resolution_height\":1080,\"window_mode\":2}")
    };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(OldSaves); ++Index)
    {
        FPhase7Settings Decoded;
        Decoded.HudScale = 1.2F;
        Decoded.TextScale = 1.5F;
        Decoded.bReducedMotion = true;
        FString Error = TEXT("STALE_ERROR");
        TestTrue(TEXT("Historical save migrates"), FPhase7SaveCodec::DecodeAndMigrate(OldSaves[Index], Decoded, Error));
        TestTrue(TEXT("Successful decode clears stale error"), Error.IsEmpty());
        TestEqual(TEXT("Migrated version is v2"), Decoded.Version, 2);
        TestEqual(TEXT("Historical vFOV survives"), Decoded.VerticalFov, 65.0F);
        TestEqual(TEXT("Missing HUD field defaults"), Decoded.HudScale, 1.0F);
        TestEqual(TEXT("Missing text field defaults"), Decoded.TextScale, 1.0F);
        TestFalse(TEXT("Missing reduced-motion field defaults"), Decoded.bReducedMotion);
        TestFalse(TEXT("Historical privacy remains opt-in"), Decoded.bTelemetryEnabled);
        TestEqual(TEXT("Historical quality defaults or survives"), Decoded.QualityTier,
            Index < 2 ? EPhase7QualityTier::DesktopHigh : EPhase7QualityTier::DesktopLow);
        TestEqual(TEXT("Historical volume defaults or survives"), Decoded.MasterVolume, Index < 2 ? 1.0F : 0.5F);
        TestEqual(TEXT("Old display width defaults or survives"), Decoded.ResolutionWidth, Index == 4 ? 1920 : 1440);
        TestEqual(TEXT("Old display height defaults or survives"), Decoded.ResolutionHeight, Index == 4 ? 1080 : 900);
        TestEqual(TEXT("Old display mode defaults or survives"), Decoded.WindowMode,
            Index == 4 ? EPhase7WindowMode::Fullscreen : EPhase7WindowMode::Windowed);
    }
    for (const FIntPoint& Resolution : FProductDisplaySettings::SupportedResolutions())
    {
        for (const EPhase7WindowMode Mode : FProductDisplaySettings::SupportedWindowModes())
        {
            for (const EPhase7QualityTier Quality : { EPhase7QualityTier::DesktopHigh, EPhase7QualityTier::DesktopLow, EPhase7QualityTier::Tablet })
            {
                FPhase7Settings Candidate;
                Candidate.ResolutionWidth = Resolution.X;
                Candidate.ResolutionHeight = Resolution.Y;
                Candidate.WindowMode = Mode;
                Candidate.QualityTier = Quality;
                Candidate.VerticalFov = 72.0F;
                Candidate.MasterVolume = 0.25F;
                Candidate.HudScale = 1.2F;
                Candidate.TextScale = 1.75F;
                Candidate.bReducedMotion = true;
                Candidate.bTelemetryEnabled = true;
                const FString Encoded = FPhase7SaveCodec::Encode(Candidate);
                TestTrue(TEXT("Encoder retains v2 schema identity"), Encoded.Contains(TEXT("\"aerosim.settings/v2\"")));
                FPhase7Settings Decoded;
                FString Error;
                TestTrue(TEXT("v2 extension round-trips all supported enum/resolution combinations"), FPhase7SaveCodec::DecodeAndMigrate(Encoded, Decoded, Error));
                TestEqual(TEXT("Every field survives round-trip"), FPhase7SaveCodec::Encode(Decoded), Encoded);
                TestEqual(TEXT("HUD value survives"), Decoded.HudScale, Candidate.HudScale);
                TestEqual(TEXT("Text value survives"), Decoded.TextScale, Candidate.TextScale);
                TestTrue(TEXT("Reduced motion survives"), Decoded.bReducedMotion);
            }
        }
    }
    FPhase7Settings Decoded;
    FString Error;
    TestTrue(TEXT("Unrelated additive v2 fields remain compatible"), FPhase7SaveCodec::DecodeAndMigrate(WithV2Field(TEXT("future_optional"), TEXT("{}")), Decoded, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProductSettingsJsonTypeSafetyTest, "AeroSim.ProductUX.SettingsSafety.StrictJsonTypes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProductSettingsJsonTypeSafetyTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    const auto Reject = [this](const FString& Json, const FString& Label)
    {
        FPhase7Settings Sentinel;
        Sentinel.VerticalFov = 77.0F;
        Sentinel.HudScale = 1.3F;
        Sentinel.TextScale = 1.75F;
        Sentinel.bReducedMotion = true;
        const FString Before = FPhase7SaveCodec::Encode(Sentinel);
        FString Error;
        TestFalse(Label, FPhase7SaveCodec::DecodeAndMigrate(Json, Sentinel, Error));
        TestFalse(TEXT("Failure returns a diagnostic"), Error.IsEmpty());
        TestEqual(TEXT("Failure does not partially replace output"), FPhase7SaveCodec::Encode(Sentinel), Before);
    };
    const TCHAR* NumberFields[] = { TEXT("version"), TEXT("vertical_fov"), TEXT("quality_tier"), TEXT("master_volume"),
        TEXT("resolution_width"), TEXT("resolution_height"), TEXT("window_mode"), TEXT("hud_scale"), TEXT("text_scale") };
    const TCHAR* WrongNumbers[] = { TEXT("null"), TEXT("true"), TEXT("false"), TEXT("\"1\""), TEXT("\"NaN\""), TEXT("{}"), TEXT("[]"),
        TEXT("1e400"), TEXT("-1e400"), TEXT("NaN"), TEXT("Infinity") };
    for (const TCHAR* Field : NumberFields)
    {
        for (const TCHAR* Literal : WrongNumbers)
        {
            Reject(WithV2Field(Field, Literal), FString::Printf(TEXT("%s rejects %s"), Field, Literal));
        }
    }
    const TCHAR* WrongBooleans[] = { TEXT("null"), TEXT("0"), TEXT("1"), TEXT("\"true\""), TEXT("\"false\""), TEXT("{}"), TEXT("[]") };
    for (const TCHAR* Field : { TEXT("telemetry_enabled"), TEXT("reduced_motion") })
    {
        for (const TCHAR* Literal : WrongBooleans)
        {
            Reject(WithV2Field(Field, Literal), FString::Printf(TEXT("%s rejects %s"), Field, Literal));
        }
    }
    for (const TCHAR* Literal : { TEXT("null"), TEXT("true"), TEXT("2"), TEXT("{}"), TEXT("[]"), TEXT("\"aerosim.settings/v1\""), TEXT("\"aerosim.settings/v3\"") })
    {
        Reject(WithV2Field(TEXT("schema"), Literal), TEXT("Present invalid schema fails"));
    }
    for (const TCHAR* Field : { TEXT("version"), TEXT("vertical_fov"), TEXT("quality_tier"), TEXT("master_volume"), TEXT("telemetry_enabled") })
    {
        Reject(WithV2Field(Field, nullptr), FString::Printf(TEXT("Required %s cannot be absent"), Field));
    }
    struct FBadField { const TCHAR* Field; const TCHAR* Literal; };
    const FBadField BadValues[] = {
        { TEXT("version"), TEXT("1.5") }, { TEXT("version"), TEXT("2.1") }, { TEXT("version"), TEXT("0") },
        { TEXT("version"), TEXT("99") }, { TEXT("version"), TEXT("2147483648") },
        { TEXT("quality_tier"), TEXT("0.5") }, { TEXT("quality_tier"), TEXT("-1") }, { TEXT("quality_tier"), TEXT("3") },
        { TEXT("window_mode"), TEXT("1.5") }, { TEXT("window_mode"), TEXT("-1") }, { TEXT("window_mode"), TEXT("256") },
        { TEXT("resolution_width"), TEXT("1440.1") }, { TEXT("resolution_height"), TEXT("900.1") },
        { TEXT("resolution_width"), TEXT("2147483648") }, { TEXT("resolution_height"), TEXT("1e20") },
        { TEXT("resolution_width"), TEXT("0") }, { TEXT("resolution_height"), TEXT("-900") },
        { TEXT("resolution_width"), TEXT("1920") }, { TEXT("resolution_height"), TEXT("1080") },
        { TEXT("vertical_fov"), TEXT("34.999999999") }, { TEXT("vertical_fov"), TEXT("100.000000001") },
        { TEXT("master_volume"), TEXT("-0.000000001") }, { TEXT("master_volume"), TEXT("1.000000001") },
        { TEXT("hud_scale"), TEXT("0.799999999") }, { TEXT("hud_scale"), TEXT("1.400000001") },
        { TEXT("text_scale"), TEXT("0.999999999") }, { TEXT("text_scale"), TEXT("2.000000001") }
    };
    for (const FBadField& Bad : BadValues)
    {
        Reject(WithV2Field(Bad.Field, Bad.Literal), FString::Printf(TEXT("%s rejects range/fraction %s"), Bad.Field, Bad.Literal));
    }
    Reject(TEXT("{\"version\":1,\"vertical_fov\":\"65\"}"), TEXT("v1 FOV also requires a number"));
    Reject(TEXT("[]"), TEXT("Non-object settings fail closed"));
    Reject(TEXT("not-json"), TEXT("Malformed JSON fails closed"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProductSettingsHistorySafetyTest, "AeroSim.ProductUX.SettingsSafety.ApplyCancelRollbackHistory", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProductSettingsHistorySafetyTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPhase7SettingsTransaction Transaction;
    const FString Original = FPhase7SaveCodec::Encode(Transaction.Applied);
    FPhase7Settings First = Transaction.Applied;
    First.VerticalFov = 72.0F;
    First.ResolutionWidth = 1920;
    First.ResolutionHeight = 1080;
    First.HudScale = 1.2F;
    First.TextScale = 1.5F;
    First.bReducedMotion = true;
    FPhase7Settings Second = First;
    Second.VerticalFov = 80.0F;
    Second.WindowMode = EPhase7WindowMode::Borderless;
    const FString FirstJson = FPhase7SaveCodec::Encode(First);
    const FString SecondJson = FPhase7SaveCodec::Encode(Second);

    TestFalse(TEXT("Cannot apply without staging"), Transaction.Apply());
    TestFalse(TEXT("Cannot roll back without history"), Transaction.Rollback());
    TestTrue(TEXT("First candidate begins"), Transaction.Begin(First));
    TestFalse(TEXT("Overlapping transaction fails"), Transaction.Begin(Second));
    TestEqual(TEXT("Begin keeps applied state untouched"), FPhase7SaveCodec::Encode(Transaction.Applied), Original);
    TestTrue(TEXT("First candidate applies"), Transaction.Apply());
    TestEqual(TEXT("Opening now tracks latest applied"), FPhase7SaveCodec::Encode(Transaction.Opening), FirstJson);
    TestEqual(TEXT("Rollback snapshot retains original"), FPhase7SaveCodec::Encode(Transaction.PreviousApplied), Original);
    TestTrue(TEXT("Another preview begins"), Transaction.Begin(Second));
    TestEqual(TEXT("Preview cancellation baseline is latest applied"), FPhase7SaveCodec::Encode(Transaction.Opening), FirstJson);
    TestFalse(TEXT("Raw transaction rollback cannot run while preview is active"), Transaction.Rollback());
    Transaction.Cancel();
    TestEqual(TEXT("Cancel does not undo committed Apply"), FPhase7SaveCodec::Encode(Transaction.Applied), FirstJson);
    TestEqual(TEXT("Cancel clears staged DTO"), FPhase7SaveCodec::Encode(Transaction.Preview), FirstJson);
    TestTrue(TEXT("Cancel preserves rollback eligibility"), Transaction.bCanRollback);
    TestEqual(TEXT("Cancel preserves previous applied history"), FPhase7SaveCodec::Encode(Transaction.PreviousApplied), Original);
    TestTrue(TEXT("Rollback restores original including accessibility"), Transaction.Rollback());
    TestEqual(TEXT("Original full DTO restored"), FPhase7SaveCodec::Encode(Transaction.Applied), Original);
    TestEqual(TEXT("Rollback synchronizes Opening"), FPhase7SaveCodec::Encode(Transaction.Opening), Original);
    TestFalse(TEXT("Rollback is only one level"), Transaction.Rollback());

    TestTrue(TEXT("Reapply first"), Transaction.Begin(First) && Transaction.Apply());
    TestTrue(TEXT("Apply second"), Transaction.Begin(Second) && Transaction.Apply());
    TestEqual(TEXT("Latest history is first, not original"), FPhase7SaveCodec::Encode(Transaction.PreviousApplied), FirstJson);
    TestTrue(TEXT("Third preview starts"), Transaction.Begin(First));
    Transaction.Preview.HudScale = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Corrupted preview cannot apply"), Transaction.Apply());
    TestEqual(TEXT("Failed Apply preserves committed state"), FPhase7SaveCodec::Encode(Transaction.Applied), SecondJson);
    TestEqual(TEXT("Failed Apply preserves history"), FPhase7SaveCodec::Encode(Transaction.PreviousApplied), FirstJson);
    Transaction.Cancel();
    TestTrue(TEXT("Latest Apply rolls back exactly once"), Transaction.Rollback());
    TestEqual(TEXT("Second Apply rolls back to first"), FPhase7SaveCodec::Encode(Transaction.Applied), FirstJson);
    Transaction.Cancel();
    TestFalse(TEXT("Cancel cannot re-enable consumed rollback"), Transaction.bCanRollback);

    Transaction.Applied.MasterVolume = std::numeric_limits<float>::infinity();
    TestFalse(TEXT("Cannot preview from corrupted applied settings"), Transaction.Begin(First));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProductSettingsPreviewTimeoutTest, "AeroSim.ProductUX.SettingsSafety.ExplicitPreviewCountdown", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProductSettingsPreviewTimeoutTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FProductDisplaySettings Display;
    TestTrue(TEXT("Display opens"), Display.Open(FPhase7Settings()));
    TestFalse(TEXT("Preview requires an explicitly staged candidate"), Display.BeginPreview());
    Display.ChangeFocusedValue(1);
    TestTrue(TEXT("Value is staged"), Display.Transaction.bPreviewActive);
    TestFalse(TEXT("Staging alone is not a visible preview"), Display.bPreviewOnDisplay);
    TestFalse(TEXT("Inactive clock cannot expire staging"), Display.AdvancePreview(100.0));
    TestEqual(TEXT("Staging has no active countdown"), Display.PreviewSecondsRemaining, 0.0);
    TestTrue(TEXT("Explicit preview starts"), Display.BeginPreview());
    TestEqual(TEXT("Preview gets fifteen seconds"), Display.PreviewSecondsRemaining, 15.0);
    const FString PreviewStatus = Display.Status;
    TestFalse(TEXT("Zero elapsed time is safe"), Display.AdvancePreview(0.0));
    TestFalse(TEXT("Fourteen seconds has not timed out"), Display.AdvancePreview(14.0));
    TestEqual(TEXT("One second remains"), Display.PreviewSecondsRemaining, 1.0);
    TestEqual(TEXT("Countdown does not churn stable status"), Display.Status, PreviewStatus);
    TestFalse(TEXT("Repeated preview cannot reset safety deadline"), Display.BeginPreview());
    Display.ChangeFocusedValue(1);
    TestTrue(TEXT("Editing cannot silently disable visible preview"), Display.bPreviewOnDisplay);
    TestEqual(TEXT("Editing cannot extend timeout"), Display.PreviewSecondsRemaining, 1.0);
    TestEqual(TEXT("Visible candidate stays frozen during legacy edit"), Display.Transaction.Preview.ResolutionWidth, 1920);
    TestFalse(TEXT("Half second leaves preview active"), Display.AdvancePreview(0.5));
    TestTrue(TEXT("Exactly fifteen seconds emits rollback signal"), Display.AdvancePreview(0.5));
    TestFalse(TEXT("Timeout clears active display preview"), Display.bPreviewOnDisplay);
    TestFalse(TEXT("Timeout clears staged transaction"), Display.Transaction.bPreviewActive);
    TestEqual(TEXT("Timeout clears countdown"), Display.PreviewSecondsRemaining, 0.0);
    TestEqual(TEXT("Timeout restores applied visible DTO"), Display.GetVisible().ResolutionWidth, 1440);
    TestEqual(TEXT("Timeout clears staged width"), Display.Transaction.Preview.ResolutionWidth, 1440);
    TestFalse(TEXT("Timeout never creates rollback history"), Display.Transaction.bCanRollback);
    TestFalse(TEXT("Timeout signal is one-shot"), Display.AdvancePreview(15.0));

    Display.ChangeFocusedValue(1);
    TestTrue(TEXT("Commit first display candidate"), Display.Apply());
    Display.ChangeFocusedValue(-1);
    TestTrue(TEXT("Preview after Apply"), Display.BeginPreview());
    TestTrue(TEXT("Long wall-clock gap expires second preview"), Display.AdvancePreview(60.0));
    TestEqual(TEXT("Timeout cannot undo prior Apply"), Display.Transaction.Applied.ResolutionWidth, 1920);
    TestEqual(TEXT("Opening remains latest applied"), Display.Transaction.Opening.ResolutionWidth, 1920);
    TestTrue(TEXT("Timeout preserves earlier rollback eligibility"), Display.Transaction.bCanRollback);
    TestEqual(TEXT("Timeout preserves earlier history"), Display.Transaction.PreviousApplied.ResolutionWidth, 1440);
    TestTrue(TEXT("User can still roll back previous Apply"), Display.Rollback());
    TestEqual(TEXT("Rollback returns to original width"), Display.Transaction.Applied.ResolutionWidth, 1440);

    const double BadDeltas[] = { std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(), -0.01, std::numeric_limits<double>::max() };
    for (const double Delta : BadDeltas)
    {
        FProductDisplaySettings InvalidClock;
        TestTrue(TEXT("Invalid-clock display opens"), InvalidClock.Open(FPhase7Settings()));
        InvalidClock.ChangeFocusedValue(1);
        TestTrue(TEXT("Invalid-clock preview starts"), InvalidClock.BeginPreview());
        TestTrue(TEXT("Invalid elapsed time fails closed to applied settings"), InvalidClock.AdvancePreview(Delta));
        TestFalse(TEXT("Invalid clock clears preview"), InvalidClock.Transaction.bPreviewActive);
        TestEqual(TEXT("Invalid clock clears timeout"), InvalidClock.PreviewSecondsRemaining, 0.0);
        TestEqual(TEXT("Invalid clock restores default display"), InvalidClock.GetVisible().ResolutionWidth, 1440);
        TestFalse(TEXT("Invalid clock expiry only fires once"), InvalidClock.AdvancePreview(Delta));
    }
    for (const double Remaining : { std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -1.0, 0.0, 16.0 })
    {
        FProductDisplaySettings InvalidTimer;
        TestTrue(TEXT("Invalid-timer display opens"), InvalidTimer.Open(FPhase7Settings()));
        InvalidTimer.ChangeFocusedValue(1);
        TestTrue(TEXT("Invalid-timer preview starts"), InvalidTimer.BeginPreview());
        InvalidTimer.PreviewSecondsRemaining = Remaining;
        TestTrue(TEXT("Corrupted public countdown fails closed"), InvalidTimer.AdvancePreview(0.0));
    }
    FProductDisplaySettings InvalidPreview;
    TestTrue(TEXT("Corrupted-preview display opens"), InvalidPreview.Open(FPhase7Settings()));
    InvalidPreview.ChangeFocusedValue(1);
    InvalidPreview.Transaction.Preview.VerticalFov = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Invalid staged settings cannot start visible preview"), InvalidPreview.BeginPreview());
    TestEqual(TEXT("Rejected preview cannot arm timer"), InvalidPreview.PreviewSecondsRemaining, 0.0);
    InvalidPreview.Cancel();
    InvalidPreview.ChangeFocusedValue(1);
    TestTrue(TEXT("Valid preview can start after cancellation"), InvalidPreview.BeginPreview());
    InvalidPreview.Transaction.Preview.TextScale = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Invalid visible preview cannot apply"), InvalidPreview.Apply());
    TestTrue(TEXT("Failed Apply leaves recovery timer armed"), InvalidPreview.bPreviewOnDisplay);
    TestTrue(TEXT("Corrupted candidate expires safely at next tick"), InvalidPreview.AdvancePreview(0.0));
    TestEqual(TEXT("Corrupted candidate restores committed values"), InvalidPreview.GetVisible().ResolutionWidth, 1440);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProductSettingsFocusWrappingTest, "AeroSim.ProductUX.SettingsSafety.LegacyFocusWrapping", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProductSettingsFocusWrappingTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    TestEqual(TEXT("Six-row legacy API remains"), FProductDisplaySettings::RowCount, 6);
    const TCHAR* Labels[] = { TEXT("RESOLUTION"), TEXT("WINDOW MODE"), TEXT("PREVIEW"), TEXT("APPLY"), TEXT("CANCEL"), TEXT("ROLLBACK") };
    const int32 Deltas[] = { MIN_int32, MAX_int32, -100, -7, -6, -1, 0, 1, 6, 7, 100 };
    for (int32 Row = 0; Row < FProductDisplaySettings::RowCount; ++Row)
    {
        for (const int32 Delta : Deltas)
        {
            FProductDisplaySettings Display;
            TestTrue(TEXT("Focus display opens"), Display.Open(FPhase7Settings()));
            Display.FocusedRow = Row;
            Display.MoveFocus(Delta);
            const int64 Position = static_cast<int64>(Row) + Delta;
            const int32 Expected = static_cast<int32>((Position % 6 + 6) % 6);
            TestEqual(TEXT("Any signed delta wraps without overflow"), Display.FocusedRow, Expected);
            TestEqual(TEXT("Wrapped focus keeps correct legacy action"), Display.GetFocusedAction(), FString(Labels[Expected]));

            Display.FocusedRow = 0;
            Display.ChangeFocusedValue(Delta);
            const TArray<FIntPoint> Resolutions = FProductDisplaySettings::SupportedResolutions();
            const int32 ResolutionCount = Resolutions.Num();
            const int64 ValuePosition = int64(1) + Delta;
            const int32 ValueIndex = static_cast<int32>((ValuePosition % ResolutionCount + ResolutionCount) % ResolutionCount);
            const FIntPoint Resolution = Resolutions[ValueIndex];
            TestEqual(TEXT("Resolution wraps large signed delta"), Display.Transaction.Preview.ResolutionWidth, Resolution.X);
            TestEqual(TEXT("Wrapped resolution retains valid pair"), Display.Transaction.Preview.ResolutionHeight, Resolution.Y);
            Display.Cancel();
            Display.FocusedRow = 1;
            Display.ChangeFocusedValue(Delta);
            const int32 ModeIndex = static_cast<int32>((static_cast<int64>(Delta) % 3 + 3) % 3);
            TestEqual(TEXT("Window mode wraps large signed delta"), Display.Transaction.Preview.WindowMode, FProductDisplaySettings::SupportedWindowModes()[ModeIndex]);
        }
    }
    FProductDisplaySettings CorruptedFocus;
    CorruptedFocus.FocusedRow = MIN_int32;
    CorruptedFocus.MoveFocus(MIN_int32);
    TestTrue(TEXT("Public invalid focus normalizes to valid row"), CorruptedFocus.FocusedRow >= 0 && CorruptedFocus.FocusedRow < 6);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProductSettingsLifecyclePersistenceTest, "AeroSim.ProductUX.SettingsSafety.NoImplicitPersistence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProductSettingsLifecyclePersistenceTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    // Read-only sentinels: pure settings operations must not create/change final, backup or temp saves.
    const TArray<FString> FilesBefore = ReadProductSaveSnapshot();
    FProductDisplaySettings Display;
    TestTrue(TEXT("Persistence display opens"), Display.Open(FPhase7Settings()));
    const FString Persisted = FPhase7SaveCodec::Encode(Display.Transaction.Applied);
    Display.ChangeFocusedValue(1);
    TestEqual(TEXT("Staging cannot change persistence source"), FPhase7SaveCodec::Encode(Display.Transaction.Applied), Persisted);
    TestTrue(TEXT("Explicit preview starts"), Display.BeginPreview());
    TestEqual(TEXT("Preview cannot change persistence source"), FPhase7SaveCodec::Encode(Display.Transaction.Applied), Persisted);
    TestTrue(TEXT("Apply updates memory"), Display.Apply());
    TestFalse(TEXT("Apply clears visible preview"), Display.bPreviewOnDisplay);
    TestFalse(TEXT("Apply clears staged transaction"), Display.Transaction.bPreviewActive);
    TestEqual(TEXT("Apply clears timer"), Display.PreviewSecondsRemaining, 0.0);
    TestEqual(TEXT("Apply synchronizes staged DTO"), Display.Transaction.Preview.ResolutionWidth, 1920);
    TestTrue(TEXT("Apply explicitly says NOT SAVED"), Display.Status.Contains(TEXT("NOT SAVED")));
    TestFalse(TEXT("Apply does not claim crash-safe saving"), Display.Status.Contains(TEXT("CRASH-SAFE SAVED")));

    Display.ChangeFocusedValue(-1);
    TestTrue(TEXT("Another preview begins"), Display.BeginPreview());
    Display.Cancel();
    TestEqual(TEXT("Cancel after Apply keeps committed width"), Display.Transaction.Applied.ResolutionWidth, 1920);
    TestFalse(TEXT("Cancel clears stage"), Display.Transaction.bPreviewActive);
    TestEqual(TEXT("Cancel clears timer"), Display.PreviewSecondsRemaining, 0.0);
    TestFalse(TEXT("Cancel cannot cause a later timeout"), Display.AdvancePreview(15.0));

    Display.ChangeFocusedValue(-1);
    TestTrue(TEXT("Preview before rollback begins"), Display.BeginPreview());
    TestTrue(TEXT("Display rollback discards preview then consumes Apply history"), Display.Rollback());
    TestFalse(TEXT("Rollback clears stage"), Display.Transaction.bPreviewActive);
    TestFalse(TEXT("Rollback clears visible preview"), Display.bPreviewOnDisplay);
    TestEqual(TEXT("Rollback clears timer"), Display.PreviewSecondsRemaining, 0.0);
    TestTrue(TEXT("Rollback explicitly says NOT SAVED"), Display.Status.Contains(TEXT("NOT SAVED")));
    TestEqual(TEXT("Rollback restores full persistence source"), FPhase7SaveCodec::Encode(Display.Transaction.Applied), Persisted);
    TestFalse(TEXT("Consumed history cannot roll back twice"), Display.Rollback());

    Display.ChangeFocusedValue(1);
    TestTrue(TEXT("Commit for reopen test"), Display.Apply());
    const FPhase7Settings LatestApplied = Display.Transaction.Applied;
    Display.ChangeFocusedValue(-1);
    TestTrue(TEXT("Stage before reopen test"), Display.BeginPreview());
    Display.bOpen = false;
    TestTrue(TEXT("Reopen from committed state"), Display.Open(LatestApplied));
    TestFalse(TEXT("Reopen clears stale preview"), Display.Transaction.bPreviewActive);
    TestFalse(TEXT("Reopen clears previous session history"), Display.Transaction.bCanRollback);
    TestFalse(TEXT("Reopen clears visible preview"), Display.bPreviewOnDisplay);
    TestEqual(TEXT("Reopen clears timeout"), Display.PreviewSecondsRemaining, 0.0);
    TestEqual(TEXT("Reopen retains committed width"), Display.GetVisible().ResolutionWidth, 1920);
    TestEqual(TEXT("Reopen preserves latest cancellation baseline"), Display.Transaction.Opening.ResolutionWidth, 1920);
    TestFalse(TEXT("Already-open session cannot be replaced"), Display.Open(FPhase7Settings()));
    Display.bOpen = false;
    TestTrue(TEXT("Reopen safely accepts its own Applied member"), Display.Open(Display.Transaction.Applied));
    TestEqual(TEXT("Resetting session does not overwrite aliased applied argument"), Display.Transaction.Applied.ResolutionWidth, 1920);
    Display.bOpen = false;
    FPhase7Settings Invalid;
    Invalid.TextScale = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Invalid settings cannot reopen"), Display.Open(Invalid));
    TestFalse(TEXT("Cannot apply while display is closed"), Display.Apply());
    TestTrue(TEXT("Pure operations never write final/backup/temp files"), ReadProductSaveSnapshot() == FilesBefore);
    return true;
}

#endif
