#include "PacificCoastGraybox.h"

#include "Components/SceneComponent.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

namespace CoastalTerrain
{
constexpr float West = -240000.0F;
constexpr float North = 220000.0F;
constexpr float SubmergedRim = -12000.0F;
constexpr float CliffFoot = -14000.0F;
constexpr float BedrockBase = -18000.0F;
constexpr int32 CoastRows = 385;
constexpr int32 CliffDepthSteps = 36;

float Smooth(float Value)
{
    const float T = FMath::Clamp(Value, 0.0F, 1.0F);
    return T * T * (3.0F - 2.0F * T);
}

float Noise(float X, float Y, float Scale)
{
    return FMath::PerlinNoise2D(FVector2D(X / Scale + 13.73F, Y / Scale - 8.19F));
}

struct FRidgeNode final
{
    FVector2D Position;
    float Height;
    float Width;
};

float RidgeSegment(const FVector2D& Point, const FRidgeNode& A, const FRidgeNode& B)
{
    const FVector2D Direction = B.Position - A.Position;
    const float T = FMath::Clamp(static_cast<float>(FVector2D::DotProduct(Point - A.Position, Direction) / Direction.SizeSquared()), 0.0F, 1.0F);
    const FVector2D Center = FMath::Lerp(A.Position, B.Position, T);
    const float Distance = static_cast<float>((Point - Center).Size());
    // An 18 m rounded crest transitions into a long shoulder, not a Gaussian dome or a razor edge.
    const float RoundedDistance = FMath::Sqrt(Distance * Distance + 1800.0F * 1800.0F) - 1800.0F;
    const float Profile = FMath::Pow(FMath::Clamp(1.0F - RoundedDistance / FMath::Lerp(A.Width, B.Width, T), 0.0F, 1.0F), 1.25F);
    return FMath::Lerp(A.Height, B.Height, T) * Profile;
}

float JoinRidge(float A, float B)
{
    // Smooth only the seam between neighboring shoulders; the ridge network stays connected.
    constexpr float SeamHeight = 700.0F;
    const float H = FMath::Clamp(.5F + .5F * (A - B) / SeamHeight, 0.0F, 1.0F);
    return FMath::Lerp(B, A, H) + SeamHeight * H * (1.0F - H);
}

FVector2D BridgeCoordinates(const float X, const float Y)
{
    constexpr float Cosine = .9510565163F;
    constexpr float Sine = .3090169944F;
    const float DX = X + 9000.0F, DY = Y - 8500.0F;
    return FVector2D(DX * Cosine + DY * Sine, -DX * Sine + DY * Cosine);
}

float ChannelWeight(const float X, const float Y)
{
    const FVector2D Bridge = BridgeCoordinates(X, Y);
    return (1.0F - Smooth((FMath::Abs(Bridge.X) - 250.0F) / 550.0F))
        * (1.0F - Smooth(FMath::Abs(Bridge.Y) / 3000.0F));
}

float CoastY(const int32 Row)
{
    const float T = Row / static_cast<float>(CoastRows - 1) * 2.0F - 1.0F;
    const float Magnitude = FMath::Abs(T);
    return FMath::Sign(T) * (Magnitude <= .6F ? Magnitude * 100000.0F : 60000.0F + (Magnitude - .6F) * 400000.0F);
}

float CliffOutcrop(const float Y, const float Depth)
{
    // Unequal gently dipping beds form supported shelves instead of one smooth bow with vertical ribs.
    const float Thresholds[] = {.09F, .27F, .46F, .69F, .85F};
    const float Widths[] = {.035F, .025F, .045F, .04F, .03F};
    const float Thickness[] = {270.0F, 510.0F, 390.0F, 660.0F, 340.0F};
    float Outcrop = Depth * 1500.0F;
    for (int32 Bed = 0; Bed < UE_ARRAY_COUNT(Thresholds); ++Bed)
    {
        const float Dip = .055F * Noise(Y + Bed * 4300.0F, 13000.0F + Bed * 3700.0F, 6800.0F);
        const float BedHeight = Thickness[Bed] * (1.0F + .25F * Noise(Y, Bed * 9100.0F, 4700.0F));
        Outcrop += BedHeight * Smooth((Depth - Thresholds[Bed] - Dip) / Widths[Bed]);
    }
    const float Interior = Smooth(Depth / .07F) * (1.0F - Smooth((Depth - .92F) / .08F));
    const float Blocks = 450.0F + 350.0F * Noise(Y + Depth * 1700.0F, Depth * 14000.0F, 2100.0F)
        + 180.0F * Noise(Y + 5000.0F, Depth * 18000.0F, 650.0F);
    return Outcrop + Interior * Blocks;
}
}

