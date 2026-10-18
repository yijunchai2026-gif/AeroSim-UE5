#include "AeroSimProductWidget.h"

#include "AeroSimGameMode.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "FlightPawn.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Types/NavigationMetaData.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

namespace ProductWidget
{
const FLinearColor Background(0.006F, 0.013F, 0.023F, 0.96F);
const FLinearColor Surface(0.016F, 0.030F, 0.047F, 0.98F);
const FLinearColor Card(0.026F, 0.047F, 0.068F, 1.0F);
const FLinearColor Stroke(0.085F, 0.155F, 0.200F, 1.0F);
const FLinearColor Cyan(0.28F, 0.86F, 0.95F, 1.0F);
const FLinearColor Amber(1.0F, 0.72F, 0.31F, 1.0F);
const FLinearColor Ink(0.006F, 0.022F, 0.032F, 1.0F);
const FLinearColor Text(0.90F, 0.95F, 0.98F, 1.0F);
const FLinearColor Muted(0.58F, 0.70F, 0.78F, 1.0F);

FText Literal(const TCHAR* Value)
{
    return FText::FromString(Value);
}

bool IsConfirmKey(const FKey& Key)
{
    return Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom;
}

const TCHAR* PageName(const EProductPage Page)
{
    switch (Page)
    {
    case EProductPage::Home: return TEXT("HOME");
    case EProductPage::Hangar: return TEXT("HANGAR");
    case EProductPage::Mission: return TEXT("MISSION");
    case EProductPage::Preflight: return TEXT("PREFLIGHT");
    case EProductPage::Flight: return TEXT("FLIGHT");
    case EProductPage::Settings: return TEXT("SETTINGS");
    case EProductPage::VisualLab: return TEXT("VISUAL LAB");
    case EProductPage::Result: return TEXT("RESULT");
    case EProductPage::Replay: return TEXT("REPLAY");
    default: return TEXT("UNAVAILABLE");
    }
}

const TCHAR* WeatherName(const EAeroSimWeather Weather)
{
    switch (Weather)
    {
    case EAeroSimWeather::Clear: return TEXT("Clear");
    case EAeroSimWeather::Overcast: return TEXT("Overcast");
    case EAeroSimWeather::Rain: return TEXT("Rain");
    default: return TEXT("UNKNOWN");
    }
}

const TCHAR* QualityName(const EPhase7QualityTier Tier)
{
    switch (Tier)
    {
    case EPhase7QualityTier::DesktopHigh: return TEXT("Desktop High");
    case EPhase7QualityTier::DesktopLow: return TEXT("Desktop Low");
    case EPhase7QualityTier::Tablet: return TEXT("Tablet candidate / NOT_VALIDATED");
    default: return TEXT("UNKNOWN");
    }
}

const TCHAR* WindowName(const EPhase7WindowMode Mode)
{
    switch (Mode)
    {
    case EPhase7WindowMode::Windowed: return TEXT("Windowed");
    case EPhase7WindowMode::Borderless: return TEXT("Borderless");
    case EPhase7WindowMode::Fullscreen: return TEXT("Fullscreen");
    default: return TEXT("UNKNOWN");
    }
}

const TCHAR* CameraName(const EFlightCameraMode Mode)
{
    switch (Mode)
    {
    case EFlightCameraMode::FPV: return TEXT("FPV / Nose");
    case EFlightCameraMode::Chase: return TEXT("Near Follow");
    case EFlightCameraMode::FarFollow: return TEXT("Far Follow");
    case EFlightCameraMode::Gimbal: return TEXT("Gimbal");
    default: return TEXT("UNKNOWN");
    }
}

const AFlightPawn* ReadPawn(const AAeroSimGameMode& GameMode)
{
    const UWorld* World = GameMode.GetWorld();
    const APlayerController* Controller = World != nullptr ? World->GetFirstPlayerController() : nullptr;
    return Controller != nullptr ? Cast<AFlightPawn>(Controller->GetPawn()) : nullptr;
}

FString ReadSetting(const FPhase7Settings& Settings, const EProductSettingField Field)
{
    switch (Field)
    {
    case EProductSettingField::Resolution:
        return FString::Printf(TEXT("%d x %d"), Settings.ResolutionWidth, Settings.ResolutionHeight);
    case EProductSettingField::WindowMode: return WindowName(Settings.WindowMode);
    case EProductSettingField::Quality: return QualityName(Settings.QualityTier);
    case EProductSettingField::MasterVolume:
        return FString::Printf(TEXT("%.0f %%"), Settings.MasterVolume * 100.0F);
    case EProductSettingField::VerticalFov:
        return FString::Printf(TEXT("%.0f degrees vertical"), Settings.VerticalFov);
    case EProductSettingField::HudScale:
        return FString::Printf(TEXT("%.0f %%"), Settings.HudScale * 100.0F);
    case EProductSettingField::TextScale:
        return FString::Printf(TEXT("%.0f %%"), Settings.TextScale * 100.0F);
    case EProductSettingField::ReducedMotion:
        return Settings.bReducedMotion ? TEXT("On") : TEXT("Off");
    default: return TEXT("UNAVAILABLE");
    }
}

/** Accept is executed on key-down once, never again on key-up or in the parent. */
class SProductActionButton final : public SButton
{
public:
    void Construct(const SButton::FArguments& Arguments)
    {
        SButton::Construct(Arguments);
    }

    void SetProductOwner(UAeroSimProductWidget* InOwner) { Owner = InOwner; }

    virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override
    {
        ReportInputKind(Event.GetKey().IsGamepadKey() ? EProductInputKind::Gamepad : EProductInputKind::KeyboardMouse);
        if (IsConfirmKey(Event.GetKey()))
        {
            if (!Event.IsRepeat() && IsEnabled() && Owner.IsValid() && Owner->IsActivated())
            {
                return ExecuteOnClick();
            }
            return FReply::Handled();
        }
        // Slate owns directional/Tab navigation; no aircraft axes are implemented here.
        return SButton::OnKeyDown(Geometry, Event);
    }

    virtual FReply OnKeyUp(const FGeometry& Geometry, const FKeyEvent& Event) override
    {
        return IsConfirmKey(Event.GetKey()) ? FReply::Handled() : SButton::OnKeyUp(Geometry, Event);
    }

    virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override
    {
        UE_LOG(LogTemp, Display, TEXT("PRODUCT_MOUSE_DOWN button=%s"), *Event.GetEffectingButton().ToString());
        ReportInputKind(EProductInputKind::KeyboardMouse);
        return SButton::OnMouseButtonDown(Geometry, Event);
    }

    virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override
    {
        UE_LOG(LogTemp, Display, TEXT("PRODUCT_MOUSE_UP button=%s"), *Event.GetEffectingButton().ToString());
        return SButton::OnMouseButtonUp(Geometry, Event);
    }

    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
        FSlateWindowElementList& Elements, int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const override
    {
        const int32 LastLayer = SButton::OnPaint(Args, Geometry, CullingRect, Elements, LayerId, WidgetStyle, bParentEnabled);
        if (HasAnyUserFocus().IsSet())
        {
            static const FSlateRoundedBoxBrush FocusOutline(FLinearColor::Transparent, 7.0F, Amber, 2.0F);
            FSlateDrawElement::MakeBox(Elements, LastLayer + 1, Geometry.ToPaintGeometry(), &FocusOutline,
                ESlateDrawEffect::None, FocusOutline.GetTint(WidgetStyle) * WidgetStyle.GetColorAndOpacityTint());
            return LastLayer + 1;
        }
        return LastLayer;
    }

private:
    void ReportInputKind(const EProductInputKind Kind) const
    {
        if (Owner.IsValid())
        {
            if (UWorld* World = Owner->GetWorld())
            {
                if (AAeroSimGameMode* GameMode = World->GetAuthGameMode<AAeroSimGameMode>()) GameMode->SetProductInputKind(Kind);
            }
        }
    }

    TWeakObjectPtr<UAeroSimProductWidget> Owner;
};
}

