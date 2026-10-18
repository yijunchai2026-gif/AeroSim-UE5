#pragma once

#include "CoreMinimal.h"
#include "Phase7Product.h"
#include "ProductUX.generated.h"

UENUM(BlueprintType)
enum class EAeroSimProductSurface : uint8
{
    Hangar,
    Preflight,
    Flight,
    DisplaySettings
};

/** Compatible UX contract layered on phase7/v1; it does not change the Phase 5/6 editor boundary. */
struct AEROSIM_API FAeroSimProductUxContract final
{
    static constexpr int32 Version = 1;
    static constexpr const TCHAR* Schema = TEXT("aerosim.product-ux/v1");
    static bool IsValid();
};

/** Pure keyboard-navigable display-settings state. Runtime application is owned by GameMode. */
struct AEROSIM_API FProductDisplaySettings final
{
    static constexpr int32 RowCount = 6;
    static constexpr double PreviewDurationSeconds = 15.0;

    FPhase7SettingsTransaction Transaction;
    int32 FocusedRow = 0;
    bool bOpen = false;
    bool bPreviewOnDisplay = false;
    FString Status = TEXT("READY");
    double PreviewSecondsRemaining = 0.0;

    static TArray<FIntPoint> SupportedResolutions();
    static TArray<EPhase7WindowMode> SupportedWindowModes();
    bool Open(const FPhase7Settings& Applied);
    void MoveFocus(int32 Delta);
    void ChangeFocusedValue(int32 Delta);
    bool BeginPreview();
    /** Wall-clock advance; true once on expiry means restore Transaction.Applied. Negative/nonfinite time expires safely. */
    bool AdvancePreview(double DeltaSeconds);
    /** Commits in memory only. GameMode must apply/persist and report the actual save outcome. */
    bool Apply();
    void Cancel();
    bool Rollback();
    const FPhase7Settings& GetVisible() const;
    FString GetFocusedAction() const;
};
