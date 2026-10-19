#include "FlightPawn.h"

#include "AeroSimRuntimeContract.h"
#include "AeroSimGameMode.h"
#include "AeroSimPlayerController.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
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
#include "PacificCoastGraybox.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogAeroSimInput, Log, All);
DEFINE_LOG_CATEGORY_STATIC(LogAeroSimFlight, Log, All);

namespace AeroSimFlight
{
constexpr float TakeoffThreshold = 0.55F;
constexpr float LandingThreshold = -0.55F;
constexpr float FlightAltitudeCm = 300.0F;
constexpr float LandingEntryAltitudeCm = 45.0F;
constexpr float TakeoffAccelerationCmPerSecondSquared = 900.0F;
constexpr float LandingDescentCmPerSecond = 250.0F;
constexpr float CruiseSpeedCmPerSecond = 1800.0F;
constexpr float VelocityResponse = 1.8F;
constexpr float YawRateDegreesPerSecond = 50.0F;
constexpr float CommandedTiltDegrees = 18.0F;
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

void FFlightModel::Advance(const FFlightControlInput& RawInput, const float DeltaSeconds, const FFlightSurfaceContext& Surface)
{
    if (Phase == EFlightPhase::Paused || DeltaSeconds <= 0.0F)
    {
        return;
    }

    FFlightControlInput Input = RawInput;
    bSupportImpact = false;
    Input.Clamp();
    const float Step = FMath::Min(DeltaSeconds, 0.1F);

    if (Phase == EFlightPhase::Grounded)
    {
        if (!Surface.bHasSupport || FMath::Abs(Position.Z-Surface.SupportedModelZCm)>2.0F)
        {
            Phase = EFlightPhase::Flight;
        }
        else
        {
            Position.Z = Surface.SupportedModelZCm;
            Velocity = FVector::ZeroVector;
            Rotation.Roll = 0.0F;
            Rotation.Pitch = 0.0F;
            if (Input.Throttle >= AeroSimFlight::TakeoffThreshold)
            {
                TakeoffSurfaceZCm = Surface.SupportedModelZCm;
                Phase = EFlightPhase::Takeoff;
            }
            return;
        }
    }

    // Generic assisted quad: command translation and auto-level when released. This is
    // an explicit prototype control model, not a named aircraft's aerodynamic simulation.
    Rotation.Pitch = FMath::FInterpTo(Rotation.Pitch,-Input.Pitch*AeroSimFlight::CommandedTiltDegrees,Step,AeroSimFlight::VelocityResponse);
    Rotation.Roll = FMath::FInterpTo(Rotation.Roll,Input.Roll*AeroSimFlight::CommandedTiltDegrees,Step,AeroSimFlight::VelocityResponse);
    Rotation.Yaw = FRotator::NormalizeAxis(
        Rotation.Yaw + Input.Yaw * AeroSimFlight::YawRateDegreesPerSecond * Step);

    if (Phase == EFlightPhase::Takeoff)
    {
        Velocity.Z += AeroSimFlight::TakeoffAccelerationCmPerSecondSquared
            * FMath::Max(Input.Throttle, 0.25F) * Step;
        Velocity.X = FMath::FInterpTo(Velocity.X, 0.0F, Step, AeroSimFlight::VelocityResponse);
        Velocity.Y = FMath::FInterpTo(Velocity.Y, 0.0F, Step, AeroSimFlight::VelocityResponse);
        Position += Velocity * Step;
        if (Surface.bHasSupport && Position.Z < Surface.SupportedModelZCm)
        {
            Position.Z = Surface.SupportedModelZCm;
            Velocity.Z = FMath::Max(Velocity.Z, 0.0F);
        }
        if (Position.Z - TakeoffSurfaceZCm >= AeroSimFlight::FlightAltitudeCm)
        {
            Phase = EFlightPhase::Flight;
        }
        return;
    }

    if (Phase == EFlightPhase::Flight
        && Surface.bHasSupport
        && (Input.Throttle <= AeroSimFlight::LandingThreshold || Velocity.Z < -1.0F)
        && Velocity.Size2D()<=40.0F && FMath::Abs(Rotation.Pitch)<=8.0F && FMath::Abs(Rotation.Roll)<=8.0F
        && Position.Z - Surface.SupportedModelZCm <= AeroSimFlight::LandingEntryAltitudeCm + FMath::Max(0.0F,-static_cast<float>(Velocity.Z))*Step)
    {
        Phase = EFlightPhase::Landing;
    }

    if (Phase == EFlightPhase::Landing)
    {
        if (!Surface.bHasSupport || Input.Throttle > 0.2F || Velocity.Size2D()>40.0F
            || FMath::Abs(Rotation.Pitch)>8.0F || FMath::Abs(Rotation.Roll)>8.0F)
        {
            Phase = EFlightPhase::Flight;
        }
        else
        {
            Velocity.X = FMath::FInterpTo(Velocity.X, 0.0F, Step, AeroSimFlight::VelocityResponse);
            Velocity.Y = FMath::FInterpTo(Velocity.Y, 0.0F, Step, AeroSimFlight::VelocityResponse);
            Velocity.Z = -AeroSimFlight::LandingDescentCmPerSecond;
            Position += Velocity * Step;
            if (Position.Z <= Surface.SupportedModelZCm)
            {
                Position.Z = Surface.SupportedModelZCm;
                Velocity = FVector::ZeroVector;
                Rotation.Pitch = 0.0F;
                Rotation.Roll = 0.0F;
                Phase = EFlightPhase::Grounded;
            }
            return;
        }
    }

    FVector Horizontal(Input.Pitch,Input.Roll,0);
    Horizontal=Horizontal.GetClampedToMaxSize(1.0F)*AeroSimFlight::CruiseSpeedCmPerSecond;
    FVector TargetVelocity=FRotator(0,Rotation.Yaw,0).RotateVector(Horizontal);
    TargetVelocity.Z=Input.Throttle*400.0F;
    Velocity = FMath::VInterpTo(Velocity, TargetVelocity, Step, AeroSimFlight::VelocityResponse);
    Position += Velocity * Step;
    if (Surface.bHasSupport && Position.Z < Surface.SupportedModelZCm)
    {
        // A valid slow, level landing returned above. Every remaining support
        // penetration is an unsafe contact, including fast lateral skids.
        bSupportImpact = true;
        Position.Z = Surface.SupportedModelZCm;
        Velocity.Z = FMath::Max(Velocity.Z, 0.0F);
    }
}

void FFlightModel::ResolveMovement(const FVector& ResolvedPosition, const FVector& ResolvedVelocity)
{
    Position = ResolvedPosition;
    Velocity = ResolvedVelocity;
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
    TakeoffSurfaceZCm = 0.0F;
    bSupportImpact = false;
}

AFlightPawn::AFlightPawn()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bTickEvenWhenPaused = true;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