float APacificCoastGraybox::GetCliffEdgeX(const float Y)
{
    // Several non-repeating spatial frequencies produce coves and projecting headlands.
    return 20000.0F + 3800.0F * CoastalTerrain::Noise(Y, 12730.0F, 7200.0F)
        + 1600.0F * CoastalTerrain::Noise(Y, -7300.0F, 1900.0F)
        + 2600.0F * FMath::Sin(Y / 14500.0F);
}

float APacificCoastGraybox::GetCoastalLandHeight(const float X, const float Y)
{
    using namespace CoastalTerrain;
    // The complete ridge must descend to the submerged perimeter over a broad shoulder.
    // A 380–400 m collar compressed a ~350 m elevation change into a steep artificial end wall.
    const float Interior = Smooth(FMath::Min((X - West) / 65000.0F, (North - FMath::Abs(Y)) / 65000.0F));
    const float RouteDistance = FMath::Max(FMath::Max(-X - 23000.0F, FMath::Abs(Y) - 17000.0F), 0.0F);
    const float Rise = Smooth(RouteDistance / 24000.0F);
    // Interprets Sierra Azul / Mount Umunhum's connected ridge-and-ravine structure at slice scale.
    // These are compositional controls, not surveyed coordinates or a geographic reconstruction.
    const FRidgeNode Backbone[] = {
        {FVector2D(-183000, -162000), 9500, 56000},
        {FVector2D(-158000, -82000), 17800, 52000},
        {FVector2D(-139000, -24000), 25000, 55000},
        {FVector2D(-132000, 35000), 17400, 49000},
        {FVector2D(-119000, 112000), 12600, 54000},
        {FVector2D(-131000, 176000), 6500, 48000}
    };
    const FRidgeNode Spurs[][3] = {
        {{FVector2D(-153000,-65000),19400,34000}, {FVector2D(-106000,-99000),10900,30000}, {FVector2D(-59000,-127000),2600,23000}},
        {{FVector2D(-139000,-24000),25000,34000}, {FVector2D(-103000,-42000),14800,27000}, {FVector2D(-58000,-62000),2900,22000}},
        {{FVector2D(-132000,35000),17400,33000}, {FVector2D(-96000,71000),10600,28000}, {FVector2D(-58000,117000),3000,25000}}
    };
    const FVector2D Point(X + 1800.0F * Noise(X, Y, 43000.0F), Y + 2400.0F * Noise(X + 13000.0F, Y, 51000.0F));
    float Mountain = 0;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Backbone) - 1; ++Index)
        Mountain = JoinRidge(Mountain, RidgeSegment(Point, Backbone[Index], Backbone[Index + 1]));
    for (const auto& Branch : Spurs)
        for (int32 Index = 0; Index < 2; ++Index)
            Mountain = JoinRidge(Mountain, RidgeSegment(Point, Branch[Index], Branch[Index + 1]));
    const float MountainWeight = Smooth(Mountain / 7000.0F);
    // Low-amplitude coherent surface relief leaves the designed drainage hierarchy readable.
    Mountain += MountainWeight * (240.0F * Noise(X, Y, 16000.0F) + 70.0F * Noise(X, Y, 6300.0F));
    const float Hills = 650.0F + 700.0F * Noise(X, Y, 26000.0F) + 100.0F * Noise(X, Y, 7500.0F);
    const float LandHeight = FMath::Lerp(SubmergedRim, Rise * FMath::Max(0.0F, Hills + Mountain), Interior);
    // A shallow seasonal swale gives the short woodland bridge an actual crossing to span.
    // Its 16 m bank-to-bank width spans several near-field samples; both bridge landings remain at grade.
    return LandHeight - 180.0F * ChannelWeight(X, Y);
}

