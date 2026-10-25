#include "PacificCoastGraybox.h"

#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

namespace CoastalWildlife
{
struct FMesh
{
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UV;
    TArray<FLinearColor> Colors;
};

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

void AddWhale(FMesh& Mesh, const FVector& Origin, const float Yaw, const float Scale)
{
    auto P = [Scale](const FVector& Value) { return Value * Scale; };
    AddEllipsoid(Mesh, Origin, P(FVector(900, 250, 245)), Yaw, P(FVector(0, 0, 130)), 14, 8);
    AddEllipsoid(Mesh, Origin, P(FVector(360, 255, 220)), Yaw, P(FVector(650, 0, 145)), 12, 7);
    AddLimb(Mesh, Origin, Yaw, P(FVector(-700, 0, 130)), P(FVector(-1120, 0, 145)), 115 * Scale, 58 * Scale, 8);
    AddEllipsoid(Mesh, Origin, P(FVector(340, 105, 38)), Yaw + 18.0F, P(FVector(-1130, 155, 145)), 10, 4);
    AddEllipsoid(Mesh, Origin, P(FVector(340, 105, 38)), Yaw - 18.0F, P(FVector(-1130, -155, 145)), 10, 4);
    AddLimb(Mesh, Origin, Yaw, P(FVector(80, 200, 90)), P(FVector(-120, 620, 15)), 85 * Scale, 24 * Scale, 7);
    AddLimb(Mesh, Origin, Yaw, P(FVector(80, -200, 90)), P(FVector(-120, -620, 15)), 85 * Scale, 24 * Scale, 7);
    AddLimb(Mesh, Origin, Yaw, P(FVector(-230, 0, 320)), P(FVector(-420, 0, 540)), 95 * Scale, 8 * Scale, 6);
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

void AddWolf(FMesh& Mesh, const FVector& Origin, const float Yaw, const float Scale)
{
    AddEllipsoid(Mesh, Origin, FVector(165, 58, 68) * Scale, Yaw, FVector(0, 0, 120) * Scale, 10, 6);
    AddEllipsoid(Mesh, Origin, FVector(68, 55, 58) * Scale, Yaw, FVector(175, 0, 165) * Scale, 9, 5);
    AddEllipsoid(Mesh, Origin, FVector(62, 34, 30) * Scale, Yaw, FVector(232, 0, 150) * Scale, 8, 4);
    AddLimb(Mesh, Origin, Yaw, FVector(152, 34, 208) * Scale, FVector(164, 36, 280) * Scale, 28 * Scale, 3 * Scale, 5);
    AddLimb(Mesh, Origin, Yaw, FVector(152, -34, 208) * Scale, FVector(164, -36, 280) * Scale, 28 * Scale, 3 * Scale, 5);
    AddLimb(Mesh, Origin, Yaw, FVector(-145, 0, 145) * Scale, FVector(-285, 18, 205) * Scale, 28 * Scale, 12 * Scale, 6);
    AddQuadrupedLegs(Mesh, Origin, Yaw, Scale, 105, -105, 38, 105, 3, 18, 34);
}

void AddBear(FMesh& Mesh, const FVector& Origin, const float Yaw, const float Scale)
{
    AddEllipsoid(Mesh, Origin, FVector(245, 118, 135) * Scale, Yaw, FVector(0, 0, 165) * Scale, 12, 7);
    AddEllipsoid(Mesh, Origin, FVector(105, 95, 98) * Scale, Yaw, FVector(230, 0, 205) * Scale, 10, 6);
    AddEllipsoid(Mesh, Origin, FVector(75, 62, 46) * Scale, Yaw, FVector(300, 0, 178) * Scale, 9, 5);
    AddEllipsoid(Mesh, Origin, FVector(34) * Scale, Yaw, FVector(205, 78, 285) * Scale, 7, 4);
    AddEllipsoid(Mesh, Origin, FVector(34) * Scale, Yaw, FVector(205, -78, 285) * Scale, 7, 4);
    AddQuadrupedLegs(Mesh, Origin, Yaw, Scale, 145, -135, 76, 128, 5, 34, 24);
}

void AddDeer(FMesh& Mesh, FMesh& Antlers, const FVector& Origin, const float Yaw, const float Scale, const bool bAntlered)
{
    AddEllipsoid(Mesh, Origin, FVector(185, 62, 88) * Scale, Yaw, FVector(0, 0, 155) * Scale, 10, 6);
    AddLimb(Mesh, Origin, Yaw, FVector(125, 0, 180) * Scale, FVector(185, 0, 285) * Scale, 45 * Scale, 32 * Scale, 7);
    AddEllipsoid(Mesh, Origin, FVector(72, 44, 46) * Scale, Yaw, FVector(220, 0, 315) * Scale, 9, 5);
    AddLimb(Mesh, Origin, Yaw, FVector(-160, 0, 175) * Scale, FVector(-245, 0, 225) * Scale, 18 * Scale, 3 * Scale, 5);
    AddQuadrupedLegs(Mesh, Origin, Yaw, Scale, 120, -120, 42, 132, 4, 11, 20);
    if (!bAntlered) return;
    for (const float Side : {-1.0F, 1.0F})
    {
        AddLimb(Antlers, Origin, Yaw, FVector(220, Side * 24, 345) * Scale, FVector(195, Side * 38, 450) * Scale, 8 * Scale, 5 * Scale, 5);
        AddLimb(Antlers, Origin, Yaw, FVector(205, Side * 35, 405) * Scale, FVector(255, Side * 52, 468) * Scale, 6 * Scale, 3 * Scale, 5);
        AddLimb(Antlers, Origin, Yaw, FVector(198, Side * 38, 435) * Scale, FVector(165, Side * 58, 500) * Scale, 5 * Scale, 2 * Scale, 5);
    }
}

void AddCow(FMesh& Mesh, const FVector& Origin, const float Yaw, const float Scale)
{
    AddEllipsoid(Mesh, Origin, FVector(235, 105, 115) * Scale, Yaw, FVector(0, 0, 175) * Scale, 12, 7);
    AddLimb(Mesh, Origin, Yaw, FVector(155, 0, 195) * Scale, FVector(220, 0, 240) * Scale, 68 * Scale, 52 * Scale, 7);
    AddEllipsoid(Mesh, Origin, FVector(92, 68, 65) * Scale, Yaw, FVector(275, 0, 245) * Scale, 10, 6);
    AddLimb(Mesh, Origin, Yaw, FVector(285, 52, 292) * Scale, FVector(335, 88, 318) * Scale, 13 * Scale, 2 * Scale, 5);
    AddLimb(Mesh, Origin, Yaw, FVector(285, -52, 292) * Scale, FVector(335, -88, 318) * Scale, 13 * Scale, 2 * Scale, 5);
    AddLimb(Mesh, Origin, Yaw, FVector(-210, 0, 205) * Scale, FVector(-275, 15, 75) * Scale, 16 * Scale, 5 * Scale, 5);
    AddQuadrupedLegs(Mesh, Origin, Yaw, Scale, 145, -145, 70, 145, 4, 26, 16);
}
}

void APacificCoastGraybox::BuildCoastalWildlife(UMaterialInterface* Material)
{
    using namespace CoastalWildlife;
    FMesh Whales, Wolves, Bears, Deer, Antlers, Cattle;

    const FVector WhaleOrigins[] = {
        FVector(47000, -61000, SeaLevelCm - 95), FVector(60000, 36000, SeaLevelCm - 190)};
    const float WhaleYaw[] = {32.0F, -18.0F};
    const float WhaleScale[] = {1.0F, .78F};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(WhaleOrigins); ++Index)
        AddWhale(Whales, WhaleOrigins[Index], WhaleYaw[Index], WhaleScale[Index]);

