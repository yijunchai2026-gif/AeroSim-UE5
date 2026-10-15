#include "PacificCoastGraybox.h"

#include "Components/DirectionalLightComponent.h"
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
    AddLabel(TEXT("StartLabel"), TEXT("START / RETURN"), FVector(0.0F, 0.0F, 80.0F), FRotator(0.0F, 0.0F, 0.0F), FColor::Yellow, 45.0F);

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
