#include "FlightPawn.h"

#include "AeroSimRuntimeContract.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CoreDelegates.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogAeroSimInput, Log, All);
DEFINE_LOG_CATEGORY_STATIC(LogAeroSimFlight, Log, All);

namespace AeroSimFlight
{
constexpr float TakeoffThreshold = 0.55F;
constexpr float LandingThreshold = -0.55F;
constexpr float FlightAltitudeCm = 300.0F;
constexpr float LandingEntryAltitudeCm = 600.0F;
constexpr float TakeoffAccelerationCmPerSecondSquared = 900.0F;
constexpr float LandingDescentCmPerSecond = 250.0F;
constexpr float CruiseSpeedCmPerSecond = 1800.0F;
constexpr float MinimumFlightSpeedCmPerSecond = 500.0F;
constexpr float VelocityResponse = 1.8F;
constexpr float PitchRateDegreesPerSecond = 40.0F;
constexpr float RollRateDegreesPerSecond = 65.0F;
constexpr float YawRateDegreesPerSecond = 50.0F;
constexpr float MaximumPitchDegrees = 60.0F;
constexpr float MaximumRollDegrees = 75.0F;
constexpr float CentimetersPerMeter = 100.0F;
}

void FFlightControlInput::Clamp()
{
    Pitch = FMath::Clamp(Pitch, -1.0F, 1.0F);
    Roll = FMath::Clamp(Roll, -1.0F, 1.0F);
    Yaw = FMath::Clamp(Yaw, -1.0F, 1.0F);
    Throttle = FMath::Clamp(Throttle, -1.0F, 1.0F);
}

void FFlightControlInput::Neutralize()
{
    Pitch = 0.0F;
    Roll = 0.0F;
    Yaw = 0.0F;
    Throttle = 0.0F;
}

bool FFlightControlInput::IsNeutral() const
{
    return FMath::IsNearlyZero(Pitch)
        && FMath::IsNearlyZero(Roll)
        && FMath::IsNearlyZero(Yaw)
        && FMath::IsNearlyZero(Throttle);
}

void FFlightModel::Advance(const FFlightControlInput& RawInput, const float DeltaSeconds)
{
    if (Phase == EFlightPhase::Paused || DeltaSeconds <= 0.0F)
    {
        return;
    }

    FFlightControlInput Input = RawInput;
    Input.Clamp();
    const float Step = FMath::Min(DeltaSeconds, 0.1F);

    if (Phase == EFlightPhase::Grounded)
    {
        Position.Z = 0.0F;
        Velocity = FVector::ZeroVector;
        Rotation.Roll = 0.0F;
        Rotation.Pitch = 0.0F;
        if (Input.Throttle >= AeroSimFlight::TakeoffThreshold)
        {
            Phase = EFlightPhase::Takeoff;
        }
        return;
    }

    Rotation.Pitch = FMath::Clamp(
        Rotation.Pitch + Input.Pitch * AeroSimFlight::PitchRateDegreesPerSecond * Step,
        -AeroSimFlight::MaximumPitchDegrees,
        AeroSimFlight::MaximumPitchDegrees);
    Rotation.Roll = FMath::Clamp(
        Rotation.Roll + Input.Roll * AeroSimFlight::RollRateDegreesPerSecond * Step,
        -AeroSimFlight::MaximumRollDegrees,
        AeroSimFlight::MaximumRollDegrees);
    Rotation.Yaw = FRotator::NormalizeAxis(
        Rotation.Yaw + Input.Yaw * AeroSimFlight::YawRateDegreesPerSecond * Step);

    if (Phase == EFlightPhase::Takeoff)
    {
        Velocity.Z += AeroSimFlight::TakeoffAccelerationCmPerSecondSquared
            * FMath::Max(Input.Throttle, 0.25F) * Step;
        Velocity.X = FMath::FInterpTo(Velocity.X, AeroSimFlight::MinimumFlightSpeedCmPerSecond, Step, AeroSimFlight::VelocityResponse);
        Position += Rotation.RotateVector(Velocity) * Step;
        if (Position.Z >= AeroSimFlight::FlightAltitudeCm)
        {
            Phase = EFlightPhase::Flight;
        }
        return;
    }

    if (Phase == EFlightPhase::Flight
        && Input.Throttle <= AeroSimFlight::LandingThreshold
        && Position.Z <= AeroSimFlight::LandingEntryAltitudeCm)
    {
        Phase = EFlightPhase::Landing;
    }

    if (Phase == EFlightPhase::Landing)
    {
        const float TargetForwardSpeed = AeroSimFlight::MinimumFlightSpeedCmPerSecond * 0.5F;
        Velocity.X = FMath::FInterpTo(Velocity.X, TargetForwardSpeed, Step, AeroSimFlight::VelocityResponse);
        Velocity.Y = 0.0F;
        Velocity.Z = -AeroSimFlight::LandingDescentCmPerSecond;
        Position += Rotation.RotateVector(FVector(Velocity.X, 0.0F, 0.0F)) * Step;
        Position.Z += Velocity.Z * Step;
        if (Position.Z <= 0.0F)
        {
            Position.Z = 0.0F;
            Velocity = FVector::ZeroVector;
            Rotation.Pitch = 0.0F;
            Rotation.Roll = 0.0F;
            Phase = EFlightPhase::Grounded;
        }
        return;
    }

    const float TargetSpeed = FMath::Lerp(
        AeroSimFlight::MinimumFlightSpeedCmPerSecond,
        AeroSimFlight::CruiseSpeedCmPerSecond,
        (Input.Throttle + 1.0F) * 0.5F);
    const FVector TargetVelocity = Rotation.Vector() * TargetSpeed;
    Velocity = FMath::VInterpTo(Velocity, TargetVelocity, Step, AeroSimFlight::VelocityResponse);
    Position += Velocity * Step;
    Position.Z = FMath::Max(Position.Z, 0.0F);
}