float APacificCoastGraybox::GetCoastalShoreX(float Y)
{
    using namespace CoastalTerrain;
    Y = FMath::Clamp(Y, -North, North);
    const float Magnitude = FMath::Abs(Y);
    const float T = FMath::Sign(Y) * (Magnitude <= 60000 ? Magnitude / 100000.0F : .6F + (Magnitude - 60000.0F) / 400000.0F);
    const int32 Row = FMath::Clamp(FMath::FloorToInt((T + 1.0F) * .5F * (CoastRows - 1)), 0, CoastRows - 2);
    const float YA = CoastY(Row), YB = CoastY(Row + 1);
    const float EdgeA = GetCliffEdgeX(YA), EdgeB = GetCliffEdgeX(YB);
    const float TopA = GetCoastalLandHeight(EdgeA, YA), TopB = GetCoastalLandHeight(EdgeB, YB);
    const float Alpha = (Y - YA) / (YB - YA);
    const float Top = FMath::Lerp(TopA, TopB, Alpha);
    const float TargetDepth = FMath::Clamp((Top - SeaLevelCm) / (Top - CliffFoot), 0.0F, 1.0F);
    const int32 CentralStep = FMath::Clamp(FMath::FloorToInt(TargetDepth * CliffDepthSteps), 0, CliffDepthSteps - 1);
    auto Point = [](float Edge, float SampleY, float SampleTop, int32 Step)
    {
        const float D = Step / static_cast<float>(CliffDepthSteps);
        return FVector(Edge + CliffOutcrop(SampleY, D), SampleY, FMath::Lerp(SampleTop, CliffFoot, D));
    };
    // Intersect the actual two triangles of the sampled cliff rather than an analytic approximation.
    for (int32 Step = FMath::Max(0, CentralStep - 2); Step <= FMath::Min(CliffDepthSteps - 1, CentralStep + 2); ++Step)
    {
        const FVector A = Point(EdgeA, YA, TopA, Step), B = Point(EdgeA, YA, TopA, Step + 1);
        const FVector C = Point(EdgeB, YB, TopB, Step), D = Point(EdgeB, YB, TopB, Step + 1);
        const FVector Faces[][3] = {{A,C,B}, {B,C,D}};
        for (const auto& Face : Faces)
        {
            TArray<FVector, TInlineAllocator<3>> Crossings;
            for (int32 Edge = 0; Edge < 3; ++Edge)
            {
                const FVector P = Face[Edge], Q = Face[(Edge + 1) % 3];
                if ((P.Z - SeaLevelCm) * (Q.Z - SeaLevelCm) > 0 || FMath::IsNearlyEqual(P.Z, Q.Z)) continue;
                Crossings.Add(FMath::Lerp(P, Q, (SeaLevelCm - P.Z) / (Q.Z - P.Z)));
            }
            if (Crossings.Num() < 2) continue;
            const FVector P = Crossings[0], Q = Crossings[1];
            if (Y < FMath::Min(P.Y,Q.Y) - .01F || Y > FMath::Max(P.Y,Q.Y) + .01F || FMath::IsNearlyEqual(P.Y,Q.Y)) continue;
            return FMath::Lerp(P.X, Q.X, (Y - P.Y) / (Q.Y - P.Y));
        }
    }
    return FMath::Lerp(EdgeA, EdgeB, Alpha) + CliffOutcrop(Y, TargetDepth);
}

