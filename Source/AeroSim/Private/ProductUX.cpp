#include "ProductUX.h"

bool FAeroSimProductUxContract::IsValid()
{
    return Version == 1 && FCString::Strcmp(Schema, TEXT("aerosim.product-ux/v1")) == 0;
}

TArray<FIntPoint> FProductDisplaySettings::SupportedResolutions()
{
    return { FIntPoint(1280, 720), FIntPoint(1440, 900), FIntPoint(1920, 1080) };
}

TArray<EPhase7WindowMode> FProductDisplaySettings::SupportedWindowModes()
{
    return { EPhase7WindowMode::Windowed, EPhase7WindowMode::Borderless, EPhase7WindowMode::Fullscreen };
}

bool FProductDisplaySettings::Open(const FPhase7Settings& Applied)
{
    if (bOpen || !Applied.IsValid()) return false;
    Transaction.Applied = Applied;
    Transaction.Opening = Applied;
    Transaction.Preview = Applied;
    bOpen = true;
    bPreviewOnDisplay = false;
    FocusedRow = 0;
    Status = TEXT("CHOOSE A VALUE, THEN PREVIEW");
    return true;
}

void FProductDisplaySettings::MoveFocus(const int32 Delta)
{
    FocusedRow = (FocusedRow + Delta + RowCount) % RowCount;
}

void FProductDisplaySettings::ChangeFocusedValue(const int32 Delta)
{
    if (!bOpen || (FocusedRow != 0 && FocusedRow != 1)) return;
    FPhase7Settings Candidate = Transaction.bPreviewActive ? Transaction.Preview : Transaction.Applied;
    if (FocusedRow == 0)
    {
        const TArray<FIntPoint> Values = SupportedResolutions();
        int32 Index = Values.IndexOfByPredicate([&Candidate](const FIntPoint& Value) { return Value.X == Candidate.ResolutionWidth && Value.Y == Candidate.ResolutionHeight; });
        if (Index == INDEX_NONE) Index = 0;
        const FIntPoint Value = Values[(Index + Delta + Values.Num()) % Values.Num()];
        Candidate.ResolutionWidth = Value.X;
        Candidate.ResolutionHeight = Value.Y;
    }
    else
    {
        const TArray<EPhase7WindowMode> Values = SupportedWindowModes();
        int32 Index = Values.IndexOfByKey(Candidate.WindowMode);
        if (Index == INDEX_NONE) Index = 0;
        Candidate.WindowMode = Values[(Index + Delta + Values.Num()) % Values.Num()];
    }
    if (Transaction.bPreviewActive) Transaction.Preview = Candidate;
    else Transaction.Begin(Candidate);
    bPreviewOnDisplay = false;
    Status = TEXT("STAGED - SELECT PREVIEW");
}

bool FProductDisplaySettings::BeginPreview()
{
    if (!bOpen || !Transaction.bPreviewActive) return false;
    bPreviewOnDisplay = true;
    Status = TEXT("PREVIEW ACTIVE - APPLY OR CANCEL");
    return true;
}

bool FProductDisplaySettings::Apply()
{
    if (!bOpen || !Transaction.Apply()) return false;
    bPreviewOnDisplay = false;
    Status = TEXT("APPLIED + SAVED - ROLLBACK AVAILABLE");
    return true;
}

void FProductDisplaySettings::Cancel()
{
    Transaction.Cancel();
    bPreviewOnDisplay = false;
    Status = TEXT("CANCELLED - OPENING DISPLAY RESTORED");
}

bool FProductDisplaySettings::Rollback()
{
    if (!Transaction.Rollback()) return false;
    bPreviewOnDisplay = false;
    Status = TEXT("ROLLED BACK + SAVED");
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