    Body = CreateDefaultSubobject<UBoxComponent>(TEXT("AircraftCollisionRoot"));
    SetRootComponent(Body);
    Body->SetBoxExtent(FVector(16.0F, 14.0F, 5.0F));
    Body->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);

    auto ConfigurePart = [this](UStaticMeshComponent* Part, UStaticMesh* Mesh, const FVector& Location, const FVector& Scale, const FRotator& Rotation = FRotator::ZeroRotator)
    {
        Part->SetupAttachment(Body);
        Part->SetStaticMesh(Mesh);
        Part->SetRelativeLocation(Location);
        Part->SetRelativeScale3D(Scale);
        Part->SetRelativeRotation(Rotation);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetCastShadow(true);
        DetailedVisualParts.Add(Part);
    };

    // Layered ellipsoids give the generic airframe a compact, molded consumer-drone silhouette.
    Fuselage = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shell_Fuselage"));
    ConfigurePart(Fuselage, SphereFinder.Object, FVector(-0.8F, 0.0F, 1.5F), FVector(0.27F, 0.135F, 0.060F), FRotator(0.0F, 0.0F, -3.0F));

    UStaticMeshComponent* UpperCanopy = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shell_UpperCanopy"));
    ConfigurePart(UpperCanopy, SphereFinder.Object, FVector(-1.5F, 0.0F, 4.0F), FVector(0.19F, 0.105F, 0.035F), FRotator(0.0F, 0.0F, -4.0F));
    ShellPanels.Add(UpperCanopy);
    UStaticMeshComponent* NoseFairing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shell_NoseFairing"));
    ConfigurePart(NoseFairing, SphereFinder.Object, FVector(11.0F, 0.0F, 0.8F), FVector(0.105F, 0.100F, 0.052F), FRotator(0.0F, 0.0F, -7.0F));
    ShellPanels.Add(NoseFairing);
    UStaticMeshComponent* BellyFairing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shell_BellyFairing"));
    ConfigurePart(BellyFairing, SphereFinder.Object, FVector(-1.5F, 0.0F, -1.2F), FVector(0.17F, 0.105F, 0.028F));
    ShellPanels.Add(BellyFairing);

    const FVector RotorLocations[] = {
        FVector(16.0F, -14.5F, 3.8F), FVector(16.0F, 14.5F, 3.8F),
        FVector(-15.0F, -14.0F, 3.2F), FVector(-15.0F, 14.0F, 3.2F)
    };
    const TCHAR* ArmNames[] = { TEXT("Arm_FL"), TEXT("Arm_FR"), TEXT("Arm_RL"), TEXT("Arm_RR") };
    const TCHAR* MotorNames[] = { TEXT("Motor_FL"), TEXT("Motor_FR"), TEXT("Motor_RL"), TEXT("Motor_RR") };
    const TCHAR* PropNames[] = { TEXT("Propeller_FL"), TEXT("Propeller_FR"), TEXT("Propeller_RL"), TEXT("Propeller_RR") };
    const TCHAR* PropCrossNames[] = { TEXT("PropellerCross_FL"), TEXT("PropellerCross_FR"), TEXT("PropellerCross_RL"), TEXT("PropellerCross_RR") };
    for (int32 Index = 0; Index < 4; ++Index)
    {
        UStaticMeshComponent* Arm = CreateDefaultSubobject<UStaticMeshComponent>(FName(ArmNames[Index]));
        const FVector ArmCenter = RotorLocations[Index] * 0.53F + FVector(0.0F, 0.0F, -0.3F);
        const float ArmYaw = FMath::RadiansToDegrees(FMath::Atan2(RotorLocations[Index].Y, RotorLocations[Index].X));
        ConfigurePart(Arm, CylinderFinder.Object, ArmCenter, FVector(0.021F, 0.021F, 0.145F), FRotator(90.0F, ArmYaw, 0.0F));
        Arms.Add(Arm);

        UStaticMeshComponent* Motor = CreateDefaultSubobject<UStaticMeshComponent>(FName(MotorNames[Index]));
        ConfigurePart(Motor, CylinderFinder.Object, RotorLocations[Index], FVector(0.038F, 0.038F, 0.030F));
        Motors.Add(Motor);

        const float BladeYaw = Index % 2 == 0 ? 18.0F : -18.0F;
        UStaticMeshComponent* Propeller = CreateDefaultSubobject<UStaticMeshComponent>(FName(PropNames[Index]));
        ConfigurePart(Propeller, CubeFinder.Object, RotorLocations[Index] + FVector(0.0F, 0.0F, 3.15F), FVector(0.115F, 0.009F, 0.0025F), FRotator(0.0F, BladeYaw, 0.0F));
        Propellers.Add(Propeller);
        UStaticMeshComponent* PropellerCross = CreateDefaultSubobject<UStaticMeshComponent>(FName(PropCrossNames[Index]));
        ConfigurePart(PropellerCross, CubeFinder.Object, RotorLocations[Index] + FVector(0.0F, 0.0F, 3.05F), FVector(0.115F, 0.009F, 0.0025F), FRotator(0.0F, BladeYaw + 90.0F, 0.0F));
        Propellers.Add(PropellerCross);
    }

    const FVector SupportLocations[] = {
        FVector(7.0F, -7.5F, -5.7F), FVector(7.0F, 7.5F, -5.7F),
        FVector(-7.0F, -7.5F, -5.7F), FVector(-7.0F, 7.5F, -5.7F)
    };
    const TCHAR* SupportNames[] = { TEXT("Support_FL"), TEXT("Support_FR"), TEXT("Support_RL"), TEXT("Support_RR") };
    for (int32 Index = 0; Index < 4; ++Index)
    {
        UStaticMeshComponent* Support = CreateDefaultSubobject<UStaticMeshComponent>(FName(SupportNames[Index]));
        const float SupportRoll = SupportLocations[Index].Y < 0.0F ? -8.0F : 8.0F;
        ConfigurePart(Support, CylinderFinder.Object, SupportLocations[Index], FVector(0.014F, 0.014F, 0.060F), FRotator(0.0F, 0.0F, SupportRoll));
        LandingSupports.Add(Support);
    }

    GimbalMount = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Metal_GimbalMount"));
    ConfigurePart(GimbalMount, CylinderFinder.Object, FVector(9.0F, 0.0F, -3.0F), FVector(0.030F, 0.030F, 0.020F));
    GimbalCamera = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Metal_GimbalCamera"));
    ConfigurePart(GimbalCamera, SphereFinder.Object, FVector(11.0F, 0.0F, -4.7F), FVector(0.054F, 0.048F, 0.048F));
    CameraLens = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lens_Gimbal"));
    ConfigurePart(CameraLens, CylinderFinder.Object, FVector(15.8F, 0.0F, -4.7F), FVector(0.025F, 0.025F, 0.012F), FRotator(0.0F, 90.0F, 0.0F));

    const FVector LightLocations[] = { FVector(13.8F, -8.8F, 4.2F), FVector(13.8F, 8.8F, 4.2F), FVector(-12.8F, -8.4F, 3.7F), FVector(-12.8F, 8.4F, 3.7F) };
    const TCHAR* LightNames[] = { TEXT("NavigationGreen_FL"), TEXT("NavigationGreen_FR"), TEXT("NavigationRed_RL"), TEXT("NavigationRed_RR") };
    for (int32 Index = 0; Index < 4; ++Index)
    {
        UStaticMeshComponent* Light = CreateDefaultSubobject<UStaticMeshComponent>(FName(LightNames[Index]));
        ConfigurePart(Light, SphereFinder.Object, LightLocations[Index], FVector(0.012F));
        NavigationLights.Add(Light);
    }

    LodProxy = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LOD1_Proxy"));
    LodProxy->SetupAttachment(Body);
    LodProxy->SetStaticMesh(CubeFinder.Object);
    LodProxy->SetRelativeLocation(FVector(0.0F, 0.0F, 1.0F));
    LodProxy->SetRelativeScale3D(FVector(0.32F, 0.27F, 0.065F));
    LodProxy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    LodProxy->SetVisibility(false, true);

    AircraftDefinition = FAircraftDefinition::MakeGenericGolden();

    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(Body);
    SpringArm->bUsePawnControlRotation = false;
    SpringArm->bDoCollisionTest = true;
    SpringArm->ProbeSize = 28.0F;
    SpringArm->ProbeChannel = ECC_Camera;
    SpringArm->bEnableCameraLag = true;
    SpringArm->CameraLagSpeed = 12.0F;

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

    UInputModifierNegate* RollLeftNegate = CreateDefaultSubobject<UInputModifierNegate>(TEXT("RollLeftNegate"));
    InputSuite.MappingContext->MapKey(InputSuite.RollAction, EKeys::D);
    InputSuite.MappingContext->MapKey(InputSuite.RollAction, EKeys::A).Modifiers.Add(RollLeftNegate);
    InputSuite.MappingContext->MapKey(InputSuite.RollAction, EKeys::Gamepad_LeftX);

    UInputModifierNegate* YawLeftNegate = CreateDefaultSubobject<UInputModifierNegate>(TEXT("YawLeftNegate"));
    InputSuite.MappingContext->MapKey(InputSuite.YawAction, EKeys::E);
    InputSuite.MappingContext->MapKey(InputSuite.YawAction, EKeys::Q).Modifiers.Add(YawLeftNegate);
    InputSuite.MappingContext->MapKey(InputSuite.YawAction, EKeys::Gamepad_RightX);

    UInputModifierNegate* ThrottleDownNegate = CreateDefaultSubobject<UInputModifierNegate>(TEXT("ThrottleDownNegate"));
    InputSuite.MappingContext->MapKey(InputSuite.ThrottleAction, EKeys::LeftShift);
    InputSuite.MappingContext->MapKey(InputSuite.ThrottleAction, EKeys::LeftControl).Modifiers.Add(ThrottleDownNegate);
    InputSuite.MappingContext->MapKey(InputSuite.ThrottleAction, EKeys::Gamepad_RightTriggerAxis);
    InputSuite.MappingContext->MapKey(InputSuite.ThrottleAction, EKeys::Gamepad_LeftTriggerAxis).Modifiers.Add(ThrottleDownNegate);

    InputSuite.MappingContext->MapKey(InputSuite.CameraAction, EKeys::C);
    InputSuite.MappingContext->MapKey(InputSuite.CameraAction, EKeys::Gamepad_FaceButton_Top);
    InputSuite.MappingContext->MapKey(InputSuite.PauseAction, EKeys::P);
    InputSuite.MappingContext->MapKey(InputSuite.PauseAction, EKeys::Escape);
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
    FlightTrace.Reset();
    FlightTrace.Record(0.0F, FlightModel.Position, FlightModel.Rotation);
    ValidateConfiguredInputSuite();

    auto ApplyProceduralMaterial = [this](UStaticMeshComponent* Part, const FLinearColor& Color, const float Metallic, const float Roughness)
    {
        if (Part == nullptr || Part->GetMaterial(0) == nullptr)
        {
            return;
        }
        UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Part->GetMaterial(0), this);
        Material->SetVectorParameterValue(TEXT("Color"), Color);
        Material->SetVectorParameterValue(TEXT("BaseColor"), Color);
        Material->SetScalarParameterValue(TEXT("Metallic"), Metallic);
        Material->SetScalarParameterValue(TEXT("Roughness"), Roughness);
        Part->SetMaterial(0, Material);
    };
    ApplyProceduralMaterial(Fuselage, FLinearColor(0.34F, 0.39F, 0.43F), 0.05F, 0.58F);
    for (UStaticMeshComponent* Panel : ShellPanels) ApplyProceduralMaterial(Panel, FLinearColor(0.46F, 0.51F, 0.54F), 0.04F, 0.52F);
    for (UStaticMeshComponent* Arm : Arms) ApplyProceduralMaterial(Arm, FLinearColor(0.24F, 0.28F, 0.31F), 0.08F, 0.52F);
    for (UStaticMeshComponent* Motor : Motors) ApplyProceduralMaterial(Motor, FLinearColor(0.08F, 0.10F, 0.12F), 0.72F, 0.28F);
    for (UStaticMeshComponent* Propeller : Propellers) ApplyProceduralMaterial(Propeller, FLinearColor(0.025F, 0.03F, 0.035F), 0.15F, 0.36F);
    for (UStaticMeshComponent* Support : LandingSupports) ApplyProceduralMaterial(Support, FLinearColor(0.12F, 0.14F, 0.15F), 0.18F, 0.48F);
    ApplyProceduralMaterial(GimbalMount, FLinearColor(0.08F, 0.09F, 0.10F), 0.76F, 0.24F);
    ApplyProceduralMaterial(GimbalCamera, FLinearColor(0.12F, 0.13F, 0.14F), 0.46F, 0.34F);
    ApplyProceduralMaterial(CameraLens, FLinearColor(0.005F, 0.025F, 0.055F), 0.35F, 0.08F);
    for (int32 Index = 0; Index < NavigationLights.Num(); ++Index)
    {
        ApplyProceduralMaterial(NavigationLights[Index], Index < 2 ? FLinearColor(0.02F, 1.0F, 0.12F) : FLinearColor(1.0F, 0.025F, 0.01F), 0.0F, 0.16F);
    }
    ApplyProceduralMaterial(LodProxy, FLinearColor(0.20F, 0.24F, 0.27F), 0.06F, 0.62F);
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
    ApplyFov();
    // Product pages own presentation; do not advance, record or overwrite their visual pose.
    if (!IsProductFlightPage() || bReplayVisualActive)
    {
        NeutralizeInput();
        if (bResumeRequested) SetSafePaused(true);
        return;
    }
    AdvanceResumeCountdown(DeltaSeconds);
    if (FlightModel.Phase == EFlightPhase::Paused) return;
    const FVector BeforeMovement = GetActorLocation();
    const FFlightSurfaceContext Surface = QuerySurface(BeforeMovement);
    FlightModel.bSupportImpact=false;
    if (!bCollisionHold || !ControlInput.IsNeutral())
    {
        bCollisionHold=false;
        FlightModel.Advance(ControlInput, DeltaSeconds, Surface);
    }
    GimbalController.Advance(DeltaSeconds);
    if (GimbalCamera != nullptr)
    {
        GimbalCamera->SetRelativeRotation(GimbalController.GetStabilizedRelativeRotation());
    }
    if (CameraMode == EFlightCameraMode::Gimbal && SpringArm != nullptr)
    {
        SpringArm->SetWorldRotation(FRotator(
            GimbalController.PitchDegrees,
            GetActorRotation().Yaw + GimbalController.YawDegrees,
            0.0F));
    }
    for (int32 Index = 0; Index < Propellers.Num(); ++Index)
    {
        if (Propellers[Index] != nullptr)
        {
            const int32 RotorIndex = Index / 2;
            Propellers[Index]->AddLocalRotation(FRotator(0.0F, (RotorIndex % 2 == 0 ? 1.0F : -1.0F) * 1080.0F * DeltaSeconds, 0.0F));
        }
    }
    ApplyFov();
    FHitResult MovementHit;
    SetActorLocationAndRotation(
        InitialTransform.TransformPosition(FlightModel.Position),
        InitialTransform.TransformRotation(FlightModel.Rotation.Quaternion()),
        true, &MovementHit, ETeleportType::None);
    // The sweep, not its unachieved requested destination, owns simulation, trace and scoring.
    FVector ResolvedVelocity = FlightModel.Velocity;
    const bool bContact = MovementHit.bBlockingHit || FlightModel.bSupportImpact;
    if (bContact)
    {
        bCollisionHold=true;
        ResolvedVelocity = FVector::ZeroVector;
        NeutralizeInput();
    }
    const FVector ResolvedWorld = GetActorLocation();
    FlightModel.ResolveMovement(WorldToModelPosition(ResolvedWorld), ResolvedVelocity);
    const bool bWaterContact = IsOverWater(ResolvedWorld)
        && ResolvedWorld.Z - GetFootDepthCm() <= APacificCoastGraybox::SeaLevelCm + 2.0F;
    RecordTrace(DeltaSeconds);
    if (bWaterContact)
    {
        bWaterIncidentLatched = true;
        FlightModel.Velocity = FVector::ZeroVector;
        SetSafePaused(true);
    }
    else if (FlightModel.bSupportImpact) SetSafePaused(true);
    if (AAeroSimGameMode* GameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<AAeroSimGameMode>() : nullptr)
    {
        GameMode->ObserveMission(DeltaSeconds, FlightModel.Phase, ResolvedWorld,
            InitialTransform.TransformVectorNoScale(FlightModel.Velocity), GetActorRotation(), bContact,
            IsSupportedOnLaunchMat(), bWaterContact);
    }
}