float APacificCoastGraybox::GetCoastalVegetationCover(const float X, const float Y)
{
    using namespace CoastalTerrain;
    if (!FMath::IsFinite(X) || !FMath::IsFinite(Y) || X < West || FMath::Abs(Y) > North || X > GetCliffEdgeX(Y)) return 0;
    const float Height = GetCoastalLandHeight(X, Y);
    if (Height < SeaLevelCm + 300.0F) return 0;
    constexpr float SampleDistance = 6000.0F;
    const float WestHeight = GetCoastalLandHeight(X - SampleDistance, Y);
    const float EastHeight = GetCoastalLandHeight(X + SampleDistance, Y);
    const float SouthHeight = GetCoastalLandHeight(X, Y - SampleDistance);
    const float NorthHeight = GetCoastalLandHeight(X, Y + SampleDistance);
    const float DX = (EastHeight - WestHeight) / (2.0F * SampleDistance);
    const float DY = (NorthHeight - SouthHeight) / (2.0F * SampleDistance);
    const float Grade = FMath::Sqrt(DX * DX + DY * DY);
    // Positive concavity marks drainage/tributary floors rather than arbitrary horizontal height bands.
    const float Concavity = .25F * (WestHeight + EastHeight + SouthHeight + NorthHeight) - Height;
    const float Drainage = Smooth((Concavity + 40.0F) / 460.0F);
    // In this northern-hemisphere-inspired setting north-facing shoulders are less exposed to midday sun.
    const float Shade = FMath::Clamp(.5F - .5F * DY / FMath::Max(Grade, .06F), 0.0F, 1.0F);
    const float RockyExposure = Smooth((Grade - .55F) / .55F) * .22F;
    const float SummitExposure = Smooth((Height - 21000.0F) / 6000.0F) * .15F;
    float Cover = .12F + .44F * Shade + .34F * Drainage - RockyExposure - SummitExposure;
    Cover += .07F * Noise(X, Y, 12000.0F);
    // Protect the actual takeoff/route corridor; its edges blend into the surrounding woodland.
    const float FlightClearance = FMath::Max(FMath::Max(-X - 8000.0F, X - 12500.0F), FMath::Abs(Y) - 5500.0F);
    Cover *= Smooth(FlightClearance / 4500.0F);
    const FVector2D Bridge = BridgeCoordinates(X, Y);
    const float BridgeClearance = FMath::Max(FMath::Abs(Bridge.X) - 2100.0F, FMath::Abs(Bridge.Y) - 300.0F);
    Cover *= Smooth(BridgeClearance / 350.0F);
    return FMath::Clamp(Cover, 0.0F, 1.0F);
}

