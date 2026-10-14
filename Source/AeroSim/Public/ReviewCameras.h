#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "ReviewCameras.generated.h"

UCLASS(Abstract, Config = Game)
class AEROSIM_API AReviewCameraBase : public ACameraActor
{
    GENERATED_BODY()

public:
    AReviewCameraBase();

    virtual void OnConstruction(const FTransform& Transform) override;

    /** Call immediately before capture. Any contract drift fails closed. */
    UFUNCTION(BlueprintPure, Category = "Review")
    bool CanGenerateFormalEvidence() const;

    bool IsEvidenceStateWithinTolerance(
        const FVector& CandidateLocation,
        const FRotator& CandidateRotation,
        float CandidateFieldOfView) const;

    const FVector& GetReviewLocation() const { return ReviewLocation; }
    const FRotator& GetReviewRotation() const { return ReviewRotation; }
    float GetReviewFieldOfView() const { return FieldOfView; }

protected:
    /** Evidence requires the configured transform. Explicitly disable only for non-evidence manual review. */
    UPROPERTY(Config, EditAnywhere, Category = "Review")
    bool bLockToReviewTransform = true;

    UPROPERTY(Config, EditDefaultsOnly, Category = "Review")
    FVector ReviewLocation = FVector::ZeroVector;

    UPROPERTY(Config, EditDefaultsOnly, Category = "Review")
    FRotator ReviewRotation = FRotator::ZeroRotator;

    UPROPERTY(Config, EditDefaultsOnly, Category = "Review", meta = (ClampMin = "5.0", ClampMax = "170.0"))
    float FieldOfView = 60.0F;

private:
    static constexpr double LocationToleranceCm = 0.1;
    static constexpr double RotationToleranceDegrees = 0.05;
    static constexpr float FieldOfViewToleranceDegrees = 0.01F;
};

UCLASS(Config = Game)
class AEROSIM_API AOrbitReviewCamera final : public AReviewCameraBase
{
    GENERATED_BODY()

public:
    AOrbitReviewCamera();
};

UCLASS(Config = Game)
class AEROSIM_API AChaseReviewCamera final : public AReviewCameraBase
{
    GENERATED_BODY()

public:
    AChaseReviewCamera();
};

UCLASS(Config = Game)
class AEROSIM_API ACockpitReviewCamera final : public AReviewCameraBase
{
    GENERATED_BODY()

public:
    ACockpitReviewCamera();
};
