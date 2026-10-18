#include "Phase7Product.h"

#include "Dom/JsonObject.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
bool ReadFiniteNumber(const FJsonObject& Root, const TCHAR* Field, double& OutNumber)
{
    // Json TryGet accessors can coerce strings/bools. Settings require exact JSON types.
    return Root.HasTypedField<EJson::Number>(Field) && Root.TryGetNumberField(Field, OutNumber)
        && FMath::IsFinite(OutNumber);
}

bool ReadInteger(const FJsonObject& Root, const TCHAR* Field, int32& OutNumber, int32 Minimum, int32 Maximum)
{
    double Number = 0.0;
    if (!ReadFiniteNumber(Root, Field, Number) || Number < Minimum || Number > Maximum
        || FMath::TruncToDouble(Number) != Number)
    {
        return false;
    }
    OutNumber = static_cast<int32>(Number);
    return true;
}

bool ReadFloat(const FJsonObject& Root, const TCHAR* Field, float& OutNumber, double Minimum, double Maximum)
{
    double Number = 0.0;
    // Check the original double before conversion so overflow and out-of-range rounding cannot pass.
    if (!ReadFiniteNumber(Root, Field, Number) || Number < Minimum || Number > Maximum)
    {
        return false;
    }
    OutNumber = static_cast<float>(Number);
    return FMath::IsFinite(OutNumber);
}

bool ReadBoolean(const FJsonObject& Root, const TCHAR* Field, bool& OutValue)
{
    return Root.HasTypedField<EJson::Boolean>(Field) && Root.TryGetBoolField(Field, OutValue);
}

FPhase7PerformanceBudget MakeBudget(const TCHAR* Id, EPhase7QualityTier Tier)
{
    FPhase7PerformanceBudget Result;
    Result.ProfileId = Id;
    Result.Tier = Tier;
    return Result;
}

void AddExceeded(TArray<FString>& Reasons, const TCHAR* Name, double Actual, double Limit)
{
    if (!FMath::IsFinite(Actual) || Actual <= 0.0 || Actual > Limit)
    {
        Reasons.Add(FString::Printf(TEXT("%s actual=%.3f limit=%.3f"), Name, Actual, Limit));
    }
}

FString RedactAssignment(const FString& Input, const FString& Key)
{
    FString Result = Input;
    int32 SearchFrom = 0;
    while (SearchFrom < Result.Len())
    {
        const int32 KeyAt = Result.Find(Key, ESearchCase::IgnoreCase, ESearchDir::FromStart, SearchFrom);
        if (KeyAt == INDEX_NONE)
        {
            break;
        }
        int32 Separator = KeyAt + Key.Len();
        while (Separator < Result.Len() && FChar::IsWhitespace(Result[Separator])) { ++Separator; }
        if (Separator >= Result.Len() || (Result[Separator] != TEXT('=') && Result[Separator] != TEXT(':')))
        {
            SearchFrom = KeyAt + Key.Len();
            continue;
        }
        int32 ValueStart = Separator + 1;
        while (ValueStart < Result.Len() && (FChar::IsWhitespace(Result[ValueStart]) || Result[ValueStart] == TEXT('"'))) { ++ValueStart; }
        int32 ValueEnd = ValueStart;
        while (ValueEnd < Result.Len() && !FChar::IsWhitespace(Result[ValueEnd]) && Result[ValueEnd] != TEXT(',') && Result[ValueEnd] != TEXT('"')) { ++ValueEnd; }
        Result = Result.Left(ValueStart) + TEXT("[REDACTED]") + Result.Mid(ValueEnd);
        SearchFrom = ValueStart + 10;
    }
    return Result;
}

FString RedactBearer(const FString& Input)
{
    FString Result = Input;
    int32 SearchFrom = 0;
    while (SearchFrom < Result.Len())
    {
        const int32 Marker = Result.Find(TEXT("Bearer "), ESearchCase::IgnoreCase, ESearchDir::FromStart, SearchFrom);
        if (Marker == INDEX_NONE) { break; }
        const int32 ValueStart = Marker + 7;
        int32 ValueEnd = ValueStart;
        while (ValueEnd < Result.Len() && !FChar::IsWhitespace(Result[ValueEnd]) && Result[ValueEnd] != TEXT(',') && Result[ValueEnd] != TEXT('"')) { ++ValueEnd; }
        Result = Result.Left(ValueStart) + TEXT("[REDACTED]") + Result.Mid(ValueEnd);
        SearchFrom = ValueStart + 10;
    }
    return Result;
}
}

