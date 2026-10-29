#include "ProductUX.h"

namespace
{
int32 WrapIndex(int32 Index, int32 Delta, int32 Count)
{
    const int64 Position = static_cast<int64>(Index) + static_cast<int64>(Delta);
    return static_cast<int32>((Position % Count + Count) % Count);
}
}

bool FAeroSimProductUxContract::IsValid()
{
    return Version == 1 && FCString::Strcmp(Schema, TEXT("aerosim.product-ux/v1")) == 0;
}

TArray<FIntPoint> FProductDisplaySettings::SupportedResolutions()
{
    return {
        FIntPoint(1280, 720), FIntPoint(1440, 900), FIntPoint(1920, 1080),
        FIntPoint(2560, 1440), FIntPoint(3840, 2160)
    };
}

TArray<EPhase7WindowMode> FProductDisplaySettings::SupportedWindowModes()
{
    return { EPhase7WindowMode::Windowed, EPhase7WindowMode::Borderless, EPhase7WindowMode::Fullscreen };
}

bool FProductDisplaySettings::Open(const FPhase7Settings& Applied)
{
    if (bOpen || !Applied.IsValid()) return false;
    // Callers may reopen with Transaction.Applied itself; snapshot before resetting the session.
    const FPhase7Settings AppliedSnapshot = Applied;
    Transaction = FPhase7SettingsTransaction();
    Transaction.Applied = AppliedSnapshot;
    Transaction.Opening = AppliedSnapshot;
    Transaction.Preview = AppliedSnapshot;
    Transaction.PreviousApplied = AppliedSnapshot;
    bOpen = true;
    bPreviewOnDisplay = false;
    PreviewSecondsRemaining = 0.0;
    FocusedRow = 0;
    Status = TEXT("CHOOSE A VALUE, THEN PREVIEW");
    return true;
}

void FProductDisplaySettings::MoveFocus(const int32 Delta)
{
    FocusedRow = WrapIndex(FocusedRow, Delta, RowCount);
}

void FProductDisplaySettings::ChangeFocusedValue(const int32 Delta)
{
    // Confirm/cancel a visible preview first; staging must not silently disable its recovery timer.
    if (!bOpen || bPreviewOnDisplay || (FocusedRow != 0 && FocusedRow != 1)) return;
    FPhase7Settings Candidate = Transaction.bPreviewActive ? Transaction.Preview : Transaction.Applied;
    if (!Candidate.IsValid()) return;
    if (FocusedRow == 0)
    {
        const TArray<FIntPoint> Values = SupportedResolutions();
        int32 Index = Values.IndexOfByPredicate([&Candidate](const FIntPoint& Value) { return Value.X == Candidate.ResolutionWidth && Value.Y == Candidate.ResolutionHeight; });
        if (Index == INDEX_NONE) Index = 0;
        const FIntPoint Value = Values[WrapIndex(Index, Delta, Values.Num())];
        Candidate.ResolutionWidth = Value.X;
        Candidate.ResolutionHeight = Value.Y;
    }
    else
    {
        const TArray<EPhase7WindowMode> Values = SupportedWindowModes();
        int32 Index = Values.IndexOfByKey(Candidate.WindowMode);
        if (Index == INDEX_NONE) Index = 0;
        Candidate.WindowMode = Values[WrapIndex(Index, Delta, Values.Num())];
    }
    if (Transaction.bPreviewActive) Transaction.Preview = Candidate;
    else if (!Transaction.Begin(Candidate)) return;
    bPreviewOnDisplay = false;
    PreviewSecondsRemaining = 0.0;
    Status = TEXT("STAGED - SELECT PREVIEW (NOT SAVED)");
}

bool FProductDisplaySettings::BeginPreview()
{
    if (!bOpen || bPreviewOnDisplay || !Transaction.bPreviewActive || !Transaction.Applied.IsValid() || !Transaction.Preview.IsValid()) return false;
    bPreviewOnDisplay = true;
    PreviewSecondsRemaining = PreviewDurationSeconds;
    Status = TEXT("PREVIEW ACTIVE - APPLY OR CANCEL (NOT SAVED)");
    return true;
}

bool FProductDisplaySettings::AdvancePreview(const double DeltaSeconds)
{
    if (!bPreviewOnDisplay) return false;
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds < 0.0 || !FMath::IsFinite(PreviewSecondsRemaining)
        || PreviewSecondsRemaining <= 0.0 || PreviewSecondsRemaining > PreviewDurationSeconds
        || !Transaction.bPreviewActive || !Transaction.Preview.IsValid() || DeltaSeconds >= PreviewSecondsRemaining)
    {
        Cancel();
        Status = TEXT("PREVIEW EXPIRED - APPLIED DISPLAY RESTORED (NOT SAVED)");
        return true;
    }
    PreviewSecondsRemaining -= DeltaSeconds;
    return false;
}

bool FProductDisplaySettings::Apply()
{
    if (!bOpen || !Transaction.Apply()) return false;
    bPreviewOnDisplay = false;
    PreviewSecondsRemaining = 0.0;
    Status = TEXT("APPLIED IN MEMORY - NOT SAVED; ROLLBACK AVAILABLE");
    return true;
}

void FProductDisplaySettings::Cancel()
{
    Transaction.Cancel();
    bPreviewOnDisplay = false;
    PreviewSecondsRemaining = 0.0;
    Status = TEXT("CANCELLED - APPLIED DISPLAY RESTORED (NOT SAVED)");
}

bool FProductDisplaySettings::Rollback()
{
    if (!Transaction.bCanRollback || !Transaction.PreviousApplied.IsValid()) return false;
    Transaction.Cancel();
    if (!Transaction.Rollback()) return false;
    bPreviewOnDisplay = false;
    PreviewSecondsRemaining = 0.0;
    Status = TEXT("ROLLED BACK IN MEMORY - NOT SAVED");
    return true;
}

const FPhase7Settings& FProductDisplaySettings::GetVisible() const
{
    return Transaction.bPreviewActive ? Transaction.Preview : Transaction.Applied;
}

FString FProductDisplaySettings::GetFocusedAction() const
{
    static const TCHAR* Labels[] = { TEXT("RESOLUTION"), TEXT("WINDOW MODE"), TEXT("PREVIEW"), TEXT("APPLY"), TEXT("CANCEL"), TEXT("ROLLBACK") };
    return Labels[FMath::Clamp(FocusedRow, 0, RowCount - 1)];
}
