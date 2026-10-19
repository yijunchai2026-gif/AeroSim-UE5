#include "PacificCoastGraybox.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/StaticMesh.h"
#include "FlightPawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
bool IsCoastalCourseDebugEnabled()
{
#if UE_BUILD_SHIPPING
    return false;
#else
    return FParse::Param(FCommandLine::Get(),TEXT("AeroSimDebugCourse"));
#endif
}

// World positions, shared by mission and environment clearance. The first slice traverses
// a representative corridor segment; full corridor traversal has its own safety/regression gate.
const TArray<FVector> PlateauClearancePoints = {
    FVector(0,0,100), FVector(1800,0,400), FVector(4000,-1200,600),
    FVector(6500,-500,700), FVector(9000,2000,700), FVector(6500,4000,600), FVector(0,0,100)
};

const TArray<FVector> RoutePoints = []
{
    const FVector Home(55,79,APacificCoastGraybox::GetCoastalLandHeight(55,79)+1.8F);
    return TArray<FVector>{Home, FVector(4500,0,500),
        FVector(APacificCoastGraybox::GetCoastalShoreX(-12000)+2500,-12000,APacificCoastGraybox::SeaLevelCm+1200),
        FVector(APacificCoastGraybox::GetCoastalShoreX(-6000)+3500,-6000,APacificCoastGraybox::SeaLevelCm+400),
        FVector(-15500,-6600,APacificCoastGraybox::GetCoastalLandHeight(-15500,-6600)+250),
        FVector(-17600,-10600,APacificCoastGraybox::GetCoastalLandHeight(-17600,-10600)+250), Home};
}();

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

const TArray<FVector>& FPacificCoastGrayboxContract::GetPlateauClearancePoints()
{
    return PlateauClearancePoints;
}

const TArray<FVector>& FPacificCoastGrayboxContract::GetRoutePoints()
{
    return RoutePoints;
}

bool FPacificCoastGrayboxContract::IsInsideWorld(const FVector& Point)
{
    return FMath::Abs(Point.X) <= WorldHalfExtentCm
        && FMath::Abs(Point.Y) <= WorldHalfExtentCm
        && Point.Z >= APacificCoastGraybox::SeaLevelCm + 100.0F;
}

