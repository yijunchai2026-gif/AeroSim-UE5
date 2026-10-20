#include "AeroSimPlayerController.h"

#include "AeroSimGameMode.h"
#include "InputKeyEventArgs.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "FlightPawn.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"

namespace AeroSimPhysicalInput
{
constexpr float NeutralDeadZone = 0.1F;
constexpr int32 MaximumDevices = 8;

const FKey DigitalKeys[] = {
    EKeys::W, EKeys::S, EKeys::A, EKeys::D, EKeys::Q, EKeys::E,
    EKeys::LeftShift, EKeys::RightShift, EKeys::LeftControl, EKeys::RightControl,
    EKeys::Gamepad_LeftTrigger, EKeys::Gamepad_RightTrigger
};
const FKey AnalogKeys[] = {
    EKeys::Gamepad_LeftX, EKeys::Gamepad_LeftY,
    EKeys::Gamepad_RightX, EKeys::Gamepad_RightY,
    EKeys::Gamepad_LeftTriggerAxis, EKeys::Gamepad_RightTriggerAxis
};

bool IsFlightControlKey(const FKey& Key)
{
    for (const FKey& Candidate : DigitalKeys) if (Candidate == Key) return true;
    for (const FKey& Candidate : AnalogKeys) if (Candidate == Key) return true;
    return false;
}

bool IsNeutralValue(const float Value)
{
    return FMath::IsFinite(Value) && FMath::Abs(Value) <= NeutralDeadZone;
}
}

/** Observe before CommonUI/Slate can consume input; never forward it to gameplay twice. */
class FAeroSimPhysicalInputProcessor final : public IInputProcessor
{
public:
    explicit FAeroSimPhysicalInputProcessor(AAeroSimPlayerController* Controller) : Owner(Controller) {}

    virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override {}
    virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& Event) override
    {
        Observe(Event, Event.GetKey(), Event.IsRepeat() ? IE_Repeat : IE_Pressed, 1.0F);
        return false;
    }
    virtual bool HandleKeyUpEvent(FSlateApplication&, const FKeyEvent& Event) override
    {
        Observe(Event, Event.GetKey(), IE_Released, 0.0F);
        return false;
    }
    virtual bool HandleAnalogInputEvent(FSlateApplication&, const FAnalogInputEvent& Event) override
    {
        Observe(Event, Event.GetKey(), IE_Axis, Event.GetAnalogValue());
        return false;
    }
    virtual bool HandleMouseButtonDownEvent(FSlateApplication&, const FPointerEvent& Event) override
    {
        Observe(Event, Event.GetEffectingButton(), IE_Pressed, 1.0F);
        return false;
    }
    virtual const TCHAR* GetDebugName() const override { return TEXT("AeroSimPhysicalInputObserver"); }

private:
    void Observe(const FInputEvent& Event, const FKey& Key, const EInputEvent Type, const float Value)
    {
        if (AAeroSimPlayerController* Controller = Owner.Get())
        {
            Controller->ObservePhysicalInput(FInputKeyEventArgs(
                nullptr, Event.GetInputDeviceId(), Key, Type, Value, false, Event.GetEventTimestamp()));
        }
    }
    TWeakObjectPtr<AAeroSimPlayerController> Owner;
};

AAeroSimPlayerController::AAeroSimPlayerController()
{
    PrimaryActorTick.bTickEvenWhenPaused = true;
}

void AAeroSimPlayerController::BeginPlay()
{
    Super::BeginPlay();
    bRequiresSlateObservation = GetLocalPlayer() != nullptr;
    if (bRequiresSlateObservation && FSlateApplication::IsInitialized())
    {
        FSlateApplication& Slate = FSlateApplication::Get();
        PhysicalInputProcessor = MakeShared<FAeroSimPhysicalInputProcessor>(this);
        bSlateObservationRegistered = Slate.RegisterInputPreProcessor(PhysicalInputProcessor, EInputPreProcessorType::PreEngine);
        ActivationHandle = Slate.OnApplicationActivationStateChanged().AddUObject(
            this, &AAeroSimPlayerController::NotifyApplicationActivationChanged);
        bApplicationActive = Slate.IsActive();
    }
    IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().AddUObject(
        this, &AAeroSimPlayerController::NotifyInputDeviceConnectionChanged);
}

void AAeroSimPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (FSlateApplication::IsInitialized())
    {
        FSlateApplication& Slate = FSlateApplication::Get();
        if (PhysicalInputProcessor.IsValid()) Slate.UnregisterInputPreProcessor(PhysicalInputProcessor);
        Slate.OnApplicationActivationStateChanged().Remove(ActivationHandle);
    }
    PhysicalInputProcessor.Reset();
    IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().RemoveAll(this);
    Super::EndPlay(EndPlayReason);
}

bool AAeroSimPlayerController::OwnsInputDevice(const FInputDeviceId Device) const
{
    // Unpossessed Automation worlds have no LocalPlayer; real local players remain isolated.
    return GetLocalPlayer() == nullptr || Device == INPUTDEVICEID_NONE
        || IPlatformInputDeviceMapper::Get().GetUserForInputDevice(Device) == GetPlatformUserId();
}

