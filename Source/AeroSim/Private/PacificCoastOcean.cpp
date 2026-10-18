#include "PacificCoastGraybox.h"

#include "Components/SceneComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
// Several different wind directions and wavelengths, in cm. Geometry is static;
// this models swell shape and normals, not a claimed hydrodynamic simulation.
float OceanHeight(float X, float Y)
{
    const float Warp = FMath::PerlinNoise2D(FVector2D(X/7000.0F,Y/7000.0F)) * .8F;
    float Height = APacificCoastGraybox::SeaLevelCm;
    for (int32 Wave = 0; Wave < 11; ++Wave)
    {
        const float Angle = Wave * 2.39996F + .47F;
        const float Length = 3700.0F / FMath::Pow(1.29F, Wave);
        const float Amplitude = 13.0F / FMath::Pow(1.43F, Wave);
        Height += Amplitude * FMath::Sin((X*FMath::Cos(Angle)+Y*FMath::Sin(Angle))/Length + Wave*1.73F + Warp);
    }
    return Height;
}
}

void APacificCoastGraybox::BuildCoastalOcean(UMaterialInterface* SolidMaterial)
{
    UProceduralMeshComponent* Water = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("PacificOcean"));
    Water->SetupAttachment(Root);
    Water->SetCollisionProfileName(TEXT("NoCollision"));
    Water->SetCastShadow(false);
    constexpr int32 Size = 385;
    TArray<FVector> Positions, Normals;
    TArray<FVector2D> UV;
    TArray<FLinearColor> ShoreMasks;
    TArray<int32> Indices;
    Positions.Reserve(Size*Size); Normals.Reserve(Size*Size); UV.Reserve(Size*Size);
    // Fine vertices around the headland, progressively larger cells toward a 20 km
    // horizon. This removes the old visible rectangular ocean edge.
    auto Axis = [](int32 Index)
    {
        const float T = (Index - (Size-1)*.5F) / ((Size-1)*.5F);
        return T * 60000.0F + FMath::Sign(T) * 1940000.0F * FMath::Pow(FMath::Abs(T), 5.0F);
    };
    for (int32 Row = 0; Row < Size; ++Row)
    {
        const float Y = Axis(Row);
        const float Shore=GetCoastalShoreX(Y);
        const float Top=GetCoastalLandHeight(GetCliffEdgeX(Y),Y);
        const float Active=Top>SeaLevelCm+100 && FMath::Abs(Y)<210000 ? 1.0F : 0.0F;
        for (int32 Column = 0; Column < Size; ++Column)
        {
            const float X = 15000.0F + Axis(Column);
            Positions.Add(FVector(X,Y,OceanHeight(X,Y)));
            const float DX = (OceanHeight(X+10,Y)-OceanHeight(X-10,Y))/20.0F;
            const float DY = (OceanHeight(X,Y+10)-OceanHeight(X,Y-10))/20.0F;
            Normals.Add(FVector(-DX,-DY,1).GetSafeNormal());
            UV.Add(FVector2D(X/1000.0F,Y/1000.0F));
            ShoreMasks.Add(FLinearColor(FMath::Clamp((X-Shore)/10000.0F,0.0F,1.0F),Active,0,1));
        }
    }
    for (int32 Row = 0; Row < Size-1; ++Row)
        for (int32 Column = 0; Column < Size-1; ++Column)
        {
            const int32 A=Row*Size+Column, B=A+1, C=A+Size, D=C+1;
            Indices.Append({A,C,B,B,C,D});
        }
    Water->CreateMeshSection_LinearColor(0,Positions,Indices,Normals,UV,ShoreMasks,TArray<FProcMeshTangent>(),false);
    Water->SetBoundsScale(4.0F); // Includes bounded shader swell displacement.
    Water->SetMaterial(0,SolidMaterial);
    CoastalWaterMaterial = SolidMaterial;
    CoastalWaterFallback = SolidMaterial;
    ++GeneratedPrimitiveCount;

    // A submerged rock shelf supplies actual shallow depth to the water volume.
    UProceduralMeshComponent* Shelf=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("CoastalSubtidalShelf"));
    Shelf->SetupAttachment(Root);Shelf->SetCollisionProfileName(TEXT("NoCollision"));
    TArray<FVector> ShelfPositions,ShelfNormals;TArray<FVector2D> ShelfUV;TArray<int32> ShelfIndices;
    constexpr int32 ShelfRows=385,ShelfColumns=49;
    for(int32 Row=0;Row<ShelfRows;++Row)
    {
        const float Y=-210000+Row*(420000.0F/(ShelfRows-1));
        const float Shore=GetCoastalShoreX(Y);
        for(int32 Col=0;Col<ShelfColumns;++Col)
        {
            const float T=Col/static_cast<float>(ShelfColumns-1),X=Shore-700+T*15000;
            // The shelf joins the actual deep seabed continuously; no translucent hard outer ledge.
            const float EndFade=1-FMath::SmoothStep(165000.0F,210000.0F,FMath::Abs(Y));
            const float Profile=SeaLevelCm-350-17150*FMath::Pow(T,1.7F)
                -120*FMath::PerlinNoise2D(FVector2D(Y/2300,T*4))*(1-T)*T;
            const float Z=FMath::Lerp(-24000.0F,Profile,EndFade);
            ShelfPositions.Add(FVector(X,Y,Z));ShelfNormals.Add(FVector::UpVector);ShelfUV.Add(FVector2D(X/1800,Y/1800));
        }
    }
    for(int32 Row=0;Row<ShelfRows-1;++Row)for(int32 Col=0;Col<ShelfColumns-1;++Col)
    {const int32 A=Row*ShelfColumns+Col,B=A+1,C=A+ShelfColumns,D=C+1;ShelfIndices.Append({A,C,B,B,C,D});}
    Shelf->CreateMeshSection_LinearColor(0,ShelfPositions,ShelfIndices,ShelfNormals,ShelfUV,TArray<FLinearColor>(),TArray<FProcMeshTangent>(),false);
    Shelf->SetMaterial(0,SolidMaterial);CoastalColorParts.Add(Shelf);CoastalPartColors.Add(FLinearColor(.13F,.16F,.13F));++GeneratedPrimitiveCount;

    // Explicit sea floor under the entire visible ocean closes the environment.
    UProceduralMeshComponent* Floor = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("PacificSeaFloor"));
    Floor->SetupAttachment(Root); Floor->SetCollisionProfileName(TEXT("NoCollision")); Floor->SetCastShadow(false);
    const TArray<FVector> Bottom = {FVector(-2100000,-2100000,-24000),FVector(2100000,-2100000,-24000),
        FVector(-2100000,2100000,-24000),FVector(2100000,2100000,-24000)};
    Floor->CreateMeshSection_LinearColor(0,Bottom,TArray<int32>{0,2,1,1,2,3},
        TArray<FVector>{FVector::UpVector,FVector::UpVector,FVector::UpVector,FVector::UpVector},
        TArray<FVector2D>{FVector2D(0,0),FVector2D(1,0),FVector2D(0,1),FVector2D(1,1)},
        TArray<FLinearColor>(),TArray<FProcMeshTangent>(),false);
    Floor->SetMaterial(0,SolidMaterial);
    CoastalColorParts.Add(Floor);CoastalPartColors.Add(FLinearColor(.025F,.045F,.040F));
    ++GeneratedPrimitiveCount;
}
