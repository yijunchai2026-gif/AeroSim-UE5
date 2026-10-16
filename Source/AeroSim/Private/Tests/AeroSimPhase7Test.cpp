#if WITH_DEV_AUTOMATION_TESTS

#include "AeroSimRuntimeContract.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Phase7Product.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhase7PerformanceContractTest, "AeroSim.Phase7.Performance.FailClosedBudget", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPhase7PerformanceContractTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    const FPhase7PerformanceBudget Mac = FPhase7PerformanceBudget::MacDevelopmentHigh();
    const FPhase7PerformanceBudget Windows = FPhase7PerformanceBudget::WindowsHighCandidate();
    const FPhase7PerformanceBudget IPad = FPhase7PerformanceBudget::IPadCandidate();
    TestTrue(TEXT("All profile contracts are valid"), Mac.IsValid() && Windows.IsValid() && IPad.IsValid());
    TestFalse(TEXT("Nanite defaults off for Mac until measured"), Mac.bNanite);
    TestFalse(TEXT("Nanite defaults off for iPad"), IPad.bNanite);

    FPhase7PerformanceEvidence Missing;
    FPhase7GateResult Result = FPhase7PerformanceGate::Evaluate(Mac, Missing);
    TestFalse(TEXT("Missing evidence cannot pass"), Result.bPass);
    TestTrue(TEXT("Missing evidence explains provenance failure"), Result.Reasons.Contains(TEXT("MISSING_HARDWARE_ID")));

    FPhase7PerformanceEvidence Complete;
    Complete.BuildId = FAeroSimRuntimeContract::BuildId;
    Complete.Platform = TEXT("Mac");
    Complete.HardwareId = TEXT("frozen-test-hardware");
    Complete.Rhi = TEXT("Metal SM6");
    Complete.QualityProfileId = Mac.ProfileId;
    Complete.WarmupSeconds = 120;
    Complete.DurationSeconds = 1800;
    Complete.ColdStarts = 3;
    Complete.bNonDevelopmentBuild = true;
    Complete.bTargetHardware = true;
    Complete.bCompleteReplay = true;
    Complete.FrameP95Ms = 16.0;
    Complete.GameThreadP95Ms = 7.0;
    Complete.RenderThreadP95Ms = 7.0;
    Complete.GpuP95Ms = 13.0;
    Complete.ResidentMemoryMiB = 3500;
    Complete.DrawCalls = 1800;
    Complete.StreamingPoolMiB = 1800;
    Complete.StreamingSpikeP95Ms = 3.0;
    TestTrue(TEXT("Synthetic complete structure exercises pass path but is not release evidence"), FPhase7PerformanceGate::Evaluate(Mac, Complete).bPass);
    Complete.GpuP95Ms = 15.0;
    TestFalse(TEXT("Exceeded GPU budget fails"), FPhase7PerformanceGate::Evaluate(Mac, Complete).bPass);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhase7SettingsSaveTest, "AeroSim.Phase7.Product.SettingsSaveMigrationRollback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPhase7SettingsSaveTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPhase7SettingsTransaction Transaction;
    FPhase7Settings Candidate = Transaction.Applied;
    Candidate.VerticalFov = 72.0F;
    Candidate.QualityTier = EPhase7QualityTier::DesktopLow;
    TestTrue(TEXT("Valid preview begins"), Transaction.Begin(Candidate));
    TestTrue(TEXT("Preview applies"), Transaction.Apply());
    TestEqual(TEXT("Applied FOV persists"), Transaction.Applied.VerticalFov, 72.0F);
    TestTrue(TEXT("Applied settings can roll back"), Transaction.Rollback());
    TestEqual(TEXT("Rollback restores opening FOV"), Transaction.Applied.VerticalFov, 60.0F);

    FString Error;
    FPhase7Settings Migrated;
    TestTrue(TEXT("v1 migrates with privacy-safe defaults"), FPhase7SaveCodec::DecodeAndMigrate(TEXT("{\"version\":1,\"vertical_fov\":65}"), Migrated, Error));
    TestEqual(TEXT("Migration reaches current version"), Migrated.Version, FPhase7Settings::CurrentVersion);
    TestFalse(TEXT("Telemetry remains opt-in after migration"), Migrated.bTelemetryEnabled);
    TestFalse(TEXT("Future versions fail closed"), FPhase7SaveCodec::DecodeAndMigrate(TEXT("{\"version\":99,\"vertical_fov\":65}"), Migrated, Error));

    const FString SavePath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/Phase7/settings.json"));
    IPlatformFile& Files = FPlatformFileManager::Get().GetPlatformFile();
    Files.CreateDirectoryTree(*FPaths::GetPath(SavePath));
    Files.DeleteFile(*SavePath);
    Files.DeleteFile(*(SavePath + TEXT(".bak")));
    TestTrue(TEXT("Initial crash-safe save succeeds"), FPhase7SaveCodec::WriteCrashSafe(SavePath, FPhase7SaveCodec::Encode(Transaction.Applied), Error));
    Candidate.VerticalFov = 70.0F;
    TestTrue(TEXT("Replacement preserves backup"), FPhase7SaveCodec::WriteCrashSafe(SavePath, FPhase7SaveCodec::Encode(Candidate), Error));
    TestTrue(TEXT("Last valid save backup exists"), Files.FileExists(*(SavePath + TEXT(".bak"))));
    Files.DeleteFile(*SavePath);
    Files.DeleteFile(*(SavePath + TEXT(".bak")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhase7PrivacySoakTest, "AeroSim.Phase7.Product.PrivacyAndDeterministicSoak", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPhase7PrivacySoakTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    const FString Sanitized = FPhase7LogSanitizer::Redact(TEXT("token=abc password:xyz user@example.com /Users/private/file Bearer qwerty"));
    TestFalse(TEXT("Token value is removed"), Sanitized.Contains(TEXT("abc")));
    TestFalse(TEXT("Password value is removed"), Sanitized.Contains(TEXT("xyz")));
    TestFalse(TEXT("Bearer credential is removed"), Sanitized.Contains(TEXT("qwerty")));
    TestFalse(TEXT("Email is removed"), Sanitized.Contains(TEXT("user@example.com")));
    TestFalse(TEXT("User path is removed"), Sanitized.Contains(TEXT("/Users/private")));

    int32 StepsA = 0;
    int32 StepsB = 0;
    const uint64 A = FPhase7DeterministicSoak::Run(1800, 21630, StepsA);
    const uint64 B = FPhase7DeterministicSoak::Run(1800, 21630, StepsB);
    TestEqual(TEXT("Thirty-minute fixed-step count"), StepsA, 108000);
    TestEqual(TEXT("Replay repeats step count"), StepsB, StepsA);
    TestEqual(TEXT("Replay digest is deterministic"), B, A);
    TestNotEqual(TEXT("Different seed changes replay digest"), FPhase7DeterministicSoak::Run(1800, 21631, StepsB), A);
    TestTrue(TEXT("Runtime is phase7/v1"), FAeroSimRuntimeContract::IsValid());
    return true;
}

#endif
