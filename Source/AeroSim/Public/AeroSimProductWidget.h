#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Phase2Systems.h"
#include "ProductShell.h"
#include "AeroSimProductWidget.generated.h"

class AAeroSimGameMode;
class AFlightPawn;
class SBox;
class SScrollBox;
class SWidget;
struct FAeroSimProductWidgetStyle;

/** Native, asset-free product presentation. All mutations go through GameMode commands. */
UCLASS(meta = (DisableNativeTick))
class AEROSIM_API UAeroSimProductWidget final : public UCommonActivatableWidget
{
    GENERATED_BODY()
    friend class UAeroSimControlBridge;

public:
    explicit UAeroSimProductWidget(const FObjectInitializer& ObjectInitializer);

    /** Updates cached metrics; replaces Slate content only on page/category transitions. */
    void Refresh();

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
    virtual void NativeConstruct() override;
    virtual void NativeOnActivated() override;
    virtual UWidget* NativeGetDesiredFocusTarget() const override;
    virtual FReply NativeOnFocusReceived(const FGeometry& Geometry, const FFocusEvent& Event) override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;

private:
    using FCommand = TFunction<void(AAeroSimGameMode&)>;
    using FTextReader = TFunction<FString(const AAeroSimGameMode&)>;
    using FEnabledReader = TFunction<bool(const AAeroSimGameMode&)>;

    AAeroSimGameMode* ResolveGameMode() const;
    void UpdateTraceMetrics();
    bool IsActiveMissionOverlay() const;
    float GetWorkspaceWidth() const;
    float GetTextScale() const;
    int32 GetSlateUserIndex() const;
    void FocusFirstAction(bool bPreviouslyOwnedFocus = false);

    TAttribute<FText> BindText(FTextReader Reader) const;
    TSharedRef<SWidget> MakeText(TAttribute<FText> Text, int32 Size = 15,
        bool bBold = false, FLinearColor Color = FLinearColor::White) const;
    TSharedRef<SWidget> MakeAction(TAttribute<FText> Label, FCommand Command,
        bool bPrimary = false, FEnabledReader Enabled = FEnabledReader());
    TSharedRef<SWidget> MakePanel(const TCHAR* Eyebrow, const TCHAR* Title, TSharedRef<SWidget> Body) const;
    TSharedRef<SWidget> MakeMetric(const TCHAR* Label, TAttribute<FText> Value) const;
    TSharedRef<SWidget> MakeSettingRow(const TCHAR* Label, const TCHAR* Description, EProductSettingField Field);
    TSharedRef<SWidget> BuildWorkspace();
    TSharedRef<SWidget> BuildNavigation();
    TSharedRef<SWidget> BuildPage();
    TSharedRef<SWidget> BuildHome();
    TSharedRef<SWidget> BuildHangar();
    TSharedRef<SWidget> BuildMission();
    TSharedRef<SWidget> BuildPreflight();
    TSharedRef<SWidget> BuildSettings();
    TSharedRef<SWidget> BuildVisualLab();
    TSharedRef<SWidget> BuildResult();
    TSharedRef<SWidget> BuildReplay();

    TSharedPtr<FAeroSimProductWidgetStyle> Style;
    TSharedPtr<SBox> ShellContainer;
    TSharedPtr<SScrollBox> PageScroll;
    TWeakPtr<SWidget> PrimaryFocusTarget;
    TArray<TWeakPtr<SWidget>> FocusTargets;
    TOptional<EProductPage> BuiltPage;
    TOptional<EProductSettingsCategory> BuiltCategory;
    TWeakObjectPtr<AFlightPawn> MetricsPawn;
    FFlightResultMetrics TraceMetrics;
    int32 MetricsSampleCount = INDEX_NONE;
    float MetricsLastSampleTime = -1.0F;
};