void FFlightModel::Pause()
{
    if (Phase != EFlightPhase::Paused)
    {
        ResumePhase = Phase;
        Phase = EFlightPhase::Paused;
    }
}

void FFlightModel::Resume()
{
    if (Phase == EFlightPhase::Paused)
    {
        Phase = ResumePhase;
    }
}

void FFlightModel::Reset()
{
    Phase = EFlightPhase::Grounded;
    ResumePhase = EFlightPhase::Grounded;
    Position = FVector::ZeroVector;
    Velocity = FVector::ZeroVector;
    Rotation = FRotator::ZeroRotator;
}

AFlightPawn::AFlightPawn()
{
    PrimaryActorTick.bCanEverTick = true;

    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
    SetRootComponent(Body);
    Body->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));

    Fuselage = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Fuselage"));
    Fuselage->SetupAttachment(Body);
    Fuselage->SetStaticMesh(CubeFinder.Object);
    Fuselage->SetRelativeScale3D(FVector(3.2F, 0.8F, 0.55F));
    Fuselage->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Fuselage->SetCastShadow(true);

    LeftWing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftWing"));
    LeftWing->SetupAttachment(Body);
    LeftWing->SetStaticMesh(CubeFinder.Object);
    LeftWing->SetRelativeLocation(FVector(-10.0F, -115.0F, 0.0F));
    LeftWing->SetRelativeScale3D(FVector(0.45F, 1.6F, 0.10F));
    LeftWing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    LeftWing->SetCastShadow(true);

    RightWing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightWing"));
    RightWing->SetupAttachment(Body);
    RightWing->SetStaticMesh(CubeFinder.Object);
    RightWing->SetRelativeLocation(FVector(-10.0F, 115.0F, 0.0F));
    RightWing->SetRelativeScale3D(FVector(0.45F, 1.6F, 0.10F));
    RightWing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RightWing->SetCastShadow(true);

    Tail = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tail"));
    Tail->SetupAttachment(Body);
    Tail->SetStaticMesh(CubeFinder.Object);
    Tail->SetRelativeLocation(FVector(-130.0F, 0.0F, 55.0F));
    Tail->SetRelativeScale3D(FVector(0.35F, 0.12F, 0.75F));
    Tail->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Tail->SetCastShadow(true);

    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(Body);
    SpringArm->bUsePawnControlRotation = false;
    SpringArm->bDoCollisionTest = true;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);

    InputSuite.MappingContext = CreateDefaultSubobject<UInputMappingContext>(TEXT("DefaultFlightMappingContext"));
    InputSuite.PitchAction = CreateDefaultSubobject<UInputAction>(TEXT("DefaultPitchAction"));
    InputSuite.RollAction = CreateDefaultSubobject<UInputAction>(TEXT("DefaultRollAction"));
    InputSuite.YawAction = CreateDefaultSubobject<UInputAction>(TEXT("DefaultYawAction"));
    InputSuite.ThrottleAction = CreateDefaultSubobject<UInputAction>(TEXT("DefaultThrottleAction"));
    InputSuite.CameraAction = CreateDefaultSubobject<UInputAction>(TEXT("DefaultCameraAction"));
    InputSuite.PauseAction = CreateDefaultSubobject<UInputAction>(TEXT("DefaultPauseAction"));
    InputSuite.ResetAction = CreateDefaultSubobject<UInputAction>(TEXT("DefaultResetAction"));

    InputSuite.PitchAction->ValueType = EInputActionValueType::Axis1D;
    InputSuite.RollAction->ValueType = EInputActionValueType::Axis1D;
    InputSuite.YawAction->ValueType = EInputActionValueType::Axis1D;
    InputSuite.ThrottleAction->ValueType = EInputActionValueType::Axis1D;
    InputSuite.CameraAction->ValueType = EInputActionValueType::Boolean;
    InputSuite.PauseAction->ValueType = EInputActionValueType::Boolean;
    InputSuite.PauseAction->bTriggerWhenPaused = true;
    InputSuite.ResetAction->ValueType = EInputActionValueType::Boolean;
    InputSuite.ResetAction->bTriggerWhenPaused = true;

    UInputModifierNegate* PitchDownNegate = CreateDefaultSubobject<UInputModifierNegate>(TEXT("PitchDownNegate"));
    InputSuite.MappingContext->MapKey(InputSuite.PitchAction, EKeys::W);
    InputSuite.MappingContext->MapKey(InputSuite.PitchAction, EKeys::S).Modifiers.Add(PitchDownNegate);
    InputSuite.MappingContext->MapKey(InputSuite.PitchAction, EKeys::Gamepad_RightY);
    InputSuite.MappingContext->MapKey(InputSuite.PitchAction, EKeys::MouseY);

    UInputModifierNegate* RollLeftNegate = CreateDefaultSubobject<UInputModifierNegate>(TEXT("RollLeftNegate"));
    InputSuite.MappingContext->MapKey(InputSuite.RollAction, EKeys::D);
    InputSuite.MappingContext->MapKey(InputSuite.RollAction, EKeys::A).Modifiers.Add(RollLeftNegate);
    InputSuite.MappingContext->MapKey(InputSuite.RollAction, EKeys::Gamepad_LeftX);

    UInputModifierNegate* YawLeftNegate = CreateDefaultSubobject<UInputModifierNegate>(TEXT("YawLeftNegate"));
    InputSuite.MappingContext->MapKey(InputSuite.YawAction, EKeys::E);
    InputSuite.MappingContext->MapKey(InputSuite.YawAction, EKeys::Q).Modifiers.Add(YawLeftNegate);
    InputSuite.MappingContext->MapKey(InputSuite.YawAction, EKeys::Gamepad_RightX);
    InputSuite.MappingContext->MapKey(InputSuite.YawAction, EKeys::MouseX);

    UInputModifierNegate* ThrottleDownNegate = CreateDefaultSubobject<UInputModifierNegate>(TEXT("ThrottleDownNegate"));
    InputSuite.MappingContext->MapKey(InputSuite.ThrottleAction, EKeys::LeftShift);
    InputSuite.MappingContext->MapKey(InputSuite.ThrottleAction, EKeys::LeftControl).Modifiers.Add(ThrottleDownNegate);
    InputSuite.MappingContext->MapKey(InputSuite.ThrottleAction, EKeys::Gamepad_RightTriggerAxis);
    InputSuite.MappingContext->MapKey(InputSuite.ThrottleAction, EKeys::Gamepad_LeftTriggerAxis).Modifiers.Add(ThrottleDownNegate);

    InputSuite.MappingContext->MapKey(InputSuite.CameraAction, EKeys::C);
    InputSuite.MappingContext->MapKey(InputSuite.CameraAction, EKeys::Gamepad_FaceButton_Top);
    InputSuite.MappingContext->MapKey(InputSuite.PauseAction, EKeys::P);
    InputSuite.MappingContext->MapKey(InputSuite.PauseAction, EKeys::Gamepad_Special_Right);
    InputSuite.MappingContext->MapKey(InputSuite.ResetAction, EKeys::R);
    InputSuite.MappingContext->MapKey(InputSuite.ResetAction, EKeys::Gamepad_FaceButton_Left);

    ApplyCameraMode();
}

