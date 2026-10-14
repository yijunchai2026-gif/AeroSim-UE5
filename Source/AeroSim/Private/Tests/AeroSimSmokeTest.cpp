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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAeroSimRuntimeSmokeTest,
    "AeroSim.Phase0.Runtime.Smoke",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAeroSimRuntimeSmokeTest::RunTest(const FString& Parameters)
{
    const AAeroSimGameMode* GameMode = GetDefault<AAeroSimGameMode>();
    TestNotNull(TEXT("GameMode CDO exists"), GameMode);
    TestTrue(TEXT("FlightPawn is the default pawn"),
        GameMode != nullptr && GameMode->DefaultPawnClass == AFlightPawn::StaticClass());

    const AFlightPawn* FlightPawn = GetDefault<AFlightPawn>();
    TestNotNull(TEXT("FlightPawn CDO exists"), FlightPawn);
    if (FlightPawn != nullptr)
    {
        TestNotNull(TEXT("Native mapping context exists"), FlightPawn->GetFlightMappingContext());
        TestNotNull(TEXT("Native move action exists"), FlightPawn->GetMoveAction());
        TestNotNull(TEXT("Native look action exists"), FlightPawn->GetLookAction());
        TestNotNull(TEXT("Native throttle action exists"), FlightPawn->GetThrottleAction());
        TestNotNull(TEXT("Native reset action exists"), FlightPawn->GetResetAction());
        TestTrue(TEXT("Native input suite is atomically consistent"),
            FlightPawn->IsInputSuiteValid(FlightPawn->GetInputSuite(), false));

        FFlightInputSuite CustomSuite;
        CustomSuite.MappingContext = NewObject<UInputMappingContext>();
        CustomSuite.MoveAction = NewObject<UInputAction>();
        CustomSuite.MoveAction->ValueType = EInputActionValueType::Axis2D;
        CustomSuite.LookAction = NewObject<UInputAction>();
        CustomSuite.LookAction->ValueType = EInputActionValueType::Axis2D;
        CustomSuite.ThrottleAction = NewObject<UInputAction>();
        CustomSuite.ThrottleAction->ValueType = EInputActionValueType::Axis1D;
        CustomSuite.ResetAction = NewObject<UInputAction>();
        CustomSuite.ResetAction->ValueType = EInputActionValueType::Boolean;
        CustomSuite.MappingContext->MapKey(CustomSuite.MoveAction, EKeys::W);
        CustomSuite.MappingContext->MapKey(CustomSuite.LookAction, EKeys::Mouse2D);
        CustomSuite.MappingContext->MapKey(CustomSuite.ThrottleAction, EKeys::SpaceBar);
        CustomSuite.MappingContext->MapKey(CustomSuite.ResetAction, EKeys::R);
        TestTrue(TEXT("Complete custom suite is accepted"), FlightPawn->IsInputSuiteValid(CustomSuite, false));

        UInputAction* ForeignAction = NewObject<UInputAction>();
        ForeignAction->ValueType = EInputActionValueType::Boolean;
        CustomSuite.MappingContext->MapKey(ForeignAction, EKeys::F);
        TestFalse(TEXT("Context containing a foreign action is rejected"),
            FlightPawn->IsInputSuiteValid(CustomSuite, false));

        CustomSuite.MappingContext = NewObject<UInputMappingContext>();
        CustomSuite.MappingContext->MapKey(CustomSuite.MoveAction, EKeys::W);
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
