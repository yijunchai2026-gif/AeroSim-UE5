#include "FlightPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

DEFINE_LOG_CATEGORY_STATIC(LogAeroSimInput, Log, All);

AFlightPawn::AFlightPawn()
{
    PrimaryActorTick.bCanEverTick = false;

    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
    SetRootComponent(Body);
    Body->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);

    Movement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Movement"));
    Movement->UpdatedComponent = Body;
    Movement->MaxSpeed = 4000.0F;
    Movement->Acceleration = 2000.0F;
    Movement->Deceleration = 1200.0F;

    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(Body);
    SpringArm->TargetArmLength = 900.0F;
    SpringArm->SetRelativeLocation(FVector(0.0, 0.0, 180.0));
    SpringArm->bUsePawnControlRotation = true;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);

    InputSuite.MappingContext = CreateDefaultSubobject<UInputMappingContext>(TEXT("DefaultFlightMappingContext"));
    InputSuite.MoveAction = CreateDefaultSubobject<UInputAction>(TEXT("DefaultMoveAction"));
    InputSuite.MoveAction->ValueType = EInputActionValueType::Axis2D;
    InputSuite.LookAction = CreateDefaultSubobject<UInputAction>(TEXT("DefaultLookAction"));
    InputSuite.LookAction->ValueType = EInputActionValueType::Axis2D;
    InputSuite.ThrottleAction = CreateDefaultSubobject<UInputAction>(TEXT("DefaultThrottleAction"));
    InputSuite.ThrottleAction->ValueType = EInputActionValueType::Axis1D;
    InputSuite.ResetAction = CreateDefaultSubobject<UInputAction>(TEXT("DefaultResetAction"));
    InputSuite.ResetAction->ValueType = EInputActionValueType::Boolean;

    UInputModifierSwizzleAxis* ForwardSwizzle = CreateDefaultSubobject<UInputModifierSwizzleAxis>(TEXT("MoveForwardSwizzle"));
    ForwardSwizzle->Order = EInputAxisSwizzle::YXZ;
    InputSuite.MappingContext->MapKey(InputSuite.MoveAction, EKeys::W).Modifiers.Add(ForwardSwizzle);

    UInputModifierNegate* BackwardNegate = CreateDefaultSubobject<UInputModifierNegate>(TEXT("MoveBackwardNegate"));
    UInputModifierSwizzleAxis* BackwardSwizzle = CreateDefaultSubobject<UInputModifierSwizzleAxis>(TEXT("MoveBackwardSwizzle"));
    BackwardSwizzle->Order = EInputAxisSwizzle::YXZ;
    FEnhancedActionKeyMapping& BackwardMapping = InputSuite.MappingContext->MapKey(InputSuite.MoveAction, EKeys::S);
    BackwardMapping.Modifiers.Add(BackwardNegate);
    BackwardMapping.Modifiers.Add(BackwardSwizzle);

    UInputModifierNegate* LeftNegate = CreateDefaultSubobject<UInputModifierNegate>(TEXT("MoveLeftNegate"));
    InputSuite.MappingContext->MapKey(InputSuite.MoveAction, EKeys::A).Modifiers.Add(LeftNegate);
    InputSuite.MappingContext->MapKey(InputSuite.MoveAction, EKeys::D);
    InputSuite.MappingContext->MapKey(InputSuite.LookAction, EKeys::Mouse2D);
    InputSuite.MappingContext->MapKey(InputSuite.ThrottleAction, EKeys::SpaceBar);

    UInputModifierNegate* ThrottleDownNegate = CreateDefaultSubobject<UInputModifierNegate>(TEXT("ThrottleDownNegate"));
    InputSuite.MappingContext->MapKey(InputSuite.ThrottleAction, EKeys::LeftControl).Modifiers.Add(ThrottleDownNegate);
    InputSuite.MappingContext->MapKey(InputSuite.ResetAction, EKeys::R);
}