bool FPacificCoastGrayboxContract::IsGeometryValid()
{
    if (RoutePoints.Num() != GateCount + 2
        || !RoutePoints[0].Equals(RoutePoints.Last())
        || FVector::Dist2D(RoutePoints[0],FVector(55,79,0)) > TakeoffPadRadiusCm)
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

    // DEV-SEASKY: Clear 16:30 PDT, mid-October, ~37 N Pacific coast. The sun sits ~21 deg
    // up in the south-west over the ocean; every weather derives from that one state.
    const FEnvironmentState Baseline;
    KeyLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("KeyLight"));
    KeyLight->SetupAttachment(Root);
    KeyLight->SetRelativeRotation(GetCoastalSunRotation(Baseline));
    KeyLight->SetIntensity(Baseline.GetLightIntensity());
    KeyLight->SetLightColor(FLinearColor(1.0F, 0.96F, 0.90F));
    KeyLight->SetCastShadows(true);
    KeyLight->bAtmosphereSunLight = true;

    FillLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("FillLight"));
    FillLight->SetupAttachment(Root);
    FillLight->SetIntensity(1.0F);
    FillLight->SetMobility(EComponentMobility::Movable);
    // Blue sky fill in shadows comes from the actual atmosphere, recaptured per weather.
    FillLight->bRealTimeCapture = true;
    FillLight->bLowerHemisphereIsBlack = false;
    FillLight->SetLowerHemisphereColor(FLinearColor(0.035F, 0.050F, 0.060F));

    // The prototype owns its sky as well as its floor; do not inherit template Landscape geometry.
    SkyAtmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("SkyAtmosphere"));
    SkyAtmosphere->SetupAttachment(Root);

    WeatherFog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("WeatherFog"));
    WeatherFog->SetupAttachment(Root);
    WeatherFog->SetFogDensity(Baseline.GetFogDensity());
    WeatherFog->SetFogHeightFalloff(0.2F);
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

    BuildCoastalLandscape();

    BuildCoastalLaunchSite(CubeMesh);

    // Core bounds are a debug guide, not physical scenery suspended across the cliff.
    if (UStaticMeshComponent* Guide = AddBox(TEXT("CoreNorth"), FVector(0.0F, 15000.0F, 30.0F), FVector(50000.0F, 60.0F, 60.0F), CubeMesh, ReferenceColor))
    { Guide->SetHiddenInGame(true); Guide->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
    if (UStaticMeshComponent* Guide = AddBox(TEXT("CoreSouth"), FVector(0.0F, -15000.0F, 30.0F), FVector(50000.0F, 60.0F, 60.0F), CubeMesh, ReferenceColor))
    { Guide->SetHiddenInGame(true); Guide->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
    if (UStaticMeshComponent* Guide = AddBox(TEXT("CoreWest"), FVector(-25000.0F, 0.0F, 30.0F), FVector(60.0F, 30000.0F, 60.0F), CubeMesh, ReferenceColor))
    { Guide->SetHiddenInGame(true); Guide->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
    if (UStaticMeshComponent* Guide = AddBox(TEXT("CoreEast"), FVector(25000.0F, 0.0F, 30.0F), FVector(60.0F, 30000.0F, 60.0F), CubeMesh, ReferenceColor))
    { Guide->SetHiddenInGame(true); Guide->SetCollisionEnabled(ECollisionEnabled::NoCollision); }

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

    // Sea-level wetness belongs to the actual rock material. Flat legacy rain
    // overlays cannot follow eroded bays and would float above the new land.

    // Hero 1: grounded small survey launch site, built above.
    ++GeneratedHeroZoneCount;

    // Hero 2: supported cliff/talus geometry is owned by BuildCoastalTerrain.
    AddLabel(TEXT("WetEdgeLabel"), TEXT("ZONE 02 / CLIFF AND SEA"), FVector(12500.0F, -8500.0F, 650.0F), FRotator(0.0F, 90.0F, 0.0F), FColor(190, 215, 225), 90.0F);
    ++GeneratedHeroZoneCount;

    // A short human-scale crossing has a purpose: the seasonal drainage swale.
    const float WalkYaw = 18.0F;
    const FVector Along(FMath::Cos(FMath::DegreesToRadians(WalkYaw)),FMath::Sin(FMath::DegreesToRadians(WalkYaw)),0);
    const FVector Across(-Along.Y,Along.X,0);
    const FVector WalkCenter(-9000,8500,0);
    const float DeckTop = 32.0F;
    AddBox(TEXT("BoardwalkDeck"),WalkCenter+FVector(0,0,DeckTop-8),FVector(2600,220,16),CubeMesh,TimberColor,FRotator(0,WalkYaw,0));
    // Individual planks, two continuous bearers and supports reaching below the real ground.
    for(int32 Index=0;Index<52;++Index)
    {
        const FVector P=WalkCenter+Along*(-1275+Index*50);
        AddBox(FName(*FString::Printf(TEXT("WalkPlank_%02d"),Index)),P+FVector(0,0,DeckTop+1),FVector(47,218,3),CubeMesh,
            TimberColor*(.88F+(Index%5)*.045F),FRotator(0,WalkYaw,0));
    }
    for(int32 Side=-1;Side<=1;Side+=2)
    {
        AddBox(FName(*FString::Printf(TEXT("WalkBearer_%d"),Side)),WalkCenter+Across*(Side*75)+FVector(0,0,12),FVector(2600,15,25),CubeMesh,TimberColor,FRotator(0,WalkYaw,0));
        AddBox(FName(*FString::Printf(TEXT("WalkRail_%d"),Side)),WalkCenter+Across*(Side*104)+FVector(0,0,DeckTop+101),FVector(2600,7,8),CubeMesh,TimberColor,FRotator(0,WalkYaw,0));
        for(int32 Index=0;Index<8;++Index)
        {
            const FVector P=WalkCenter+Along*(-1250+Index*(2500.0F/7))+Across*(Side*104);
            const float Ground=GetCoastalLandHeight(P.X,P.Y);
            const float Bottom=Ground-60,Top=DeckTop+105;
            AddBox(FName(*FString::Printf(TEXT("WalkPost_%d_%d"),Side,Index)),FVector(P.X,P.Y,(Bottom+Top)*.5F),FVector(10,10,Top-Bottom),CubeMesh,TimberColor,FRotator(0,WalkYaw,0));
        }
    }
    // Sloped timber approaches meet the plank surface and the at-grade trail without a step.
    const float RampRun=220, RampDrop=33.5F;
    const float RampAngle=FMath::RadiansToDegrees(FMath::Atan2(RampDrop,RampRun));
    for(int32 Side=-1;Side<=1;Side+=2)
    {
        const FVector RampCenter=WalkCenter+Along*(Side*1410);
        AddBox(FName(*FString::Printf(TEXT("WalkRamp_%d"),Side)),RampCenter+FVector(0,0,17.75F-3*FMath::Cos(FMath::DegreesToRadians(RampAngle))),
            FVector(FMath::Sqrt(RampRun*RampRun+RampDrop*RampDrop),220,6),CubeMesh,TimberColor,
            FRotator(-Side*RampAngle,WalkYaw,0));
    }
    // One terrain-following trail strip: scanned surface detail, irregular shoulders and no box sidewalls.
    UProceduralMeshComponent* Trail=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("CoastalForestTrail"));
    Trail->SetupAttachment(Root);Trail->SetCollisionProfileName(TEXT("NoCollision"));Trail->SetCastShadow(false);
    TArray<FVector> TrailVertices,TrailNormals;TArray<FVector2D> TrailUV;TArray<FLinearColor> TrailColors;TArray<int32> TrailIndices;
    constexpr int32 TrailRows=121,TrailColumns=7;
    const float Lateral[TrailColumns]={-1,-.8F,-.45F,0,.45F,.8F,1};
    for(int32 Side=-1;Side<=1;Side+=2)
    {
        const int32 Base=TrailVertices.Num();
        for(int32 Row=0;Row<TrailRows;++Row)
        {
            const float Distance=1520+Row*55;
            const float Bend=FMath::Square(FMath::Max(0.0F,Distance-2200)/6000.0F)*1600;
            const FVector Center=WalkCenter+Along*(Side*Distance)+Across*Bend;
            const float EndFade=FMath::Clamp((8120-Distance)/1200.0F,0.0F,1.0F);
            const float Width=(110+15*FMath::Sin(Distance/380.0F)+8*FMath::Sin(Distance/137.0F))*FMath::Max(.02F,EndFade);
            for(int32 Column=0;Column<TrailColumns;++Column)
            {
                FVector P=Center+Across*(Lateral[Column]*Width);
                const float Edge=FMath::Abs(Lateral[Column]);
                P.Z=GetCoastalLandHeight(P.X,P.Y)+FMath::Lerp(.8F,-.2F,FMath::Square(Edge));
                const float Cover=GetCoastalVegetationCover(P.X,P.Y);
                const FLinearColor Ground=FMath::Lerp(FLinearColor(1.08F,.86F,.55F),FLinearColor(.25F,.48F,.31F),Cover);
                const float Dirt=(1-FMath::SmoothStep(.35F,1.0F,Edge))*EndFade;
                FLinearColor TrailColor=Ground;TrailColor.A=Dirt;TrailColors.Add(TrailColor);
                TrailVertices.Add(P);TrailUV.Add(FVector2D(P.X/4000.0F,P.Y/4000.0F));
                const float DX=(GetCoastalLandHeight(P.X+10,P.Y)-GetCoastalLandHeight(P.X-10,P.Y))/20;
                const float DY=(GetCoastalLandHeight(P.X,P.Y+10)-GetCoastalLandHeight(P.X,P.Y-10))/20;
                TrailNormals.Add(FVector(-DX,-DY,1).GetSafeNormal());
            }
        }
        for(int32 Row=0;Row<TrailRows-1;++Row)
            for(int32 Column=0;Column<TrailColumns-1;++Column)
            {
                const int32 A=Base+Row*TrailColumns+Column,B=A+1,C=A+TrailColumns,D=C+1;
                if(Side>0)TrailIndices.Append({A,B,C,B,D,C});else TrailIndices.Append({A,C,B,B,C,D});
            }
    }
    Trail->CreateMeshSection_LinearColor(0,TrailVertices,TrailIndices,TrailNormals,TrailUV,TrailColors,TArray<FProcMeshTangent>(),false);
    const UMeshComponent* GroundMesh=Cast<UMeshComponent>(GetDefaultSubobjectByName(TEXT("CoastalPlateau")));
    if(GroundMesh)Trail->SetMaterial(0,CoastalGravelMaterial ? CoastalGravelMaterial.Get() : GroundMesh->GetMaterial(0));
    CoastalColorParts.Add(Trail);CoastalPartColors.Add(FLinearColor::White);++GeneratedPrimitiveCount;
    ++GeneratedHeroZoneCount;

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
    ApplyCoastalMaterials();
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
    ApplyCoastalSky(State);

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
    ApplyCoastalSurfaceWeather(State);
}

// DEV-SEASKY: solar position for the fixed site (36.97 N, 122.03 W, Santa Cruz-like),
// 18 October, local clock in PDT (UTC-7). NOAA general solar position equations.
FVector2D APacificCoastGraybox::GetCoastalSunElevationAzimuth(const FEnvironmentState& State)
{
    constexpr double Latitude = 36.97, Longitude = -122.03, UtcOffsetHours = -7.0;
    constexpr int32 DayOfYear = 291;
    const double Hours = State.TimeMinutes / 60.0;
    const double Gamma = 2.0 * UE_DOUBLE_PI / 365.0 * (DayOfYear - 1 + (Hours - 12.0) / 24.0);
    const double EquationOfTime = 229.18 * (0.000075 + 0.001868 * FMath::Cos(Gamma) - 0.032077 * FMath::Sin(Gamma)
        - 0.014615 * FMath::Cos(2 * Gamma) - 0.040849 * FMath::Sin(2 * Gamma));
    const double Declination = 0.006918 - 0.399912 * FMath::Cos(Gamma) + 0.070257 * FMath::Sin(Gamma)
        - 0.006758 * FMath::Cos(2 * Gamma) + 0.000907 * FMath::Sin(2 * Gamma)
        - 0.002697 * FMath::Cos(3 * Gamma) + 0.00148 * FMath::Sin(3 * Gamma);
    const double TrueSolarMinutes = State.TimeMinutes + EquationOfTime + 4.0 * Longitude - 60.0 * UtcOffsetHours;
    const double HourAngle = FMath::DegreesToRadians(TrueSolarMinutes / 4.0 - 180.0);
    const double Phi = FMath::DegreesToRadians(Latitude);
    const double SinElevation = FMath::Sin(Phi) * FMath::Sin(Declination) + FMath::Cos(Phi) * FMath::Cos(Declination) * FMath::Cos(HourAngle);
    const double Elevation = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(SinElevation, -1.0, 1.0)));
    const double Azimuth = FMath::RadiansToDegrees(FMath::Atan2(FMath::Sin(HourAngle),
        FMath::Cos(HourAngle) * FMath::Sin(Phi) - FMath::Tan(Declination) * FMath::Cos(Phi))) + 180.0;
    return FVector2D(Elevation, FMath::Fmod(Azimuth + 360.0, 360.0));
}

