#include "PacificCoastGraybox.h"

#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace CoastalWildlife
{
struct FMeshRange
{
    int32 StartVertex = 0;
    int32 EndVertex = 0;
    FVector Origin = FVector::ZeroVector;
    float Yaw = 0.0F;
    float Scale = 1.0F;
    float Phase = 0.0F;
    int32 Kind = 0;
};

struct FMesh
{
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UV;
    TArray<FLinearColor> Colors;
    TArray<FMeshRange> Ranges;
};

void RecordRange(FMesh& Mesh, const int32 StartVertex, const FVector& Origin, const float Yaw,
    const float Scale, const float Phase, const int32 Kind)
{
    FMeshRange& Range = Mesh.Ranges.AddDefaulted_GetRef();
    Range.StartVertex = StartVertex;
    Range.EndVertex = Mesh.Vertices.Num();
    Range.Origin = Origin;
    Range.Yaw = Yaw;
    Range.Scale = Scale;
    Range.Phase = Phase;
    Range.Kind = Kind;
}

FVector TransformPoint(const FVector& Local, const FVector& Origin, const float Yaw)
{
    return Origin + FRotator(0.0F, Yaw, 0.0F).RotateVector(Local);
}

void AddEllipsoid(FMesh& Mesh, const FVector& Origin, const FVector& Radii, const float Yaw,
    const FVector& LocalCenter, const int32 Sides = 10, const int32 Rings = 6)
{
    const int32 Base = Mesh.Vertices.Num();
    for (int32 Ring = 0; Ring <= Rings; ++Ring)
    {
        const float Latitude = -HALF_PI + PI * Ring / static_cast<float>(Rings);
        const float CosLatitude = FMath::Cos(Latitude);
        for (int32 Side = 0; Side <= Sides; ++Side)
        {
            const float Longitude = 2.0F * PI * Side / static_cast<float>(Sides);
            const FVector Local = LocalCenter + FVector(
                Radii.X * CosLatitude * FMath::Cos(Longitude),
                Radii.Y * CosLatitude * FMath::Sin(Longitude),
                Radii.Z * FMath::Sin(Latitude));
            Mesh.Vertices.Add(TransformPoint(Local, Origin, Yaw));
            Mesh.UV.Add(FVector2D(Side / static_cast<float>(Sides), Ring / static_cast<float>(Rings)));
            Mesh.Colors.Add(FLinearColor::White);
        }
    }
    for (int32 Ring = 0; Ring < Rings; ++Ring)
        for (int32 Side = 0; Side < Sides; ++Side)
        {
            const int32 A = Base + Ring * (Sides + 1) + Side;
            const int32 B = A + Sides + 1;
            const int32 C = A + 1;
            const int32 D = B + 1;
            Mesh.Triangles.Append({A, C, B, C, D, B});
        }
}

void AddLimb(FMesh& Mesh, const FVector& Origin, const float Yaw, const FVector& LocalStart,
    const FVector& LocalEnd, const float StartRadius, const float EndRadius, const int32 Sides = 7)
{
    const FVector Start = TransformPoint(LocalStart, Origin, Yaw);
    const FVector End = TransformPoint(LocalEnd, Origin, Yaw);
    const FVector Axis = (End - Start).GetSafeNormal();
    FVector Across = FVector::CrossProduct(Axis, FVector::UpVector).GetSafeNormal();
    if (Across.IsNearlyZero()) Across = FVector::RightVector;
    const FVector Up = FVector::CrossProduct(Across, Axis).GetSafeNormal();
    const int32 Base = Mesh.Vertices.Num();
    for (int32 Ring = 0; Ring < 2; ++Ring)
        for (int32 Side = 0; Side < Sides; ++Side)
        {
            const float Arc = 2.0F * PI * Side / static_cast<float>(Sides);
            const FVector Radial = Across * FMath::Cos(Arc) + Up * FMath::Sin(Arc);
            Mesh.Vertices.Add((Ring == 0 ? Start : End) + Radial * (Ring == 0 ? StartRadius : EndRadius));
            Mesh.UV.Add(FVector2D(Side / static_cast<float>(Sides), static_cast<float>(Ring)));
            Mesh.Colors.Add(FLinearColor::White);
        }
    for (int32 Side = 0; Side < Sides; ++Side)
    {
        const int32 Next = (Side + 1) % Sides;
        Mesh.Triangles.Append({Base + Side, Base + Next, Base + Sides + Side,
            Base + Next, Base + Sides + Next, Base + Sides + Side});
    }
}

void FinalizeNormals(FMesh& Mesh)
{
    Mesh.Normals.Init(FVector::ZeroVector, Mesh.Vertices.Num());
    for (int32 Index = 0; Index < Mesh.Triangles.Num(); Index += 3)
    {
        const int32 A = Mesh.Triangles[Index], B = Mesh.Triangles[Index + 1], C = Mesh.Triangles[Index + 2];
        const FVector Normal = FVector::CrossProduct(Mesh.Vertices[B] - Mesh.Vertices[A], Mesh.Vertices[C] - Mesh.Vertices[A]);
        Mesh.Normals[A] += Normal; Mesh.Normals[B] += Normal; Mesh.Normals[C] += Normal;
    }
    for (FVector& Normal : Mesh.Normals) Normal.Normalize();
}

void AddWhale(FMesh& Mesh, FMesh& Dark, FMesh& Light, const FVector& Origin, const float Yaw, const float Scale)
{
    auto P = [Scale](const FVector& Value) { return Value * Scale; };
    AddEllipsoid(Mesh, Origin, P(FVector(900, 250, 245)), Yaw, P(FVector(0, 0, 130)), 20, 12);
    AddEllipsoid(Mesh, Origin, P(FVector(360, 255, 220)), Yaw, P(FVector(650, 0, 145)), 18, 10);
    AddLimb(Mesh, Origin, Yaw, P(FVector(-700, 0, 130)), P(FVector(-1120, 0, 145)), 115 * Scale, 58 * Scale, 8);
    AddEllipsoid(Mesh, Origin, P(FVector(340, 105, 38)), Yaw + 18.0F, P(FVector(-1130, 155, 145)), 10, 4);
    AddEllipsoid(Mesh, Origin, P(FVector(340, 105, 38)), Yaw - 18.0F, P(FVector(-1130, -155, 145)), 10, 4);
    AddLimb(Mesh, Origin, Yaw, P(FVector(80, 200, 90)), P(FVector(-120, 620, 15)), 85 * Scale, 24 * Scale, 7);
    AddLimb(Mesh, Origin, Yaw, P(FVector(80, -200, 90)), P(FVector(-120, -620, 15)), 85 * Scale, 24 * Scale, 7);
    AddEllipsoid(Light, Origin, P(FVector(720, 185, 120)), Yaw, P(FVector(120, 0, 25)), 14, 6);
    AddEllipsoid(Dark, Origin, P(FVector(34, 18, 18)), Yaw, P(FVector(610, 232, 230)), 8, 4);
    AddEllipsoid(Dark, Origin, P(FVector(34, 18, 18)), Yaw, P(FVector(610, -232, 230)), 8, 4);
    AddLimb(Dark, Origin, Yaw, P(FVector(690, 246, 120)), P(FVector(890, 214, 105)), 10 * Scale, 4 * Scale, 5);
    AddLimb(Dark, Origin, Yaw, P(FVector(690, -246, 120)), P(FVector(890, -214, 105)), 10 * Scale, 4 * Scale, 5);
    AddLimb(Light, Origin, Yaw, P(FVector(-230, 0, 320)), P(FVector(-420, 0, 540)), 95 * Scale, 8 * Scale, 6);
}

void AddQuadrupedLegs(FMesh& Mesh, const FVector& Origin, const float Yaw, const float Scale,
    const float XFront, const float XBack, const float YOffset, const float HipZ, const float GroundZ,
    const float Radius, const float Stride)
{
    const FVector Hips[] = {{XFront, YOffset, HipZ}, {XFront, -YOffset, HipZ}, {XBack, YOffset, HipZ}, {XBack, -YOffset, HipZ}};
    for (int32 Index = 0; Index < 4; ++Index)
    {
        const float Direction = Index % 2 == 0 ? 1.0F : -1.0F;
        const FVector Knee = Hips[Index] + FVector(Direction * Stride, 0, (GroundZ - HipZ) * .55F);
        const FVector Foot = FVector(Hips[Index].X - Direction * Stride * .35F, Hips[Index].Y, GroundZ);
        AddLimb(Mesh, Origin, Yaw, Hips[Index] * Scale, Knee * Scale, Radius * Scale, Radius * .72F * Scale, 6);
        AddLimb(Mesh, Origin, Yaw, Knee * Scale, Foot * Scale, Radius * .72F * Scale, Radius * .5F * Scale, 6);
    }
}

void AddWolf(FMesh& Mesh, FMesh& Dark, FMesh& Light, const FVector& Origin, const float Yaw, const float Scale)
{
    AddEllipsoid(Mesh, Origin, FVector(165, 58, 68) * Scale, Yaw, FVector(0, 0, 120) * Scale, 16, 9);
    AddEllipsoid(Mesh, Origin, FVector(68, 55, 58) * Scale, Yaw, FVector(175, 0, 165) * Scale, 14, 8);
    AddEllipsoid(Mesh, Origin, FVector(62, 34, 30) * Scale, Yaw, FVector(232, 0, 150) * Scale, 12, 7);
    AddLimb(Mesh, Origin, Yaw, FVector(152, 34, 208) * Scale, FVector(164, 36, 280) * Scale, 28 * Scale, 3 * Scale, 5);
    AddLimb(Mesh, Origin, Yaw, FVector(152, -34, 208) * Scale, FVector(164, -36, 280) * Scale, 28 * Scale, 3 * Scale, 5);
    AddLimb(Mesh, Origin, Yaw, FVector(-145, 0, 145) * Scale, FVector(-285, 18, 205) * Scale, 28 * Scale, 12 * Scale, 6);
    AddQuadrupedLegs(Mesh, Origin, Yaw, Scale, 105, -105, 38, 105, 3, 18, 34);
    AddEllipsoid(Light, Origin, FVector(105, 48, 48) * Scale, Yaw, FVector(90, 0, 93) * Scale, 9, 5);
    AddEllipsoid(Dark, Origin, FVector(24, 15, 14) * Scale, Yaw, FVector(239, 0, 151) * Scale, 8, 4);
    for (const float Side : {-1.0F, 1.0F})
        AddEllipsoid(Dark, Origin, FVector(13, 8, 8) * Scale, Yaw, FVector(220, Side * 35, 178) * Scale, 7, 4);
    for (const FVector& Paw : {FVector(112,38,6), FVector(112,-38,6), FVector(-112,38,6), FVector(-112,-38,6)})
        AddEllipsoid(Dark, Origin, FVector(28, 17, 8) * Scale, Yaw, Paw * Scale, 7, 4);
}

void AddBear(FMesh& Mesh, FMesh& Dark, FMesh& Light, const FVector& Origin, const float Yaw, const float Scale)
{
    AddEllipsoid(Mesh, Origin, FVector(245, 118, 135) * Scale, Yaw, FVector(0, 0, 165) * Scale, 18, 10);
    AddEllipsoid(Mesh, Origin, FVector(105, 95, 98) * Scale, Yaw, FVector(230, 0, 205) * Scale, 16, 9);
    AddEllipsoid(Mesh, Origin, FVector(75, 62, 46) * Scale, Yaw, FVector(300, 0, 178) * Scale, 14, 8);
    AddEllipsoid(Mesh, Origin, FVector(34) * Scale, Yaw, FVector(205, 78, 285) * Scale, 7, 4);
    AddEllipsoid(Mesh, Origin, FVector(34) * Scale, Yaw, FVector(205, -78, 285) * Scale, 7, 4);
    AddQuadrupedLegs(Mesh, Origin, Yaw, Scale, 145, -135, 76, 128, 5, 34, 24);
    AddEllipsoid(Light, Origin, FVector(120, 74, 74) * Scale, Yaw, FVector(132, 0, 112) * Scale, 10, 5);
    AddEllipsoid(Dark, Origin, FVector(34, 24, 20) * Scale, Yaw, FVector(347, 0, 181) * Scale, 8, 4);
    for (const float Side : {-1.0F, 1.0F})
        AddEllipsoid(Dark, Origin, FVector(14, 9, 9) * Scale, Yaw, FVector(288, Side * 51, 224) * Scale, 7, 4);
    for (const FVector& Paw : {FVector(158,76,8), FVector(158,-76,8), FVector(-145,76,8), FVector(-145,-76,8)})
        AddEllipsoid(Dark, Origin, FVector(42, 31, 12) * Scale, Yaw, Paw * Scale, 8, 4);
}

void AddDeer(FMesh& Mesh, FMesh& Antlers, FMesh& Dark, FMesh& Light, const FVector& Origin, const float Yaw, const float Scale, const bool bAntlered)
{
    AddEllipsoid(Mesh, Origin, FVector(185, 62, 88) * Scale, Yaw, FVector(0, 0, 155) * Scale, 16, 9);
    AddLimb(Mesh, Origin, Yaw, FVector(125, 0, 180) * Scale, FVector(185, 0, 285) * Scale, 45 * Scale, 32 * Scale, 7);
    AddEllipsoid(Mesh, Origin, FVector(72, 44, 46) * Scale, Yaw, FVector(220, 0, 315) * Scale, 14, 8);
    AddLimb(Mesh, Origin, Yaw, FVector(-160, 0, 175) * Scale, FVector(-245, 0, 225) * Scale, 18 * Scale, 3 * Scale, 5);
    AddQuadrupedLegs(Mesh, Origin, Yaw, Scale, 120, -120, 42, 132, 4, 11, 20);
    AddEllipsoid(Light, Origin, FVector(55, 30, 28) * Scale, Yaw, FVector(-160, 0, 184) * Scale, 8, 4);
    AddEllipsoid(Dark, Origin, FVector(19, 14, 12) * Scale, Yaw, FVector(277, 0, 306) * Scale, 7, 4);
    for (const float Side : {-1.0F, 1.0F})
        AddEllipsoid(Dark, Origin, FVector(11, 7, 7) * Scale, Yaw, FVector(255, Side * 34, 330) * Scale, 7, 4);
    for (const FVector& Hoof : {FVector(120,42,5), FVector(120,-42,5), FVector(-120,42,5), FVector(-120,-42,5)})
        AddEllipsoid(Dark, Origin, FVector(18, 12, 7) * Scale, Yaw, Hoof * Scale, 7, 4);
    for (const FVector& Spot : {FVector(15,55,205), FVector(-35,57,180), FVector(-85,50,220), FVector(65,52,165)})
    {
        AddEllipsoid(Light, Origin, FVector(15, 7, 12) * Scale, Yaw, Spot * Scale, 7, 4);
        AddEllipsoid(Light, Origin, FVector(15, 7, 12) * Scale, Yaw, FVector(Spot.X, -Spot.Y, Spot.Z) * Scale, 7, 4);
    }
    if (!bAntlered) return;
    for (const float Side : {-1.0F, 1.0F})
    {
        AddLimb(Antlers, Origin, Yaw, FVector(220, Side * 24, 345) * Scale, FVector(195, Side * 38, 450) * Scale, 8 * Scale, 5 * Scale, 5);
        AddLimb(Antlers, Origin, Yaw, FVector(205, Side * 35, 405) * Scale, FVector(255, Side * 52, 468) * Scale, 6 * Scale, 3 * Scale, 5);
        AddLimb(Antlers, Origin, Yaw, FVector(198, Side * 38, 435) * Scale, FVector(165, Side * 58, 500) * Scale, 5 * Scale, 2 * Scale, 5);
    }
}

void AddCow(FMesh& Mesh, FMesh& Dark, FMesh& Light, const FVector& Origin, const float Yaw, const float Scale)
{
    AddEllipsoid(Mesh, Origin, FVector(235, 105, 115) * Scale, Yaw, FVector(0, 0, 175) * Scale, 18, 10);
    AddLimb(Mesh, Origin, Yaw, FVector(155, 0, 195) * Scale, FVector(220, 0, 240) * Scale, 68 * Scale, 52 * Scale, 7);
    AddEllipsoid(Mesh, Origin, FVector(92, 68, 65) * Scale, Yaw, FVector(275, 0, 245) * Scale, 15, 9);
    AddLimb(Mesh, Origin, Yaw, FVector(285, 52, 292) * Scale, FVector(335, 88, 318) * Scale, 13 * Scale, 2 * Scale, 5);
    AddLimb(Mesh, Origin, Yaw, FVector(285, -52, 292) * Scale, FVector(335, -88, 318) * Scale, 13 * Scale, 2 * Scale, 5);
    AddLimb(Mesh, Origin, Yaw, FVector(-210, 0, 205) * Scale, FVector(-275, 15, 75) * Scale, 16 * Scale, 5 * Scale, 5);
    AddQuadrupedLegs(Mesh, Origin, Yaw, Scale, 145, -145, 70, 145, 4, 26, 16);
    AddEllipsoid(Light, Origin, FVector(105, 72, 58) * Scale, Yaw, FVector(275, 0, 235) * Scale, 10, 5);
    AddEllipsoid(Dark, Origin, FVector(42, 35, 24) * Scale, Yaw, FVector(350, 0, 230) * Scale, 8, 4);
    for (const float Side : {-1.0F, 1.0F})
        AddEllipsoid(Dark, Origin, FVector(13, 8, 8) * Scale, Yaw, FVector(315, Side * 50, 264) * Scale, 7, 4);
    for (const FVector& Hoof : {FVector(148,70,5), FVector(148,-70,5), FVector(-148,70,5), FVector(-148,-70,5)})
        AddEllipsoid(Dark, Origin, FVector(26, 20, 9) * Scale, Yaw, Hoof * Scale, 7, 4);
    AddEllipsoid(Light, Origin, FVector(110, 16, 74) * Scale, Yaw, FVector(15, 101, 185) * Scale, 9, 5);
    AddEllipsoid(Light, Origin, FVector(72, 15, 58) * Scale, Yaw, FVector(-105, -102, 205) * Scale, 9, 5);
}
}

