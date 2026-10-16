#pragma once

#include "CoreMinimal.h"
#include "Phase3Aircraft.generated.h"

UENUM(BlueprintType)
enum class EAircraftAssetStatus : uint8
{
    ContentNotDelivered,
    Unverified,
    ReferenceLocked,
    GeometryVerified,
    ProductionReady,
    UserConfirmed,
    Rejected,
    BlockedAsset
};

UENUM(BlueprintType)
enum class EAircraftVisualSource : uint8
{
    ProgrammaticEngineShapes,
    ReplaceableStaticMeshPackage
};

USTRUCT(BlueprintType)
struct AEROSIM_API FAircraftMaterialSlot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aircraft")
    FName SlotId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aircraft")
    FString Purpose;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aircraft")
    bool bRequired = true;
};

USTRUCT(BlueprintType)
struct AEROSIM_API FAircraftLodContract
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aircraft")
    FName Name;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aircraft")
    float ScreenSize = 1.0F;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aircraft")
    int32 TriangleBudget = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aircraft")
    bool bProxy = false;
};

USTRUCT(BlueprintType)
struct AEROSIM_API FAircraftVariant
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aircraft")
    FName VariantId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aircraft")
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aircraft")
    FLinearColor ShellColor = FLinearColor(0.62F, 0.68F, 0.72F);
};

/** Versioned, fail-closed Phase 3 aircraft identity and replacement boundary. */
USTRUCT(BlueprintType)
struct AEROSIM_API FAircraftDefinition
{
    GENERATED_BODY()

    static constexpr int32 SchemaVersion = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FName RuntimeId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FName CatalogId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FString AssetVersion;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FString ContentSha256;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Provenance")
    FString SourceUri;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Provenance")
    FString Authorization;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Provenance")
    FString Disclosure;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status")
    EAircraftAssetStatus AssetStatus = EAircraftAssetStatus::BlockedAsset;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status")
    EAircraftVisualSource VisualSource = EAircraftVisualSource::ProgrammaticEngineShapes;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Replacement")
    FSoftObjectPath ReplacementAsset;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dimensions")
    FVector DimensionsCm = FVector(38.0F, 32.0F, 12.0F);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dimensions")
    FVector CenterOfMassCm = FVector(-1.0F, 0.0F, 1.5F);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dimensions")
    float GroundSupportZCm = -6.0F;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aircraft")
    TArray<FAircraftVariant> Variants;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aircraft")
    TArray<FAircraftLodContract> Lods;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aircraft")
    TArray<FAircraftMaterialSlot> MaterialSlots;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quality")
    TArray<FName> PassedQualityGates;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quality")
    TArray<FName> BlockingQualityGates;

    bool IsStructurallyValid(FString* OutReason = nullptr) const;
    bool CanEnterFormalTask(FString* OutReason = nullptr) const;
    static FAircraftDefinition MakeGenericGolden();
};

/** Pure controller: clamped targets, rate-independent smoothing and explicit horizon stabilization. */
USTRUCT(BlueprintType)
struct AEROSIM_API FGimbalController
{
    GENERATED_BODY()

    static constexpr float MinimumPitchDegrees = -90.0F;
    static constexpr float MaximumPitchDegrees = 30.0F;
    static constexpr float MaximumYawDegrees = 75.0F;

    UPROPERTY(BlueprintReadOnly, Category = "Gimbal")
    float PitchDegrees = -12.0F;

    UPROPERTY(BlueprintReadOnly, Category = "Gimbal")
    float YawDegrees = 0.0F;

    UPROPERTY(BlueprintReadOnly, Category = "Gimbal")
    float TargetPitchDegrees = -12.0F;

    UPROPERTY(BlueprintReadOnly, Category = "Gimbal")
    float TargetYawDegrees = 0.0F;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gimbal")
    float SmoothSpeed = 8.0F;

    void SetTarget(float Pitch, float Yaw);
    void AddTargetInput(float PitchDelta, float YawDelta);
    void Advance(float DeltaSeconds);
    FRotator GetStabilizedRelativeRotation() const;
};

USTRUCT(BlueprintType)
struct AEROSIM_API FPhase3EvidenceCameraContract
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Evidence")
    FName Id;

    UPROPERTY(BlueprintReadOnly, Category = "Evidence")
    FTransform PawnTransform;

    UPROPERTY(BlueprintReadOnly, Category = "Evidence")
    FName CameraPreset;

    UPROPERTY(BlueprintReadOnly, Category = "Evidence")
    float VerticalFov = 60.0F;

    UPROPERTY(BlueprintReadOnly, Category = "Evidence")
    float GimbalPitch = -12.0F;

    bool IsValid() const;
    static TArray<FPhase3EvidenceCameraContract> FixedSet();
};