FRotator APacificCoastGraybox::GetCoastalSunRotation(const FEnvironmentState& State)
{
    // Light travels away from the sun: yaw = bearing - 270 + 180 (+X west, +Y north).
    const FVector2D Sun = GetCoastalSunElevationAzimuth(State);
    return FRotator(-Sun.X, FRotator::NormalizeAxis(Sun.Y - 90.0), 0.0F);
}

void APacificCoastGraybox::ApplyCoastalSky(const FEnvironmentState& State)
{
    const int32 W = State.Weather == EAeroSimWeather::Clear ? 0 : (State.Weather == EAeroSimWeather::Overcast ? 1 : 2);
    // Index: Clear, Overcast, Rain. One state, one table: weather never moves the sun.
    const FLinearColor SunColor[] = {FLinearColor(1.0F,.96F,.90F), FLinearColor(.74F,.78F,.82F), FLinearColor(.52F,.60F,.70F)};
    const float SkyFill[] = {1.0F, .75F, .5F};
    const float Mie[] = {.0034F, .014F, .024F}, MieG[] = {.80F, .62F, .55F}, Aerial[] = {1.6F, 2.6F, 3.2F};
    const float FogFalloff[] = {.20F, .18F, .12F}, FogOpacity[] = {.75F, .95F, 1.0F}, FogStart[] = {250000.0F, 0.0F, 0.0F};
    const FLinearColor FogColor[] = {FLinearColor(.60F,.66F,.72F), FLinearColor(.30F,.36F,.41F), FLinearColor(.12F,.20F,.28F)};
    const float LayerDensity[] = {.006F, .03F, .05F}, LayerOffset[] = {200.0F, 1500.0F, 2500.0F}, LayerFalloff[] = {1.2F, .5F, .35F};
    const float CloudBottomKm[] = {1.6F, .7F, .45F}, CloudHeightKm[] = {1.6F, 2.4F, 3.5F};
    const float WaterRoughness[] = {.07F, .11F, .16F}, WaveScale[] = {1.0F, 1.2F, 1.45F};

    KeyLight->SetRelativeRotation(GetCoastalSunRotation(State));
    KeyLight->SetIntensity(State.GetLightIntensity());
    KeyLight->SetLightColor(SunColor[W]);
    FillLight->SetIntensity(SkyFill[W]);

    // Dry October air: default Rayleigh keeps a deep zenith blue; low-lying coastal
    // aerosol gives a thin warm horizon; aerial perspective is boosted so 2–4 km ridges
    // go bluer and lighter at this compact slice scale.
    SkyAtmosphere->SetMieScatteringScale(Mie[W]);
    SkyAtmosphere->SetMieAnisotropy(MieG[W]);
    SkyAtmosphere->SetMieExponentialDistribution(.9F);
    SkyAtmosphere->SetMieAbsorptionScale(.0006F);
    SkyAtmosphere->SetAerialPespectiveViewDistanceScale(Aerial[W]);

    // Fog height sits at sea level so the sea stays readable at low altitude; Clear has
    // no fog near the camera, a warm sun-side glow and a thin marine layer hugging the
    // offshore horizon (second fog). Density itself is the Environment State's value.
    WeatherFog->SetRelativeLocation(FVector(0, 0, SeaLevelCm));
    WeatherFog->SetFogDensity(State.GetFogDensity());
    WeatherFog->SetFogHeightFalloff(FogFalloff[W]);
    WeatherFog->SetFogInscatteringColor(FogColor[W]);
    WeatherFog->SetFogMaxOpacity(FogOpacity[W]);
    WeatherFog->SetStartDistance(FogStart[W]);
    // Sun-side warm haze: with SkyAtmosphere this colour is added to the transmitted sun
    // illuminance in the fog's directional lobe, so it only tints toward the low SW sun.
    WeatherFog->SetDirectionalInscatteringExponent(5.0F);
    WeatherFog->SetDirectionalInscatteringStartDistance(15000.0F);
    WeatherFog->SetDirectionalInscatteringColor(W == 0 ? FLinearColor(.75F,.46F,.22F) : FLinearColor(.06F,.06F,.06F));
    WeatherFog->SetSecondFogDensity(LayerDensity[W]);
    WeatherFog->SetSecondFogHeightOffset(LayerOffset[W]);
    WeatherFog->SetSecondFogHeightFalloff(LayerFalloff[W]);

    if (CoastalClouds != nullptr)
    {
        CoastalClouds->SetLayerBottomAltitude(CloudBottomKm[W]);
        CoastalClouds->SetLayerHeight(CloudHeightKm[W]);
        UMaterialInterface* CloudMaterial = CoastalClouds->GetMaterial();
        UMaterialInstanceDynamic* Clouds = Cast<UMaterialInstanceDynamic>(CloudMaterial);
        if (Clouds == nullptr && CloudMaterial != nullptr)
        {
            Clouds = UMaterialInstanceDynamic::Create(CloudMaterial, this);
            CoastalClouds->SetMaterial(Clouds);
        }
        if (Clouds != nullptr)
        {
            // Cloud_GlobalCoverage is an additive bias (engine default -0.2 already reads
            // as broken cumulus). Clear pushes well below it toward ~10-20% sparse
            // fair-weather cumulus; Overcast/Rain raise it. Wind frozen so the seed-21630
            // comparison baseline does not evolve between captures.
            Clouds->SetScalarParameterValue(TEXT("Cloud_GlobalCoverage"), -.50F + .90F * State.GetCloudCover());
            Clouds->SetScalarParameterValue(TEXT("WindMultiplier"), 0.0F);
        }
    }

    if (UMaterialInstanceDynamic* Water = Cast<UMaterialInstanceDynamic>(CoastalWaterMaterial.Get()))
    {
        Water->SetScalarParameterValue(TEXT("Roughness"), WaterRoughness[W]);
        Water->SetScalarParameterValue(TEXT("WaveAmplitudeScale"), WaveScale[W]);
    }
    FillLight->RecaptureSky();
}