FPhase7PerformanceBudget FPhase7PerformanceBudget::MacDevelopmentHigh()
{
    FPhase7PerformanceBudget Result = MakeBudget(TEXT("mac-development-high-v1"), EPhase7QualityTier::DesktopHigh);
    Result.bNanite = false;
    return Result;
}

FPhase7PerformanceBudget FPhase7PerformanceBudget::WindowsHighCandidate()
{
    FPhase7PerformanceBudget Result = MakeBudget(TEXT("windows-1440p-high-candidate-v1"), EPhase7QualityTier::DesktopHigh);
    Result.ResidentMemoryMiB = 6144;
    Result.bNanite = true;
    return Result;
}

FPhase7PerformanceBudget FPhase7PerformanceBudget::IPadCandidate()
{
    FPhase7PerformanceBudget Result = MakeBudget(TEXT("ipad-device-candidate-v1"), EPhase7QualityTier::Tablet);
    Result.FrameP95Ms = 33.33;
    Result.GameThreadP95Ms = 14.0;
    Result.RenderThreadP95Ms = 14.0;
    Result.GpuP95Ms = 28.0;
    Result.ResidentMemoryMiB = 2048;
    Result.DrawCalls = 800;
    Result.StreamingPoolMiB = 768;
    Result.StreamingSpikeP95Ms = 8.0;
    Result.bHlod = true;
    Result.bNanite = false;
    return Result;
}

bool FPhase7PerformanceBudget::IsValid() const
{
    return !ProfileId.IsEmpty() && FrameP95Ms > 0.0 && GameThreadP95Ms > 0.0 && RenderThreadP95Ms > 0.0
        && GpuP95Ms > 0.0 && ResidentMemoryMiB > 0 && DrawCalls > 0 && StreamingPoolMiB > 0
        && StreamingSpikeP95Ms > 0.0;
}

FPhase7GateResult FPhase7PerformanceGate::Evaluate(const FPhase7PerformanceBudget& Budget, const FPhase7PerformanceEvidence& Evidence)
{
    FPhase7GateResult Result;
    if (!Budget.IsValid()) { Result.Reasons.Add(TEXT("INVALID_BUDGET")); }
    if (Evidence.BuildId.IsEmpty()) { Result.Reasons.Add(TEXT("MISSING_BUILD_ID")); }
    if (Evidence.Platform.IsEmpty()) { Result.Reasons.Add(TEXT("MISSING_PLATFORM")); }
    if (Evidence.HardwareId.IsEmpty()) { Result.Reasons.Add(TEXT("MISSING_HARDWARE_ID")); }
    if (Evidence.Rhi.IsEmpty()) { Result.Reasons.Add(TEXT("MISSING_RHI")); }
    if (Evidence.QualityProfileId != Budget.ProfileId) { Result.Reasons.Add(TEXT("QUALITY_PROFILE_MISMATCH")); }
    if (!Evidence.bTargetHardware) { Result.Reasons.Add(TEXT("NOT_TARGET_HARDWARE")); }
    if (!Evidence.bNonDevelopmentBuild) { Result.Reasons.Add(TEXT("NON_DEVELOPMENT_BUILD_REQUIRED")); }
    if (!Evidence.bCompleteReplay || Evidence.WarmupSeconds < 120 || Evidence.DurationSeconds < 1800 || Evidence.ColdStarts < 3)
    {
        Result.Reasons.Add(TEXT("INCOMPLETE_QUALIFICATION_PROTOCOL"));
    }
    AddExceeded(Result.Reasons, TEXT("frame_p95_ms"), Evidence.FrameP95Ms, Budget.FrameP95Ms);
    AddExceeded(Result.Reasons, TEXT("game_thread_p95_ms"), Evidence.GameThreadP95Ms, Budget.GameThreadP95Ms);
    AddExceeded(Result.Reasons, TEXT("render_thread_p95_ms"), Evidence.RenderThreadP95Ms, Budget.RenderThreadP95Ms);
    AddExceeded(Result.Reasons, TEXT("gpu_p95_ms"), Evidence.GpuP95Ms, Budget.GpuP95Ms);
    AddExceeded(Result.Reasons, TEXT("resident_memory_mib"), Evidence.ResidentMemoryMiB, Budget.ResidentMemoryMiB);
    AddExceeded(Result.Reasons, TEXT("draw_calls"), Evidence.DrawCalls, Budget.DrawCalls);
    AddExceeded(Result.Reasons, TEXT("streaming_pool_mib"), Evidence.StreamingPoolMiB, Budget.StreamingPoolMiB);
    AddExceeded(Result.Reasons, TEXT("streaming_spike_p95_ms"), Evidence.StreamingSpikeP95Ms, Budget.StreamingSpikeP95Ms);
    Result.bPass = Result.Reasons.IsEmpty();
    Result.Status = Result.bPass ? TEXT("PASS") : TEXT("BLOCKED_OR_FAILED");
    return Result;
}