float AFlightPawn::GetFootDepthCm() const
{
    double Lowest = 0;
    for (const UStaticMeshComponent* Support : LandingSupports)
        if (Support && Support->GetStaticMesh())
            Lowest = FMath::Min(Lowest, Support->CalcBounds(Support->GetRelativeTransform()).GetBox().Min.Z);
    return static_cast<float>(-Lowest + .3);
}

bool AFlightPawn::IsOverWater(const FVector& WorldOrigin) const
{
    return WorldOrigin.X < -240000.0F || FMath::Abs(WorldOrigin.Y) > 220000.0F
        || WorldOrigin.X > APacificCoastGraybox::GetCoastalShoreX(WorldOrigin.Y) + 30.0F;
}

FFlightSurfaceContext AFlightPawn::QuerySurface(const FVector& WorldOrigin) const
{
    FFlightSurfaceContext Result;
    Result.bHasSupport = false;
    if (!GetWorld()) return Result;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(AeroSimFlightSupport), false, this);
    const FCollisionObjectQueryParams Objects(ECC_WorldStatic);
    const float FootDepth = GetFootDepthCm();
    float Highest = -BIG_NUMBER, Lowest = BIG_NUMBER;
    // The thin flexible mat is intentionally visual-only; read its actual centre plane,
    // and only accept it when solid terrain underneath has already been hit.
    const AAeroSimGameMode* Mode=GetWorld()->GetAuthGameMode<AAeroSimGameMode>();
    APacificCoastGraybox* Coast=Mode ? Mode->GetEnvironmentActor() : nullptr;
    UProceduralMeshComponent* Mat=Coast ? Cast<UProceduralMeshComponent>(Coast->GetDefaultSubobjectByName(TEXT("CoastalLaunchMat"))) : nullptr;
    const FProcMeshSection* MatSection=Mat ? Mat->GetProcMeshSection(0) : nullptr;
    // Corners bound the landing footprint, including the actual four support feet.
    const FVector Offsets[] = {FVector(0,0,0), FVector(16,14,0), FVector(16,-14,0), FVector(-16,14,0), FVector(-16,-14,0)};
    for (const FVector& Offset : Offsets)
    {
        const FVector Point = WorldOrigin + FRotator(0, GetActorRotation().Yaw, 0).RotateVector(Offset);
        FHitResult Hit;
        if (!GetWorld()->LineTraceSingleByObjectType(Hit, Point + FVector(0,0,20), Point - FVector(0,0,800), Objects, Query)
            || Hit.ImpactNormal.Z < .8F) return Result;
        // Water cannot inherit a distant seabed as an aircraft landing support.
        if (IsOverWater(Point) && Hit.ImpactPoint.Z <= APacificCoastGraybox::SeaLevelCm + 5.0F) return Result;
        float Height=static_cast<float>(Hit.ImpactPoint.Z);
        if (MatSection && MatSection->ProcVertexBuffer.Num()>0 && FVector::Dist2D(Point,FVector(55,79,0))<90.0F)
        {
            const float MatZ=static_cast<float>(Mat->GetComponentTransform().TransformPosition(MatSection->ProcVertexBuffer[0].Position).Z);
            if (MatZ>=Height && MatZ-Height<5.0F) Height=MatZ;
        }
        Highest = FMath::Max(Highest, Height);
        Lowest = FMath::Min(Lowest, Height);
    }
    if (Highest - Lowest > 8.0F) return Result;
    Result.bHasSupport = true;
    Result.SupportedModelZCm = static_cast<float>(WorldToModelPosition(FVector(WorldOrigin.X,WorldOrigin.Y,Highest + FootDepth)).Z);
    return Result;
}