/** Brushes/styles outlive the Slate widgets holding pointers into them. */
struct FAeroSimProductWidgetStyle final
{
    FSlateColorBrush Backdrop{ProductWidget::Background};
    FSlateRoundedBoxBrush Workspace{ProductWidget::Surface, 14.0F, ProductWidget::Stroke, 1.0F};
    FSlateRoundedBoxBrush Panel{ProductWidget::Card, 10.0F, ProductWidget::Stroke, 1.0F};
    FButtonStyle Secondary;
    FButtonStyle Primary;

    FAeroSimProductWidgetStyle()
    {
        Secondary = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
        Secondary.SetNormal(FSlateRoundedBoxBrush(ProductWidget::Card, 7.0F, ProductWidget::Stroke, 1.0F));
        Secondary.SetHovered(FSlateRoundedBoxBrush(FLinearColor(0.04F, 0.11F, 0.15F), 7.0F, ProductWidget::Cyan, 1.0F));
        Secondary.SetPressed(FSlateRoundedBoxBrush(FLinearColor(0.06F, 0.16F, 0.19F), 7.0F, ProductWidget::Cyan, 2.0F));
        Secondary.SetNormalPadding(FMargin(16.0F, 12.0F));
        Secondary.SetPressedPadding(FMargin(16.0F, 12.0F));
        Primary = Secondary;
        Primary.SetNormal(FSlateRoundedBoxBrush(ProductWidget::Cyan, 7.0F));
        Primary.SetHovered(FSlateRoundedBoxBrush(FLinearColor(0.52F, 0.96F, 1.0F), 7.0F));
        Primary.SetPressed(FSlateRoundedBoxBrush(FLinearColor(0.17F, 0.66F, 0.78F), 7.0F));
    }
};

UAeroSimProductWidget::UAeroSimProductWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SetIsFocusable(true);
    bAutoActivate = true;
    bAutoRestoreFocus = false;
    // Back is handled below, not also registered as a CommonUI action binding.
    bIsBackHandler = false;
}

AAeroSimGameMode* UAeroSimProductWidget::ResolveGameMode() const
{
    return GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<AAeroSimGameMode>() : nullptr;
}

TSharedRef<SWidget> UAeroSimProductWidget::RebuildWidget()
{
    // Preserve CommonUI rebuild notification and UUserWidget initialization/metadata.
    Super::RebuildWidget();
    Style = MakeShared<FAeroSimProductWidgetStyle>();
    BuiltPage.Reset();
    BuiltCategory.Reset();
    SAssignNew(ShellContainer, SBox);
    Refresh();
    return ShellContainer.ToSharedRef();
}

void UAeroSimProductWidget::ReleaseSlateResources(const bool bReleaseChildren)
{
    ShellContainer.Reset();
    PageScroll.Reset();
    PrimaryFocusTarget.Reset();
    FocusTargets.Reset();
    BuiltPage.Reset();
    BuiltCategory.Reset();
    // Content captures the shared style too, so an in-flight Slate event cannot dangle brushes.
    Style.Reset();
    Super::ReleaseSlateResources(bReleaseChildren);
}

void UAeroSimProductWidget::NativeConstruct()
{
    Super::NativeConstruct();
    Refresh();
    FocusFirstAction();
}

void UAeroSimProductWidget::NativeOnActivated()
{
    Super::NativeOnActivated();
    Refresh();
    FocusFirstAction();
}

UWidget* UAeroSimProductWidget::NativeGetDesiredFocusTarget() const
{
    const AAeroSimGameMode* GameMode = ResolveGameMode();
    // CommonUI focuses the UMG wrapper; NativeOnFocusReceived redirects into native Slate.
    return GameMode != nullptr && GameMode->GetProductPage() != EProductPage::Flight
        ? const_cast<UAeroSimProductWidget*>(this) : nullptr;
}

FReply UAeroSimProductWidget::NativeOnFocusReceived(const FGeometry& Geometry, const FFocusEvent& Event)
{
    Refresh();
    const AAeroSimGameMode* GameMode = ResolveGameMode();
    if (IsActivated() && GameMode != nullptr && GameMode->GetProductPage() != EProductPage::Flight)
    {
        TSharedPtr<SWidget> Target = PrimaryFocusTarget.Pin();
        if (!Target.IsValid() || !Target->IsEnabled())
        {
            for (const TWeakPtr<SWidget>& Candidate : FocusTargets)
            {
                Target = Candidate.Pin();
                if (Target.IsValid() && Target->IsEnabled() && Target->GetVisibility().IsVisible()) break;
                Target.Reset();
            }
        }
        if (Target.IsValid()) return FReply::Handled().SetUserFocus(Target.ToSharedRef(), Event.GetCause());
    }
    return Super::NativeOnFocusReceived(Geometry, Event);
}

FReply UAeroSimProductWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    // Observation only: the focused Slate control remains the sole command dispatcher.
    if (AAeroSimGameMode* GameMode = ResolveGameMode())
    {
        if (IsActivated() && GameMode->GetProductPage() != EProductPage::Flight)
        {
            GameMode->SetProductInputKind(Event.GetKey().IsGamepadKey()
                ? EProductInputKind::Gamepad : EProductInputKind::KeyboardMouse);
        }
    }
    return FReply::Unhandled();
}

FReply UAeroSimProductWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    AAeroSimGameMode* GameMode = ResolveGameMode();
    if (!IsActivated() || GameMode == nullptr || GameMode->GetProductPage() == EProductPage::Flight)
    {
        return FReply::Unhandled();
    }
    const FKey Key = Event.GetKey();
    GameMode->SetProductInputKind(Key.IsGamepadKey() ? EProductInputKind::Gamepad : EProductInputKind::KeyboardMouse);
    EUINavigation Direction = EUINavigation::Invalid;
    if (Key == EKeys::Gamepad_DPad_Up) Direction = EUINavigation::Up;
    else if (Key == EKeys::Gamepad_DPad_Down) Direction = EUINavigation::Down;
    else if (Key == EKeys::Gamepad_DPad_Left) Direction = EUINavigation::Left;
    else if (Key == EKeys::Gamepad_DPad_Right) Direction = EUINavigation::Right;
    if (Direction != EUINavigation::Invalid)
    {
        return FReply::Handled().SetNavigation(Direction, ENavigationGenesis::Controller);
    }
    if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right)
    {
        if (!Event.IsRepeat())
        {
            GameMode->HandleProductBack();
            Refresh();
        }
        return FReply::Handled();
    }
    if (ProductWidget::IsConfirmKey(Key))
    {
        // No global Apply/replay/lab command. Focused controls have already consumed Accept.
        const EProductPage Page = GameMode->GetProductPage();
        if (!Event.IsRepeat() && (Page == EProductPage::Home || Page == EProductPage::Hangar
            || Page == EProductPage::Mission || Page == EProductPage::Preflight))
        {
            GameMode->ActivatePrimaryAction();
            Refresh();
        }
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(Geometry, Event);
}

FReply UAeroSimProductWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
    if (AAeroSimGameMode* GameMode = ResolveGameMode()) GameMode->SetProductInputKind(EProductInputKind::KeyboardMouse);
    return Super::NativeOnMouseButtonDown(Geometry, Event);
}

void UAeroSimProductWidget::UpdateTraceMetrics()
{
    const AAeroSimGameMode* GameMode = ResolveGameMode();
    if (GameMode == nullptr || (GameMode->GetProductPage() != EProductPage::Result
        && GameMode->GetProductPage() != EProductPage::Replay)) return;
    AFlightPawn* Pawn = GetOwningPlayerPawn<AFlightPawn>();
    if (Pawn == nullptr)
    {
        MetricsPawn.Reset();
        MetricsSampleCount = INDEX_NONE;
        TraceMetrics = FFlightResultMetrics();
        return;
    }
    const TArray<FFlightTraceSample>& Samples = Pawn->GetFlightTrace().Samples;
    const float LastTime = Samples.IsEmpty() ? -1.0F : Samples.Last().TimeSeconds;
    if (MetricsPawn.Get() != Pawn || MetricsSampleCount != Samples.Num() || MetricsLastSampleTime != LastTime)
    {
        MetricsPawn = Pawn;
        MetricsSampleCount = Samples.Num();
        MetricsLastSampleTime = LastTime;
        TraceMetrics = Pawn->GetResultMetrics();
    }
}

