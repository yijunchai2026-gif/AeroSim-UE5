#include "PacificCoastGraybox.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "FlightPawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
const TArray<FVector> RoutePoints = {
    FVector(0.0F, 0.0F, 100.0F),
    FVector(1800.0F, 0.0F, 400.0F),
    FVector(4000.0F, -1200.0F, 600.0F),
    FVector(6500.0F, -500.0F, 700.0F),
    FVector(9000.0F, 2000.0F, 700.0F),
    FVector(6500.0F, 4000.0F, 600.0F),
    FVector(0.0F, 0.0F, 100.0F)
};

constexpr FLinearColor LandA(0.12F, 0.28F, 0.10F);
constexpr FLinearColor LandB(0.16F, 0.36F, 0.13F);
constexpr FLinearColor OceanA(0.02F, 0.16F, 0.34F);
constexpr FLinearColor OceanB(0.03F, 0.23F, 0.46F);
constexpr FLinearColor CoastColor(0.64F, 0.48F, 0.24F);
constexpr FLinearColor PadColor(0.15F, 0.17F, 0.20F);
constexpr FLinearColor GateColor(4.0F, 0.08F, 0.01F);
constexpr FLinearColor RouteColor(4.0F, 2.4F, 0.01F);
constexpr FLinearColor ReturnColor(0.02F, 3.0F, 0.25F);
constexpr FLinearColor ForestColor(0.04F, 0.20F, 0.06F);
constexpr FLinearColor RockColor(0.17F, 0.19F, 0.20F);
constexpr FLinearColor WetRockColor(0.06F, 0.11F, 0.13F);
constexpr FLinearColor TimberColor(0.28F, 0.15F, 0.06F);
constexpr FLinearColor FernColor(0.03F, 0.31F, 0.09F);
constexpr FLinearColor PadAccentColor(0.85F, 0.74F, 0.17F);
constexpr FLinearColor ReferenceColor(0.92F, 0.92F, 0.92F);
constexpr float GroundTileSizeCm = 10000.0F;
constexpr float GroundThicknessCm = 200.0F;

float SurfaceZ(const FVector& Point)
{
    return Point.X >= 10000.0F ? -100.0F : 0.0F;
}
}

const TArray<FVector>& FPacificCoastGrayboxContract::GetRoutePoints()
{
    return RoutePoints;
}

bool FPacificCoastGrayboxContract::IsInsideWorld(const FVector& Point)
{
    return FMath::Abs(Point.X) <= WorldHalfExtentCm
        && FMath::Abs(Point.Y) <= WorldHalfExtentCm
        && Point.Z >= 0.0F;
}

bool FPacificCoastGrayboxContract::IsGeometryValid()
{
    if (RoutePoints.Num() != GateCount + 2
        || !RoutePoints[0].Equals(RoutePoints.Last())
        || RoutePoints[0].Size2D() > TakeoffPadRadiusCm)
    {
        return false;
    }
    for (const FVector& Point : RoutePoints)
    {
        if (!IsInsideWorld(Point))
        {
            return false;
        }
    }
    return true;
}

void FPacificCoastRouteProgress::Observe(const EFlightPhase Phase, const FVector& RelativePosition)
{
    const TArray<FVector>& Points = FPacificCoastGrayboxContract::GetRoutePoints();
    if (!bTookOff)
    {
        bTookOff = Phase == EFlightPhase::Takeoff || Phase == EFlightPhase::Flight;
        if (bTookOff)
        {
            NextPointIndex = 1;
        }
        return;
    }

    if (NextPointIndex < Points.Num() - 1
        && FVector::Dist(RelativePosition, Points[NextPointIndex]) <= FPacificCoastGrayboxContract::RouteToleranceCm)
    {
        ++NextPointIndex;
    }

    if (NextPointIndex >= Points.Num() - 1
        && Phase == EFlightPhase::Grounded
        && FVector::Dist2D(RelativePosition, Points.Last()) <= FPacificCoastGrayboxContract::TakeoffPadRadiusCm)
    {
        bReturned = true;
    }
}