bool AFlightPawn::IsSupportedOnLaunchMat() const
{
    if (bWaterIncidentLatched || FlightModel.Phase != EFlightPhase::Grounded || !ControlInput.IsNeutral()) return false;
    const FVector World = GetActorLocation();
    if (FVector::Dist2D(World, FVector(55,79,0)) > FPacificCoastGrayboxContract::TakeoffPadRadiusCm - 22.0F
        || FMath::Abs(GetActorRotation().Pitch) > 8 || FMath::Abs(GetActorRotation().Roll) > 8) return false;
    const FFlightSurfaceContext Support = QuerySurface(World);
    return Support.bHasSupport && FMath::Abs(FlightModel.Position.Z - Support.SupportedModelZCm) <= 2.0F
        && FlightModel.Velocity.Size() < 40.0F;
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
    PlayerInputComponent->BindKey(EKeys::M, IE_Pressed, this, &AFlightPawn::HandleMissionAdvance); // debug compatibility only
    PlayerInputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AFlightPawn::HandleUiActivate).bExecuteWhenPaused = true;
    PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AFlightPawn::HandleUiActivate).bExecuteWhenPaused = true;
    PlayerInputComponent->BindKey(EKeys::Gamepad_FaceButton_Bottom, IE_Pressed, this, &AFlightPawn::HandleUiActivate).bExecuteWhenPaused = true;
    PlayerInputComponent->BindKey(EKeys::F10, IE_Pressed, this, &AFlightPawn::HandleSettingsToggle).bExecuteWhenPaused = true;
    PlayerInputComponent->BindKey(EKeys::H, IE_Pressed, this, &AFlightPawn::HandleReturnToHome);
    PlayerInputComponent->BindKey(EKeys::T, IE_Pressed, this, &AFlightPawn::HandleWeatherCycle);
    PlayerInputComponent->BindKey(EKeys::Up, IE_Pressed, this, &AFlightPawn::HandleUiUp);
    PlayerInputComponent->BindKey(EKeys::Down, IE_Pressed, this, &AFlightPawn::HandleUiDown);
    PlayerInputComponent->BindKey(EKeys::Left, IE_Pressed, this, &AFlightPawn::HandleUiLeft);
    PlayerInputComponent->BindKey(EKeys::Right, IE_Pressed, this, &AFlightPawn::HandleUiRight);
    // Widgets consume menu navigation first; only an active flight may interpret D-pad down as RTH.
    PlayerInputComponent->BindKey(EKeys::Gamepad_DPad_Down, IE_Pressed, this, &AFlightPawn::HandleReturnToHome);
}