void UAeroSimProductWidget::Refresh()
{
    UpdateTraceMetrics();
    if (!ShellContainer.IsValid() || !Style.IsValid()) return;
    const AAeroSimGameMode* GameMode = ResolveGameMode();
    const EProductPage Page = GameMode != nullptr ? GameMode->GetProductPage() : EProductPage::Home;
    const EProductSettingsCategory Category = GameMode != nullptr ? GameMode->GetSettingsCategory() : EProductSettingsCategory::Display;
    if (BuiltPage.IsSet() && BuiltPage.GetValue() == Page && BuiltCategory.IsSet()
        && (Page != EProductPage::Settings || BuiltCategory.GetValue() == Category)) return;

    const TSharedPtr<SWidget> Cached = GetCachedWidget();
    const bool bOwnedFocus = Cached.IsValid() && Cached->HasAnyUserFocusOrFocusedDescendants();
    BuiltPage = Page;
    BuiltCategory = Category;
    PrimaryFocusTarget.Reset();
    FocusTargets.Reset();
    PageScroll.Reset();
    ShellContainer->SetVisibility(Page == EProductPage::Flight ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible);
    if (Page == EProductPage::Flight)
    {
        // Canvas alone renders flight telemetry/perimeter hints; no center overlay or hidden navigation.
        ShellContainer->SetContent(SNullWidget::NullWidget);
        return;
    }
    ShellContainer->SetContent(BuildWorkspace());
    FocusFirstAction(bOwnedFocus);
}

bool UAeroSimProductWidget::IsActiveMissionOverlay() const
{
    const AAeroSimGameMode* GameMode = ResolveGameMode();
    return GameMode != nullptr && (GameMode->GetProductPage() == EProductPage::VisualLab
        || (FProductShellState::IsOverlay(GameMode->GetProductPage())
            && GameMode->GetMissionFlow().State == EAeroSimMissionState::Flight));
}

float UAeroSimProductWidget::GetWorkspaceWidth() const
{
    int32 Width = 1280;
    int32 Height = 720;
    if (const APlayerController* Controller = GetOwningPlayer()) Controller->GetViewportSize(Width, Height);
    const float Scale = FMath::Max(0.1F, UWidgetLayoutLibrary::GetViewportScale(this));
    const float LogicalWidth = FMath::Max(1.0F, static_cast<float>(Width)) / Scale;
    return IsActiveMissionOverlay() ? FMath::Min(460.0F, LogicalWidth * 0.34F)
        : FMath::Clamp(LogicalWidth - 48.0F, 1.0F, 1080.0F);
}

float UAeroSimProductWidget::GetTextScale() const
{
    const AAeroSimGameMode* GameMode = ResolveGameMode();
    return GameMode != nullptr ? FMath::Clamp(GameMode->GetEditableSettings().TextScale, 0.75F, 2.0F) : 1.0F;
}

int32 UAeroSimProductWidget::GetSlateUserIndex() const
{
    if (ULocalPlayer* Player = GetOwningLocalPlayer())
    {
        if (const TSharedPtr<FSlateUser> User = Player->GetSlateUser()) return User->GetUserIndex();
    }
    return 0;
}

void UAeroSimProductWidget::FocusFirstAction(const bool bPreviouslyOwnedFocus)
{
    if (IsDesignTime() || !IsActivated() || !FSlateApplication::IsInitialized()
        || !FSlateApplication::Get().IsActive()) return;
    const AAeroSimGameMode* GameMode = ResolveGameMode();
    const APlayerController* Controller = GetOwningPlayer();
    const TSharedPtr<SWidget> Cached = GetCachedWidget();
    if (GameMode == nullptr || GameMode->GetProductPage() == EProductPage::Flight
        || Controller == nullptr || !Controller->IsLocalController() || !Cached.IsValid()) return;

    FSlateApplication& Slate = FSlateApplication::Get();
    const int32 UserIndex = GetSlateUserIndex();
    const TSharedPtr<SWidget> Focused = Slate.GetUserFocusedWidget(UserIndex);
    const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
    TSharedPtr<SWidget> ViewportWidget;
    if (Viewport != nullptr) ViewportWidget = Viewport->GetGameViewportWidget();
    const TSharedPtr<SWindow> Window = Slate.FindWidgetWindow(Cached.ToSharedRef());
    // Never take focus from an editor panel, another window, or on telemetry-only refresh.
    if (!Window.IsValid() || !Window->IsActive() || (!bPreviouslyOwnedFocus
        && Focused != Cached && !Cached->HasUserFocusedDescendants(UserIndex) && Focused != ViewportWidget)) return;

    TSharedPtr<SWidget> Target = PrimaryFocusTarget.Pin();
    if (!Target.IsValid() || !Target->IsEnabled())
    {
        for (const TWeakPtr<SWidget>& Candidate : FocusTargets)
        {
            Target = Candidate.Pin();
            if (Target.IsValid() && Target->IsEnabled() && Target->GetVisibility().IsVisible()) break;
            Target.Reset();
        }
    }
    if (Target.IsValid())
    {
        Slate.SetUserFocus(UserIndex, Target, EFocusCause::SetDirectly);
        if (PageScroll.IsValid()) PageScroll->ScrollDescendantIntoView(Target, false);
    }
}

TAttribute<FText> UAeroSimProductWidget::BindText(FTextReader Reader) const
{
    const TWeakObjectPtr<UAeroSimProductWidget> WeakThis(const_cast<UAeroSimProductWidget*>(this));
    return TAttribute<FText>::CreateLambda([WeakThis, Reader = MoveTemp(Reader)]()
    {
        const AAeroSimGameMode* GameMode = WeakThis.IsValid() ? WeakThis->ResolveGameMode() : nullptr;
        return FText::FromString(GameMode != nullptr ? Reader(*GameMode) : TEXT("UNAVAILABLE"));
    });
}

TSharedRef<SWidget> UAeroSimProductWidget::MakeText(TAttribute<FText> Value, const int32 Size,
    const bool bBold, const FLinearColor Color) const
{
    const TWeakObjectPtr<UAeroSimProductWidget> WeakThis(const_cast<UAeroSimProductWidget*>(this));
    return SNew(STextBlock)
        .Text(Value)
        .Font_Lambda([WeakThis, Size, bBold]()
        {
            const float Scale = WeakThis.IsValid() ? WeakThis->GetTextScale() : 1.0F;
            return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", FMath::RoundToInt(Size * Scale));
        })
        .ColorAndOpacity(Color)
        .AutoWrapText(true);
}

TSharedRef<SWidget> UAeroSimProductWidget::MakeAction(TAttribute<FText> Label, FCommand Command,
    const bool bPrimary, FEnabledReader Enabled)
{
    const TWeakObjectPtr<UAeroSimProductWidget> WeakThis(this);
    const TSharedPtr<FAeroSimProductWidgetStyle> SharedStyle = Style;
    const EProductPage ExpectedPage = BuiltPage.Get(EProductPage::Home);
    const EProductSettingsCategory ExpectedCategory = BuiltCategory.Get(EProductSettingsCategory::Display);
    TSharedRef<ProductWidget::SProductActionButton> Button = SNew(ProductWidget::SProductActionButton)
        .ButtonStyle(bPrimary ? &SharedStyle->Primary : &SharedStyle->Secondary)
        .IsFocusable(true)
        .HAlign(HAlign_Center)
        .IsEnabled_Lambda([WeakThis, ExpectedPage, ExpectedCategory, Enabled = MoveTemp(Enabled)]()
        {
            const AAeroSimGameMode* GameMode = WeakThis.IsValid() ? WeakThis->ResolveGameMode() : nullptr;
            return GameMode != nullptr && WeakThis->IsActivated() && GameMode->GetProductPage() == ExpectedPage
                && (ExpectedPage != EProductPage::Settings || GameMode->GetSettingsCategory() == ExpectedCategory)
                && (!Enabled || Enabled(*GameMode));
        })
        .OnClicked_Lambda([WeakThis, SharedStyle, ExpectedPage, ExpectedCategory, Command = MoveTemp(Command)]()
        {
            AAeroSimGameMode* GameMode = WeakThis.IsValid() ? WeakThis->ResolveGameMode() : nullptr;
            if (GameMode != nullptr && WeakThis->IsActivated() && GameMode->GetProductPage() == ExpectedPage
                && (ExpectedPage != EProductPage::Settings || GameMode->GetSettingsCategory() == ExpectedCategory))
            {
                Command(*GameMode);
                WeakThis->Refresh();
            }
            return FReply::Handled();
        })
        [MakeText(Label, 14, true, bPrimary ? ProductWidget::Ink : ProductWidget::Text)];
    Button->SetProductOwner(this);
    FocusTargets.Add(Button);
    if (bPrimary && !PrimaryFocusTarget.IsValid()) PrimaryFocusTarget = Button;
    return Button;
}

