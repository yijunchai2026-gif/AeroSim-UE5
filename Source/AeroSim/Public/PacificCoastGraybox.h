#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Phase2Systems.h"
#include "PacificCoastGraybox.generated.h"

class USkyAtmosphereComponent;
class UMeshComponent;
class UMaterialInterface;
class UVolumetricCloudComponent;

class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UPostProcessComponent;
class USceneComponent;
class USkyLightComponent;
class UStaticMesh;
class UStaticMeshComponent;
enum class EFlightPhase : uint8;

/** Pure geometry and route contract for the 1 km x 1 km Phase 1 training area. */
struct AEROSIM_API FPacificCoastGrayboxContract final
{
    static constexpr float WorldSizeCm = 100000.0F;
    static constexpr float WorldHalfExtentCm = WorldSizeCm * 0.5F;
    static constexpr float CoreWidthCm = 30000.0F;
    static constexpr float CoreLengthCm = 50000.0F;
    static constexpr float TakeoffPadRadiusCm = 1800.0F;
    static constexpr float RouteToleranceCm = 2500.0F;
    static constexpr float StandardReferenceHeightCm = 180.0F;
    static constexpr int32 GateCount = 5;
    static constexpr int32 RouteArrowCount = 18;

    static const TArray<FVector>& GetRoutePoints();
    static bool IsInsideWorld(const FVector& Point);
    static bool IsGeometryValid();
};

/** Ordered route validator used by runtime diagnostics and Automation. */
struct AEROSIM_API FPacificCoastRouteProgress
{
    int32 NextPointIndex = 0;
    bool bTookOff = false;
    bool bReturned = false;

    void Observe(EFlightPhase Phase, const FVector& RelativePosition);
    bool IsComplete() const;
};

/** Pure-C++ visible graybox; all geometry uses engine basic shapes, no project uasset. */
UCLASS()
class AEROSIM_API APacificCoastGraybox final : public AActor
{
    GENERATED_BODY()

public:
    APacificCoastGraybox();
    virtual void BeginPlay() override;

    void SetCoastalQuality(bool bHigh);
    static float GetCliffEdgeX(float Y);
    static float GetCoastalShoreX(float Y);
    static float GetCoastalLandHeight(float X, float Y);
    static float GetCoastalVegetationCover(float X, float Y);
    static constexpr float SeaLevelCm = -6500.0F;
    int32 GetCoastalTreeCount() const { return CoastalTreeCount; }

    int32 GetGeneratedPrimitiveCount() const { return GeneratedPrimitiveCount; }
    int32 GetGeneratedGateCount() const { return GeneratedGateCount; }
    int32 GetGeneratedRouteArrowCount() const { return GeneratedRouteArrowCount; }
    int32 GetGeneratedScaleReferenceCount() const { return GeneratedScaleReferenceCount; }
    int32 GetGeneratedHeroZoneCount() const { return GeneratedHeroZoneCount; }
    int32 GetRainStreakCount() const { return RainStreaks.Num(); }
    bool IsRainCurtainVisible() const;
    float GetAppliedFogDensity() const;
    const FEnvironmentState& GetAppliedEnvironmentState() const { return AppliedEnvironmentState; }

    UFUNCTION(BlueprintCallable, Category = "Environment")
    void ApplyEnvironmentState(const FEnvironmentState& State);

private:
    void BuildCoastalLandscape();
    void BuildCoastalLaunchSite(UStaticMesh* CubeMesh);
    void BuildCoastalTerrain(UMaterialInterface* Material);
    void BuildCoastalTrees(UMaterialInterface* Material);
    void BuildCoastalOcean(UMaterialInterface* Material);
    void ApplyCoastalMaterials();
    UPROPERTY(Transient)
    TArray<TObjectPtr<UMeshComponent>> CoastalColorParts;
    TArray<FLinearColor> CoastalPartColors;
    UPROPERTY(VisibleAnywhere, Category = "Environment")
    TObjectPtr<UVolumetricCloudComponent> CoastalClouds;
    UPROPERTY(Transient)
    TObjectPtr<UMaterialInterface> CoastalWaterMaterial;
    UPROPERTY(Transient)
    TObjectPtr<UMaterialInterface> CoastalWaterFallback;
    UPROPERTY(Transient)
    TObjectPtr<UMaterialInterface> CoastalGravelMaterial;
    int32 CoastalTreeCount = 0;

    UStaticMeshComponent* AddBox(
        const FName Name,
        const FVector& Location,
        const FVector& Size,
        UStaticMesh* CubeMesh,
        const FLinearColor& Color,
        const FRotator& Rotation = FRotator::ZeroRotator);
    void AddLabel(
        FName Name,
        const FString& Text,
        const FVector& Location,
        const FRotator& Rotation,
        const FColor& Color,
        float WorldSize);

    UPROPERTY(VisibleAnywhere, Category = "Graybox")
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere, Category = "Graybox")
    TObjectPtr<UDirectionalLightComponent> KeyLight;

    UPROPERTY(VisibleAnywhere, Category = "Environment")
    TObjectPtr<USkyLightComponent> FillLight;

    UPROPERTY(VisibleAnywhere, Category = "Environment")
    TObjectPtr<USkyAtmosphereComponent> SkyAtmosphere;

    UPROPERTY(VisibleAnywhere, Category = "Environment")
    TObjectPtr<UExponentialHeightFogComponent> WeatherFog;

    UPROPERTY(VisibleAnywhere, Category = "Environment")
    TObjectPtr<UPostProcessComponent> WeatherPostProcess;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> RainStreaks;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> WetHighlightPrimitives;

    UPROPERTY(VisibleInstanceOnly, Category = "Environment")
    FEnvironmentState AppliedEnvironmentState;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> ColoredPrimitives;

    TArray<FLinearColor> PrimitiveColors;
    int32 GeneratedPrimitiveCount = 0;
    int32 GeneratedGateCount = 0;
    int32 GeneratedRouteArrowCount = 0;
    int32 GeneratedScaleReferenceCount = 0;
    int32 GeneratedHeroZoneCount = 0;
};