bool AFlightPawn::IsProductFlightPage() const
{
    const AAeroSimGameMode* GameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<AAeroSimGameMode>() : nullptr;
    return GameMode == nullptr || GameMode->GetProductPage() == EProductPage::Flight;
}

bool AFlightPawn::IsGameplayInputAllowed() const
{
    return IsProductFlightPage() && !bReplayVisualActive && FlightModel.Phase != EFlightPhase::Paused && !bResumeRequested;
}

bool AFlightPawn::IsPhysicalInputNeutral() const
{
    if (const AAeroSimPlayerController* PlayerController = Cast<AAeroSimPlayerController>(GetController()))
    {
        return PlayerController->IsPhysicalInputNeutral();
    }
    if (const APlayerController* PlayerController = Cast<APlayerController>(GetController()))
    {
        const FKey Keys[] = { EKeys::W, EKeys::S, EKeys::A, EKeys::D, EKeys::Q, EKeys::E,
            EKeys::LeftShift, EKeys::RightShift, EKeys::LeftControl, EKeys::RightControl };
        for (const FKey& Key : Keys) if (PlayerController->IsInputKeyDown(Key)) return false;
        const FKey Axes[] = { EKeys::Gamepad_LeftX, EKeys::Gamepad_LeftY, EKeys::Gamepad_RightX,
            EKeys::Gamepad_RightY, EKeys::Gamepad_LeftTriggerAxis, EKeys::Gamepad_RightTriggerAxis };
        for (const FKey& Key : Axes)
        {
            const float Value = PlayerController->GetInputAnalogKeyState(Key);
            if (!FMath::IsFinite(Value) || FMath::Abs(Value) > 0.1F) return false;
        }
        // A product controller without the independent Slate observer cannot certify recentering.
        if (GetWorld() != nullptr && GetWorld()->GetAuthGameMode<AAeroSimGameMode>() != nullptr) return false;
        return true;
    }
    // Preserve unpossessed, no-GameMode pure Automation injection compatibility.
    return GetWorld() == nullptr || GetWorld()->GetAuthGameMode<AAeroSimGameMode>() == nullptr;
}