TSharedRef<SWidget> UAeroSimProductWidget::MakePanel(const TCHAR* Eyebrow, const TCHAR* Title,
    TSharedRef<SWidget> Body) const
{
    const TSharedPtr<FAeroSimProductWidgetStyle> SharedStyle = Style;
    return SNew(SBorder).BorderImage(&SharedStyle->Panel).Padding(24.0F)
        [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[MakeText(ProductWidget::Literal(Eyebrow), 12, true, ProductWidget::Cyan)]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 9, 0, 20)[MakeText(ProductWidget::Literal(Title), 24, true, ProductWidget::Text)]
            + SVerticalBox::Slot().AutoHeight()[Body]];
}

TSharedRef<SWidget> UAeroSimProductWidget::MakeMetric(const TCHAR* Label, TAttribute<FText> Value) const
{
    return SNew(SBox).WidthOverride(172.0F)
        [SNew(SBorder).BorderImage(&Style->Panel).Padding(FMargin(18, 16))
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[MakeText(ProductWidget::Literal(Label), 12, true, ProductWidget::Muted)]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)[MakeText(Value, 26, true, ProductWidget::Cyan)]]];
}

TSharedRef<SWidget> UAeroSimProductWidget::BuildNavigation()
{
    TSharedRef<SWrapBox> Navigation = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8, 8));
    const EProductPage Pages[] = { EProductPage::Home, EProductPage::Hangar, EProductPage::Mission,
        EProductPage::Settings, EProductPage::VisualLab };
    for (const EProductPage Page : Pages)
    {
        Navigation->AddSlot()[MakeAction(BindText([Page](const AAeroSimGameMode& GameMode)
            {
                return GameMode.GetProductPage() == Page
                    ? FString::Printf(TEXT("[ %s ]"), ProductWidget::PageName(Page)) : FString(ProductWidget::PageName(Page));
            }), [Page](AAeroSimGameMode& GameMode) { GameMode.NavigateProductPage(Page); })];
    }
    const TWeakObjectPtr<UAeroSimProductWidget> WeakThis(this);
    Navigation->SetVisibility(TAttribute<EVisibility>::CreateLambda([WeakThis]()
    {
        return WeakThis.IsValid() && !WeakThis->IsActiveMissionOverlay() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
    }));
    return Navigation;
}

TSharedRef<SWidget> UAeroSimProductWidget::BuildWorkspace()
{
    const TWeakObjectPtr<UAeroSimProductWidget> WeakThis(this);
    const TSharedPtr<FAeroSimProductWidgetStyle> SharedStyle = Style;
    // Body first ensures the page CTA, not a workspace tab, is the default action target.
    const TSharedRef<SWidget> PageBody = BuildPage();
    const TSharedRef<SWidget> Navigation = BuildNavigation();
    TSharedRef<SWidget> Workspace = SNew(SBorder).BorderImage(&SharedStyle->Workspace).Padding(24.0F)
        [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
                [SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1.0F)
                        [SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight()[MakeText(ProductWidget::Literal(TEXT("A E R O S I M")), 19, true, ProductWidget::Cyan)]
                            + SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 0)
                                [MakeText(BindText([](const AAeroSimGameMode& GameMode)
                                {
                                    return FString::Printf(TEXT("PACIFIC COAST  /  %s"), ProductWidget::PageName(GameMode.GetProductPage()));
                                }), 12, false, ProductWidget::Muted)]]]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 18, 0, 16)[Navigation]
            + SVerticalBox::Slot().FillHeight(1.0F)
                [SAssignNew(PageScroll, SScrollBox)
                    .ScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll)
                    .AnimateWheelScrolling(false)
                    + SScrollBox::Slot().Padding(0, 0, 12, 8)[PageBody]]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 14, 0, 0)
                [MakeText(BindText([](const AAeroSimGameMode& GameMode)
                {
                    const AFlightPawn* Pawn = ProductWidget::ReadPawn(GameMode);
                    if (Pawn != nullptr && Pawn->IsResumeCountdownActive())
                    {
                        return FString::Printf(TEXT("INPUTS NEUTRAL / RESUME IN %d s"), FMath::CeilToInt(Pawn->GetResumeCountdownSeconds()));
                    }
                    return GameMode.GetProductStatus().IsEmpty() ? FString(TEXT("LOCAL TRAINING / PROTOTYPE CONTENT")) : GameMode.GetProductStatus();
                }), 13, true, ProductWidget::Amber)]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
                [MakeText(BindText([](const AAeroSimGameMode& GameMode)
                {
                    return GameMode.GetProductInputKind() == EProductInputKind::Gamepad
                        ? TEXT("D-pad / stick: focus   Bottom: select   Right: back")
                        : TEXT("Arrow keys / Tab: focus   Enter / Space: select   Esc: back");
                }), 12, false, ProductWidget::Muted)]];

    const TSharedRef<FNavigationMetaData> NavigationRules = Workspace->FindOrAddMetaData<FNavigationMetaData>();
    NavigationRules->SetNavigationWrap(EUINavigation::Next);
    NavigationRules->SetNavigationWrap(EUINavigation::Previous);
    NavigationRules->SetNavigationStop(EUINavigation::Left);
    NavigationRules->SetNavigationStop(EUINavigation::Right);
    NavigationRules->SetNavigationStop(EUINavigation::Up);
    NavigationRules->SetNavigationStop(EUINavigation::Down);

    return SNew(SOverlay)
        + SOverlay::Slot()
            [SNew(SBorder).BorderImage(&SharedStyle->Backdrop)
                .Visibility_Lambda([WeakThis, SharedStyle]()
                {
                    return WeakThis.IsValid() && WeakThis->IsActiveMissionOverlay() ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
                })]
        + SOverlay::Slot()
            [SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0F)[SNew(SSpacer)]
                + SHorizontalBox::Slot().AutoWidth().Padding(FMargin(24, 20))
                    [SNew(SBox).WidthOverride_Lambda([WeakThis]()
                    {
                        return FOptionalSize(WeakThis.IsValid() ? WeakThis->GetWorkspaceWidth() : 640.0F);
                    })[Workspace]]
                + SHorizontalBox::Slot().FillWidth(TAttribute<float>::CreateLambda([WeakThis]()
                    {
                        return WeakThis.IsValid() && WeakThis->IsActiveMissionOverlay() ? 0.0F : 1.0F;
                    }))[SNew(SSpacer)]];
}

TSharedRef<SWidget> UAeroSimProductWidget::BuildPage()
{
    switch (BuiltPage.Get(EProductPage::Home))
    {
    case EProductPage::Home: return BuildHome();
    case EProductPage::Hangar: return BuildHangar();
    case EProductPage::Mission: return BuildMission();
    case EProductPage::Preflight: return BuildPreflight();
    case EProductPage::Settings: return BuildSettings();
    case EProductPage::VisualLab: return BuildVisualLab();
    case EProductPage::Result: return BuildResult();
    case EProductPage::Replay: return BuildReplay();
    case EProductPage::Flight: return SNullWidget::NullWidget;
    default: return MakeText(ProductWidget::Literal(TEXT("CONTENT_NOT_DELIVERED")), 18, true, ProductWidget::Amber);
    }
}