bool FPacificCoastRouteProgress::IsComplete() const
{
    return bTookOff && bReturned && NextPointIndex >= FPacificCoastGrayboxContract::GetRoutePoints().Num() - 1;
}

APacificCoastGraybox::APacificCoastGraybox()
{
    PrimaryActorTick.bCanEverTick = false;
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    KeyLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("KeyLight"));
    KeyLight->SetupAttachment(Root);
    KeyLight->SetRelativeRotation(FRotator(-48.0F, -35.0F, 0.0F));
    KeyLight->SetIntensity(7.0F);
    KeyLight->SetLightColor(FLinearColor(1.0F, 0.91F, 0.78F));
    KeyLight->SetCastShadows(true);

    FillLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("FillLight"));
    FillLight->SetupAttachment(Root);
    FillLight->SetIntensity(0.7F);
    FillLight->SetMobility(EComponentMobility::Movable);

    WeatherFog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("WeatherFog"));
    WeatherFog->SetupAttachment(Root);
    WeatherFog->SetFogDensity(0.002F);
    WeatherFog->SetFogHeightFalloff(0.25F);
    WeatherFog->SetFogInscatteringColor(FLinearColor(0.58F, 0.67F, 0.72F));

    WeatherPostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("WeatherPostProcess"));
    WeatherPostProcess->SetupAttachment(Root);
    WeatherPostProcess->bUnbound = true;
    WeatherPostProcess->Priority = 20.0F;
    WeatherPostProcess->Settings.bOverride_AutoExposureBias = true;
    WeatherPostProcess->Settings.bOverride_ColorSaturation = true;
    WeatherPostProcess->Settings.bOverride_SceneColorTint = true;
    WeatherPostProcess->Settings.bOverride_VignetteIntensity = true;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* CubeMesh = CubeFinder.Object;

    for (int32 XIndex = 0; XIndex < 10; ++XIndex)
    {
        for (int32 YIndex = 0; YIndex < 10; ++YIndex)
        {
            const float X = -45000.0F + static_cast<float>(XIndex) * GroundTileSizeCm;
            const float Y = -45000.0F + static_cast<float>(YIndex) * GroundTileSizeCm;
            const bool bOcean = X >= 10000.0F;
            const float TopZ = bOcean ? -100.0F : 0.0F;
            const FLinearColor Color = bOcean
                ? (((XIndex + YIndex) % 2 == 0) ? OceanA : OceanB)
                : (((XIndex + YIndex) % 2 == 0) ? LandA : LandB);
            AddBox(
                FName(*FString::Printf(TEXT("GroundTile_%02d_%02d"), XIndex, YIndex)),
                FVector(X, Y, TopZ - GroundThicknessCm * 0.5F),
                FVector(GroundTileSizeCm, GroundTileSizeCm, GroundThicknessCm),
                CubeMesh,
                Color);
        }
    }

    AddBox(TEXT("Coastline"), FVector(10000.0F, 0.0F, 30.0F), FVector(1200.0F, 100000.0F, 60.0F), CubeMesh, CoastColor);
    AddBox(TEXT("TakeoffPad"), FVector(0.0F, 0.0F, 20.0F), FVector(3200.0F, 3200.0F, 40.0F), CubeMesh, PadColor);
    AddBox(TEXT("StartStripe"), FVector(900.0F, 0.0F, 43.0F), FVector(1000.0F, 180.0F, 6.0F), CubeMesh, RouteColor);
    AddLabel(TEXT("StartLabel"), TEXT("ZONE 01 / TAKEOFF PAD"), FVector(0.0F, 0.0F, 80.0F), FRotator(0.0F, 0.0F, 0.0F), FColor(232, 196, 84), 42.0F);

    // Core acceptance slice is 500 m along X and 300 m across Y. Low boundary bars remain readable from follow cameras.
    AddBox(TEXT("CoreNorth"), FVector(0.0F, 15000.0F, 30.0F), FVector(50000.0F, 60.0F, 60.0F), CubeMesh, ReferenceColor);
    AddBox(TEXT("CoreSouth"), FVector(0.0F, -15000.0F, 30.0F), FVector(50000.0F, 60.0F, 60.0F), CubeMesh, ReferenceColor);
    AddBox(TEXT("CoreWest"), FVector(-25000.0F, 0.0F, 30.0F), FVector(60.0F, 30000.0F, 60.0F), CubeMesh, ReferenceColor);
    AddBox(TEXT("CoreEast"), FVector(25000.0F, 0.0F, 30.0F), FVector(60.0F, 30000.0F, 60.0F), CubeMesh, ReferenceColor);

    // Camera-readable rain volume: deterministic slanted streaks across the core, hidden outside Rain.
    for (int32 Index = 0; Index < 120; ++Index)
    {
        const float X = -5000.0F + static_cast<float>((Index * 997) % 24000);
        const float Y = -14500.0F + static_cast<float>((Index * 613) % 29000);
        const float Z = 650.0F + static_cast<float>((Index * 389) % 4600);
        UStaticMeshComponent* Streak = AddBox(
            FName(*FString::Printf(TEXT("RainStreak_%03d"), Index)),
            FVector(X, Y, Z),
            FVector(14.0F, 14.0F, 720.0F + static_cast<float>((Index * 47) % 280)),
            CubeMesh,
            FLinearColor(0.42F, 0.62F, 0.78F),
            FRotator(-12.0F, 18.0F, 0.0F));
        Streak->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Streak->SetCastShadow(false);
        Streak->SetVisibility(false);
        RainStreaks.Add(Streak);
    }

    // Thin glossy overlays make pad and shoreline read as wet without introducing binary materials.
    UStaticMeshComponent* PadWet = AddBox(TEXT("RainWetPad"), FVector(0.0F, 0.0F, 49.0F), FVector(3000.0F, 3000.0F, 5.0F), CubeMesh, FLinearColor(0.025F, 0.06F, 0.085F));
    PadWet->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PadWet->SetVisibility(false);
    WetHighlightPrimitives.Add(PadWet);
    for (int32 Index = 0; Index < 12; ++Index)
    {
        UStaticMeshComponent* WetPatch = AddBox(
            FName(*FString::Printf(TEXT("RainWetCoast_%02d"), Index)),
            FVector(8400.0F + static_cast<float>((Index * 271) % 2400), -13750.0F + Index * 2450.0F, 58.0F),
            FVector(1350.0F + static_cast<float>((Index * 97) % 650), 1100.0F, 7.0F),
            CubeMesh,
            FLinearColor(0.018F, 0.055F, 0.075F),
            FRotator(0.0F, Index * 17.0F, 0.0F));
        WetPatch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        WetPatch->SetCastShadow(false);
        WetPatch->SetVisibility(false);
        WetHighlightPrimitives.Add(WetPatch);
    }

    // Hero 1: layered pad surface, expansion seams, perimeter curb and inset safety marks.
    for (int32 Index = -3; Index <= 3; ++Index)
    {
        AddBox(FName(*FString::Printf(TEXT("PadSeam_%02d"), Index + 4)), FVector(Index * 420.0F, 0.0F, 45.0F), FVector(14.0F, 3000.0F, 8.0F), CubeMesh, RockColor);
    }
    AddBox(TEXT("PadNorthCurb"), FVector(0.0F, 1570.0F, 75.0F), FVector(3400.0F, 140.0F, 110.0F), CubeMesh, PadAccentColor);
    AddBox(TEXT("PadSouthCurb"), FVector(0.0F, -1570.0F, 75.0F), FVector(3400.0F, 140.0F, 110.0F), CubeMesh, PadAccentColor);
    ++GeneratedHeroZoneCount;

    // Hero 2: interlocked dry/wet rock bands and a bright waterline make the tidal edge legible.
    for (int32 Index = 0; Index < 18; ++Index)
    {
        const float Y = -13500.0F + static_cast<float>(Index) * 1550.0F;
        const float X = 9000.0F + static_cast<float>((Index * 977) % 1500);
        const float Scale = 420.0F + static_cast<float>((Index * 131) % 360);
        AddBox(FName(*FString::Printf(TEXT("DryRock_%02d"), Index)), FVector(X, Y, 130.0F), FVector(Scale * 1.4F, Scale, 360.0F), CubeMesh, RockColor, FRotator(0.0F, Index * 23.0F, Index % 2 == 0 ? 12.0F : -8.0F));
        AddBox(FName(*FString::Printf(TEXT("WetRock_%02d"), Index)), FVector(X + 900.0F, Y + 260.0F, -5.0F), FVector(Scale, Scale * 0.8F, 170.0F), CubeMesh, WetRockColor, FRotator(0.0F, Index * 31.0F, 0.0F));
    }
    AddBox(TEXT("TidalFoam"), FVector(10950.0F, 0.0F, -5.0F), FVector(180.0F, 30000.0F, 28.0F), CubeMesh, ReferenceColor);
    AddLabel(TEXT("WetEdgeLabel"), TEXT("ZONE 02 / TIDAL ROCKS"), FVector(8500.0F, -8500.0F, 650.0F), FRotator(0.0F, 90.0F, 0.0F), FColor(190, 215, 225), 90.0F);
    ++GeneratedHeroZoneCount;

    // Hero 3: boardwalk deck, handrails and layered forest edge with grounded understory.
    AddBox(TEXT("BoardwalkDeck"), FVector(-9000.0F, 8500.0F, 170.0F), FVector(11500.0F, 900.0F, 130.0F), CubeMesh, TimberColor, FRotator(0.0F, 18.0F, 0.0F));
    for (int32 Index = 0; Index < 12; ++Index)
    {
        const float X = -14300.0F + static_cast<float>(Index) * 950.0F;
        const float Y = 6750.0F + (X + 14300.0F) * 0.325F;
        AddBox(FName(*FString::Printf(TEXT("WalkPostL_%02d"), Index)), FVector(X, Y - 520.0F, 320.0F), FVector(90.0F, 90.0F, 620.0F), CubeMesh, TimberColor);
        AddBox(FName(*FString::Printf(TEXT("WalkPostR_%02d"), Index)), FVector(X, Y + 520.0F, 320.0F), FVector(90.0F, 90.0F, 620.0F), CubeMesh, TimberColor);
    }
    AddBox(TEXT("WalkRailL"), FVector(-9000.0F, 7970.0F, 590.0F), FVector(11500.0F, 80.0F, 90.0F), CubeMesh, TimberColor, FRotator(0.0F, 18.0F, 0.0F));
    AddBox(TEXT("WalkRailR"), FVector(-9000.0F, 9030.0F, 590.0F), FVector(11500.0F, 80.0F, 90.0F), CubeMesh, TimberColor, FRotator(0.0F, 18.0F, 0.0F));
    for (int32 Index = 0; Index < 16; ++Index)
    {
        const float X = -15500.0F + static_cast<float>((Index * 811) % 9000);
        const float Y = 11200.0F + static_cast<float>((Index * 577) % 3500);
        const float Height = 1100.0F + static_cast<float>((Index * 173) % 900);
        AddBox(FName(*FString::Printf(TEXT("ForestTrunk_%02d"), Index)), FVector(X, Y, Height * 0.5F), FVector(180.0F, 180.0F, Height), CubeMesh, TimberColor);
        AddBox(FName(*FString::Printf(TEXT("ForestCanopy_%02d"), Index)), FVector(X, Y, Height + 420.0F), FVector(900.0F, 900.0F, 850.0F), CubeMesh, ForestColor, FRotator(0.0F, Index * 19.0F, 0.0F));
        AddBox(FName(*FString::Printf(TEXT("Fern_%02d"), Index)), FVector(X + 350.0F, Y - 420.0F, 120.0F), FVector(600.0F, 600.0F, 240.0F), CubeMesh, FernColor);
    }
    AddLabel(TEXT("BoardwalkLabel"), TEXT("ZONE 03 / FOREST WALK"), FVector(-9500.0F, 10500.0F, 1250.0F), FRotator(0.0F, 18.0F, 0.0F), FColor(224, 216, 188), 78.0F);
    ++GeneratedHeroZoneCount;

    for (int32 Index = 0; Index < 8; ++Index)
    {
        const float Y = -42000.0F + static_cast<float>(Index) * 12000.0F;
        AddBox(
            FName(*FString::Printf(TEXT("Forest_%02d"), Index + 1)),
            FVector(-43000.0F, Y, 450.0F),
            FVector(1200.0F, 1200.0F, 900.0F),
            CubeMesh,
            ForestColor);
    }

    for (int32 Index = 0; Index < 4; ++Index)
    {
        const float X = Index < 2 ? -2300.0F : 2300.0F;
        const float Y = Index % 2 == 0 ? -2300.0F : 2300.0F;
        const FString Prefix = FString::Printf(TEXT("ScaleRef_%02d"), Index + 1);
        AddBox(FName(*(Prefix + TEXT("_Base"))), FVector(X, Y, 10.0F), FVector(80.0F, 80.0F, 20.0F), CubeMesh, ReferenceColor);
        AddBox(
            FName(*(Prefix + TEXT("_Person"))),
            FVector(X, Y, FPacificCoastGrayboxContract::StandardReferenceHeightCm * 0.5F),
            FVector(45.0F, 45.0F, FPacificCoastGrayboxContract::StandardReferenceHeightCm),
            CubeMesh,
            ReferenceColor);
        ++GeneratedScaleReferenceCount;
    }

    for (int32 Index = 0; Index < FPacificCoastGrayboxContract::GateCount; ++Index)
    {
        const FVector Center = RoutePoints[Index + 1];
        const FVector Approach = (Center - RoutePoints[Index]).GetSafeNormal2D();
        const FVector Right = FVector::CrossProduct(FVector::UpVector, Approach).GetSafeNormal();
        const float BaseZ = SurfaceZ(Center);
        const float TopZ = Center.Z + 900.0F;
        const float PostHeight = TopZ - BaseZ;
        const FRotator GateRotation(0.0F, Approach.Rotation().Yaw, 0.0F);
        const FString Prefix = FString::Printf(TEXT("Gate_%02d"), Index + 1);

        AddBox(FName(*(Prefix + TEXT("_LeftBase"))), Center + Right * -1000.0F + FVector(0.0F, 0.0F, BaseZ - Center.Z + 20.0F), FVector(420.0F, 420.0F, 40.0F), CubeMesh, GateColor);
        AddBox(FName(*(Prefix + TEXT("_RightBase"))), Center + Right * 1000.0F + FVector(0.0F, 0.0F, BaseZ - Center.Z + 20.0F), FVector(420.0F, 420.0F, 40.0F), CubeMesh, GateColor);
        AddBox(FName(*(Prefix + TEXT("_Left"))), Center + Right * -1000.0F + FVector(0.0F, 0.0F, (BaseZ + TopZ) * 0.5F - Center.Z), FVector(160.0F, 160.0F, PostHeight), CubeMesh, GateColor);
        AddBox(FName(*(Prefix + TEXT("_Right"))), Center + Right * 1000.0F + FVector(0.0F, 0.0F, (BaseZ + TopZ) * 0.5F - Center.Z), FVector(160.0F, 160.0F, PostHeight), CubeMesh, GateColor);
        AddBox(FName(*(Prefix + TEXT("_Top"))), FVector(Center.X, Center.Y, TopZ), FVector(160.0F, 2160.0F, 160.0F), CubeMesh, GateColor, GateRotation);
        AddLabel(FName(*(Prefix + TEXT("_Number"))), FString::FromInt(Index + 1), FVector(Center.X, Center.Y, TopZ + 350.0F), GateRotation, FColor::White, 180.0F);
        ++GeneratedGateCount;
    }

    constexpr int32 MarkersPerLeg = 3;
    for (int32 LegIndex = 0; LegIndex < RoutePoints.Num() - 1; ++LegIndex)
    {
        const FVector Start = RoutePoints[LegIndex];
        const FVector End = RoutePoints[LegIndex + 1];
        const FVector Direction = (End - Start).GetSafeNormal2D();
        const FRotator Rotation(0.0F, Direction.Rotation().Yaw, 0.0F);
        const bool bReturnLeg = LegIndex == RoutePoints.Num() - 2;
        for (int32 MarkerIndex = 1; MarkerIndex <= MarkersPerLeg; ++MarkerIndex)
        {
            const float Alpha = static_cast<float>(MarkerIndex) / static_cast<float>(MarkersPerLeg + 1);
            FVector Position = FMath::Lerp(Start, End, Alpha);
            Position.Z = SurfaceZ(Position) + 70.0F;
            AddBox(
                FName(*FString::Printf(TEXT("Route_%02d_%02d"), LegIndex + 1, MarkerIndex)),
                Position,
                FVector(700.0F, 130.0F, 35.0F),
                CubeMesh,
                bReturnLeg ? ReturnColor : RouteColor,
                Rotation);
            ++GeneratedRouteArrowCount;
        }
    }
}

