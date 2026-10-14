#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "FlightPawn.generated.h"

class UCameraComponent;
class UFloatingPawnMovement;
class UInputAction;
class UInputMappingContext;
class USpringArmComponent;
class UStaticMeshComponent;
struct FInputActionValue;

/** Mapping context and actions form one atomic override unit. */
USTRUCT(BlueprintType)
struct AEROSIM_API FFlightInputSuite
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<UInputMappingContext> MappingContext;

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<UInputAction> MoveAction;

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<UInputAction> LookAction;

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<UInputAction> ThrottleAction;

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<UInputAction> ResetAction;
};

UCLASS()
class AEROSIM_API AFlightPawn final : public APawn
{
    GENERATED_BODY()

public:
    AFlightPawn();

    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void PawnClientRestart() override;

    const FFlightInputSuite& GetInputSuite() const { return InputSuite; }
    UInputMappingContext* GetFlightMappingContext() const { return InputSuite.MappingContext; }
    UInputAction* GetMoveAction() const { return InputSuite.MoveAction; }
    UInputAction* GetLookAction() const { return InputSuite.LookAction; }
    UInputAction* GetThrottleAction() const { return InputSuite.ThrottleAction; }
    UInputAction* GetResetAction() const { return InputSuite.ResetAction; }

    /** Fail-closed validation used before mapping application and action binding. */
    bool IsInputSuiteValid(const FFlightInputSuite& Candidate, bool bLogErrors = true) const;

private:
    bool ValidateConfiguredInputSuite() const;
    void ApplyMappingContext() const;
    void HandleMove(const FInputActionValue& Value);
    void HandleLook(const FInputActionValue& Value);
    void HandleThrottle(const FInputActionValue& Value);
    void HandleReset(const FInputActionValue& Value);

    UPROPERTY(VisibleAnywhere, Category = "Flight")
    TObjectPtr<UStaticMeshComponent> Body;

    UPROPERTY(VisibleAnywhere, Category = "Flight")
    TObjectPtr<UFloatingPawnMovement> Movement;

    UPROPERTY(VisibleAnywhere, Category = "Camera")
    TObjectPtr<USpringArmComponent> SpringArm;

    UPROPERTY(VisibleAnywhere, Category = "Camera")
    TObjectPtr<UCameraComponent> Camera;

    /** Override the complete suite. Partial or mismatched overrides are rejected and never applied. */
    UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (ShowOnlyInnerProperties))
    FFlightInputSuite InputSuite;

    FTransform InitialTransform = FTransform::Identity;
};