bool FPhase7Settings::IsValid() const
{
    const bool bSupportedResolution = (ResolutionWidth == 1280 && ResolutionHeight == 720)
        || (ResolutionWidth == 1440 && ResolutionHeight == 900)
        || (ResolutionWidth == 1920 && ResolutionHeight == 1080);
    const bool bSupportedWindowMode = static_cast<uint8>(WindowMode) <= static_cast<uint8>(EPhase7WindowMode::Fullscreen);
    const bool bSupportedQuality = static_cast<uint8>(QualityTier) <= static_cast<uint8>(EPhase7QualityTier::Tablet);
    return Version == CurrentVersion && FMath::IsFinite(VerticalFov) && VerticalFov >= 35.0F && VerticalFov <= 100.0F
        && FMath::IsFinite(MasterVolume) && MasterVolume >= 0.0F && MasterVolume <= 1.0F
        && FMath::IsFinite(HudScale) && HudScale >= 0.8F && HudScale <= 1.4F
        && FMath::IsFinite(TextScale) && TextScale >= 1.0F && TextScale <= 2.0F
        && bSupportedResolution && bSupportedWindowMode && bSupportedQuality;
}

bool FPhase7SettingsTransaction::Begin(const FPhase7Settings& Candidate)
{
    if (!Applied.IsValid() || !Candidate.IsValid() || bPreviewActive) { return false; }
    Opening = Applied;
    Preview = Candidate;
    bPreviewActive = true;
    return true;
}

bool FPhase7SettingsTransaction::Apply()
{
    if (!bPreviewActive || !Applied.IsValid() || !Preview.IsValid()) { return false; }
    PreviousApplied = Applied;
    Applied = Preview;
    Opening = Applied;
    Preview = Applied;
    bPreviewActive = false;
    bCanRollback = true;
    return true;
}

void FPhase7SettingsTransaction::Cancel()
{
    Opening = Applied;
    Preview = Applied;
    bPreviewActive = false;
}

bool FPhase7SettingsTransaction::Rollback()
{
    if (bPreviewActive || !bCanRollback || !PreviousApplied.IsValid()) { return false; }
    Applied = PreviousApplied;
    Opening = Applied;
    Preview = Applied;
    PreviousApplied = Applied;
    bCanRollback = false;
    return true;
}

FString FPhase7SaveCodec::Encode(const FPhase7Settings& Settings)
{
    if (!Settings.IsValid()) { return FString(); }
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schema"), TEXT("aerosim.settings/v2"));
    Root->SetNumberField(TEXT("version"), Settings.Version);
    Root->SetNumberField(TEXT("vertical_fov"), Settings.VerticalFov);
    Root->SetNumberField(TEXT("quality_tier"), static_cast<uint8>(Settings.QualityTier));
    Root->SetNumberField(TEXT("master_volume"), Settings.MasterVolume);
    Root->SetBoolField(TEXT("telemetry_enabled"), Settings.bTelemetryEnabled);
    Root->SetNumberField(TEXT("resolution_width"), Settings.ResolutionWidth);
    Root->SetNumberField(TEXT("resolution_height"), Settings.ResolutionHeight);
    Root->SetNumberField(TEXT("window_mode"), static_cast<uint8>(Settings.WindowMode));
    Root->SetNumberField(TEXT("hud_scale"), Settings.HudScale);
    Root->SetNumberField(TEXT("text_scale"), Settings.TextScale);
    Root->SetBoolField(TEXT("reduced_motion"), Settings.bReducedMotion);
    FString Output;
    FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Output));
    return Output;
}