bool AFlightPawn::IsInputSuiteValid(const FFlightInputSuite& Candidate, const bool bLogErrors) const
{
    const bool bReferencesPresent = Candidate.MappingContext != nullptr
        && Candidate.PitchAction != nullptr
        && Candidate.RollAction != nullptr
        && Candidate.YawAction != nullptr
        && Candidate.ThrottleAction != nullptr
        && Candidate.CameraAction != nullptr
        && Candidate.PauseAction != nullptr
        && Candidate.ResetAction != nullptr;
    if (!bReferencesPresent)
    {
        if (bLogErrors)
        {
            UE_LOG(LogAeroSimInput, Error, TEXT("Input suite rejected: mapping context and all seven semantic actions are required."));
            ensureAlwaysMsgf(false, TEXT("AeroSim input suite must be overridden atomically."));
        }
        return false;
    }

    const bool bTypesValid = Candidate.PitchAction->ValueType == EInputActionValueType::Axis1D
        && Candidate.RollAction->ValueType == EInputActionValueType::Axis1D
        && Candidate.YawAction->ValueType == EInputActionValueType::Axis1D
        && Candidate.ThrottleAction->ValueType == EInputActionValueType::Axis1D
        && Candidate.CameraAction->ValueType == EInputActionValueType::Boolean
        && Candidate.PauseAction->ValueType == EInputActionValueType::Boolean
        && Candidate.ResetAction->ValueType == EInputActionValueType::Boolean;
    if (!bTypesValid)
    {
        if (bLogErrors)
        {
            UE_LOG(LogAeroSimInput, Error, TEXT("Input suite rejected: action ValueTypes do not match the Phase 1 contract."));
            ensureAlwaysMsgf(false, TEXT("AeroSim input suite action ValueTypes are invalid."));
        }
        return false;
    }

    TSet<const UInputAction*> ExpectedActions = {
        Candidate.PitchAction.Get(), Candidate.RollAction.Get(), Candidate.YawAction.Get(),
        Candidate.ThrottleAction.Get(), Candidate.CameraAction.Get(), Candidate.PauseAction.Get(),
        Candidate.ResetAction.Get()
    };
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
        UE_LOG(LogAeroSimInput, Error, TEXT("Input suite rejected: context mappings do not exactly reference the configured actions."));
        ensureAlwaysMsgf(false, TEXT("AeroSim input mapping context/action suite is inconsistent."));
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
    FlightModel.Reset();
    ValidateConfiguredInputSuite();

    UStaticMeshComponent* ShellParts[] = { Fuselage, LeftWing, RightWing, Tail };
    for (UStaticMeshComponent* ShellPart : ShellParts)
    {
        if (ShellPart != nullptr && ShellPart->GetMaterial(0) != nullptr)
        {
            UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(ShellPart->GetMaterial(0), this);
            Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.0F, 0.12F, 0.02F));
            ShellPart->SetMaterial(0, Material);
        }
    }
    ApplyCameraMode();

    FCoreDelegates::ApplicationWillDeactivateDelegate.AddUObject(this, &AFlightPawn::HandleApplicationDeactivated);
    IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().AddUObject(
        this, &AFlightPawn::HandleInputDeviceConnectionChanged);
    UE_LOG(LogAeroSimFlight, Display, TEXT("RuntimeContract=%s BuildId=%s"),
        FAeroSimRuntimeContract::Contract, FAeroSimRuntimeContract::BuildId);
}

void AFlightPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    FCoreDelegates::ApplicationWillDeactivateDelegate.RemoveAll(this);
    IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().RemoveAll(this);
    Super::EndPlay(EndPlayReason);
}

void AFlightPawn::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    FlightModel.Advance(ControlInput, DeltaSeconds);
    SetActorLocationAndRotation(
        InitialTransform.TransformPosition(FlightModel.Position),
        InitialTransform.TransformRotation(FlightModel.Rotation.Quaternion()),
        false, nullptr, ETeleportType::None);
}

void AFlightPawn::PawnClientRestart()
{
    Super::PawnClientRestart();
    NeutralizeInput();
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

    EnhancedInput->BindAction(InputSuite.PitchAction, ETriggerEvent::Triggered, this, &AFlightPawn::HandlePitch);
    EnhancedInput->BindAction(InputSuite.RollAction, ETriggerEvent::Triggered, this, &AFlightPawn::HandleRoll);
    EnhancedInput->BindAction(InputSuite.YawAction, ETriggerEvent::Triggered, this, &AFlightPawn::HandleYaw);
    EnhancedInput->BindAction(InputSuite.ThrottleAction, ETriggerEvent::Triggered, this, &AFlightPawn::HandleThrottle);
    EnhancedInput->BindAction(InputSuite.PitchAction, ETriggerEvent::Completed, this, &AFlightPawn::HandlePitchReleased);
    EnhancedInput->BindAction(InputSuite.RollAction, ETriggerEvent::Completed, this, &AFlightPawn::HandleRollReleased);
    EnhancedInput->BindAction(InputSuite.YawAction, ETriggerEvent::Completed, this, &AFlightPawn::HandleYawReleased);
    EnhancedInput->BindAction(InputSuite.ThrottleAction, ETriggerEvent::Completed, this, &AFlightPawn::HandleThrottleReleased);
    EnhancedInput->BindAction(InputSuite.CameraAction, ETriggerEvent::Started, this, &AFlightPawn::HandleCamera);
    EnhancedInput->BindAction(InputSuite.PauseAction, ETriggerEvent::Started, this, &AFlightPawn::HandlePause);
    EnhancedInput->BindAction(InputSuite.ResetAction, ETriggerEvent::Started, this, &AFlightPawn::HandleReset);
}