void AFlightPawn::HandlePhysicalInputActivity()
{
    if (bResumeRequested)
    {
        // Events restart the interval even if press and release both occur between Pawn ticks.
        ResumeCountdownSeconds = 3.0F;
        FlightModel.Pause();
        NeutralizeInput();
    }
}

void AFlightPawn::SetSemanticInput(const FFlightControlInput& NewInput)
{
    if (!IsGameplayInputAllowed())
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
        bResumeRequested = false;
        ResumeCountdownSeconds = 0.0F;
        FlightModel.Pause();
        if (PlayerController != nullptr)
        {
            PlayerController->SetPause(true);
        }
    }
    else
    {
        // A water incident cannot be resumed into continued underwater movement; reset is explicit.
        if (bWaterIncidentLatched) return;
        if (PlayerController != nullptr)
        {
            PlayerController->SetPause(false);
        }
        if (FlightModel.Phase == EFlightPhase::Paused && IsProductFlightPage() && !bReplayVisualActive)
        {
            bResumeRequested = true;
            ResumeCountdownSeconds = 3.0F;
        }
    }
}

void AFlightPawn::AdvanceResumeCountdown(const float DeltaSeconds)
{
    if (!bResumeRequested)
    {
        return;
    }
    NeutralizeInput();
    if (!IsPhysicalInputNeutral())
    {
        ResumeCountdownSeconds = 3.0F;
        return;
    }
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0F) return;
    ResumeCountdownSeconds = FMath::Max(0.0F, ResumeCountdownSeconds - DeltaSeconds);
    if (ResumeCountdownSeconds <= 0.0F)
    {
        bResumeRequested = false;
        FlightModel.Resume();
    }
}

void AFlightPawn::CycleCamera()
{
    if (!IsGameplayInputAllowed()) return;
    switch (CameraMode)
    {
    case EFlightCameraMode::Chase: CameraMode = EFlightCameraMode::FPV; break;
    case EFlightCameraMode::FPV: CameraMode = EFlightCameraMode::Gimbal; break;
    case EFlightCameraMode::Gimbal: CameraMode = EFlightCameraMode::FarFollow; break;
    case EFlightCameraMode::FarFollow: CameraMode = EFlightCameraMode::Chase; break;
    default: CameraMode = EFlightCameraMode::Chase; break;
    }
    ApplyCameraMode();
}

void AFlightPawn::SetProductCameraMode(const EFlightCameraMode Mode)
{
    if (static_cast<uint8>(Mode) > static_cast<uint8>(EFlightCameraMode::Gimbal)) return;
    CameraMode = Mode;
    ApplyCameraMode();
}

bool AFlightPawn::ApplyProductVerticalFov(const float VerticalDegrees)
{
    if (!FovTransaction.Preview(VerticalDegrees)) return false;
    FovTransaction.Apply();
    ApplyFov();
    return true;
}

void AFlightPawn::ApplyReplaySample(const FFlightTraceSample& Sample)
{
    const AAeroSimGameMode* GameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<AAeroSimGameMode>() : nullptr;
    if ((GameMode != nullptr && GameMode->GetProductPage() != EProductPage::Replay)
        || !FMath::IsFinite(Sample.TimeSeconds) || Sample.Position.ContainsNaN() || Sample.Rotation.ContainsNaN()) return;
    bReplayVisualActive = true;
    NeutralizeInput();
    SetActorLocationAndRotation(InitialTransform.TransformPosition(Sample.Position),
        InitialTransform.TransformRotation(Sample.Rotation.Quaternion()), false, nullptr, ETeleportType::TeleportPhysics);
}

void AFlightPawn::SetGimbalTarget(const float PitchDegrees, const float YawDegrees)
{
    GimbalController.SetTarget(PitchDegrees, YawDegrees);
}

void AFlightPawn::SetUseLodProxy(const bool bUseProxy)
{
    bUseLodProxy = bUseProxy;
    ApplyCameraMode();
}