void APacificCoastGraybox::BuildCoastalTerrain(UMaterialInterface* Material)
{
    using namespace CoastalTerrain;
    constexpr int32 Rows = CoastRows;
    constexpr int32 Columns = 193;
    TArray<FVector> Vertices;
    TArray<FVector2D> UV;
    TArray<FLinearColor> CoverColors;
    TArray<int32> Triangles[5];
    auto Vertex = [&Vertices, &UV, &CoverColors](FVector Position)
    {
        UV.Add(FVector2D(Position.X / 4000.0F, Position.Y / 4000.0F));
        const float Cover=APacificCoastGraybox::GetCoastalVegetationCover(Position.X,Position.Y);
        const FLinearColor Vegetated = FMath::Lerp(FLinearColor(1.08F,.86F,.55F),FLinearColor(.25F,.48F,.31F),Cover);
        const float ExposedBed = .9F * ChannelWeight(Position.X, Position.Y);
        CoverColors.Add(FMath::Lerp(Vegetated, FLinearColor(.95F,.50F,.23F), ExposedBed));
        return Vertices.Add(Position);
    };
    auto Triangle = [&Triangles](int32 A, int32 B, int32 C, int32 Group)
    {
        Triangles[Group].Append({A, B, C});
    };
    for (int32 Row = 0; Row < Rows; ++Row)
    {
        const float Y = CoastY(Row);
        const float Edge = GetCliffEdgeX(Y);
        for (int32 Column = 0; Column < Columns; ++Column)
        {
            const float U = Column / 192.0F;
            const float X = U <= .6F ? FMath::Lerp(West, -40000.0F, U / .6F) : FMath::Lerp(-40000.0F, Edge, (U - .6F) / .4F);
            Vertex(FVector(X, Y, GetCoastalLandHeight(X, Y)));
        }
    }
    auto LandTriangle = [&Vertices, &Triangle](int32 A, int32 B, int32 C)
    {
        const FVector Normal = FVector::CrossProduct(Vertices[C] - Vertices[A], Vertices[B] - Vertices[A]).GetSafeNormal();
        const FVector Center = (Vertices[A] + Vertices[B] + Vertices[C]) / 3.0F;
        // Surface zones follow localized slope/exposure rather than drawing a height contour across the range.
        const int32 Group = Normal.Z < .80F ? 1 :
            (Normal.Z < .91F && Noise(Center.X, Center.Y, 29000.0F) > .12F ? 2 : 0);
        Triangle(A, B, C, Group);
    };
    for (int32 Row = 0; Row < Rows - 1; ++Row)
        for (int32 Column = 0; Column < Columns - 1; ++Column)
        {
            const int32 A = Row * Columns + Column, B = A + 1, C = A + Columns, D = C + 1;
            LandTriangle(A, C, B); LandTriangle(B, C, D);
        }

    // Cliff shares the exact last land column; fractures displace the rock face, not the seam.
    TArray<int32> Cliff;
    constexpr int32 DepthSteps = CliffDepthSteps;
    for (int32 Row = 0; Row < Rows; ++Row)
    {
        const int32 Rim = Row * Columns + Columns - 1;
        Cliff.Add(Rim);
        const FVector Top = Vertices[Rim];
        for (int32 Depth = 1; Depth <= DepthSteps; ++Depth)
        {
            const float D = Depth / static_cast<float>(DepthSteps);
            const float Outcrop = CliffOutcrop(Top.Y, D);
            Cliff.Add(Vertex(FVector(Top.X + Outcrop, Top.Y, FMath::Lerp(static_cast<float>(Top.Z), CliffFoot, D))));
        }
    }
    for (int32 Row = 0; Row < Rows - 1; ++Row)
        for (int32 Depth = 0; Depth < DepthSteps; ++Depth)
        {
            const int32 A = Cliff[Row * (DepthSteps + 1) + Depth];
            const int32 B = Cliff[Row * (DepthSteps + 1) + Depth + 1];
            const int32 C = Cliff[(Row + 1) * (DepthSteps + 1) + Depth];
            const int32 D = Cliff[(Row + 1) * (DepthSteps + 1) + Depth + 1];
            const int32 Group = Noise(Vertices[A].Y, Vertices[A].Z * 1.4F, 3700.0F) > .12F ? 4 : 3;
            Triangle(A, C, B, Group); Triangle(B, C, D, Group);
        }

    // Ordered counter-clockwise perimeter viewed from above; every boundary has bedrock underneath.
    TArray<int32> Boundary;
    for (int32 Column = 0; Column < Columns; ++Column) Boundary.Add(Column);
    for (int32 Depth = 1; Depth <= DepthSteps; ++Depth) Boundary.Add(Cliff[Depth]);
    for (int32 Row = 1; Row < Rows; ++Row) Boundary.Add(Cliff[Row * (DepthSteps + 1) + DepthSteps]);
    for (int32 Depth = DepthSteps - 1; Depth >= 0; --Depth) Boundary.Add(Cliff[(Rows - 1) * (DepthSteps + 1) + Depth]);
    for (int32 Column = Columns - 2; Column >= 0; --Column) Boundary.Add((Rows - 1) * Columns + Column);
    for (int32 Row = Rows - 2; Row > 0; --Row) Boundary.Add(Row * Columns);
    TArray<int32> Bottom;
    for (int32 Rim : Boundary) Bottom.Add(Vertex(FVector(Vertices[Rim].X, Vertices[Rim].Y, BedrockBase)));
    const int32 Center = Vertex(FVector(-110000.0F, 0.0F, BedrockBase));
    for (int32 Index = 0; Index < Boundary.Num(); ++Index)
    {
        const int32 Next = (Index + 1) % Boundary.Num();
        Triangle(Boundary[Index], Boundary[Next], Bottom[Index], 3);
        Triangle(Boundary[Next], Bottom[Next], Bottom[Index], 3);
        Triangle(Center, Bottom[Index], Bottom[Next], 3);
    }
    TArray<FVector> Normals;
    Normals.Init(FVector::ZeroVector, Vertices.Num());
    for (const TArray<int32>& Group : Triangles)
        for (int32 Index = 0; Index < Group.Num(); Index += 3)
        {
            const int32 A = Group[Index], B = Group[Index + 1], C = Group[Index + 2];
            const FVector N = FVector::CrossProduct(Vertices[C] - Vertices[A], Vertices[B] - Vertices[A]);
            Normals[A] += N; Normals[B] += N; Normals[C] += N;
        }
    for (FVector& Normal : Normals) Normal.Normalize();
    const TCHAR* Names[] = {TEXT("CoastalPlateau"), TEXT("CoastalMountainRock"), TEXT("CoastalMountainScrub"), TEXT("CoastalBedrock"), TEXT("CoastalFracturedRock")};
    const FLinearColor Colors[] = {FLinearColor(.115F,.155F,.065F), FLinearColor(.13F,.153F,.087F), FLinearColor(.12F,.155F,.075F), FLinearColor(.19F,.175F,.14F), FLinearColor(.215F,.20F,.165F)};
    for (int32 Group = 0; Group < 5; ++Group)
    {
        UProceduralMeshComponent* Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(Names[Group]);
        Mesh->SetupAttachment(Root);
        Mesh->bUseAsyncCooking = true;
        Mesh->SetCollisionProfileName(TEXT("BlockAll"));
        Mesh->CreateMeshSection_LinearColor(0, Vertices, Triangles[Group], Normals, UV, CoverColors, TArray<FProcMeshTangent>(), true);
        Mesh->SetMaterial(0, Material);
        Mesh->SetCastShadow(true);
        CoastalColorParts.Add(Mesh); CoastalPartColors.Add(Colors[Group]);
        ++GeneratedPrimitiveCount;
    }

    // Angular fallen buttresses intersect the supported cliff foot, with submerged bases.
    // Unequal spacing, rotation and proportions avoid the former row of identical cubes.
    TArray<FVector> TalusVertices, TalusNormals;
    TArray<int32> TalusIndices;
    FRandomStream RockRandom(914823);
    for (int32 Rock = 0; Rock < 32; ++Rock)
    {
        const float Y = RockRandom.FRandRange(-57000.0F, 57000.0F);
        const float Edge = GetCliffEdgeX(Y);
        const float Radius = RockRandom.FRandRange(650.0F, 1850.0F);
        const float Peak = RockRandom.FRandRange(-7500.0F, -3900.0F);
        const int32 BaseIndex = TalusVertices.Num();
        const float Rotation = RockRandom.FRandRange(0.0F, 2.0F * PI);
        TalusVertices.Add(FVector(Edge - 300.0F, Y, -15500.0F));
        for (int32 Ring = 0; Ring < 2; ++Ring)
            for (int32 Corner = 0; Corner < 7; ++Corner)
            {
                const float Angle = Rotation + Corner * (2.0F * PI / 7.0F);
                const float R = Radius * RockRandom.FRandRange(.65F, 1.15F) * (Ring == 0 ? 1.0F : .72F);
                TalusVertices.Add(FVector(Edge + 150.0F + FMath::Cos(Angle) * R, Y + FMath::Sin(Angle) * R * 1.35F,
                    Ring == 0 ? -11100.0F : Peak - RockRandom.FRandRange(900.0F, 1800.0F)));
            }
        TalusVertices.Add(FVector(Edge + RockRandom.FRandRange(-100.0F, 500.0F), Y + Radius * .2F, Peak));
        for (int32 Corner = 0; Corner < 7; ++Corner)
        {
            const int32 Next = (Corner + 1) % 7;
            const int32 A = BaseIndex + 1 + Corner, B = BaseIndex + 1 + Next;
            const int32 C = A + 7, D = B + 7;
            TalusIndices.Append({BaseIndex, A, B, A, C, B, B, C, D, C, BaseIndex + 15, D});
        }
    }
    TalusNormals.Init(FVector::ZeroVector, TalusVertices.Num());
    for (int32 Index = 0; Index < TalusIndices.Num(); Index += 3)
    {
        const int32 A = TalusIndices[Index], B = TalusIndices[Index + 1], C = TalusIndices[Index + 2];
        const FVector N = FVector::CrossProduct(TalusVertices[C] - TalusVertices[A], TalusVertices[B] - TalusVertices[A]);
        TalusNormals[A] += N; TalusNormals[B] += N; TalusNormals[C] += N;
    }
    for (FVector& Normal : TalusNormals) Normal.Normalize();
    UProceduralMeshComponent* Talus = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("CoastalTalusButtresses"));
    Talus->SetupAttachment(Root); Talus->bUseAsyncCooking = true;
    Talus->SetCollisionProfileName(TEXT("BlockAll"));
    Talus->CreateMeshSection_LinearColor(0, TalusVertices, TalusIndices, TalusNormals, TArray<FVector2D>(), TArray<FLinearColor>(), TArray<FProcMeshTangent>(), true);
    Talus->SetMaterial(0, Material);
    CoastalColorParts.Add(Talus); CoastalPartColors.Add(FLinearColor(.18F,.18F,.15F));
    ++GeneratedPrimitiveCount;
}