TSharedRef<SWidget> UAeroSimProductWidget::BuildHome()
{
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
            [MakePanel(TEXT("01 / FLIGHT STARTS HERE"), TEXT("Find your line. Fly the coast."),
                SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                        [MakeText(ProductWidget::Literal(TEXT("A focused place to learn, practice camera movement, and review your flight. Start with the training aircraft, then follow the Pacific Coast inspection route.")), 17, false, ProductWidget::Text)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 28, 0, 0).HAlign(HAlign_Left)
                        [MakeAction(ProductWidget::Literal(TEXT("OPEN HANGAR")), [](AAeroSimGameMode& GameMode)
                        { GameMode.ActivatePrimaryAction(); }, true)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0).HAlign(HAlign_Left)
                        [MakeAction(ProductWidget::Literal(TEXT("EXPLORE THE COAST")), [](AAeroSimGameMode& GameMode)
                        { GameMode.NavigateProductPage(EProductPage::VisualLab); }, false)])]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 20, 0, 0)
            [MakePanel(TEXT("YOUR FIRST SESSION"), TEXT("Pacific Coast / coastal inspection"),
                SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()[MakeText(ProductWidget::Literal(TEXT("60-90 seconds   /   5 ordered checkpoints   /   return and land")), 16, true, ProductWidget::Text)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0)
                        [MakeText(ProductWidget::Literal(TEXT("Choose aircraft > review mission > preflight > take off > debrief. Your applied settings stay with you.")), 15, false, ProductWidget::Muted)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 16, 0, 0)
                        [MakeText(ProductWidget::Literal(TEXT("PROTOTYPE / UNVERIFIED. Generic aircraft and development environment; not a certified flight controller or a production aircraft delivery.")), 13, true, ProductWidget::Amber)])];
}

TSharedRef<SWidget> UAeroSimProductWidget::BuildHangar()
{
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
            [MakePanel(TEXT("AIRCRAFT BAY / TRAINING"), TEXT("AeroSim Generic Golden Aircraft"),
                SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()[MakeText(ProductWidget::Literal(TEXT("PROTOTYPE / UNVERIFIED")), 18, true, ProductWidget::Amber)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 14, 0, 0)
                        [MakeText(ProductWidget::Literal(TEXT("A generic training quadcopter for the systems slice. It does not represent DJI or another named production model.")), 16, false, ProductWidget::Text)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 16, 0, 0)
                        [MakeText(BindText([](const AAeroSimGameMode& GameMode)
                        {
                            const AFlightPawn* Pawn = ProductWidget::ReadPawn(GameMode);
                            return Pawn != nullptr ? Pawn->GetHudData().AssetStatus : FString(TEXT("CONTENT_NOT_DELIVERED / NO AIRCRAFT"));
                        }), 13, true, ProductWidget::Amber)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 26, 0, 0).HAlign(HAlign_Left)
                        [MakeAction(ProductWidget::Literal(TEXT("CHOOSE MISSION")), [](AAeroSimGameMode& GameMode)
                        { GameMode.ActivatePrimaryAction(); }, true)])]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 20, 0, 0)
            [MakePanel(TEXT("FORMAL FLEET / NOT AVAILABLE"), TEXT("BLOCKED_ASSET"),
                MakeText(ProductWidget::Literal(TEXT("Named aircraft remain CONTENT_NOT_DELIVERED. Source, license, version, SHA-256 and per-aircraft user appearance confirmation are required. No generic substitution is promoted to production.")), 15, false, ProductWidget::Muted))];
}

TSharedRef<SWidget> UAeroSimProductWidget::BuildMission()
{
    return MakePanel(TEXT("MISSION 01 / PACIFIC COAST"), TEXT("Coastal inspection"),
        SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[MakeText(BindText([](const AAeroSimGameMode& GameMode)
                { return GameMode.GetPhase4Mission().Task.Goal; }), 17, false, ProductWidget::Text)]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 20, 0, 0)
                [MakeText(ProductWidget::Literal(TEXT("01  Takeoff pad\n02  Grass / soil transition\n03  Rock shore / wet edge\n04  Boardwalk and low water\n05  Forest edge\n06  Return home and land")), 16, true, ProductWidget::Text)]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 20, 0, 0)
                [MakeText(BindText([](const AAeroSimGameMode& GameMode)
                {
                    const FEnvironmentState& Environment = GameMode.GetEnvironmentState();
                    return FString::Printf(TEXT("%s  %02d:%02d  /  seed %d\n%s  /  target %.0f s  /  %d checkpoints"),
                        ProductWidget::WeatherName(Environment.Weather), Environment.TimeMinutes / 60, Environment.TimeMinutes % 60,
                        Environment.Seed, ProductWidget::QualityName(GameMode.GetAppliedSettings().QualityTier),
                        GameMode.GetPhase4Mission().Task.TargetDurationSeconds, GameMode.GetPhase4Mission().Task.Checkpoints.Num());
                }), 14, false, ProductWidget::Muted)]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 16, 0, 0)
                [MakeText(ProductWidget::Literal(TEXT("Stay inside the marked training volume. Inspect in order; H requests return-to-home. This is a development training route, not a real-world safety certification.")), 14, false, ProductWidget::Amber)]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 26, 0, 0).HAlign(HAlign_Left)
                [MakeAction(ProductWidget::Literal(TEXT("CONTINUE TO PREFLIGHT")), [](AAeroSimGameMode& GameMode)
                { GameMode.ActivatePrimaryAction(); }, true)]);
}

TSharedRef<SWidget> UAeroSimProductWidget::BuildPreflight()
{
    return MakePanel(TEXT("PREFLIGHT / BEFORE YOU ARM"), TEXT("Check your setup. Then take off."),
        SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[MakeText(BindText([](const AAeroSimGameMode& GameMode)
                {
                    const AFlightPawn* Pawn = ProductWidget::ReadPawn(GameMode);
                    const bool bCentered = Pawn != nullptr && Pawn->IsPhysicalInputNeutral();
                    return FString::Printf(TEXT("INPUT / %s\n%s"), GameMode.GetProductInputKind() == EProductInputKind::Gamepad
                        ? TEXT("Gamepad last used; verify connection") : TEXT("Keyboard / mouse last used"),
                        Pawn == nullptr ? TEXT("BLOCKED / NO INPUT PAWN") : bCentered
                            ? TEXT("CENTERED / controls neutral") : TEXT("WAIT / release keys and center sticks"));
                }), 17, true, ProductWidget::Cyan)]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 20, 0, 0)
                [MakeText(BindText([](const AAeroSimGameMode& GameMode)
                {
                    const AFlightPawn* Pawn = ProductWidget::ReadPawn(GameMode);
                    if (Pawn == nullptr) return FString(TEXT("CAMERA / NOT_AVAILABLE"));
                    return FString::Printf(TEXT("CAMERA / %s\nvFOV %.0f degrees / vertical is the saved truth"),
                        ProductWidget::CameraName(Pawn->GetCameraMode()), Pawn->GetVerticalFov());
                }), 16, false, ProductWidget::Text)]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 18, 0, 0)
                [MakeText(BindText([](const AAeroSimGameMode& GameMode)
                {
                    const FEnvironmentState& Environment = GameMode.GetEnvironmentState();
                    const FPhase7Settings& Settings = GameMode.GetAppliedSettings();
                    return FString::Printf(TEXT("ENVIRONMENT / %s %02d:%02d / seed %d\nDISPLAY / %d x %d / %s / %s"),
                        ProductWidget::WeatherName(Environment.Weather), Environment.TimeMinutes / 60, Environment.TimeMinutes % 60,
                        Environment.Seed, Settings.ResolutionWidth, Settings.ResolutionHeight, ProductWidget::WindowName(Settings.WindowMode),
                        ProductWidget::QualityName(Settings.QualityTier));
                }), 15, false, ProductWidget::Text)]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 20, 0, 0)
                [MakeText(ProductWidget::Literal(TEXT("After starting: hold Left Shift to raise throttle. W/S pitch, A/D roll, Q/E yaw, Shift/Ctrl throttle. C changes camera; arrows move the gimbal; H requests return-home.")), 15, false, ProductWidget::Muted)]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 16, 0, 0)
                [MakeText(ProductWidget::Literal(TEXT("Esc opens safe settings. Pause, focus loss or disconnect clears input; resuming requires centered controls and a three-second countdown. PROTOTYPE / UNVERIFIED.")), 14, true, ProductWidget::Amber)]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 26, 0, 0).HAlign(HAlign_Left)
                [MakeAction(ProductWidget::Literal(TEXT("ARM & START MISSION")), [](AAeroSimGameMode& GameMode)
                { GameMode.ActivatePrimaryAction(); }, true, [](const AAeroSimGameMode& GameMode)
                {
                    const AFlightPawn* Pawn = ProductWidget::ReadPawn(GameMode);
                    return Pawn != nullptr && Pawn->IsPhysicalInputNeutral();
                })]);
}