void AFlightPawn::ApplyCameraMode()
{
    if (SpringArm == nullptr)
    {
        return;
    }
    const bool bFirstPerson = CameraMode == EFlightCameraMode::FPV || CameraMode == EFlightCameraMode::Gimbal;
    const bool bDetailedVisible = !bFirstPerson && !bUseLodProxy;
    for (UStaticMeshComponent* Part : DetailedVisualParts)
    {
        if (Part != nullptr)
        {
            Part->SetVisibility(bDetailedVisible, true);
        }
    }
    if (LodProxy != nullptr)
    {
        LodProxy->SetVisibility(!bFirstPerson && bUseLodProxy, true);
    }

    SpringArm->bDoCollisionTest = !bFirstPerson;
    SpringArm->SetUsingAbsoluteRotation(CameraMode == EFlightCameraMode::Gimbal);
    if (CameraMode == EFlightCameraMode::FPV)
    {
        SpringArm->TargetArmLength = 0.0F;
        SpringArm->SetRelativeLocation(FVector(15.8F, 0.0F, -4.4F));
        SpringArm->SetRelativeRotation(FRotator(-6.0F, 0.0F, 0.0F));
    }
    else if (CameraMode == EFlightCameraMode::Gimbal)
    {
        SpringArm->TargetArmLength = 0.0F;
        SpringArm->SetRelativeLocation(FVector(15.8F, 0.0F, -4.4F));
        SpringArm->SetWorldRotation(FRotator(GimbalController.PitchDegrees, GetActorRotation().Yaw + GimbalController.YawDegrees, 0.0F));
    }
    else if (CameraMode == EFlightCameraMode::FarFollow)
    {
        SpringArm->TargetArmLength = 1450.0F;
        SpringArm->SetRelativeLocation(FVector(-40.0F, 0.0F, 80.0F));
        SpringArm->SetRelativeRotation(FRotator(-4.0F, 0.0F, 0.0F));
    }
    else
    {
        SpringArm->TargetArmLength = 650.0F;
        SpringArm->SetRelativeLocation(FVector(-24.0F, 0.0F, 35.0F));
        SpringArm->SetRelativeRotation(FRotator(-3.0F, 0.0F, 0.0F));
    }
    ApplyFov();
}

void AFlightPawn::ApplyFov()
{
    if (Camera == nullptr)
    {
        return;
    }
    int32 Width = 16;
    int32 Height = 9;
    if (const APlayerController* PlayerController = Cast<APlayerController>(GetController()))
    {
        PlayerController->GetViewportSize(Width, Height);
    }
    const float AspectRatio = Height > 0 ? static_cast<float>(Width) / static_cast<float>(Height) : 16.0F / 9.0F;
    Camera->SetFieldOfView(GetHorizontalFov(AspectRatio));
}

bool AFlightPawn::PreviewVerticalFov(const float VerticalDegrees)
{
    const bool bAccepted = FovTransaction.Preview(VerticalDegrees);
    if (bAccepted)
    {
        ApplyFov();
    }
    return bAccepted;
}

bool AFlightPawn::ApplyVerticalFov()
{
    const bool bApplied = FovTransaction.Apply();
    ApplyFov();
    return bApplied;
}

void AFlightPawn::CancelVerticalFov()
{
    FovTransaction.Cancel();
    ApplyFov();
}

bool AFlightPawn::UndoVerticalFov()
{
    const bool bUndone = FovTransaction.Undo();
    ApplyFov();
    return bUndone;
}

bool AFlightPawn::IsAircraftShellVisible() const
{
    if (DetailedVisualParts.IsEmpty())
    {
        return false;
    }
    for (const UStaticMeshComponent* Part : DetailedVisualParts)
    {
        if (Part == nullptr || !Part->IsVisible())
        {
            return false;
        }
    }
    return true;
}

bool AFlightPawn::IsLodProxyVisible() const
{
    return LodProxy != nullptr && LodProxy->IsVisible();
}

bool AFlightPawn::InitializeGroundedSpawn(const FVector& SurfacePoint)
{
    if(FlightModel.Phase!=EFlightPhase::Grounded||!FlightModel.Position.IsNearlyZero()||SurfacePoint.ContainsNaN())return false;
    double LowestSupport=0;
    for(const UStaticMeshComponent* Support:LandingSupports)
        if(Support&&Support->GetStaticMesh())LowestSupport=FMath::Min(LowestSupport,Support->CalcBounds(Support->GetRelativeTransform()).GetBox().Min.Z);
    if(LowestSupport>=0)return false;
    FTransform Pose=GetActorTransform();Pose.SetLocation(SurfacePoint+FVector(0,0,-LowestSupport+.3));Pose.SetRotation(FQuat::Identity);
    InitialTransform=Pose;SetActorTransform(Pose,false,nullptr,ETeleportType::ResetPhysics);
    FlightTrace.Reset();FlightTrace.Record(0,FlightModel.Position,FlightModel.Rotation);
    return true;
}

void AFlightPawn::ConfigureReviewView(const FTransform& PawnTransform, const EFlightCameraMode Mode)
{
    bReplayVisualActive = false;
    InitialTransform = PawnTransform;
    FlightModel.Reset();
    CameraMode = Mode;
    ApplyCameraMode();
    SetActorTransform(PawnTransform, false, nullptr, ETeleportType::ResetPhysics);
}

void AFlightPawn::ConfigureReviewFlightState(const float AltitudeCm, const float SpeedCmPerSecond)
{
    FlightModel.Phase = EFlightPhase::Flight;
    FlightModel.Position = FVector(0.0F, 0.0F, FMath::Max(AltitudeCm, 0.0F));
    FlightModel.Velocity = FVector(FMath::Max(SpeedCmPerSecond, 0.0F), 0.0F, 0.0F);
    SetActorLocation(InitialTransform.GetLocation() + FlightModel.Position, false, nullptr, ETeleportType::ResetPhysics);
}

void AFlightPawn::ResetFlight()
{
    bWaterIncidentLatched = false;
    bCollisionHold = false;
    NeutralizeInput();
    ResumeCountdownSeconds = 0.0F;
    bResumeRequested = false;
    bReplayVisualActive = false;
    FlightElapsedSeconds = 0.0F;
    TraceAccumulatorSeconds = 0.0F;
    FlightTrace.Reset();
    FlightModel.Reset();
    FlightTrace.Record(0.0F, FlightModel.Position, FlightModel.Rotation);
    if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
    {
        PlayerController->SetPause(false);
    }
    SetActorTransform(InitialTransform, false, nullptr, ETeleportType::ResetPhysics);
}

