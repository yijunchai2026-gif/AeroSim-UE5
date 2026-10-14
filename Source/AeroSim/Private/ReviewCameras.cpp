#include "ReviewCameras.h"

#include "Camera/CameraComponent.h"

AReviewCameraBase::AReviewCameraBase()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AReviewCameraBase::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (bLockToReviewTransform)
    {
        SetActorLocationAndRotation(ReviewLocation, ReviewRotation);
    }
    GetCameraComponent()->SetFieldOfView(FieldOfView);
}

bool AReviewCameraBase::IsEvidenceStateWithinTolerance(
    const FVector& CandidateLocation,
    const FRotator& CandidateRotation,
    const float CandidateFieldOfView) const
{
    return bLockToReviewTransform
        && CandidateLocation.Equals(ReviewLocation, LocationToleranceCm)
        && CandidateRotation.Equals(ReviewRotation, RotationToleranceDegrees)
        && FMath::IsNearlyEqual(CandidateFieldOfView, FieldOfView, FieldOfViewToleranceDegrees);
}

bool AReviewCameraBase::CanGenerateFormalEvidence() const
{
    const UCameraComponent* ReviewCamera = GetCameraComponent();
    return ReviewCamera != nullptr
        && IsEvidenceStateWithinTolerance(
            GetActorLocation(),
            GetActorRotation(),
            ReviewCamera->FieldOfView);
}

AOrbitReviewCamera::AOrbitReviewCamera()
{
    ReviewLocation = FVector(1200.0, 1200.0, 800.0);
    ReviewRotation = FRotator(-20.0, -135.0, 0.0);
    FieldOfView = 50.0F;
}

AChaseReviewCamera::AChaseReviewCamera()
{
    ReviewLocation = FVector(-900.0, 0.0, 300.0);
    ReviewRotation = FRotator(-8.0, 0.0, 0.0);
    FieldOfView = 65.0F;
}

ACockpitReviewCamera::ACockpitReviewCamera()
{
    ReviewLocation = FVector(80.0, 0.0, 120.0);
    ReviewRotation = FRotator::ZeroRotator;
    FieldOfView = 80.0F;
}