bool AFlightPawn::IsInputSuiteValid(const FFlightInputSuite& Candidate, const bool bLogErrors) const
{
    const bool bReferencesPresent = Candidate.MappingContext != nullptr
        && Candidate.MoveAction != nullptr
        && Candidate.LookAction != nullptr
        && Candidate.ThrottleAction != nullptr
        && Candidate.ResetAction != nullptr;
    if (!bReferencesPresent)
    {
        if (bLogErrors)
        {
            UE_LOG(LogAeroSimInput, Error, TEXT("Input suite rejected: MappingContext and all four actions are required."));
            ensureAlwaysMsgf(false, TEXT("AeroSim input suite must be overridden atomically."));
        }
        return false;
    }

    const bool bTypesValid = Candidate.MoveAction->ValueType == EInputActionValueType::Axis2D
        && Candidate.LookAction->ValueType == EInputActionValueType::Axis2D
        && Candidate.ThrottleAction->ValueType == EInputActionValueType::Axis1D
        && Candidate.ResetAction->ValueType == EInputActionValueType::Boolean;
    if (!bTypesValid)
    {
        if (bLogErrors)
        {
            UE_LOG(LogAeroSimInput, Error, TEXT("Input suite rejected: action ValueTypes do not match the flight contract."));
            ensureAlwaysMsgf(false, TEXT("AeroSim input suite action ValueTypes are invalid."));
        }
        return false;
    }

    TSet<const UInputAction*> ExpectedActions;
    ExpectedActions.Add(Candidate.MoveAction.Get());
    ExpectedActions.Add(Candidate.LookAction.Get());
    ExpectedActions.Add(Candidate.ThrottleAction.Get());
    ExpectedActions.Add(Candidate.ResetAction.Get());
    TSet<const UInputAction*> MappedActions;
    bool bContainsUnexpectedAction = false;
    for (const FEnhancedActionKeyMapping& Mapping : Candidate.MappingContext->GetMappings())
    {
        const UInputAction* MappedAction = Mapping.Action.Get();
        bContainsUnexpectedAction |= !ExpectedActions.Contains(MappedAction);
        if (MappedAction != nullptr)
        {
            MappedActions.Add(MappedAction);
        }
    }

    bool bContainsEveryExpectedAction = true;
    for (const UInputAction* ExpectedAction : ExpectedActions)
    {
        bContainsEveryExpectedAction &= MappedActions.Contains(ExpectedAction);
    }
    const bool bMappingsValid = !bContainsUnexpectedAction
        && MappedActions.Num() == ExpectedActions.Num()
        && bContainsEveryExpectedAction;
    if (!bMappingsValid && bLogErrors)
    {
        UE_LOG(LogAeroSimInput, Error, TEXT("Input suite rejected: MappingContext mappings do not exactly reference the configured actions."));
        ensureAlwaysMsgf(false, TEXT("AeroSim input MappingContext/action suite is inconsistent."));
    }
    return bMappingsValid;
}

bool AFlightPawn::ValidateConfiguredInputSuite() const
{
    return IsInputSuiteValid(InputSuite, true);
}

void AFlightPawn::BeginPlay()
{
    Super::BeginPlay();
    InitialTransform = GetActorTransform();
    ValidateConfiguredInputSuite();
}

void AFlightPawn::PawnClientRestart()
{
    Super::PawnClientRestart();
    ApplyMappingContext();
}

void AFlightPawn::ApplyMappingContext() const
{
    if (!ValidateConfiguredInputSuite())
    {
        return;
    }

    APlayerController* PlayerController = Cast<APlayerController>(GetController());
    ULocalPlayer* LocalPlayer = PlayerController != nullptr ? PlayerController->GetLocalPlayer() : nullptr;
    UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer != nullptr
        ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer)
        : nullptr;

    if (Subsystem != nullptr)
    {
        Subsystem->RemoveMappingContext(InputSuite.MappingContext);
        Subsystem->AddMappingContext(InputSuite.MappingContext, 0);
    }
}

void AFlightPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    if (EnhancedInput == nullptr || !ValidateConfiguredInputSuite())
    {
        return;
    }

    EnhancedInput->BindAction(InputSuite.MoveAction, ETriggerEvent::Triggered, this, &AFlightPawn::HandleMove);
    EnhancedInput->BindAction(InputSuite.LookAction, ETriggerEvent::Triggered, this, &AFlightPawn::HandleLook);
    EnhancedInput->BindAction(InputSuite.ThrottleAction, ETriggerEvent::Triggered, this, &AFlightPawn::HandleThrottle);
    EnhancedInput->BindAction(InputSuite.ResetAction, ETriggerEvent::Started, this, &AFlightPawn::HandleReset);
}

void AFlightPawn::HandleMove(const FInputActionValue& Value)
{
    const FVector2D Input = Value.Get<FVector2D>();
    AddMovementInput(GetActorForwardVector(), Input.Y);
    AddMovementInput(GetActorRightVector(), Input.X);
}

void AFlightPawn::HandleLook(const FInputActionValue& Value)
{
    const FVector2D Input = Value.Get<FVector2D>();
    AddControllerYawInput(Input.X);
    AddControllerPitchInput(Input.Y);
}

void AFlightPawn::HandleThrottle(const FInputActionValue& Value)
{
    AddMovementInput(GetActorUpVector(), Value.Get<float>());
}

void AFlightPawn::HandleReset(const FInputActionValue& Value)
{
    if (!Value.Get<bool>())
    {
        return;
    }

    Movement->StopMovementImmediately();
    SetActorTransform(InitialTransform, false, nullptr, ETeleportType::ResetPhysics);
}
