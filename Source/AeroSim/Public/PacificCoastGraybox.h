#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Phase2Systems.h"
#include "PacificCoastGraybox.generated.h"

class UProceduralMeshComponent;
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
class UAnimSequence;
class USkeletalMeshComponent;
enum class EFlightPhase : uint8;

// DEV-TERRAIN begin (RD-08 terrain owner): habitat classification shared with DEV-FLORA / materials.
/** Dominant ground habitat; soft weights are in FCoastHabitatSample for broad, irregular transitions. */
enum class ECoastHabitatClass : uint8
{
    ExposedGrass,    // sun-facing (south/south-west) convex slopes: golden dry annual grass
    ShelteredGrass,  // transitional, partly shaded or moister grass/forb ground
    Woodland,        // north-facing shoulders and gully floors: oak/bay/chaparral ground with litter
    SteepRock,       // locally steep faces and spurs: exposed rock and thin soil
    CliffShore       // sea cliff rim, cliff face, submerged ground or outside the land
};

struct AEROSIM_API FCoastHabitatSample
{
    ECoastHabitatClass Class = ECoastHabitatClass::CliffShore;
    /** Soft weights; ExposedGrass + ShelteredGrass + Woodland + Rock + Shore == 1. */
    float ExposedGrass = 0.0F;
    float ShelteredGrass = 0.0F;
    float Woodland = 0.0F;
    float Rock = 0.0F;
    float Shore = 1.0F;
    float Grade = 0.0F;        // local rise/run over ~12 m
    float SunExposure = 0.0F;  // -1 fully shaded aspect .. +1 sun-facing aspect (0 on flats)
    float Drainage = 0.0F;     // 0 convex ridge/shoulder .. 1 concave gully/swale floor
};

/** Deterministic pure functions of world XY (cm, actor space); safe to call from any constructor. */
struct AEROSIM_API FPacificCoastHabitat
{
    static FCoastHabitatSample Sample(float X, float Y);
    static ECoastHabitatClass GetHabitatClass(float X, float Y);
    /** Ground vertex colour: R exposed grass, G sheltered grass, B woodland; rock/shore = 1-R-G-B. */
    static FLinearColor GetGroundVertexColor(float X, float Y);
    /** Height of the distant receding ridgelines mesh west of the land (not part of the playable land). */
    static float GetDistantRangeHeight(float X, float Y);
    /** X of the closed sea-cliff face at (Y, Z) between its rim and foot (analytic, per coast row). */
    static float GetCliffFaceX(float Y, float Z);
    /** 0..1 forest-floor litter weight (DEV-FLORA trunk-base mask), carried as 1 - ground vertex alpha. */
    static float GetForestLitter(float X, float Y);
};
// DEV-TERRAIN end

/** Geometry and world-space route contract; negative world Z is valid above the coastal sea. */
struct AEROSIM_API FPacificCoastGrayboxContract final
{
    static constexpr float WorldSizeCm = 100000.0F;
    static constexpr float WorldHalfExtentCm = WorldSizeCm * 0.5F;
    static constexpr float CoreWidthCm = 30000.0F;
    static constexpr float CoreLengthCm = 50000.0F;
    static constexpr float TakeoffPadRadiusCm = 90.0F;
    static constexpr float RouteToleranceCm = 2500.0F;
    static constexpr float StandardReferenceHeightCm = 180.0F;
    static constexpr int32 GateCount = 5;
    static constexpr int32 RouteArrowCount = 18;

    /** Fixed land clearing footprint; mission routes never reshape the forest. */
    static const TArray<FVector>& GetPlateauClearancePoints();
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
    virtual void Tick(float DeltaSeconds) override;