void APacificCoastGraybox::ApplyCoastalSurfaceWeather(const FEnvironmentState& State)
{
    // Rain wetness follows the real terrain/rock surfaces instead of floating boxes:
    // darker albedo and much lower roughness on every coastal surface material.
    AppliedSurfaceWetness = State.GetWetness();
    for (int32 Index = 0; Index < CoastalColorParts.Num(); ++Index)
    {
        UMeshComponent* Mesh = CoastalColorParts[Index];
        if (Mesh == nullptr || Mesh->GetMaterial(0) == nullptr || !CoastalPartColors.IsValidIndex(Index)) continue;
        UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
        if (Material == nullptr)
        {
            Material = UMaterialInstanceDynamic::Create(Mesh->GetMaterial(0), this);
            Mesh->SetMaterial(0, Material);
        }
        const bool bGravel = CoastalGravelMaterial != nullptr && Material->Parent == CoastalGravelMaterial;
        const FLinearColor Base = bGravel ? FLinearColor(.72F,.74F,.72F) : CoastalPartColors[Index];
        Material->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(Base, Base * FLinearColor(.52F,.56F,.60F), AppliedSurfaceWetness));
        Material->SetScalarParameterValue(TEXT("Roughness"), FMath::Lerp(.95F, .32F, AppliedSurfaceWetness));
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
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> ColorMaterial(TEXT("/Engine/EngineDebugMaterials/M_SimpleOpaque.M_SimpleOpaque"));
    Component->SetMaterial(0, ColorMaterial.Object);
    Component->SetCollisionProfileName(TEXT("BlockAll"));
    Component->SetCastShadow(true);
    Component->SetCastContactShadow(true);
    const FString PartName=Name.ToString();
    if(PartName.StartsWith(TEXT("Gate_"))||PartName.StartsWith(TEXT("ScaleRef_"))||PartName.StartsWith(TEXT("Route_")))
    {
        const bool bDebug=IsCoastalCourseDebugEnabled();
        Component->SetHiddenInGame(!bDebug);Component->SetVisibility(bDebug);
        // The semantic route owns progression; diagnostics never create invisible obstacles.
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->ComponentTags.Add(TEXT("CoastalCourseDiagnostic"));
    }
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
    const bool bDebug=IsCoastalCourseDebugEnabled();
    Label->SetHiddenInGame(!bDebug);Label->SetVisibility(bDebug);Label->SetCastShadow(false);
    Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Label->ComponentTags.Add(TEXT("CoastalCourseDiagnostic"));
}