void APacificCoastGraybox::BeginPlay()
{
    Super::BeginPlay();
    for (int32 Index = 0; Index < ColoredPrimitives.Num(); ++Index)
    {
        UStaticMeshComponent* Component = ColoredPrimitives[Index];
        if (Component == nullptr || Component->GetMaterial(0) == nullptr || !PrimitiveColors.IsValidIndex(Index))
        {
            continue;
        }
        UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Component->GetMaterial(0), this);
        Material->SetVectorParameterValue(TEXT("Color"), PrimitiveColors[Index]);
        Component->SetMaterial(0, Material);
    }
    ApplyEnvironmentState(AppliedEnvironmentState);
}

void APacificCoastGraybox::ApplyEnvironmentState(const FEnvironmentState& State)
{
    if (!State.IsValid())
    {
        return;
    }
    AppliedEnvironmentState = State;
    const bool bRain = State.Weather == EAeroSimWeather::Rain;
    KeyLight->SetIntensity(State.GetLightIntensity());
    KeyLight->SetLightColor(State.Weather == EAeroSimWeather::Clear
        ? FLinearColor(1.0F, 0.91F, 0.78F)
        : (bRain ? FLinearColor(0.48F, 0.58F, 0.70F) : FLinearColor(0.70F, 0.74F, 0.78F)));
    FillLight->SetIntensity(State.Weather == EAeroSimWeather::Clear ? 0.7F : (bRain ? 0.18F : 0.34F));
    WeatherFog->SetFogDensity(State.GetFogDensity());
    WeatherFog->SetFogHeightFalloff(bRain ? 0.12F : 0.25F);
    WeatherFog->SetFogInscatteringColor(State.Weather == EAeroSimWeather::Clear
        ? FLinearColor(0.58F, 0.67F, 0.72F)
        : (bRain ? FLinearColor(0.12F, 0.20F, 0.28F) : FLinearColor(0.30F, 0.36F, 0.41F)));

    WeatherPostProcess->Settings.AutoExposureBias = State.GetExposureBias();
    const float Saturation = State.GetSceneSaturation();
    WeatherPostProcess->Settings.ColorSaturation = FVector4(Saturation, Saturation, Saturation, 1.0F);
    WeatherPostProcess->Settings.SceneColorTint = State.Weather == EAeroSimWeather::Clear
        ? FLinearColor::White
        : (bRain ? FLinearColor(0.62F, 0.76F, 0.88F) : FLinearColor(0.82F, 0.86F, 0.90F));
    WeatherPostProcess->Settings.VignetteIntensity = bRain ? 0.55F : (State.Weather == EAeroSimWeather::Overcast ? 0.30F : 0.16F);

    for (UStaticMeshComponent* RainStreak : RainStreaks)
    {
        if (RainStreak != nullptr)
        {
            RainStreak->SetVisibility(bRain);
        }
    }
    for (UStaticMeshComponent* WetHighlight : WetHighlightPrimitives)
    {
        if (WetHighlight != nullptr)
        {
            WetHighlight->SetVisibility(bRain);
        }
    }

    const float Brightness = State.Weather == EAeroSimWeather::Clear ? 1.0F : (State.Weather == EAeroSimWeather::Overcast ? 0.62F : 0.38F);
    const float Wetness = State.GetWetness();
    for (int32 Index = 0; Index < ColoredPrimitives.Num(); ++Index)
    {
        UStaticMeshComponent* Component = ColoredPrimitives[Index];
        if (Component == nullptr || !PrimitiveColors.IsValidIndex(Index))
        {
            continue;
        }
        UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Component->GetMaterial(0));
        if (Material == nullptr && Component->GetMaterial(0) != nullptr)
        {
            Material = UMaterialInstanceDynamic::Create(Component->GetMaterial(0), this);
            Component->SetMaterial(0, Material);
        }
        if (Material != nullptr)
        {
            const FLinearColor Base = PrimitiveColors[Index];
            const FLinearColor RainTint = FMath::Lerp(Base, Base * FLinearColor(0.45F, 0.58F, 0.64F), Wetness * 0.45F);
            Material->SetVectorParameterValue(TEXT("Color"), RainTint * Brightness);
            Material->SetScalarParameterValue(TEXT("Roughness"), FMath::Lerp(0.72F, 0.22F, Wetness));
        }
    }
}