    void SetCoastalQuality(bool bHigh);
    static float GetCliffEdgeX(float Y);
    static float GetCoastalShoreX(float Y);
    static float GetCoastalLandHeight(float X, float Y);
    static float GetCoastalVegetationCover(float X, float Y);
    // DEV-FLORA: deterministic samplers for ground materials (DEV-TERRAIN may blend with them).
    /** 0..1 worn-floor weight of the drone training corridor path (1 on the trodden centre). */
    static float GetTrainingCorridorWear(float X, float Y);
    /** Trunk-base litter mask: R conifer needles, G oak leaf litter; saturate(1-(d-r)/R)^1.5 from the placed trees. */
    static FLinearColor GetForestLitterMask(float X, float Y);
    // DEV-FLORA end
    static constexpr float SeaLevelCm = -6500.0F;
    int32 GetCoastalTreeCount() const { return CoastalTreeCount; }
    int32 GetCoastalWhaleCount() const { return CoastalWhaleCount; }
    int32 GetCoastalWolfCount() const { return CoastalWolfCount; }
    int32 GetCoastalBearCount() const { return CoastalBearCount; }
    int32 GetCoastalDeerCount() const { return CoastalDeerCount; }
    int32 GetCoastalCattleCount() const { return CoastalCattleCount; }
    int32 GetCoastalEagleCount() const { return CoastalEagleCount; }
    int32 GetCoastalWildlifeAnimatedBatchCount() const { return CoastalWildlifeAnimatedBatches.Num(); }
    int32 GetCoastalWildlifeAutonomousCount() const { return CoastalWildlifeAutonomousStates.Num(); }
    bool IsCoastalWildlifeAlert() const { return bCoastalWildlifeAlert; }
    bool IsCoastalEagleAttacking() const { return bCoastalEagleAttacking; }
    int32 GetCoastalWildlifeAnimatedRangeCount() const
    {
        int32 Count = 0;
        for (const FCoastalWildlifeAnimatedBatch& Batch : CoastalWildlifeAnimatedBatches) Count += Batch.Ranges.Num();
        return Count;
    }

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
    // Lightweight deterministic wildlife batches: ocean whales and habitat-placed land animals.
    void BuildCoastalWildlife(UMaterialInterface* Material);
    void UpdateCoastalWildlifeAnimation(float DeltaSeconds);
    enum class ECoastalWildlifeKind : uint8 { Whale, Wolf, Bear, Deer, Cattle, Eagle };
    enum class ECoastalWildlifeBehavior : uint8 { Roaming, Alert, Fleeing, Attacking, Recovering };
    struct FCoastalWildlifeVertexRange
    {
        int32 StartVertex = 0;
        int32 EndVertex = 0;
        FVector Origin = FVector::ZeroVector;
        float Yaw = 0.0F;
        float Scale = 1.0F;
        float Phase = 0.0F;
        ECoastalWildlifeKind Kind = ECoastalWildlifeKind::Deer;
    };
    struct FCoastalWildlifeAnimatedBatch
    {
        TObjectPtr<UProceduralMeshComponent> Component;
        TArray<FVector> BaseVertices;
        TArray<FVector> Normals;
        TArray<FVector2D> UV;
        TArray<FLinearColor> Colors;
        TArray<FCoastalWildlifeVertexRange> Ranges;
    };
    struct FCoastalWildlifeAutonomousState
    {
        TObjectPtr<USkeletalMeshComponent> Component;
        TObjectPtr<UAnimSequence> IdleAnimation;
        TObjectPtr<UAnimSequence> TravelAnimation;
        TObjectPtr<UAnimSequence> FastAnimation;
        TObjectPtr<UAnimSequence> CurrentAnimation;
        FVector Anchor = FVector::ZeroVector;
        FVector2D RoamRadius = FVector2D::ZeroVector;
        float Speed = 0.0F;
        float Phase = 0.0F;
        float GroundOffset = 0.0F;
        float YawBias = 0.0F;
        float Roll = 0.0F;
        float NominalTravelSpeed = 1.0F;
        FVector Velocity = FVector::ZeroVector;
        float BehaviorTime = 0.0F;
        float MotionStartTime = 0.0F;
        ECoastalWildlifeBehavior Behavior = ECoastalWildlifeBehavior::Roaming;
        ECoastalWildlifeKind Kind = ECoastalWildlifeKind::Deer;
    };
    // DEV-FLORA: instanced forest-floor understory (ferns, shrubs, edge grass) for the near stands.
    void BuildCoastalHabitat();
    // DEV-FLORA end
    void ApplyCoastalMaterials();
    UPROPERTY(Transient)
    TArray<TObjectPtr<UMeshComponent>> CoastalColorParts;
    UPROPERTY(Transient)
    TArray<TObjectPtr<UProceduralMeshComponent>> CoastalWhaleAnimatedParts;
    TArray<FCoastalWildlifeAnimatedBatch> CoastalWildlifeAnimatedBatches;
    TArray<FCoastalWildlifeAutonomousState> CoastalWildlifeAutonomousStates;
    float CoastalWildlifeAnimationTime = 0.0F;
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
    int32 CoastalWhaleCount = 0;
    int32 CoastalWolfCount = 0;
    int32 CoastalBearCount = 0;
    int32 CoastalDeerCount = 0;
    int32 CoastalCattleCount = 0;
    int32 CoastalEagleCount = 0;
    bool bCoastalWildlifeAlert = false;
    bool bCoastalEagleAttacking = false;

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

    // DEV-SEASKY begin: sea/sky contract shared by runtime, the editor water import and tests.
    // World axes: +X is west/offshore, +Y is north, Z up (compass bearing = 270 deg + yaw).
public:
    struct FCoastalWaveComponent
    {
        FVector2D Direction;      // unit propagation direction in world XY
        float WavelengthCm;
        float AmplitudeCm;        // at WaveAmplitudeScale = 1
        float Steepness;          // Gerstner Q, 0..1
        float AngularFrequency;   // deep-water dispersion, rad/s
        float Phase;
    };
    enum class ECoastalWaterShader : uint8 { Offset, Normal, Foam, Scattering, Absorption, Roughness, RockWetness, Cirrus };
    static const TArray<FCoastalWaveComponent>& GetCoastalWaveSpectrum();
    static FVector GetCoastalOceanDisplacement(float X, float Y, float TimeSeconds, float AmplitudeScale = 1.0F);
    static float GetCoastalSeabedDepthCm(float X, float Y);
    static FString GetCoastalWaterShaderCode(ECoastalWaterShader Kind);
    /** Solar (elevation, azimuth) in degrees, azimuth clockwise from north, for the fixed coast site/date. */
    static FVector2D GetCoastalSunElevationAzimuth(const FEnvironmentState& State);
    static FRotator GetCoastalSunRotation(const FEnvironmentState& State);
    float GetAppliedSurfaceWetness() const { return AppliedSurfaceWetness; }
    int32 GetWaterlinePointCount() const { return WaterlinePointCount; }
private:
    void ApplyCoastalSky(const FEnvironmentState& State);
    void ApplyCoastalSurfaceWeather(const FEnvironmentState& State);
    float AppliedSurfaceWetness = 0.0F;
    int32 WaterlinePointCount = 0;
    UPROPERTY(VisibleAnywhere, Category = "Environment")
    TObjectPtr<UStaticMeshComponent> CoastalCirrusDome;
    UPROPERTY(VisibleAnywhere, Category = "Environment")
    TObjectPtr<UDirectionalLightComponent> CoastalBounceFill;
    // DEV-SEASKY end
};