void AFlightPawn::SetSemanticInput(const FFlightControlInput& NewInput)
{
    if (FlightModel.Phase == EFlightPhase::Paused)
    {
        NeutralizeInput();
        return;
    }
    ControlInput = NewInput;
    ControlInput.Clamp();
}

void AFlightPawn::NeutralizeInput()
{
    ControlInput.Neutralize();
    UE_LOG(LogAeroSimInput, Verbose, TEXT("NeutralizeInput"));
}

void AFlightPawn::SetSafePaused(const bool bPaused)
{
    NeutralizeInput();
    APlayerController* PlayerController = Cast<APlayerController>(GetController());
    if (bPaused)
    {
        FlightModel.Pause();
        if (PlayerController != nullptr)
        {
            PlayerController->SetPause(true);
        }
    }
    else
    {
        if (PlayerController != nullptr)
        {
            PlayerController->SetPause(false);
        }
        FlightModel.Resume();
    }
}

void AFlightPawn::CycleCamera()
{
    CameraMode = CameraMode == EFlightCameraMode::Chase
        ? EFlightCameraMode::FPV
        : EFlightCameraMode::Chase;
    ApplyCameraMode();
}

void AFlightPawn::ApplyCameraMode()
{
    if (SpringArm == nullptr)
    {
        return;
    }
    const bool bShellVisible = CameraMode == EFlightCameraMode::Chase;
    Fuselage->SetVisibility(bShellVisible, true);
    LeftWing->SetVisibility(bShellVisible, true);
    RightWing->SetVisibility(bShellVisible, true);
    Tail->SetVisibility(bShellVisible, true);

    if (CameraMode == EFlightCameraMode::FPV)
    {
        SpringArm->TargetArmLength = 0.0F;
        SpringArm->SetRelativeLocation(FVector(170.0, 0.0, 65.0));
    }
    else
    {
        SpringArm->TargetArmLength = 1050.0F;
        SpringArm->SetRelativeLocation(FVector(-40.0, 0.0, 240.0));
        SpringArm->SetRelativeRotation(FRotator(-12.0F, 0.0F, 0.0F));
    }
}