    const FVector2D WolfXY[] = {{-30500, 33500}, {-32100, 34700}, {-28900, 35700}};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(WolfXY); ++Index)
        AddWolf(Wolves, FVector(WolfXY[Index].X, WolfXY[Index].Y, GetCoastalLandHeight(WolfXY[Index].X, WolfXY[Index].Y)),
            35.0F + Index * 24.0F, .9F + Index * .06F);

    const FVector2D BearXY(-35500, 38200);
    AddBear(Bears, FVector(BearXY.X, BearXY.Y, GetCoastalLandHeight(BearXY.X, BearXY.Y)), 118.0F, 1.08F);

    const FVector2D DeerXY[] = {{-21800,31500}, {-23200,32200}, {-20500,33000}, {-24500,30700}, {-19400,31400}};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(DeerXY); ++Index)
        AddDeer(Deer, Antlers, FVector(DeerXY[Index].X, DeerXY[Index].Y, GetCoastalLandHeight(DeerXY[Index].X, DeerXY[Index].Y)),
            105.0F + Index * 31.0F, .88F + (Index % 3) * .08F, Index == 0 || Index == 3);

    const FVector2D CowXY[] = {{-21500,34500}, {-23200,35300}, {-19700,35900}, {-24800,34100}};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(CowXY); ++Index)
        AddCow(Cattle, FVector(CowXY[Index].X, CowXY[Index].Y, GetCoastalLandHeight(CowXY[Index].X, CowXY[Index].Y)),
            70.0F + Index * 37.0F, .95F + (Index % 2) * .08F);

    struct FBatch { const TCHAR* Name; FMesh* Mesh; FLinearColor Color; float DrawDistance; };
    FBatch Batches[] = {
        {TEXT("CoastalWhales"), &Whales, FLinearColor(.095F,.14F,.17F), 220000.0F},
        {TEXT("CoastalWolves"), &Wolves, FLinearColor(.24F,.27F,.25F), 120000.0F},
        {TEXT("CoastalBear"), &Bears, FLinearColor(.19F,.105F,.055F), 120000.0F},
        {TEXT("CoastalDeer"), &Deer, FLinearColor(.42F,.24F,.105F), 120000.0F},
        {TEXT("CoastalDeerAntlers"), &Antlers, FLinearColor(.67F,.57F,.40F), 120000.0F},
        {TEXT("CoastalCattle"), &Cattle, FLinearColor(.17F,.12F,.08F), 120000.0F}};
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
        ++GeneratedPrimitiveCount;
    }
    CoastalWhaleCount = UE_ARRAY_COUNT(WhaleOrigins);
    CoastalWolfCount = UE_ARRAY_COUNT(WolfXY);
    CoastalBearCount = 1;
    CoastalDeerCount = UE_ARRAY_COUNT(DeerXY);
    CoastalCattleCount = UE_ARRAY_COUNT(CowXY);
}