void APacificCoastGraybox::BuildCoastalWildlife(UMaterialInterface* Material)
{
    using namespace CoastalWildlife;
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> WhaleMesh(TEXT("/Game/ThirdParty/Wildlife/Whale/Idle_Swim.Idle_Swim"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> WhaleAnimation(TEXT("/Game/ThirdParty/Wildlife/Whale/Idle_Swim_Anim.Idle_Swim_Anim"));
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> BearMesh(TEXT("/Game/ThirdParty/Wildlife/Bear/Bear_Animated.Bear_Animated"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> BearAnimation(TEXT("/Game/ThirdParty/Wildlife/Bear/Bear_AnimatedStand_Idle_01.Bear_AnimatedStand_Idle_01"));
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> DeerMesh(TEXT("/Game/ThirdParty/Wildlife/Deer/deer_animation/SkeletalMeshes/deer_animation.deer_animation"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> DeerAnimation(TEXT("/Game/ThirdParty/Wildlife/Deer/deer_animation/SkeletalMeshes/deer_animation_Anim.deer_animation_Anim"));

    if (WhaleMesh.Succeeded() && WhaleAnimation.Succeeded() && BearMesh.Succeeded()
        && BearAnimation.Succeeded() && DeerMesh.Succeeded() && DeerAnimation.Succeeded())
    {
        const auto AddAssetAnimal = [this](const FName Name, USkeletalMesh* Mesh, UAnimSequence* Animation,
            const FVector& Location, const FRotator& Rotation, const FVector& Scale, const float Phase)
        {
            USkeletalMeshComponent* Component = CreateDefaultSubobject<USkeletalMeshComponent>(Name);
            Component->SetupAttachment(Root);
            Component->SetSkeletalMeshAsset(Mesh);
            Component->SetRelativeLocation(Location);
            Component->SetRelativeRotation(Rotation);
            Component->SetRelativeScale3D(Scale);
            Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Component->SetGenerateOverlapEvents(false);
            Component->SetCastShadow(true);
            Component->bReceivesDecals = false;
            Component->LDMaxDrawDistance = 220000.0F;
            Component->SetAnimationMode(EAnimationMode::AnimationSingleNode);
            Component->PlayAnimation(Animation, true);
            Component->SetPosition(Phase, false);
            Component->SetPlayRate(1.0F);
            return Component;
        };

        AddAssetAnimal(TEXT("CoastalWhaleAssetA"), WhaleMesh.Object, WhaleAnimation.Object,
            FVector(47000, -61000, SeaLevelCm - 40), FRotator(0, 122, 180), FVector(100.0F), 0.0F);
        AddAssetAnimal(TEXT("CoastalWhaleAssetB"), WhaleMesh.Object, WhaleAnimation.Object,
            FVector(60000, 36000, SeaLevelCm - 100), FRotator(0, 72, 180), FVector(78.0F), 1.2F);

        const FVector2D BearXY(-35500, 38200);
        AddAssetAnimal(TEXT("CoastalBearAsset"), BearMesh.Object, BearAnimation.Object,
            FVector(BearXY.X, BearXY.Y, GetCoastalLandHeight(BearXY.X, BearXY.Y)),
            FRotator(0, 128, 0), FVector(1.15F), 0.6F);

        const FVector2D DeerXY[] = {{-21800,31500}, {-23500,32200}, {-20200,32900}, {-25100,31000}, {-18700,31800}};
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(DeerXY); ++Index)
        {
            const FVector Origin(DeerXY[Index].X, DeerXY[Index].Y, GetCoastalLandHeight(DeerXY[Index].X, DeerXY[Index].Y) + 42.0F);
            AddAssetAnimal(*FString::Printf(TEXT("CoastalDeerAsset%02d"), Index + 1), DeerMesh.Object, DeerAnimation.Object,
                Origin, FRotator(0, 20.0F + Index * 31.0F, 0), FVector(.28F + (Index % 2) * .025F), Index * .45F);
        }

        CoastalWhaleCount = 2;
        CoastalWolfCount = 0;
        CoastalBearCount = 1;
        CoastalDeerCount = UE_ARRAY_COUNT(DeerXY);
        CoastalCattleCount = 0;
        GeneratedPrimitiveCount += 8;
        return;
    }

    FMesh Whales, WhaleDarkDetails, WhaleLightDetails, Bears, Deer, Antlers, DarkDetails, LightDetails;

    const FVector WhaleOrigins[] = {
        FVector(47000, -61000, SeaLevelCm - 95), FVector(60000, 36000, SeaLevelCm - 190)};
    const float WhaleYaw[] = {32.0F, -18.0F};
    const float WhaleScale[] = {1.0F, .78F};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(WhaleOrigins); ++Index)
    {
        const int32 StartVertex = Whales.Vertices.Num();
        const int32 DarkStart = WhaleDarkDetails.Vertices.Num();
        const int32 LightStart = WhaleLightDetails.Vertices.Num();
        AddWhale(Whales, WhaleDarkDetails, WhaleLightDetails, WhaleOrigins[Index], WhaleYaw[Index], WhaleScale[Index]);
        RecordRange(Whales, StartVertex, WhaleOrigins[Index], WhaleYaw[Index], WhaleScale[Index], Index * 2.31F, 0);
        RecordRange(WhaleDarkDetails, DarkStart, WhaleOrigins[Index], WhaleYaw[Index], WhaleScale[Index], Index * 2.31F, 0);
        RecordRange(WhaleLightDetails, LightStart, WhaleOrigins[Index], WhaleYaw[Index], WhaleScale[Index], Index * 2.31F, 0);
    }

    const FVector2D BearXY(-35500, 38200);
    const FVector BearOrigin(BearXY.X, BearXY.Y, GetCoastalLandHeight(BearXY.X, BearXY.Y));
    const int32 BearDarkStart = DarkDetails.Vertices.Num();
    const int32 BearLightStart = LightDetails.Vertices.Num();
    AddBear(Bears, DarkDetails, LightDetails, BearOrigin, 118.0F, 1.08F);
    RecordRange(Bears, 0, BearOrigin, 118.0F, 1.08F, 2.19F, 2);
    RecordRange(DarkDetails, BearDarkStart, BearOrigin, 118.0F, 1.08F, 2.19F, 2);
    RecordRange(LightDetails, BearLightStart, BearOrigin, 118.0F, 1.08F, 2.19F, 2);

    const FVector2D DeerXY[] = {{-21800,31500}, {-23200,32200}, {-20500,33000}, {-24500,30700}, {-19400,31400}};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(DeerXY); ++Index)
    {
        const FVector Origin(DeerXY[Index].X, DeerXY[Index].Y, GetCoastalLandHeight(DeerXY[Index].X, DeerXY[Index].Y));
        const float Yaw = 105.0F + Index * 31.0F;
        const float Scale = .88F + (Index % 3) * .08F;
        const int32 StartVertex = Deer.Vertices.Num();
        const int32 AntlerStart = Antlers.Vertices.Num();
        const int32 DarkStart = DarkDetails.Vertices.Num();
        const int32 LightStart = LightDetails.Vertices.Num();
        AddDeer(Deer, Antlers, DarkDetails, LightDetails, Origin, Yaw, Scale, Index == 0 || Index == 3);
        const float Phase = .37F + Index * 1.29F;
        RecordRange(Deer, StartVertex, Origin, Yaw, Scale, Phase, 3);
        RecordRange(DarkDetails, DarkStart, Origin, Yaw, Scale, Phase, 3);
        RecordRange(LightDetails, LightStart, Origin, Yaw, Scale, Phase, 3);
        if (Antlers.Vertices.Num() > AntlerStart) RecordRange(Antlers, AntlerStart, Origin, Yaw, Scale, Phase, 3);
    }

    struct FBatch { const TCHAR* Name; FMesh* Mesh; FLinearColor Color; float DrawDistance; };
    FBatch Batches[] = {
        {TEXT("CoastalWhales"), &Whales, FLinearColor(.095F,.14F,.17F), 220000.0F},
        {TEXT("CoastalWhaleDarkDetails"), &WhaleDarkDetails, FLinearColor(.012F,.018F,.021F), 220000.0F},
        {TEXT("CoastalWhaleLightDetails"), &WhaleLightDetails, FLinearColor(.63F,.68F,.66F), 220000.0F},
        {TEXT("CoastalBear"), &Bears, FLinearColor(.19F,.105F,.055F), 120000.0F},
        {TEXT("CoastalDeer"), &Deer, FLinearColor(.42F,.24F,.105F), 120000.0F},
        {TEXT("CoastalDeerAntlers"), &Antlers, FLinearColor(.67F,.57F,.40F), 120000.0F},
        {TEXT("CoastalWildlifeDarkDetails"), &DarkDetails, FLinearColor(.012F,.009F,.007F), 120000.0F},
        {TEXT("CoastalWildlifeLightDetails"), &LightDetails, FLinearColor(.68F,.64F,.53F), 120000.0F}};
    for (FBatch& Batch : Batches)
    {
        FinalizeNormals(*Batch.Mesh);
        UProceduralMeshComponent* Component = CreateDefaultSubobject<UProceduralMeshComponent>(Batch.Name);
        Component->SetupAttachment(Root);
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetCastShadow(true);
        Component->SetCastContactShadow(true);
        Component->LDMaxDrawDistance = Batch.DrawDistance;
        Component->CreateMeshSection_LinearColor(0, Batch.Mesh->Vertices, Batch.Mesh->Triangles, Batch.Mesh->Normals,
            Batch.Mesh->UV, Batch.Mesh->Colors, TArray<FProcMeshTangent>(), false);
        Component->SetMaterial(0, Material);
        CoastalColorParts.Add(Component);
        CoastalPartColors.Add(Batch.Color);
        if (FString(Batch.Name).Contains(TEXT("CoastalWhale"))) CoastalWhaleAnimatedParts.Add(Component);
        if (!Batch.Mesh->Ranges.IsEmpty())
        {
            FCoastalWildlifeAnimatedBatch& Animated = CoastalWildlifeAnimatedBatches.AddDefaulted_GetRef();
            Animated.Component = Component;
            Animated.BaseVertices = Batch.Mesh->Vertices;
            Animated.Normals = Batch.Mesh->Normals;
            Animated.UV = Batch.Mesh->UV;
            Animated.Colors = Batch.Mesh->Colors;
            for (const FMeshRange& Source : Batch.Mesh->Ranges)
            {
                FCoastalWildlifeVertexRange& Range = Animated.Ranges.AddDefaulted_GetRef();
                Range.StartVertex = Source.StartVertex;
                Range.EndVertex = Source.EndVertex;
                Range.Origin = Source.Origin;
                Range.Yaw = Source.Yaw;
                Range.Scale = Source.Scale;
                Range.Phase = Source.Phase;
                Range.Kind = static_cast<ECoastalWildlifeKind>(Source.Kind);
            }
        }
        ++GeneratedPrimitiveCount;
    }
    CoastalWhaleCount = UE_ARRAY_COUNT(WhaleOrigins);
    CoastalWolfCount = 0;
    CoastalBearCount = 1;
    CoastalDeerCount = UE_ARRAY_COUNT(DeerXY);
    CoastalCattleCount = 0;
}