bool FPhase7SaveCodec::DecodeAndMigrate(const FString& Json, FPhase7Settings& OutSettings, FString& OutError)
{
    TSharedPtr<FJsonObject> Root;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
    {
        OutError = TEXT("INVALID_JSON"); return false;
    }
    double VersionNumber = 0.0;
    if (!Root->HasField(TEXT("version"))) { OutError = TEXT("MISSING_VERSION"); return false; }
    if (!ReadFiniteNumber(*Root, TEXT("version"), VersionNumber) || FMath::TruncToDouble(VersionNumber) != VersionNumber)
    {
        OutError = TEXT("INVALID_VERSION"); return false;
    }
    if (VersionNumber < 1.0 || VersionNumber > FPhase7Settings::CurrentVersion) { OutError = TEXT("UNSUPPORTED_VERSION"); return false; }
    const int32 Version = static_cast<int32>(VersionNumber);
    if (Root->HasField(TEXT("schema")))
    {
        FString Schema;
        if (!Root->HasTypedField<EJson::String>(TEXT("schema")) || !Root->TryGetStringField(TEXT("schema"), Schema)
            || Schema != FString::Printf(TEXT("aerosim.settings/v%d"), Version))
        {
            OutError = TEXT("INVALID_SCHEMA"); return false;
        }
    }
    FPhase7Settings Candidate;
    Candidate.Version = FPhase7Settings::CurrentVersion;
    if (!Root->HasField(TEXT("vertical_fov"))) { OutError = TEXT("MISSING_VERTICAL_FOV"); return false; }
    if (!ReadFloat(*Root, TEXT("vertical_fov"), Candidate.VerticalFov, 35.0, 100.0)) { OutError = TEXT("INVALID_VERTICAL_FOV"); return false; }
    if (Version >= 2)
    {
        int32 EnumValue = 0;
        if (!ReadInteger(*Root, TEXT("quality_tier"), EnumValue, 0, 2)) { OutError = TEXT("INVALID_QUALITY_TIER"); return false; }
        Candidate.QualityTier = static_cast<EPhase7QualityTier>(EnumValue);
        if (!Root->HasField(TEXT("master_volume"))) { OutError = TEXT("MISSING_MASTER_VOLUME"); return false; }
        if (!ReadFloat(*Root, TEXT("master_volume"), Candidate.MasterVolume, 0.0, 1.0)) { OutError = TEXT("INVALID_MASTER_VOLUME"); return false; }
        if (!Root->HasField(TEXT("telemetry_enabled"))) { OutError = TEXT("MISSING_TELEMETRY_CONSENT"); return false; }
        if (!ReadBoolean(*Root, TEXT("telemetry_enabled"), Candidate.bTelemetryEnabled)) { OutError = TEXT("INVALID_TELEMETRY_CONSENT"); return false; }
        // Only absent optional fields default. Present null/wrong-type/fractional values fail closed.
        if (Root->HasField(TEXT("resolution_width")) && !ReadInteger(*Root, TEXT("resolution_width"), Candidate.ResolutionWidth, 1, MAX_int32))
        {
            OutError = TEXT("INVALID_RESOLUTION_WIDTH"); return false;
        }
        if (Root->HasField(TEXT("resolution_height")) && !ReadInteger(*Root, TEXT("resolution_height"), Candidate.ResolutionHeight, 1, MAX_int32))
        {
            OutError = TEXT("INVALID_RESOLUTION_HEIGHT"); return false;
        }
        if (Root->HasField(TEXT("window_mode")))
        {
            if (!ReadInteger(*Root, TEXT("window_mode"), EnumValue, 0, 2)) { OutError = TEXT("INVALID_WINDOW_MODE"); return false; }
            Candidate.WindowMode = static_cast<EPhase7WindowMode>(EnumValue);
        }
        if (Root->HasField(TEXT("hud_scale")) && !ReadFloat(*Root, TEXT("hud_scale"), Candidate.HudScale, 0.8, 1.4))
        {
            OutError = TEXT("INVALID_HUD_SCALE"); return false;
        }
        if (Root->HasField(TEXT("text_scale")) && !ReadFloat(*Root, TEXT("text_scale"), Candidate.TextScale, 1.0, 2.0))
        {
            OutError = TEXT("INVALID_TEXT_SCALE"); return false;
        }
        if (Root->HasField(TEXT("reduced_motion")) && !ReadBoolean(*Root, TEXT("reduced_motion"), Candidate.bReducedMotion))
        {
            OutError = TEXT("INVALID_REDUCED_MOTION"); return false;
        }
    }
    else
    {
        Candidate.QualityTier = EPhase7QualityTier::DesktopHigh;
        Candidate.MasterVolume = 1.0F;
        Candidate.bTelemetryEnabled = false;
    }
    if (!Candidate.IsValid()) { OutError = TEXT("INVALID_VALUES"); return false; }
    OutSettings = Candidate;
    OutError.Reset();
    return true;
}