bool AFlightPawn::IsAircraftShellVisible() const
{
    return Fuselage != nullptr && Fuselage->IsVisible()
        && LeftWing != nullptr && LeftWing->IsVisible()
        && RightWing != nullptr && RightWing->IsVisible()
        && Tail != nullptr && Tail->IsVisible();
}

void AFlightPawn::ResetFlight()
{
    NeutralizeInput();
    FlightModel.Reset();
    if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
    {
        PlayerController->SetPause(false);
    }
    SetActorTransform(InitialTransform, false, nullptr, ETeleportType::ResetPhysics);
}

FFlightHudData AFlightPawn::GetHudData() const
{
    FFlightHudData Data;
    Data.Phase = FlightModel.Phase;
    Data.CameraMode = CameraMode;
    Data.AltitudeMeters = FlightModel.Position.Z / AeroSimFlight::CentimetersPerMeter;
    Data.SpeedMetersPerSecond = FlightModel.Velocity.Size() / AeroSimFlight::CentimetersPerMeter;
    Data.Throttle = ControlInput.Throttle;
    Data.bInputNeutral = ControlInput.IsNeutral();
    Data.BuildId = FAeroSimRuntimeContract::BuildId;
    return Data;
}

void AFlightPawn::HandlePitch(const FInputActionValue& Value)
{
    FFlightControlInput NewInput = ControlInput;
    NewInput.Pitch = Value.Get<float>();
    SetSemanticInput(NewInput);
}

void AFlightPawn::HandleRoll(const FInputActionValue& Value)
{
    FFlightControlInput NewInput = ControlInput;
    NewInput.Roll = Value.Get<float>();
    SetSemanticInput(NewInput);
}

void AFlightPawn::HandleYaw(const FInputActionValue& Value)
{
    FFlightControlInput NewInput = ControlInput;
    NewInput.Yaw = Value.Get<float>();
    SetSemanticInput(NewInput);
}

void AFlightPawn::HandleThrottle(const FInputActionValue& Value)
{
    FFlightControlInput NewInput = ControlInput;
    NewInput.Throttle = Value.Get<float>();
    SetSemanticInput(NewInput);
}

void AFlightPawn::HandlePitchReleased(const FInputActionValue& Value)
{
    static_cast<void>(Value);
    ControlInput.Pitch = 0.0F;
}

void AFlightPawn::HandleRollReleased(const FInputActionValue& Value)
{
    static_cast<void>(Value);
    ControlInput.Roll = 0.0F;
}

void AFlightPawn::HandleYawReleased(const FInputActionValue& Value)
{
    static_cast<void>(Value);
    ControlInput.Yaw = 0.0F;
}

void AFlightPawn::HandleThrottleReleased(const FInputActionValue& Value)
{
    static_cast<void>(Value);
    ControlInput.Throttle = 0.0F;
}

void AFlightPawn::HandleCamera(const FInputActionValue& Value)
{
    if (Value.Get<bool>())
    {
        CycleCamera();
    }
}

void AFlightPawn::HandlePause(const FInputActionValue& Value)
{
    if (Value.Get<bool>())
    {
        SetSafePaused(FlightModel.Phase != EFlightPhase::Paused);
    }
}

void AFlightPawn::HandleReset(const FInputActionValue& Value)
{
    if (Value.Get<bool>())
    {
        ResetFlight();
    }
}

void AFlightPawn::HandleApplicationDeactivated()
{
    SetSafePaused(true);
}

void AFlightPawn::HandleInputDeviceConnectionChanged(
    const EInputDeviceConnectionState ConnectionState,
    const FPlatformUserId PlatformUserId,
    const FInputDeviceId InputDeviceId)
{
    static_cast<void>(PlatformUserId);
    static_cast<void>(InputDeviceId);
    if (ConnectionState == EInputDeviceConnectionState::Disconnected)
    {
        SetSafePaused(true);
    }
}