bool APacificCoastGraybox::IsRainCurtainVisible() const
{
    return !RainStreaks.IsEmpty()
        && RainStreaks[0] != nullptr
        && RainStreaks[0]->IsVisible();
}

float APacificCoastGraybox::GetAppliedFogDensity() const
{
    return WeatherFog != nullptr ? WeatherFog->FogDensity : 0.0F;
}

UStaticMeshComponent* APacificCoastGraybox::AddBox(
    const FName Name,
    const FVector& Location,
    const FVector& Size,
    UStaticMesh* CubeMesh,
    const FLinearColor& Color,
    const FRotator& Rotation)
{
    UStaticMeshComponent* Component = CreateDefaultSubobject<UStaticMeshComponent>(Name);
    Component->SetupAttachment(Root);
    Component->SetRelativeLocation(Location);
    Component->SetRelativeRotation(Rotation);
    Component->SetRelativeScale3D(Size / 100.0F);
    Component->SetStaticMesh(CubeMesh);
    Component->SetCollisionProfileName(TEXT("BlockAll"));
    Component->SetCastShadow(true);
    Component->SetCastContactShadow(true);
    ColoredPrimitives.Add(Component);
    PrimitiveColors.Add(Color);
    ++GeneratedPrimitiveCount;
    return Component;
}

void APacificCoastGraybox::AddLabel(
    const FName Name,
    const FString& Text,
    const FVector& Location,
    const FRotator& Rotation,
    const FColor& Color,
    const float WorldSize)
{
    UTextRenderComponent* Label = CreateDefaultSubobject<UTextRenderComponent>(Name);
    Label->SetupAttachment(Root);
    Label->SetRelativeLocation(Location);
    Label->SetRelativeRotation(Rotation);
    Label->SetText(FText::FromString(Text));
    Label->SetTextRenderColor(Color);
    Label->SetHorizontalAlignment(EHTA_Center);
    Label->SetWorldSize(WorldSize);
    Label->SetCastShadow(true);
}