bool FPhase7SaveCodec::WriteCrashSafe(const FString& FinalPath, const FString& Payload, FString& OutError)
{
    if (FinalPath.IsEmpty() || Payload.IsEmpty()) { OutError = TEXT("INVALID_SAVE_INPUT"); return false; }
    const FString TempPath = FinalPath + TEXT(".tmp");
    const FString BackupPath = FinalPath + TEXT(".bak");
    IPlatformFile& Files = FPlatformFileManager::Get().GetPlatformFile();
    Files.DeleteFile(*TempPath);
    FTCHARToUTF8 Utf8(*Payload);
    TUniquePtr<IFileHandle> TempFile(Files.OpenWrite(*TempPath, false, false));
    if (!TempFile || !TempFile->Write(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length()) || !TempFile->Flush(true))
    {
        TempFile.Reset();
        Files.DeleteFile(*TempPath);
        OutError = TEXT("TEMP_WRITE_FAILED"); return false;
    }
    TempFile.Reset();
    if (Files.FileExists(*FinalPath))
    {
        Files.DeleteFile(*BackupPath);
        if (!Files.MoveFile(*BackupPath, *FinalPath)) { Files.DeleteFile(*TempPath); OutError = TEXT("BACKUP_FAILED"); return false; }
    }
    if (!Files.MoveFile(*FinalPath, *TempPath))
    {
        if (Files.FileExists(*BackupPath)) { Files.MoveFile(*FinalPath, *BackupPath); }
        Files.DeleteFile(*TempPath);
        OutError = TEXT("ATOMIC_REPLACE_FAILED"); return false;
    }
    OutError.Reset();
    return true;
}

FString FPhase7LogSanitizer::Redact(const FString& Input)
{
    FString Result = RedactBearer(Input.Left(4096));
    for (const FString& Key : { FString(TEXT("token")), FString(TEXT("api_key")), FString(TEXT("apikey")), FString(TEXT("password")), FString(TEXT("secret")) })
    {
        Result = RedactAssignment(Result, Key);
    }
    TArray<FString> Words;
    Result.ParseIntoArrayWS(Words);
    for (const FString& Word : Words)
    {
        if (Word.Contains(TEXT("@")) && Word.Contains(TEXT("."))) { Result.ReplaceInline(*Word, TEXT("[REDACTED_EMAIL]")); }
        if (Word.StartsWith(TEXT("/Users/")) || Word.StartsWith(TEXT("C:\\Users\\"))) { Result.ReplaceInline(*Word, TEXT("[REDACTED_USER_PATH]")); }
    }
    return Result;
}

uint64 FPhase7DeterministicSoak::Run(const int32 DurationSeconds, const int32 Seed, int32& OutSteps)
{
    const int32 SafeDuration = FMath::Clamp(DurationSeconds, 1, 1800);
    OutSteps = SafeDuration * StepsPerSecond;
    uint64 State = static_cast<uint32>(Seed);
    uint64 Digest = 1469598103934665603ULL;
    for (int32 Step = 0; Step < OutSteps; ++Step)
    {
        State = State * 6364136223846793005ULL + 1442695040888963407ULL;
        const uint64 Input = (State >> 24U) ^ static_cast<uint64>(Step);
        Digest ^= Input;
        Digest *= 1099511628211ULL;
    }
    return Digest;
}