TSharedRef<SWidget> UAeroSimProductWidget::MakeSettingRow(const TCHAR* Label, const TCHAR* Description,
    const EProductSettingField Field)
{
    TSharedRef<SWrapBox> Controls = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(10, 8));
    const FString Decrease = FString::Printf(TEXT("-  %s"), Label);
    const FString Increase = FString::Printf(TEXT("+  %s"), Label);
    Controls->AddSlot()[MakeAction(FText::FromString(Decrease), [Field](AAeroSimGameMode& GameMode)
        { GameMode.StageProductSetting(Field, -1); })];
    Controls->AddSlot()[MakeAction(FText::FromString(Increase), [Field](AAeroSimGameMode& GameMode)
        { GameMode.StageProductSetting(Field, 1); })];
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()[MakeText(ProductWidget::Literal(Label), 16, true, ProductWidget::Text)]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
            [MakeText(BindText([Field](const AAeroSimGameMode& GameMode)
                { return ProductWidget::ReadSetting(GameMode.GetEditableSettings(), Field); }), 19, true, ProductWidget::Cyan)]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 7, 0, 12)[MakeText(ProductWidget::Literal(Description), 13, false, ProductWidget::Muted)]
        + SVerticalBox::Slot().AutoHeight()[Controls];
}

TSharedRef<SWidget> UAeroSimProductWidget::BuildSettings()
{
    TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);
    TSharedRef<SWrapBox> Tabs = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8, 8));
    const EProductSettingsCategory Categories[] = { EProductSettingsCategory::Display, EProductSettingsCategory::Graphics,
        EProductSettingsCategory::Audio, EProductSettingsCategory::Controls, EProductSettingsCategory::Camera,
        EProductSettingsCategory::Accessibility };
    const TCHAR* Labels[] = { TEXT("Display"), TEXT("Graphics"), TEXT("Audio"), TEXT("Controls"), TEXT("Camera"), TEXT("Accessibility") };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Categories); ++Index)
    {
        const EProductSettingsCategory Category = Categories[Index];
        const FString Label(Labels[Index]);
        Tabs->AddSlot()[MakeAction(BindText([Category, Label](const AAeroSimGameMode& GameMode)
            { return GameMode.GetSettingsCategory() == Category ? FString::Printf(TEXT("[ %s ]"), *Label) : Label; }),
            [Category](AAeroSimGameMode& GameMode) { GameMode.SetSettingsCategory(Category); })];
    }
    Content->AddSlot().AutoHeight()[Tabs];
    TSharedRef<SVerticalBox> Fields = SNew(SVerticalBox);
    const auto AddField = [this, &Fields](const TCHAR* Label, const TCHAR* Help, const EProductSettingField Field)
    {
        Fields->AddSlot().AutoHeight().Padding(0, 0, 0, 24)[MakeSettingRow(Label, Help, Field)];
    };
    switch (BuiltCategory.Get(EProductSettingsCategory::Display))
    {
    case EProductSettingsCategory::Display:
        AddField(TEXT("Resolution"), TEXT("1280 x 720, 1440 x 900, or 1920 x 1080."), EProductSettingField::Resolution);
        AddField(TEXT("Window mode"), TEXT("Windowed, borderless, or fullscreen; preview before applying."), EProductSettingField::WindowMode);
        break;
    case EProductSettingsCategory::Graphics:
        AddField(TEXT("Quality"), TEXT("Desktop High / Low. Tablet profile is a separate candidate, not a desktop option."), EProductSettingField::Quality);
        Fields->AddSlot().AutoHeight()[MakeText(ProductWidget::Literal(TEXT("See Visual Lab for live diagnostics, not performance qualification. Windows and iPad hardware validation: NOT_RUN.")), 13, false, ProductWidget::Amber)];
        break;
    case EProductSettingsCategory::Audio:
        AddField(TEXT("Master volume"), TEXT("Stages the master mix level. Apply saves the change."), EProductSettingField::MasterVolume);
        break;
    case EProductSettingsCategory::Controls:
        Fields->AddSlot().AutoHeight()[MakeText(BindText([](const AAeroSimGameMode& GameMode)
            { return GameMode.GetProductInputKind() == EProductInputKind::Gamepad ? TEXT("LAST INPUT / GAMEPAD") : TEXT("LAST INPUT / KEYBOARD & MOUSE"); }), 17, true, ProductWidget::Cyan)];
        Fields->AddSlot().AutoHeight().Padding(0, 18, 0, 0)[MakeText(ProductWidget::Literal(TEXT("W / S          Pitch\nA / D          Roll\nQ / E          Yaw\nShift / Ctrl   Throttle\nC              Camera\nArrow keys     Gimbal (in flight)\nH              Return to home\nR              Reset\nEsc            Safe settings / back")), 15, false, ProductWidget::Text)];
        Fields->AddSlot().AutoHeight().Padding(0, 18, 0, 0)[MakeText(ProductWidget::Literal(TEXT("Menus: arrows / Tab or gamepad D-pad move focus; Enter / Space or Bottom selects. Remapping and per-axis tuning: NOT_IMPLEMENTED. A last-used input label is not a device connection check.")), 13, false, ProductWidget::Muted)];
        break;
    case EProductSettingsCategory::Camera:
        AddField(TEXT("Vertical FOV"), TEXT("35-100 degrees. Horizontal FOV is derived from aspect ratio, never saved separately."), EProductSettingField::VerticalFov);
        Fields->AddSlot().AutoHeight().Padding(0, 0, 0, 12)[MakeText(BindText([](const AAeroSimGameMode& GameMode)
            {
                const AFlightPawn* Pawn = ProductWidget::ReadPawn(GameMode);
                return Pawn != nullptr ? FString::Printf(TEXT("CURRENT CAMERA / %s"), ProductWidget::CameraName(Pawn->GetCameraMode()))
                    : FString(TEXT("CAMERA / NOT_AVAILABLE"));
            }), 15, true, ProductWidget::Cyan)];
        Fields->AddSlot().AutoHeight()[MakeText(ProductWidget::Literal(TEXT("Nose / Near Follow / Far Follow / Gimbal. FPV link simulation: NOT_IMPLEMENTED.")), 13, false, ProductWidget::Muted)];
        break;
    case EProductSettingsCategory::Accessibility:
        AddField(TEXT("HUD scale"), TEXT("Scales flight telemetry without covering the center corridor."), EProductSettingField::HudScale);
        AddField(TEXT("Text scale"), TEXT("Changes menu text size; content scrolls instead of being clipped."), EProductSettingField::TextScale);
        AddField(TEXT("Reduced motion"), TEXT("No animated menu transitions or focus scrolling; apply to persist the preference."), EProductSettingField::ReducedMotion);
        break;
    }
    Content->AddSlot().AutoHeight().Padding(0, 22, 0, 0)[Fields];
    Content->AddSlot().AutoHeight().Padding(0, 6, 0, 12)[MakeText(BindText([](const AAeroSimGameMode& GameMode)
        {
            // The settings owner supplies live recovery countdown/status. UI never starts a second timer.
            const FProductDisplaySettings& Display = GameMode.GetDisplaySettings();
            return Display.bPreviewOnDisplay ? FString::Printf(TEXT("%s\nCONFIRM OR RECOVER IN %d s"), *Display.Status,
                FMath::CeilToInt(Display.PreviewSecondsRemaining)) : Display.Status;
        }), 15, true, ProductWidget::Amber)];
    Content->AddSlot().AutoHeight()[MakeText(ProductWidget::Literal(TEXT("Preview does not save. Unconfirmed display preview recovers after 15 s. Apply saves; Cancel restores; Rollback restores the last applied configuration.")), 13, false, ProductWidget::Muted)];

    TSharedRef<SWrapBox> Actions = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(10, 10));
    Actions->AddSlot()[MakeAction(ProductWidget::Literal(TEXT("PREVIEW")), [](AAeroSimGameMode& GameMode)
        { GameMode.ExecuteSettingsAction(EProductSettingsAction::Preview); }, true, [](const AAeroSimGameMode& GameMode)
        { return GameMode.GetDisplaySettings().Transaction.bPreviewActive && !GameMode.GetDisplaySettings().bPreviewOnDisplay; })];
    Actions->AddSlot()[MakeAction(ProductWidget::Literal(TEXT("APPLY + SAVE")), [](AAeroSimGameMode& GameMode)
        { GameMode.ExecuteSettingsAction(EProductSettingsAction::Apply); }, false, [](const AAeroSimGameMode& GameMode)
        { return GameMode.GetDisplaySettings().Transaction.bPreviewActive; })];
    Actions->AddSlot()[MakeAction(ProductWidget::Literal(TEXT("CANCEL")), [](AAeroSimGameMode& GameMode)
        { GameMode.ExecuteSettingsAction(EProductSettingsAction::Cancel); })];
    Actions->AddSlot()[MakeAction(ProductWidget::Literal(TEXT("ROLLBACK")), [](AAeroSimGameMode& GameMode)
        { GameMode.ExecuteSettingsAction(EProductSettingsAction::Rollback); }, false, [](const AAeroSimGameMode& GameMode)
        { return GameMode.GetDisplaySettings().Transaction.bCanRollback; })];
    Actions->AddSlot()[MakeAction(ProductWidget::Literal(TEXT("BACK / CLOSE")), [](AAeroSimGameMode& GameMode)
        { GameMode.HandleProductBack(); })];
    Content->AddSlot().AutoHeight().Padding(0, 22, 0, 0)[Actions];
    return MakePanel(TEXT("SETUP / STAGED CHANGES"), TEXT("Settings"), Content);
}

