#if WITH_DEV_AUTOMATION_TESTS

#include "AeroSimGameMode.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "FlightPawn.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Misc/AutomationTest.h"
#include "ReviewCameras.h"

namespace
{
FFlightInputSuite MakeCompleteSuite()
{
    FFlightInputSuite Suite;
    Suite.MappingContext = NewObject<UInputMappingContext>();
    Suite.PitchAction = NewObject<UInputAction>();
    Suite.RollAction = NewObject<UInputAction>();
    Suite.YawAction = NewObject<UInputAction>();
    Suite.ThrottleAction = NewObject<UInputAction>();
    Suite.CameraAction = NewObject<UInputAction>();
    Suite.PauseAction = NewObject<UInputAction>();
    Suite.ResetAction = NewObject<UInputAction>();
    Suite.PitchAction->ValueType = EInputActionValueType::Axis1D;
    Suite.RollAction->ValueType = EInputActionValueType::Axis1D;
    Suite.YawAction->ValueType = EInputActionValueType::Axis1D;
    Suite.ThrottleAction->ValueType = EInputActionValueType::Axis1D;
    Suite.CameraAction->ValueType = EInputActionValueType::Boolean;
    Suite.PauseAction->ValueType = EInputActionValueType::Boolean;
    Suite.ResetAction->ValueType = EInputActionValueType::Boolean;
    Suite.MappingContext->MapKey(Suite.PitchAction, EKeys::W);
    Suite.MappingContext->MapKey(Suite.RollAction, EKeys::A);
    Suite.MappingContext->MapKey(Suite.YawAction, EKeys::Q);
    Suite.MappingContext->MapKey(Suite.ThrottleAction, EKeys::SpaceBar);
    Suite.MappingContext->MapKey(Suite.CameraAction, EKeys::C);
    Suite.MappingContext->MapKey(Suite.PauseAction, EKeys::P);
    Suite.MappingContext->MapKey(Suite.ResetAction, EKeys::R);
    return Suite;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAeroSimRuntimeSmokeTest,
    "AeroSim.Phase0.Runtime.Smoke",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAeroSimRuntimeSmokeTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    const AAeroSimGameMode* GameMode = GetDefault<AAeroSimGameMode>();
    TestNotNull(TEXT("GameMode CDO exists"), GameMode);
    TestTrue(TEXT("FlightPawn is the default pawn"),
        GameMode != nullptr && GameMode->DefaultPawnClass == AFlightPawn::StaticClass());

    const AFlightPawn* FlightPawn = GetDefault<AFlightPawn>();
    TestNotNull(TEXT("FlightPawn CDO exists"), FlightPawn);
    if (FlightPawn != nullptr)
    {
        TestNotNull(TEXT("Native mapping context exists"), FlightPawn->GetFlightMappingContext());
        TestNotNull(TEXT("Native pitch action exists"), FlightPawn->GetPitchAction());
        TestNotNull(TEXT("Native roll action exists"), FlightPawn->GetRollAction());
        TestNotNull(TEXT("Native yaw action exists"), FlightPawn->GetYawAction());
        TestNotNull(TEXT("Native throttle action exists"), FlightPawn->GetThrottleAction());
        TestNotNull(TEXT("Native camera action exists"), FlightPawn->GetCameraAction());
        TestNotNull(TEXT("Native pause action exists"), FlightPawn->GetPauseAction());
        TestNotNull(TEXT("Native reset action exists"), FlightPawn->GetResetAction());
        TestTrue(TEXT("Native input suite is atomically consistent"),
            FlightPawn->IsInputSuiteValid(FlightPawn->GetInputSuite(), false));

        FFlightInputSuite CustomSuite = MakeCompleteSuite();
        TestTrue(TEXT("Complete custom suite is accepted"), FlightPawn->IsInputSuiteValid(CustomSuite, false));

        UInputAction* ForeignAction = NewObject<UInputAction>();
        ForeignAction->ValueType = EInputActionValueType::Boolean;
        CustomSuite.MappingContext->MapKey(ForeignAction, EKeys::F);
        TestFalse(TEXT("Context containing a foreign action is rejected"),
            FlightPawn->IsInputSuiteValid(CustomSuite, false));

        CustomSuite = MakeCompleteSuite();
        CustomSuite.MappingContext = NewObject<UInputMappingContext>();
        CustomSuite.MappingContext->MapKey(CustomSuite.PitchAction, EKeys::W);
        TestFalse(TEXT("Partial context/action override is rejected"),
            FlightPawn->IsInputSuiteValid(CustomSuite, false));
    }

    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
    TestNotNull(TEXT("Evidence camera test world exists"), TestWorld);
    if (TestWorld != nullptr)
    {
        const TSubclassOf<AReviewCameraBase> CameraClasses[] = {
            AOrbitReviewCamera::StaticClass(),
            AChaseReviewCamera::StaticClass(),
            ACockpitReviewCamera::StaticClass()
        };
        for (const TSubclassOf<AReviewCameraBase>& CameraClass : CameraClasses)
        {
            AReviewCameraBase* ReviewCamera = TestWorld->SpawnActor<AReviewCameraBase>(CameraClass);
            TestNotNull(TEXT("Evidence camera spawned"), ReviewCamera);
            if (ReviewCamera == nullptr)
            {
                continue;
            }

            TestTrue(TEXT("Exact spawned camera contract is accepted"), ReviewCamera->CanGenerateFormalEvidence());
            ReviewCamera->SetActorLocation(ReviewCamera->GetReviewLocation() + FVector(1.0, 0.0, 0.0));
            TestFalse(TEXT("Current location drift fails closed"), ReviewCamera->CanGenerateFormalEvidence());

            ReviewCamera->SetActorLocationAndRotation(
                ReviewCamera->GetReviewLocation(),
                ReviewCamera->GetReviewRotation() + FRotator(0.0, 1.0, 0.0));
            TestFalse(TEXT("Current rotation drift fails closed"), ReviewCamera->CanGenerateFormalEvidence());

            ReviewCamera->SetActorLocationAndRotation(
                ReviewCamera->GetReviewLocation(),
                ReviewCamera->GetReviewRotation());
            ReviewCamera->GetCameraComponent()->SetFieldOfView(ReviewCamera->GetReviewFieldOfView() + 1.0F);
            TestFalse(TEXT("Current FOV drift fails closed"), ReviewCamera->CanGenerateFormalEvidence());
        }
        TestWorld->DestroyWorld(false);
    }
    return true;
}

#endif
