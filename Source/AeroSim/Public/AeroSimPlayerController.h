#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "AeroSimPlayerController.generated.h"

class IInputProcessor;
struct FInputKeyEventArgs;
enum class EInputDeviceConnectionState : uint8;

/** Owns bounded physical observations independently of flushed/consumed gameplay input. */
UCLASS()
class AEROSIM_API AAeroSimPlayerController final : public APlayerController
{
    GENERATED_BODY()

public:
    AAeroSimPlayerController();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual bool InputKey(const FInputKeyEventArgs& Params) override;

    bool IsPhysicalInputNeutral() const;

    /** Observation only: does not dispatch gameplay or invoke a widget's primary action. */
    void ObservePhysicalInput(const FInputKeyEventArgs& Params);
    void NotifyApplicationActivationChanged(bool bActive);
    void NotifyInputDeviceConnectionChanged(EInputDeviceConnectionState State, FPlatformUserId User, FInputDeviceId Device);

private:
    bool OwnsInputDevice(FInputDeviceId Device) const;
    void ReportInputKind(const FInputKeyEventArgs& Params);

    // At most eight devices and a fixed flight-control key set. No arbitrary event history.
    TMap<FInputDeviceId, TMap<FKey, float>> PhysicalValues;
    TSet<FInputDeviceId> DevicesAwaitingObservation;
    TSharedPtr<IInputProcessor> PhysicalInputProcessor;
    FDelegateHandle ActivationHandle;
    bool bApplicationActive = true;
    bool bObservationOverflow = false;
    bool bRequiresSlateObservation = false;
    bool bSlateObservationRegistered = false;
};