void AFlightPawn::RecordTrace(const float DeltaSeconds)
{
    if (FlightModel.Phase == EFlightPhase::Paused || DeltaSeconds <= 0.0F)
    {
        return;
    }
    FlightElapsedSeconds += DeltaSeconds;
    TraceAccumulatorSeconds += DeltaSeconds;
    while (TraceAccumulatorSeconds + KINDA_SMALL_NUMBER >= FDeterministicFlightTrace::SampleIntervalSeconds)
    {
        TraceAccumulatorSeconds -= FDeterministicFlightTrace::SampleIntervalSeconds;
        const float SampleTime = FlightElapsedSeconds - TraceAccumulatorSeconds;
        FlightTrace.Record(SampleTime, FlightModel.Position, FlightModel.Rotation);
    }
}

FFlightHudData AFlightPawn::GetHudData() const
{
    FFlightHudData Data;
    Data.Phase = FlightModel.Phase;
    Data.CameraMode = CameraMode;
    Data.AltitudeMeters = FlightModel.Position.Z / AeroSimFlight::CentimetersPerMeter;
    const FFlightSurfaceContext Surface=QuerySurface(GetActorLocation());
    if (IsOverWater(GetActorLocation()))
    {
        Data.bSurfaceClearanceValid=true;
        Data.SurfaceClearanceMeters=FMath::Max(0.0F,static_cast<float>(GetActorLocation().Z-GetFootDepthCm()-APacificCoastGraybox::SeaLevelCm))*.01F;
    }
    else if (Surface.bHasSupport)
    {
        Data.bSurfaceClearanceValid=true;
        Data.SurfaceClearanceMeters=FMath::Max(0.0F,static_cast<float>(FlightModel.Position.Z-Surface.SupportedModelZCm))*.01F;
    }
    Data.SpeedMetersPerSecond = FlightModel.Velocity.Size() / AeroSimFlight::CentimetersPerMeter;
    Data.Throttle = ControlInput.Throttle;
    Data.bInputNeutral = IsPhysicalInputNeutral();
    Data.bWaterContact = bWaterIncidentLatched;
    Data.AircraftName = AircraftDefinition.DisplayName;
    Data.AssetStatus = TEXT("BLOCKED_ASSET / GENERIC PROXY");
    Data.GimbalPitchDegrees = GimbalController.PitchDegrees;
    Data.GimbalYawDegrees = GimbalController.YawDegrees;
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
        HandleSettingsToggle();
    }
}

void AFlightPawn::HandleReset(const FInputActionValue& Value)
{
    if (Value.Get<bool>() && bWaterIncidentLatched && IsProductFlightPage())
    {
        if (AAeroSimGameMode* Mode=GetWorld()->GetAuthGameMode<AAeroSimGameMode>()) Mode->RestartProductMission();
        else ResetFlight();
        return;
    }
    if (Value.Get<bool>() && IsGameplayInputAllowed())
    {
        ResetFlight();
    }
}

void AFlightPawn::HandleMissionAdvance()
{
    if (AAeroSimGameMode* GameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<AAeroSimGameMode>() : nullptr)
    {
        GameMode->AdvanceMission();
    }
}

void AFlightPawn::HandleReturnToHome()
{
    if (!IsGameplayInputAllowed()) return;
    if (AAeroSimGameMode* GameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<AAeroSimGameMode>() : nullptr)
    {
        GameMode->RequestReturnToHome();
    }
}

void AFlightPawn::HandleSettingsToggle()
{
    if (AAeroSimGameMode* GameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<AAeroSimGameMode>() : nullptr)
    {
        GameMode->HandleProductBack();
    }
    else SetSafePaused(FlightModel.Phase != EFlightPhase::Paused);
}

void AFlightPawn::HandleUiUp()
{
    HandleGimbalPitchUp(); // Menus are navigated exclusively by the product widget.
}

void AFlightPawn::HandleUiDown()
{
    HandleGimbalPitchDown();
}

void AFlightPawn::HandleUiLeft()
{
    HandleGimbalYawLeft();
}

void AFlightPawn::HandleUiRight()
{
    HandleGimbalYawRight();
}

void AFlightPawn::HandleUiActivate()
{
    if (AAeroSimGameMode* GameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<AAeroSimGameMode>() : nullptr)
    {
        // Widget owns menu primary; Pawn fallback is only the explicit Flight resume action.
        if (GameMode->GetProductPage() == EProductPage::Flight) GameMode->ActivatePrimaryAction();
    }
}

void AFlightPawn::HandleWeatherCycle()
{
    if (!IsGameplayInputAllowed()) return;
    if (AAeroSimGameMode* GameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<AAeroSimGameMode>() : nullptr)
    {
        const EAeroSimWeather Current = GameMode->GetEnvironmentState().Weather;
        const EAeroSimWeather Next = Current == EAeroSimWeather::Clear
            ? EAeroSimWeather::Overcast
            : (Current == EAeroSimWeather::Overcast ? EAeroSimWeather::Rain : EAeroSimWeather::Clear);
        GameMode->SetWeather(Next);
    }
}

void AFlightPawn::HandleGimbalPitchUp()
{
    if (!IsGameplayInputAllowed() || CameraMode != EFlightCameraMode::Gimbal) return;
    GimbalController.AddTargetInput(5.0F, 0.0F);
}

void AFlightPawn::HandleGimbalPitchDown()
{
    if (!IsGameplayInputAllowed() || CameraMode != EFlightCameraMode::Gimbal) return;
    GimbalController.AddTargetInput(-5.0F, 0.0F);
}

void AFlightPawn::HandleGimbalYawLeft()
{
    if (!IsGameplayInputAllowed() || CameraMode != EFlightCameraMode::Gimbal) return;
    GimbalController.AddTargetInput(0.0F, -5.0F);
}

void AFlightPawn::HandleGimbalYawRight()
{
    if (!IsGameplayInputAllowed() || CameraMode != EFlightCameraMode::Gimbal) return;
    GimbalController.AddTargetInput(0.0F, 5.0F);
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