TSharedRef<SWidget> UAeroSimProductWidget::BuildVisualLab()
{
    TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);
    Content->AddSlot().AutoHeight()[MakeText(ProductWidget::Literal(TEXT("A fixed reference with sequential quality A/B. One scene render at a time; no split-screen, wipe, production art approval or invented profiler values.")), 15, false, ProductWidget::Text)];
    Content->AddSlot().AutoHeight().Padding(0, 18, 0, 0)[MakeText(BindText([](const AAeroSimGameMode& GameMode)
        {
            const FEnvironmentState& Environment = GameMode.GetEnvironmentState();
            const FProductLabState& Lab = GameMode.GetLabState();
            return FString::Printf(TEXT("STATE / %s %02d:%02d / seed %d\nREFERENCE / %s / env revision %d / vFOV %.0f\nVIEW / %s\n%s"),
                ProductWidget::WeatherName(Environment.Weather), Environment.TimeMinutes / 60, Environment.TimeMinutes % 60,
                Environment.Seed, Lab.bReferenceLocked ? TEXT("LOCKED") : TEXT("NOT_LOCKED"), Lab.EnvironmentRevision, Lab.VerticalFov,
                !Lab.bReferenceLocked ? TEXT("NOT_CAPTURED") : Lab.bShowingReference ? TEXT("A / REFERENCE") : TEXT("B / COMPARISON"), *Lab.Status);
        }), 15, true, ProductWidget::Cyan)];
    Content->AddSlot().AutoHeight().Padding(0, 12, 0, 0)[MakeText(BindText([](const AAeroSimGameMode& GameMode)
        {
            const FProductLabState& Lab = GameMode.GetLabState();
            const AFlightPawn* Pawn = ProductWidget::ReadPawn(GameMode);
            const EPhase7QualityTier CurrentQuality = Lab.bReferenceLocked
                ? (Lab.bShowingReference ? Lab.ReferenceQuality : Lab.ComparisonQuality) : GameMode.GetAppliedSettings().QualityTier;
            return FString::Printf(TEXT("A / %s\nB / %s\nCURRENT / %s\nCAMERA / %s\nBUILD / %s"), ProductWidget::QualityName(Lab.ReferenceQuality),
                ProductWidget::QualityName(Lab.ComparisonQuality), ProductWidget::QualityName(CurrentQuality),
                TEXT("Fixed Lab observer (flight camera restored on exit)"),
                Pawn != nullptr ? *Pawn->GetHudData().BuildId : TEXT("NOT_RECORDED"));
        }), 14, false, ProductWidget::Muted)];

    TSharedRef<SWrapBox> Weather = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8, 8));
    const EAeroSimWeather Values[] = { EAeroSimWeather::Clear, EAeroSimWeather::Overcast, EAeroSimWeather::Rain };
    for (const EAeroSimWeather Value : Values)
    {
        Weather->AddSlot()[MakeAction(BindText([Value](const AAeroSimGameMode& GameMode)
            {
                return GameMode.GetEnvironmentState().Weather == Value
                    ? FString::Printf(TEXT("[ %s ]"), ProductWidget::WeatherName(Value)) : FString(ProductWidget::WeatherName(Value));
            }), [Value](AAeroSimGameMode& GameMode) { GameMode.SetLabWeather(Value); })];
    }
    Content->AddSlot().AutoHeight().Padding(0, 22, 0, 0)[Weather];
    Content->AddSlot().AutoHeight().Padding(0, 10, 0, 0)[MakeText(ProductWidget::Literal(TEXT("Changing weather invalidates the comparison; lock a new reference before comparing or capturing.")), 13, false, ProductWidget::Amber)];

    TSharedRef<SWrapBox> Actions = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(10, 10));
    Actions->AddSlot()[MakeAction(ProductWidget::Literal(TEXT("LOCK REFERENCE")), [](AAeroSimGameMode& GameMode)
        { GameMode.LockLabReference(); }, true)];
    Actions->AddSlot()[MakeAction(BindText([](const AAeroSimGameMode& GameMode)
        { return GameMode.GetLabState().bShowingReference ? TEXT("SHOW B / COMPARISON") : TEXT("SHOW A / REFERENCE"); }),
        [](AAeroSimGameMode& GameMode) { GameMode.ToggleLabComparison(); }, false, [](const AAeroSimGameMode& GameMode)
        { return GameMode.GetLabState().bReferenceLocked; })];
    Actions->AddSlot()[MakeAction(ProductWidget::Literal(TEXT("CAPTURE EVIDENCE")), [](AAeroSimGameMode& GameMode)
        { GameMode.CaptureProductEvidence(); }, false, [](const AAeroSimGameMode& GameMode)
        { return GameMode.GetLabState().bReferenceLocked; })];
    Actions->AddSlot()[MakeAction(ProductWidget::Literal(TEXT("BACK / CLOSE LAB")), [](AAeroSimGameMode& GameMode)
        { GameMode.HandleProductBack(); })];
    Content->AddSlot().AutoHeight().Padding(0, 22, 0, 0)[Actions];
    Content->AddSlot().AutoHeight().Padding(0, 26, 0, 0)[MakeText(ProductWidget::Literal(TEXT("LIVE DIAGNOSTICS / NOT QUALIFICATION")), 14, true, ProductWidget::Amber)];
    Content->AddSlot().AutoHeight().Padding(0, 10, 0, 0)[MakeText(BindText([](const AAeroSimGameMode& GameMode)
        { return GameMode.GetDiagnostics().Summary; }), 14, false, ProductWidget::Text)];
    Content->AddSlot().AutoHeight().Padding(0, 16, 0, 0)[MakeText(ProductWidget::Literal(TEXT("Capture success is not a visual PASS. Human G0-G4 and user approval remain NOT_RUN / pending_user; formal content remains BLOCKED_ASSET.")), 13, false, ProductWidget::Muted)];
    return MakePanel(TEXT("VISUAL LAB / REPRODUCIBLE, NOT CERTIFIED"), TEXT("Compare one variable."), Content);
}