bool AAeroSimPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
    ObservePhysicalInput(Params);
    return Super::InputKey(Params);
}

void AAeroSimPlayerController::ReportInputKind(const FInputKeyEventArgs& Params)
{
    if (Params.Event != IE_Pressed && Params.Event != IE_Repeat
        && !(Params.Event == IE_Axis && !AeroSimPhysicalInput::IsNeutralValue(Params.AmountDepressed))) return;
    if (AAeroSimGameMode* GameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<AAeroSimGameMode>() : nullptr)
    {
        GameMode->SetProductInputKind(Params.IsGamepad() ? EProductInputKind::Gamepad : EProductInputKind::KeyboardMouse);
    }
}

void AAeroSimPlayerController::ObservePhysicalInput(const FInputKeyEventArgs& Params)
{
    if (Params.IsSimulatedInput() || !OwnsInputDevice(Params.InputDevice)) return;
    ReportInputKind(Params);
    if (!AeroSimPhysicalInput::IsFlightControlKey(Params.Key)) return;
    if (Params.Event != IE_Pressed && Params.Event != IE_Repeat && Params.Event != IE_Released && Params.Event != IE_Axis) return;

    if (!PhysicalValues.Contains(Params.InputDevice) && PhysicalValues.Num() >= AeroSimPhysicalInput::MaximumDevices)
    {
        bObservationOverflow = true;
        if (AFlightPawn* Pawn = Cast<AFlightPawn>(GetPawn())) Pawn->SetSafePaused(true);
        return;
    }
    const float Value = Params.Event == IE_Released ? 0.0F
        : (Params.Key.IsAnalog() ? Params.AmountDepressed : 1.0F);
    PhysicalValues.FindOrAdd(Params.InputDevice).Add(Params.Key, Value);
    DevicesAwaitingObservation.Remove(Params.InputDevice);
    if (!AeroSimPhysicalInput::IsNeutralValue(Value))
    {
        if (AFlightPawn* Pawn = Cast<AFlightPawn>(GetPawn())) Pawn->HandlePhysicalInputActivity();
    }
}

bool AAeroSimPlayerController::IsPhysicalInputNeutral() const
{
    // R6b: inactivity alone is not "held input". While inactive the OS routes no physical keys to this
    // app; the only possible sources are events we observe (e.g. the local control bridge). Held keys
    // observed before deactivation stay held until an actual release/zero is observed.
    if (bObservationOverflow || !DevicesAwaitingObservation.IsEmpty()
        || (bRequiresSlateObservation && !bSlateObservationRegistered)) return false;

    TSet<FKey> ObservedKeys;
    for (const auto& Device : PhysicalValues)
    {
        for (const auto& Sample : Device.Value)
        {
            ObservedKeys.Add(Sample.Key);
            if (!AeroSimPhysicalInput::IsNeutralValue(Sample.Value)) return false;
        }
    }
    // Raw observations win over PlayerInput, whose values can be flushed or stale under a menu.
    for (const FKey& Key : AeroSimPhysicalInput::DigitalKeys)
    {
        if (!ObservedKeys.Contains(Key) && IsInputKeyDown(Key)) return false;
    }
    for (const FKey& Key : AeroSimPhysicalInput::AnalogKeys)
    {
        if (!ObservedKeys.Contains(Key) && !AeroSimPhysicalInput::IsNeutralValue(GetInputAnalogKeyState(Key))) return false;
    }
    return true;
}

void AAeroSimPlayerController::NotifyApplicationActivationChanged(const bool bActive)
{
    bApplicationActive = bActive;
    // Keep held observations until actual release/zero events, even across focus flushing.
    if (!bActive)
    {
        if (AFlightPawn* Pawn = Cast<AFlightPawn>(GetPawn())) Pawn->SetSafePaused(true);
    }
    else if (AFlightPawn* Pawn = Cast<AFlightPawn>(GetPawn()))
    {
        // Regaining focus restarts any pending resume so a full neutral window is observed under focus.
        Pawn->HandlePhysicalInputActivity();
    }
}

void AAeroSimPlayerController::NotifyInputDeviceConnectionChanged(
    const EInputDeviceConnectionState State, const FPlatformUserId User, const FInputDeviceId Device)
{
    if (GetLocalPlayer() != nullptr && User != GetPlatformUserId()) return;
    if (State == EInputDeviceConnectionState::Disconnected)
    {
        if (DevicesAwaitingObservation.Num() >= AeroSimPhysicalInput::MaximumDevices) bObservationOverflow = true;
        else DevicesAwaitingObservation.Add(Device);
        if (AFlightPawn* Pawn = Cast<AFlightPawn>(GetPawn())) Pawn->SetSafePaused(true);
    }
    // Reconnection alone neither clears held axes nor starts a resume countdown.
}