TSharedRef<SWidget> UAeroSimProductWidget::BuildResult()
{
    TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);
    Content->AddSlot().AutoHeight()[MakeText(BindText([](const AAeroSimGameMode& GameMode)
        {
            const FPhase4Score& Score = GameMode.GetPhase4Mission().Score;
            return Score.bComplete ? FString::Printf(TEXT("TOTAL / %03d / 100"), Score.Total)
                : FString(TEXT("RESULT / NOT_COMPLETE"));
        }), 30, true, ProductWidget::Cyan)];
    TSharedRef<SWrapBox> Scores = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(12, 12));
    const TCHAR* Labels[] = { TEXT("SAFETY"), TEXT("PRECISION"), TEXT("EFFICIENCY"), TEXT("SMOOTHNESS") };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Labels); ++Index)
    {
        Scores->AddSlot()[MakeMetric(Labels[Index], BindText([Index](const AAeroSimGameMode& GameMode)
        {
            const FPhase4Score& Score = GameMode.GetPhase4Mission().Score;
            if (!Score.bComplete) return FString(TEXT("NOT_SCORED"));
            const int32 Values[] = { Score.Safety, Score.Precision, Score.Efficiency, Score.Smoothness };
            return FString::Printf(TEXT("%03d"), Values[Index]);
        }))];
    }
    Content->AddSlot().AutoHeight().Padding(0, 22, 0, 0)[Scores];
    const TWeakObjectPtr<UAeroSimProductWidget> WeakThis(this);
    Content->AddSlot().AutoHeight().Padding(0, 24, 0, 0)[MakeText(BindText([WeakThis](const AAeroSimGameMode& GameMode)
        {
            if (!WeakThis.IsValid() || WeakThis->TraceMetrics.Samples == 0) return FString(TEXT("TRACE / NOT_RECORDED"));
            const FFlightResultMetrics& Metrics = WeakThis->TraceMetrics;
            return FString::Printf(TEXT("TRACE / %.1f s / %.1f m traveled / %.1f m max altitude\n%d recorded samples / replay digest %08llX\nIncidents / %d boundary / %d collision / %d missed checkpoint"),
                Metrics.DurationSeconds, Metrics.DistanceMeters, Metrics.MaximumAltitudeMeters, Metrics.Samples,
                static_cast<unsigned long long>(GameMode.GetPhase4Mission().Score.ReplayDigest), GameMode.GetPhase4Mission().BoundaryViolations,
                GameMode.GetPhase4Mission().Collisions, GameMode.GetPhase4Mission().MissedCheckpoints);
        }), 15, false, ProductWidget::Text)];
    TSharedRef<SWrapBox> Actions = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(10, 10));
    Actions->AddSlot()[MakeAction(ProductWidget::Literal(TEXT("OPEN REPLAY")), [](AAeroSimGameMode& GameMode)
        { GameMode.ActivatePrimaryAction(); }, true, [](const AAeroSimGameMode& GameMode)
        { return GameMode.GetPhase4Mission().Score.bComplete; })];
    Actions->AddSlot()[MakeAction(ProductWidget::Literal(TEXT("FLY AGAIN")), [](AAeroSimGameMode& GameMode)
        { GameMode.RestartProductMission(); })];
    Actions->AddSlot()[MakeAction(ProductWidget::Literal(TEXT("RETURN TO HANGAR")), [](AAeroSimGameMode& GameMode)
        { GameMode.NavigateProductPage(EProductPage::Hangar); })];
    Content->AddSlot().AutoHeight().Padding(0, 26, 0, 0)[Actions];
    Content->AddSlot().AutoHeight().Padding(0, 18, 0, 0)[MakeText(ProductWidget::Literal(TEXT("Scores describe this prototype training session only. They do not certify real-world piloting, production content, or target-hardware performance.")), 13, false, ProductWidget::Muted)];
    return MakePanel(TEXT("DEBRIEF / PACIFIC COAST"), TEXT("Your flight, in perspective."), Content);
}

TSharedRef<SWidget> UAeroSimProductWidget::BuildReplay()
{
    TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);
    const TWeakObjectPtr<UAeroSimProductWidget> WeakThis(this);
    Content->AddSlot().AutoHeight()[MakeText(BindText([WeakThis](const AAeroSimGameMode& GameMode)
        {
            if (!WeakThis.IsValid() || WeakThis->TraceMetrics.Samples == 0) return FString(TEXT("REPLAY / NO RECORDED TRACE"));
            const float Duration = WeakThis->TraceMetrics.DurationSeconds;
            const float Time = FMath::Clamp(GameMode.GetReplayNormalizedTime(), 0.0F, 1.0F) * Duration;
            return FString::Printf(TEXT("%s / %.1f s of %.1f s\n%d samples / %.1f m / max altitude %.1f m\nRESULT %03d / 100 / digest %08llX"),
                GameMode.IsReplayPlaying() ? TEXT("PLAYING") : TEXT("PAUSED"), Time, Duration, WeakThis->TraceMetrics.Samples,
                WeakThis->TraceMetrics.DistanceMeters, WeakThis->TraceMetrics.MaximumAltitudeMeters, GameMode.GetPhase4Mission().Score.Total,
                static_cast<unsigned long long>(GameMode.GetPhase4Mission().Score.ReplayDigest));
        }), 18, true, ProductWidget::Cyan)];
    Content->AddSlot().AutoHeight().Padding(0, 24, 0, 0)
        [SNew(SBox).HeightOverride(8.0F)
            [SNew(SProgressBar).FillColorAndOpacity(ProductWidget::Cyan)
                .Percent_Lambda([WeakThis]() -> TOptional<float>
                {
                    const AAeroSimGameMode* GameMode = WeakThis.IsValid() ? WeakThis->ResolveGameMode() : nullptr;
                    return GameMode != nullptr ? FMath::Clamp(GameMode->GetReplayNormalizedTime(), 0.0F, 1.0F) : 0.0F;
                })]];
    const FEnabledReader HasTrace = [WeakThis](const AAeroSimGameMode& GameMode)
        { return WeakThis.IsValid() && WeakThis->TraceMetrics.Samples > 0; };
    TSharedRef<SWrapBox> Transport = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(10, 10));
    Transport->AddSlot()[MakeAction(BindText([](const AAeroSimGameMode& GameMode)
        { return GameMode.IsReplayPlaying() ? TEXT("PAUSE REPLAY") : TEXT("PLAY REPLAY"); }),
        [](AAeroSimGameMode& GameMode) { GameMode.SetReplayPlaying(!GameMode.IsReplayPlaying()); }, true, HasTrace)];
    Transport->AddSlot()[MakeAction(ProductWidget::Literal(TEXT("BACK 5%")), [](AAeroSimGameMode& GameMode)
        { GameMode.SeekReplay(FMath::Clamp(GameMode.GetReplayNormalizedTime() - 0.05F, 0.0F, 1.0F)); }, false, HasTrace)];
    Transport->AddSlot()[MakeAction(ProductWidget::Literal(TEXT("FORWARD 5%")), [](AAeroSimGameMode& GameMode)
        { GameMode.SeekReplay(FMath::Clamp(GameMode.GetReplayNormalizedTime() + 0.05F, 0.0F, 1.0F)); }, false, HasTrace)];
    Transport->AddSlot()[MakeAction(ProductWidget::Literal(TEXT("RESTART REPLAY")), [](AAeroSimGameMode& GameMode)
        { GameMode.SetReplayPlaying(false); GameMode.SeekReplay(0.0F); }, false, HasTrace)];
    Content->AddSlot().AutoHeight().Padding(0, 24, 0, 0)[Transport];
    TSharedRef<SWrapBox> Exit = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(10, 10));
    Exit->AddSlot()[MakeAction(ProductWidget::Literal(TEXT("BACK TO RESULT")), [](AAeroSimGameMode& GameMode)
        { GameMode.HandleProductBack(); })];
    Exit->AddSlot()[MakeAction(ProductWidget::Literal(TEXT("FLY AGAIN")), [](AAeroSimGameMode& GameMode)
        { GameMode.RestartProductMission(); })];
    Exit->AddSlot()[MakeAction(ProductWidget::Literal(TEXT("RETURN TO HANGAR")), [](AAeroSimGameMode& GameMode)
        { GameMode.NavigateProductPage(EProductPage::Hangar); })];
    Content->AddSlot().AutoHeight().Padding(0, 20, 0, 0)[Exit];
    Content->AddSlot().AutoHeight().Padding(0, 22, 0, 0)[MakeText(ProductWidget::Literal(TEXT("Recorded transform playback, not a new flight or a performance measurement. The result digest remains visible to correlate this trace with the completed session.")), 14, false, ProductWidget::Muted)];
    return MakePanel(TEXT("REPLAY / RECORDED SESSION"), TEXT("Review your line."), Content);
}
