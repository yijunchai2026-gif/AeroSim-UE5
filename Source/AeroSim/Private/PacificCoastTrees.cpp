#include "PacificCoastGraybox.h"
#include "PacificCoastFlora.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshResources.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/Package.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

// Bay Area coastal forest: Douglas-fir and Monterey pine stands with coast live oak
// patches, built as a small library of instanced archetypes instead of unique merged
// geometry. Each archetype is a four-LOD static mesh generated once per process.
namespace CoastFlora
{
namespace
{
constexpr int32 LodCount = 4;
constexpr float LodScreenSize[LodCount] = {1.0F, .75F, .36F, .12F};

float Smooth01(float Value) { Value = FMath::Clamp(Value, 0.0F, 1.0F); return Value * Value * (3.0F - 2.0F * Value); }
float Hash2(float X, float Y) { return FMath::Frac(FMath::Sin(X * 127.1F + Y * 311.7F) * 43758.5453F); }
float ValueNoise(float X, float Y)
{
    const float IX = FMath::FloorToFloat(X), IY = FMath::FloorToFloat(Y);
    const float FX = Smooth01(X - IX), FY = Smooth01(Y - IY);
    return FMath::Lerp(FMath::Lerp(Hash2(IX, IY), Hash2(IX + 1, IY), FX), FMath::Lerp(Hash2(IX, IY + 1), Hash2(IX + 1, IY + 1), FX), FY);
}
float Fbm(float X, float Y, float Scale)
{
    X /= Scale; Y /= Scale;
    return .55F * ValueNoise(X, Y) + .30F * ValueNoise(X * 2.03F + 17.0F, Y * 2.03F - 9.0F) + .15F * ValueNoise(X * 4.1F - 31.0F, Y * 4.1F + 5.0F);
}

// ---------------------------------------------------------------------------------
// Geometry assembly
// ---------------------------------------------------------------------------------
struct FSection
{
    TArray<FVector3f> P, N, T;
    TArray<float> Sign;
    TArray<FVector2f> UV;
    TArray<FVector4f> C;
    TArray<int32> I;
    int32 Add(const FVector& Position, const FVector& Normal, const FVector& Tangent, const FVector& Bitangent, const FVector2f& Coord, const FVector4f& Color)
    {
        const FVector Nn = Normal.GetSafeNormal(), Tn = (Tangent - Nn * FVector::DotProduct(Tangent, Nn)).GetSafeNormal();
        P.Add(FVector3f(Position)); N.Add(FVector3f(Nn)); T.Add(FVector3f(Tn.IsNearlyZero() ? FVector::ForwardVector : Tn));
        Sign.Add(FVector::DotProduct(FVector::CrossProduct(Nn, Tn), Bitangent) < 0 ? -1.0F : 1.0F);
        UV.Add(Coord); C.Add(Color);
        return P.Num() - 1;
    }
    // Unreal's clockwise convention: Cross(C-A, B-A) agrees with the shading normal.
    void Tri(int32 A, int32 B, int32 Cc)
    {
        const FVector3f Geometric = FVector3f::CrossProduct(P[Cc] - P[A], P[B] - P[A]);
        if (Geometric.SizeSquared() < 1e-6F) return;
        if (FVector3f::DotProduct(Geometric, N[A] + N[B] + N[Cc]) < 0) I.Append({A, Cc, B});
        else I.Append({A, B, Cc});
    }
};
struct FLod { FSection Bark, Leaf; };

// Cooked fast-built meshes do not execute the Editor-only UV-density builder.
// Derive channel 0 from the same LOD0 triangles that will be rendered, in cm/UV.
// Match UE 5.8's sqrt(area) weighting and 10% low/high density trimming without
// depending on Engine/Private/Streaming/UVChannelDensity.h or Editor-only data.
TOptional<FMeshUVChannelInfo> StreamingUVInfo(const FSection& Section)
{
    if (Section.UV.Num() != Section.P.Num() || Section.I.Num() % 3 != 0) return {};
    struct FSample { float Density, Weight; };
    TArray<FSample> Samples; Samples.Reserve(Section.I.Num() / 3);
    for (int32 I = 0; I < Section.I.Num(); I += 3)
    {
        const int32 A = Section.I[I], B = Section.I[I + 1], C = Section.I[I + 2];
        if (!Section.P.IsValidIndex(A) || !Section.P.IsValidIndex(B) || !Section.P.IsValidIndex(C)) return {};
        if (Section.P[A].ContainsNaN() || Section.P[B].ContainsNaN() || Section.P[C].ContainsNaN()
            || Section.UV[A].ContainsNaN() || Section.UV[B].ContainsNaN() || Section.UV[C].ContainsNaN()) return {};
        const float Area = FVector3f::CrossProduct(Section.P[B] - Section.P[A], Section.P[C] - Section.P[A]).Size();
        const FVector2f AB = Section.UV[B] - Section.UV[A], AC = Section.UV[C] - Section.UV[A];
        const float UVArea = FMath::Abs(AB.X * AC.Y - AB.Y * AC.X);
        if (!FMath::IsFinite(Area) || !FMath::IsFinite(UVArea)) return {};
        if (Area <= UE_SMALL_NUMBER || UVArea <= UE_SMALL_NUMBER) continue;
        const float Density = FMath::Sqrt(Area / UVArea), Weight = FMath::Sqrt(Area);
        if (!FMath::IsFinite(Density) || Density <= 0 || !FMath::IsFinite(Weight)) return {};
        Samples.Add({Density, Weight});
    }
    if (Samples.IsEmpty()) return {};
    Samples.Sort([](const FSample& A, const FSample& B) { return A.Density < B.Density; });
    const int32 Trim = FMath::FloorToInt(.10F * Samples.Num());
    double WeightedDensity = 0, Weight = 0;
    for (int32 I = Trim; I < Samples.Num() - Trim; ++I)
    {
        WeightedDensity += static_cast<double>(Samples[I].Density) * Samples[I].Weight;
        Weight += Samples[I].Weight;
    }
    if (Weight <= UE_SMALL_NUMBER || !FMath::IsFinite(WeightedDensity / Weight)) return {};
    FMeshUVChannelInfo Result(0.0F);
    Result.LocalUVDensities[0] = static_cast<float>(WeightedDensity / Weight);
    return Result;
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalRuntimeUVDensityMathTest, "AeroSim.Coastal.Flora.RuntimeUVChannelDensityMath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCoastalRuntimeUVDensityMathTest::RunTest(const FString& Parameters)
{
    FSection Section;
    Section.P = {FVector3f(0,0,0), FVector3f(200,0,0), FVector3f(0,200,0)};
    Section.UV = {FVector2f(0,0), FVector2f(1,0), FVector2f(0,1)}; Section.I = {0,1,2};
    auto Info = StreamingUVInfo(Section);
    TestTrue(TEXT("A 2m triangle mapped to one UV unit measures 200cm/UV"), Info.IsSet() && FMath::IsNearlyEqual(Info->LocalUVDensities[0], 200.0F));
    for (FVector2f& UV : Section.UV) UV *= 2;
    Info = StreamingUVInfo(Section);
    TestTrue(TEXT("Twice the UV tiling halves the world-space density"), Info.IsSet() && FMath::IsNearlyEqual(Info->LocalUVDensities[0], 100.0F));
    for (FVector3f& P : Section.P) P *= .5F;
    Info = StreamingUVInfo(Section);
    TestTrue(TEXT("Half-size geometry halves the world-space density"), Info.IsSet() && FMath::IsNearlyEqual(Info->LocalUVDensities[0], 50.0F));
    Section.UV[1] = Section.UV[0]; Section.UV[2] = Section.UV[0];
    TestFalse(TEXT("A section with only collapsed UVs cannot publish fabricated density"), StreamingUVInfo(Section).IsSet());
    Section.I[2] = 17;
    TestFalse(TEXT("Invalid triangle references fail closed"), StreamingUVInfo(Section).IsSet());
    Section.I[2] = 2; Section.UV = {FVector2f(0,0), FVector2f(1,0), FVector2f(0,1)};
    Section.P[1] = Section.P[0]; Section.P[2] = Section.P[0];
    TestFalse(TEXT("A section with only collapsed world triangles cannot publish density"), StreamingUVInfo(Section).IsSet());
    return true;
}
#endif

/** Generalised swept tube. Ring vertices duplicate the seam so U wraps cleanly. */
void Sweep(FSection& S, const TArray<FVector>& Path, TFunctionRef<float(int32 Ring, float Theta)> Radius,
    int32 Sides, float URepeats, float TileV, TFunctionRef<FVector4f(int32 Ring)> Color, float VStart = 0)
{
    const int32 Rings = Path.Num();
    if (Rings < 2) return;
    TArray<FVector> Axis; Axis.SetNum(Rings);
    for (int32 R = 0; R < Rings; ++R)
        Axis[R] = (Path[FMath::Min(R + 1, Rings - 1)] - Path[FMath::Max(R - 1, 0)]).GetSafeNormal();
    FVector U, V; Axis[0].FindBestAxisVectors(U, V);
    TArray<TArray<FVector>> Grid; Grid.SetNum(Rings);
    TArray<FVector> Us, Vs; Us.SetNum(Rings); Vs.SetNum(Rings);
    for (int32 R = 0; R < Rings; ++R)
    {
        if (R > 0)
        {   // Parallel transport keeps the frame from twisting along bent limbs.
            const FQuat Q = FQuat::FindBetweenNormals(Axis[R - 1], Axis[R]);
            U = Q.RotateVector(U); V = Q.RotateVector(V);
        }
        Us[R] = U; Vs[R] = V;
        Grid[R].SetNum(Sides + 1);
        for (int32 Side = 0; Side <= Sides; ++Side)
        {
            const float Theta = 2.0F * PI * (Side % Sides) / Sides;
            Grid[R][Side] = Path[R] + (U * FMath::Cos(Theta) + V * FMath::Sin(Theta)) * Radius(R, Theta);
        }
    }
    float Along = VStart;
    const int32 Base = S.P.Num();
    for (int32 R = 0; R < Rings; ++R)
    {
        if (R > 0) Along += FVector::Distance(Path[R], Path[R - 1]);
        for (int32 Side = 0; Side <= Sides; ++Side)
        {
            const int32 S0 = (Side + Sides - 1) % Sides, S1 = (Side + 1) % Sides;
            const FVector Around = Grid[R][S1] - Grid[R][S0];
            const FVector Up = Grid[FMath::Min(R + 1, Rings - 1)][Side % Sides] - Grid[FMath::Max(R - 1, 0)][Side % Sides];
            const float Theta = 2.0F * PI * (Side % Sides) / Sides;
            const FVector Radial = Us[R] * FMath::Cos(Theta) + Vs[R] * FMath::Sin(Theta);
            FVector Normal = FVector::CrossProduct(Up, Around).GetSafeNormal();
            if (FVector::DotProduct(Normal, Radial) < 0) Normal = -Normal;
            if (Normal.IsNearlyZero()) Normal = Radial;
            S.Add(Grid[R][Side], Normal, Around, Up, FVector2f(URepeats * Side / static_cast<float>(Sides), Along / TileV), Color(R));
        }
    }
    for (int32 R = 0; R + 1 < Rings; ++R)
        for (int32 Side = 0; Side < Sides; ++Side)
        {
            const int32 A = Base + R * (Sides + 1) + Side, B = A + 1, Cc = A + Sides + 1, D = Cc + 1;
            S.Tri(A, B, Cc); S.Tri(B, D, Cc);
        }
}

struct FIsland { FVector2f Min, Max; bool bAlongU; bool bCentered = false; };
// Atlas regions measured from the ART-FLORA alpha maps (isolated foliage only).
// T_PineClumpCards (24-28 real pine_tree_01 sprigs per cell): radial clump, tip fan, spray, sapling column.
const FIsland PineSprigs[] = {{FVector2f(.0F, .0F), FVector2f(.5F, .5F), false, true}, {FVector2f(.5F, .0F), FVector2f(1.0F, .5F), false},
    {FVector2f(.0F, .5F), FVector2f(.5F, 1.0F), true}, {FVector2f(.5F, .5F), FVector2f(1.0F, 1.0F), false}};
// LeafSet019 dense conifer sprays (base at the left, tip at the right).
const FIsland FirSprays[] = {{FVector2f(.277F, .055F), FVector2f(.723F, .277F), true}, {FVector2f(.070F, .305F), FVector2f(.906F, .617F), true},
    {FVector2f(.031F, .633F), FVector2f(.938F, .984F), true}};
// ART-FLORA T_LiveOakTwigCards (LeafSet024 leaves, 3-6 cm, on twigs): A terminal cluster,
// B twig spray, C dense rosette (centred, seen from below/above), D drooping twig. Each cell = 40 cm.
const FIsland OakRows[] = {{FVector2f(.0F, .0F), FVector2f(.5F, .5F), false}, {FVector2f(.5F, .0F), FVector2f(1.0F, .5F), false},
    {FVector2f(.0F, .5F), FVector2f(.5F, 1.0F), false, true}, {FVector2f(.5F, .5F), FVector2f(1.0F, 1.0F), false}};

/** A single two-sided card, base at Origin, extending along Along. Normal is bent outward from the crown. */
void Card(FSection& S, const FVector& Origin, const FVector& Along, const FVector& Across, float Length, const FIsland& Island,
    const FVector& Outward, const FVector4f& Color)
{
    const FVector2f Extent = Island.Max - Island.Min;
    const float Aspect = Island.bAlongU ? Extent.X / Extent.Y : Extent.Y / Extent.X;
    const float Width = Length / FMath::Max(Aspect, .25F);
    const FVector A = Along.GetSafeNormal(), W = (Across - A * FVector::DotProduct(Across, A)).GetSafeNormal();
    FVector Face = FVector::CrossProduct(W, A).GetSafeNormal();
    if (FVector::DotProduct(Face, Outward) < 0) Face = -Face;
    // Spherical crown normals (bent toward the crown-centre direction) so a spray is not one flat lit plane.
    const FVector Normal = (Face * .25F + Outward.GetSafeNormal() * .75F).GetSafeNormal();
    auto Coord = [&Island](float AlongT, float AcrossT)
    {
        return Island.bAlongU ? FVector2f(FMath::Lerp(Island.Min.X, Island.Max.X, AlongT), FMath::Lerp(Island.Min.Y, Island.Max.Y, AcrossT))
            : FVector2f(FMath::Lerp(Island.Min.X, Island.Max.X, AcrossT), FMath::Lerp(Island.Max.Y, Island.Min.Y, AlongT));
    };
    const FVector Base = Island.bCentered ? Origin - A * Length * .5F : Origin;
    const FVector Half = W * Width * .5F;
    const FVector Tangent = Island.bAlongU ? A : W, Bitangent = Island.bAlongU ? W : -A;
    const int32 V0 = S.Add(Base - Half, Normal, Tangent, Bitangent, Coord(0, 0), Color);
    const int32 V1 = S.Add(Base + Half, Normal, Tangent, Bitangent, Coord(0, 1), Color);
    const int32 V2 = S.Add(Base + Half + A * Length, Normal, Tangent, Bitangent, Coord(1, 1), Color);
    const int32 V3 = S.Add(Base - Half + A * Length, Normal, Tangent, Bitangent, Coord(1, 0), Color);
    S.Tri(V0, V1, V2); S.Tri(V0, V2, V3);
}

// ---------------------------------------------------------------------------------
// Skeletons
// ---------------------------------------------------------------------------------
struct FLimb { TArray<FVector> Path; float R0 = 1, R1 = .5F; int32 Level = 1; float Wind0 = 0, Wind1 = .5F; };
// Mass cards are large branch-spray cards that carry crown volume into the mid/far LODs.
struct FTuft { FVector Origin, Dir, Across, Outward; float Length; float Occlusion; float Wind; int32 Island; bool bMass = false;
    int32 Cluster = -1; int32 Rank = 0; float Fold = 0; };
/** R5 branchlet group: a connected foliage cluster with a stable id, its local envelope and lobe. */
struct FCluster { FVector Center = FVector::ZeroVector, Axis = FVector::ForwardVector, Up = FVector::UpVector; FVector Radii = FVector(30);
    int32 Lobe = 0; int32 Island = 0; float Wind = .5F, Occlusion = 1; int32 Cards = 0; };
struct FSkeleton
{
    ESpecies Species = ESpecies::DouglasFir;
    float Height = 0, Breast = 0, CrownBase = 0, CrownRadius = 0;
    TArray<FVector> Trunk;            // main stem spine, index 0 at z = -Buried
    TArray<float> TrunkRadius;
    TArray<FLimb> Limbs;
    TArray<FTuft> Tufts;
    TArray<FTrunkSegment> Collision;
    int32 Stems = 1;
    bool bHeroShape = false;
    /** R5: foliage rebuilt as connected branchlet clusters with cluster-stable LODs. */
    bool bHeroFoliage = false;
    /** <1 thins branchlet groups so LOD0 foliage stays within the archetype's R4 slot budget. */
    float FoliageDensity = 1.0F;
    TArray<FCluster> Clusters;
    float TileV = 200, TileU = 200;
    const FIsland* Islands = PineSprigs;
    int32 IslandCount = 2;
};
constexpr float Buried = 60.0F;

FVector Dir(float Yaw, float Pitch) { return FVector(FMath::Cos(Pitch) * FMath::Cos(Yaw), FMath::Cos(Pitch) * FMath::Sin(Yaw), FMath::Sin(Pitch)); }

/** Ring heights for a stem: dense through the collar, then sparser up the bole. */
TArray<float> StemHeights(float Top)
{
    TArray<float> Z = {-Buried, 0, 7, 16, 27, 40, 56, 76, 100, 130};
    for (float H = 200; H < Top - 60; H += FMath::Clamp(Top / 12.0F, 120.0F, 260.0F)) Z.Add(H);
    Z.Add(Top);
    return Z;
}

/** Root collar: smooth flare plus 4-6 low buttress swells, capped near 1.3x breast radius. */
float CollarRadius(float Z, float Theta, float BaseRadius, const TArray<TPair<float, float>>& Lobes)
{
    const float Zc = FMath::Max(Z, 0.0F);
    // Concave root collar: sharp flare in the lowest ~30 cm, so the base reads as rooted, not a cylinder.
    float Flare = .18F * FMath::Exp(-Zc / 30.0F) + .03F * FMath::Exp(-Zc / 90.0F);
    float Buttress = 0;
    for (const auto& Lobe : Lobes) Buttress += Lobe.Value * FMath::Pow(FMath::Max(0.0F, FMath::Cos(Theta - Lobe.Key)), 6.0F);
    return BaseRadius * (1.0F + Flare + Buttress * FMath::Exp(-Zc / 45.0F));
}

void AddConiferBranches(FSkeleton& K, FRandomStream& Random, bool bPine, bool bSapling, float Wind0)
{
    const float H = K.Height;
    auto Spine = [&K](float Z) -> FVector
    {
        for (int32 I = 1; I < K.Trunk.Num(); ++I)
            if (K.Trunk[I].Z >= Z) return FMath::Lerp(K.Trunk[I - 1], K.Trunk[I], (Z - K.Trunk[I - 1].Z) / FMath::Max(1.0, K.Trunk[I].Z - K.Trunk[I - 1].Z));
        return K.Trunk.Last();
    };
    auto RadiusAt = [&K](float Z) -> float
    {
        for (int32 I = 1; I < K.Trunk.Num(); ++I)
            if (K.Trunk[I].Z >= Z) return FMath::Lerp(K.TrunkRadius[I - 1], K.TrunkRadius[I], (Z - K.Trunk[I - 1].Z) / FMath::Max(1.0, K.Trunk[I].Z - K.Trunk[I - 1].Z));
        return K.TrunkRadius.Last();
    };
    const float AsymYaw = Random.FRandRange(0, 2 * PI), Asym = Random.FRandRange(.12F, .32F);
    const float Lmax = bSapling ? H * .30F : bPine ? H * .30F : H * .20F;
    float Azimuth = Random.FRandRange(0, 2 * PI);
    // Self-pruned dead stubs below the live crown (common in dense stands).
    if (!bSapling)
        for (float Z = K.CrownBase * .45F; Z < K.CrownBase - 40; Z += Random.FRandRange(70, 160))
        {
            const FVector Start = Spine(Z);
            const float Yaw = Random.FRandRange(0, 2 * PI), L = Random.FRandRange(18, 55);
            FLimb Stub; Stub.Path = {Start, Start + Dir(Yaw, FMath::DegreesToRadians(-8)) * (RadiusAt(Z) + L)};
            Stub.R0 = FMath::Max(1.2F, RadiusAt(Z) * .10F); Stub.R1 = .8F; Stub.Level = 3; Stub.Wind0 = Stub.Wind1 = 0;
            K.Limbs.Add(Stub);
        }
    // Larger (veteran) crowns use proportionally larger, sparser sprays so LOD0 stays in budget.
    const float Big = FMath::Max(1.0F, H / 2900.0F);
    const float Step0 = (bSapling ? 34.0F : bPine ? 48.0F : 26.0F) * Big, Step1 = (bSapling ? 48.0F : bPine ? 80.0F : 40.0F) * Big;
    for (float Z = K.CrownBase; Z < H * .965F; Z += Random.FRandRange(Step0, Step1))
    {
        const float U = (Z - K.CrownBase) / FMath::Max(1.0F, H - K.CrownBase);
        const int32 Count = bSapling ? Random.RandRange(3, 5) : 1;
        for (int32 Whorl = 0; Whorl < Count; ++Whorl)
        {
            Azimuth += bSapling ? 2 * PI / Count + Random.FRandRange(-.35F, .35F) : 2.39996F + Random.FRandRange(-.85F, .85F);
            if (!bSapling && Random.FRand() < .13F) continue; // irregular crown windows
            float Envelope = bPine ? .50F + .50F * FMath::Sin(PI * FMath::Min(1.0F, .18F + U * 1.05F)) : FMath::Pow(1.0F - U, .88F);
            if (bPine && U > .80F) Envelope *= 1.0F - (U - .80F) * 2.2F; // broad, flattened top
            float L = Lmax * Envelope * Random.FRandRange(.62F, 1.18F) * (1.0F + Asym * FMath::Cos(Azimuth - AsymYaw)) + (bSapling ? 12 : 30);
            if (!bSapling && Random.FRand() < .12F) L *= .55F;
            const float Pitch = FMath::DegreesToRadians(bPine ? FMath::Lerp(8.0F, 48.0F, U) + Random.FRandRange(-10, 10)
                : FMath::Lerp(-24.0F, 12.0F, U) + Random.FRandRange(-8, 8));
            const FVector Out = Dir(Azimuth, 0), Base = Spine(Z) + Out * RadiusAt(Z) * .7F;
            FLimb Limb; Limb.Level = 1; Limb.Wind0 = Wind0 + .12F * U; Limb.Wind1 = FMath::Min(1.0F, Limb.Wind0 + L / 380.0F);
            const FVector Lateral = FVector::CrossProduct(FVector::UpVector, Out).GetSafeNormal() * Random.FRandRange(-.12F, .12F);
            for (int32 Step = 0; Step <= 4; ++Step)
            {
                const float S = Step / 4.0F;
                // Fir: droop then upturned tip. Pine: stout ascending limbs.
                const float Rise = bPine ? FMath::Sin(Pitch) * S - .05F * FMath::Sin(PI * S) : FMath::Sin(Pitch) * S - .11F * FMath::Sin(PI * S) + .10F * S * S * S;
                Limb.Path.Add(Base + (Out * FMath::Cos(Pitch * .6F) + Lateral * S) * L * S + FVector::UpVector * L * Rise);
            }
            Limb.R0 = FMath::Max(bSapling ? .7F : 1.3F, K.Breast * (bPine ? .26F : .17F) * FMath::Pow(L / Lmax, .9F));
            Limb.R1 = bSapling ? .3F : .6F;
            const int32 LimbIndex = K.Limbs.Add(Limb);
            const FVector Tip = Limb.Path.Last();
            auto PointAt = [&K, LimbIndex](float S) { const auto& Path = K.Limbs[LimbIndex].Path; const float F = S * (Path.Num() - 1); const int32 I0 = FMath::Min(FMath::FloorToInt(F), Path.Num() - 2); return FMath::Lerp(Path[I0], Path[I0 + 1], F - I0); };
            const FVector Center = Spine(FMath::Min(Z + L * .2F, H));
            const FVector CrownCentre(0, 0, K.CrownBase + (H - K.CrownBase) * .45F);
            auto Outward = [&CrownCentre](const FVector& P) { return (P - CrownCentre).GetSafeNormal(); };
            auto Occl = [&K, &Spine](const FVector& P) { const FVector Axis = Spine(P.Z); return FMath::Clamp(.35F + .65F * FVector::Dist2D(P, Axis) / FMath::Max(80.0F, K.CrownRadius), .35F, 1.0F); };
            if (bPine)
            {
                // Monterey pine: forked limbs ending in dense needle tufts (pom-poms).
                TArray<TPair<FVector, FVector>> Tips;
                Tips.Add({Tip, (Tip - PointAt(.8F)).GetSafeNormal()});
                const int32 Forks = bSapling ? 1 : Random.RandRange(2, 4);
                for (int32 Fork = 0; Fork < Forks; ++Fork)
                {
                    const float S = Random.FRandRange(.40F, .82F);
                    const FVector From = PointAt(S);
                    const float Yaw = Azimuth + Random.FRandRange(.45F, .9F) * (Fork % 2 ? 1 : -1);
                    const float FL = (1.0F - S) * L * Random.FRandRange(.55F, .85F) + 25;
                    FLimb Child; Child.Level = 2; Child.Wind0 = FMath::Lerp(Limb.Wind0, Limb.Wind1, S); Child.Wind1 = FMath::Min(1.0F, Child.Wind0 + FL / 300.0F);
                    const FVector D = Dir(Yaw, Pitch + FMath::DegreesToRadians(Random.FRandRange(5, 25)));
                    Child.Path = {From, From + D * FL * .5F + FVector(0, 0, FL * .06F), From + D * FL + FVector(0, 0, FL * .14F)};
                    Child.R0 = FMath::Max(.8F, Limb.R0 * .45F); Child.R1 = .5F;
                    Tips.Add({Child.Path.Last(), D});
                    K.Limbs.Add(Child);
                }
                for (const auto& TipPair : Tips)
                {
                    const int32 Needles = bSapling ? 2 : 3;
                    for (int32 Needle = 0; Needle < Needles; ++Needle)
                    {
                        // The radial clump (cell A) faces outward from the tip; fans and sprays radiate.
                        FVector D = Needle == 0 && !bSapling ? FVector::CrossProduct(TipPair.Value, Random.VRand()).GetSafeNormal()
                            : (TipPair.Value * .75F + Random.VRand() * .85F + FVector(0, 0, .35F)).GetSafeNormal();
                        const FVector Across = Needle == 0 && !bSapling ? FVector::CrossProduct(D, TipPair.Value) : FVector::CrossProduct(D, Random.VRand());
                        FTuft Tuft{TipPair.Key - TipPair.Value * 8.0F, D, Across, Outward(TipPair.Key),
                            Random.FRandRange(62, 86) * (bSapling ? .6F : 1.0F), Occl(TipPair.Key), Limb.Wind1, bSapling ? 3 : Needle};
                        K.Tufts.Add(Tuft);
                    }
                    // A second, smaller tuft behind the tip thickens each foliage clump.
                    for (int32 Needle = 0; Needle < (bSapling ? 0 : 2); ++Needle)
                    {
                        const FVector At = TipPair.Key - TipPair.Value * Random.FRandRange(30, 55);
                        FVector D = (Random.VRand() + FVector(0, 0, .5F)).GetSafeNormal();
                        K.Tufts.Add({At, D, FVector::CrossProduct(D, TipPair.Value), Outward(At), Random.FRandRange(55, 75), Occl(At) * .9F, Limb.Wind1 * .8F, Random.RandRange(1, 2)});
                    }
                    // Crossed clump cards give each tuft canopy mass at distance.
                    for (int32 Mass = 0; Mass < (bSapling ? 1 : 3); ++Mass)
                    {
                        const FVector D = (TipPair.Value * .5F + Dir(Mass * 2.09F + Random.FRandRange(-.4F, .4F), .35F)).GetSafeNormal();
                        FTuft Clump{TipPair.Key - D * 45.0F, D, Dir(Mass * 2.09F + 1.57F, 0), Outward(TipPair.Key), Random.FRandRange(110, 150) * (bSapling ? .5F : 1.0F), Occl(TipPair.Key), Limb.Wind1, Random.RandRange(0, 1)};
                        Clump.bMass = true;
                        K.Tufts.Add(Clump);
                    }
                }
            }
            else
            {
                // Douglas-fir: flat two-ranked sprays along the limb and short side branchlets.
                const FVector Flat = FVector::CrossProduct(FVector::UpVector, (Tip - Base).GetSafeNormal()).GetSafeNormal();
                const float Spacing = (bSapling ? 16.0F : 24.0F) * Big;
                // Branch-spray mass cards layered along the limb (both sides, slightly above).
                for (float D = L * .15F; D < L; D += (bSapling ? 60.0F : 75.0F) * Random.FRandRange(.8F, 1.2F))
                {
                    const float S = D / L;
                    const FVector At = PointAt(S), Ahead = (PointAt(FMath::Min(1.0F, S + .08F)) - At).GetSafeNormal();
                    for (int32 Side = -1; Side <= 1; Side += 2)
                    {
                        const FVector Spray = (Ahead * .65F + Flat * Side * .62F + FVector(0, 0, Random.FRandRange(.0F, .2F))).GetSafeNormal();
                        FTuft Clump{At + FVector(0, 0, 6), Spray, FVector::CrossProduct(Spray, FVector::UpVector) + FVector(0, 0, Random.FRandRange(-.25F, .25F)), Outward(At),
                            Random.FRandRange(115, 165) * (bSapling ? .45F : 1.0F) * FMath::Lerp(1.0F, .7F, S), Occl(At), FMath::Lerp(Limb.Wind0, Limb.Wind1, S), Random.RandRange(0, 2)};
                        Clump.bMass = true;
                        K.Tufts.Add(Clump);
                    }
                }
                for (float D = L * .22F; D < L; D += Spacing * Random.FRandRange(.8F, 1.2F))
                {
                    const float S = D / L;
                    const FVector At = PointAt(S), Ahead = (PointAt(FMath::Min(1.0F, S + .05F)) - At).GetSafeNormal();
                    for (int32 Side = -1; Side <= 1; Side += 2)
                    {
                        const FVector Spray = (Ahead * .55F + Flat * Side * .80F + FVector(0, 0, Random.FRandRange(-.05F, .18F))).GetSafeNormal();
                        K.Tufts.Add({At, Spray, FVector::CrossProduct(Spray, FVector::UpVector) + FVector(0, 0, Random.FRandRange(-.3F, .3F)), Outward(At),
                            Random.FRandRange(68, 104) * Big * (bSapling ? .5F : 1.0F) * FMath::Lerp(1.0F, .72F, S), Occl(At), FMath::Lerp(Limb.Wind0, Limb.Wind1, S), Random.RandRange(0, 2)});
                    }
                    if (!bSapling && L > 140 && FMath::Fmod(D, 48.0F) < Spacing && S < .85F)
                    {
                        const int32 Side = Random.FRand() < .5F ? -1 : 1;
                        const float BL = (L - D) * .42F + 25;
                        const FVector BD = (Ahead * .55F + Flat * Side * .8F + FVector(0, 0, -.06F)).GetSafeNormal();
                        FLimb Branchlet; Branchlet.Level = 2; Branchlet.Path = {At, At + BD * BL * .5F - FVector(0, 0, BL * .05F), At + BD * BL};
                        Branchlet.R0 = FMath::Max(.6F, Limb.R0 * .30F); Branchlet.R1 = .35F;
                        Branchlet.Wind0 = FMath::Lerp(Limb.Wind0, Limb.Wind1, S); Branchlet.Wind1 = FMath::Min(1.0F, Branchlet.Wind0 + .25F);
                        K.Limbs.Add(Branchlet);
                        const FVector Across = FVector::CrossProduct(FVector::UpVector, BD).GetSafeNormal();
                        for (float BDist = BL * .2F; BDist < BL; BDist += 24.0F)
                            for (int32 BS = -1; BS <= 1; BS += 2)
                            {
                                const FVector P = At + BD * BDist, Spray = (BD * .5F + Across * BS * .85F + FVector(0, 0, .06F)).GetSafeNormal();
                                K.Tufts.Add({P, Spray, FVector::CrossProduct(Spray, FVector::UpVector), Outward(P), Random.FRandRange(55, 80), Occl(P), Branchlet.Wind1, Random.RandRange(0, 2)});
                            }
                    }
                }
            }
        }
    }
    // Leader tuft.
    const FVector Top = K.Trunk.Last();
    for (int32 Leader = 0; Leader < (bSapling ? 3 : 6); ++Leader)
    {
        const FVector D = (FVector(0, 0, 1) + Dir(Leader * 2.1F, 0) * .55F).GetSafeNormal();
        K.Tufts.Add({Top - FVector(0, 0, 25), D, Dir(Leader * 2.1F + 1.57F, 0), D, bPine ? 30.0F : 40.0F, 1.0F, Wind0 + .35F, Leader % K.IslandCount});
    }
}

FSkeleton MakeConifer(ESpecies Species, EAge Age, int32 Seed, float Size = 1.0F)
{
    FRandomStream Random(Seed);
    FSkeleton K; K.Species = Species;
    const bool bPine = Species == ESpecies::MontereyPine, bSapling = Age == EAge::Sapling;
    K.Islands = bPine ? PineSprigs : FirSprays; K.IslandCount = bPine ? 4 : 3;
    K.TileU = bPine ? 200.0F : 120.0F; K.TileV = K.TileU; // pine_bark 2.0 m, Bark014 1.2 m tiles
    K.Height = bSapling ? Random.FRandRange(170, 440) : Age == EAge::Pole ? Random.FRandRange(bPine ? 900 : 1100, bPine ? 1400 : 1700)
        : Random.FRandRange(bPine ? 1700 : 2300, bPine ? 2600 : 3400);
    K.Height *= Size;
    const float H = K.Height;
    // Size classes: veterans carry a disproportionately thick bole (DBH up to ~1.2 m), young poles ~0.25 m.
    K.Breast = (Size > 1.0F ? 1.22F : 1.0F) * (bSapling ? 2.5F + H * .006F : H * (bPine ? .0155F : .0125F) + 3.0F);
    // Live crown 52-68% of height for conifers (PMO R1: no bare poles).
    // R6: lower, varied crown bases (live crown 62-80% of height) for a closed canopy.
    K.CrownBase = bSapling ? H * .12F : H * (Age == EAge::Pole ? Random.FRandRange(.18F, .26F) : bPine ? Random.FRandRange(.22F, .32F) : Random.FRandRange(.20F, .34F));
    K.CrownRadius = bSapling ? H * .28F : H * (bPine ? .30F : .19F);
    const float LeanYaw = Random.FRandRange(0, 2 * PI), Sweep = bPine ? Random.FRandRange(.015F, .045F) : Random.FRandRange(.004F, .012F);
    for (float Z : StemHeights(H))
    {
        const float T = FMath::Max(Z, 0.0F) / H;
        K.Trunk.Add(FVector(FMath::Cos(LeanYaw), FMath::Sin(LeanYaw), 0) * H * Sweep * T * T + FVector(0, 0, Z)
            + FVector(FMath::Sin(T * 9.0F + Seed), FMath::Cos(T * 7.0F + Seed), 0) * H * .0025F * T);
        // Cylindrical butt to breast height; the flare is added only by the collar profile.
        const float Above = FMath::Max(0.0F, Z - 130.0F) / FMath::Max(1.0F, H - 130.0F);
        K.TrunkRadius.Add(FMath::Max(.6F, K.Breast * (1.0F - .93F * FMath::Pow(Above, 1.0F))));
    }
    if (!bSapling)
    {
        const float Top = H * .82F;
        const FVector3f A(K.Trunk[1]), M(FVector(0, 0, Top * .45F)), B(FVector(K.Trunk.Last().X * .82F, K.Trunk.Last().Y * .82F, Top));
        K.Collision = {{A, M, K.Breast * 1.02F}, {M, B, K.Breast * .62F}};
    }
    AddConiferBranches(K, Random, bPine, bSapling, 0.0F);
    return K;
}

FSkeleton MakeOak(int32 Seed)
{
    FRandomStream Random(Seed);
    FSkeleton K; K.Species = ESpecies::CoastLiveOak; K.Islands = OakRows; K.IslandCount = 4; K.TileU = 100; K.TileV = 200;
    K.Breast = Random.FRandRange(30, 44);
    const float Fork = Random.FRandRange(70, 170);
    K.Stems = Random.RandRange(3, 5); // 3-5 unequal crown lobes (R5/R6)
    // Short shared bole with the root collar; stems lean outward from the fork.
    for (float Z : {-Buried, 0.0F, 7.0F, 16.0F, 27.0F, 40.0F, 56.0F, 76.0F, Fork})
    {
        K.Trunk.Add(FVector(0, 0, Z));
        K.TrunkRadius.Add(K.Breast * (Z <= 130 ? 1.0F : .96F));
    }
    K.Collision.Add({FVector3f(0, 0, 0), FVector3f(0, 0, Fork), K.Breast});
    const float StemYaw0 = Random.FRandRange(0, 2 * PI);
    float Highest = Fork, Widest = 300;
    TArray<FVector> TwigTips;
    for (int32 Stem = 0; Stem < K.Stems; ++Stem)
    {
        const float Yaw = StemYaw0 + Stem * 2 * PI / K.Stems + Random.FRandRange(-.4F, .4F);
        const float Lean = FMath::DegreesToRadians(Random.FRandRange(18, 40));
        const float Length = Random.FRandRange(460, 720);
        FLimb Trunk; Trunk.Level = 0; Trunk.R0 = K.Breast * .66F; Trunk.R1 = K.Breast * .40F; Trunk.Wind0 = 0; Trunk.Wind1 = .05F;
        const FVector Start = FVector(0, 0, Fork - 25) + Dir(Yaw, 0) * K.Breast * .3F;
        for (int32 Step = 0; Step <= 5; ++Step)
        {
            const float S = Step / 5.0F;
            const float Bend = Lean * (1.0F + .35F * S) + .12F * FMath::Sin(S * 5.0F + Seed + Stem);
            Trunk.Path.Add(Start + FVector(FMath::Cos(Yaw), FMath::Sin(Yaw), 0) * Length * FMath::Sin(Bend) * S + FVector(0, 0, Length * FMath::Cos(Bend) * S));
        }
        K.Collision.Add({FVector3f(Trunk.Path[0]), FVector3f(Trunk.Path[3]), Trunk.R0 * .95F});
        K.Collision.Add({FVector3f(Trunk.Path[3]), FVector3f(Trunk.Path[5]), Trunk.R1 * 1.05F});
        K.Limbs.Add(Trunk);
        const FVector StemTop = Trunk.Path.Last();
        const int32 LimbCount = Random.RandRange(2, 3);
        for (int32 LimbIndex = 0; LimbIndex < LimbCount; ++LimbIndex)
        {
            const float LYaw = Yaw + Random.FRandRange(-1.1F, 1.1F);
            const float LPitch = FMath::DegreesToRadians(Random.FRandRange(8, 38));
            const float LL = Random.FRandRange(380, 560);
            FLimb Limb; Limb.Level = 1; Limb.R0 = Trunk.R1 * .78F; Limb.R1 = 3.0F; Limb.Wind0 = .05F; Limb.Wind1 = .35F;
            const FVector From = FMath::Lerp(Trunk.Path[4], StemTop, Random.FRandRange(.3F, 1.0F));
            for (int32 Step = 0; Step <= 4; ++Step)
            {
                const float S = Step / 4.0F;
                Limb.Path.Add(From + Dir(LYaw + .25F * FMath::Sin(S * 3.0F + LimbIndex), LPitch * (1.0F - .9F * S)) * LL * S - FVector(0, 0, LL * .10F * S * S));
            }
            if (Limb.R0 > 10) K.Collision.Add({FVector3f(Limb.Path[0]), FVector3f(Limb.Path[2]), Limb.R0 * .8F});
            const int32 LimbSlot = K.Limbs.Add(Limb);
            const int32 BranchCount = Random.RandRange(3, 4);
            for (int32 Branch = 0; Branch < BranchCount; ++Branch)
            {
                const auto& Path = K.Limbs[LimbSlot].Path;
                const float S = Random.FRandRange(.35F, 1.0F);
                const float F = S * (Path.Num() - 1); const int32 I0 = FMath::Min(FMath::FloorToInt(F), Path.Num() - 2);
                const FVector BFrom = FMath::Lerp(Path[I0], Path[I0 + 1], F - I0);
                const float BYaw = LYaw + Random.FRandRange(-1.3F, 1.3F), BPitch = FMath::DegreesToRadians(Random.FRandRange(-5, 40));
                const float BL = Random.FRandRange(150, 260);
                FLimb B; B.Level = 2; B.R0 = FMath::Max(1.5F, Limb.R0 * .35F * (1.0F - S * .5F)); B.R1 = .8F; B.Wind0 = .3F; B.Wind1 = .7F;
                B.Path = {BFrom, BFrom + Dir(BYaw, BPitch) * BL * .5F, BFrom + Dir(BYaw + .2F, BPitch * .4F) * BL - FVector(0, 0, BL * .08F)};
                K.Limbs.Add(B);
                const int32 Twigs = Random.RandRange(6, 8);
                for (int32 Twig = 0; Twig < Twigs; ++Twig)
                {
                    const float TS = .25F + .75F * (Twig + .5F) / Twigs;
                    const FVector TFrom = TS < .5F ? FMath::Lerp(B.Path[0], B.Path[1], TS * 2) : FMath::Lerp(B.Path[1], B.Path[2], TS * 2 - 1);
                    const FVector TD = (Dir(BYaw + Random.FRandRange(-1.4F, 1.4F), FMath::DegreesToRadians(Random.FRandRange(-25, 45))));
                    const float TL = Random.FRandRange(45, 85);
                    FLimb T; T.Level = 3; T.R0 = .7F; T.R1 = .3F; T.Wind0 = .6F; T.Wind1 = .9F;
                    T.Path = {TFrom, TFrom + TD * TL};
                    K.Limbs.Add(T);
                    TwigTips.Add(T.Path.Last());
                    TwigTips.Add(FMath::Lerp(TFrom, T.Path.Last(), .45F));
                    Highest = FMath::Max<float>(Highest, T.Path.Last().Z);
                    Widest = FMath::Max<float>(Widest, FVector::Dist2D(T.Path.Last(), FVector::ZeroVector));
                }
            }
        }
    }
    K.Height = Highest + 40;
    K.CrownRadius = Widest;
    K.CrownBase = Fork + 120;
    const FVector CrownCenter(0, 0, K.Height * .55F);
    for (const FVector& Tip : TwigTips)
    {
        const FVector Outward = (Tip - CrownCenter).GetSafeNormal();
        const float Occlusion = FMath::Clamp(.35F + .65F * FVector::Distance(Tip, CrownCenter) / FMath::Max(300.0F, K.CrownRadius), .35F, 1.0F);
        for (int32 Leaf = 0; Leaf < 4; ++Leaf)
        {
            const FVector D = (Outward * .55F + Random.VRand() * .9F).GetSafeNormal();
            const FVector At = Tip + Random.VRand() * Random.FRandRange(0, 28);
            K.Tufts.Add({At, D, FVector::CrossProduct(D, Random.VRand()), Outward, Random.FRandRange(40, 56), Occlusion, .9F, Random.RandRange(0, 3)});
        }
        for (int32 Mass = 0; Mass < 2; ++Mass)
        {
            const FVector D = (Outward * .6F + Random.VRand() * .7F).GetSafeNormal();
            FTuft Clump{Tip - D * 30.0F, D, FVector::CrossProduct(D, Random.VRand()), Outward, Random.FRandRange(55, 75), Occlusion, .9F, Random.RandRange(0, 1)};
            Clump.bMass = true;
            K.Tufts.Add(Clump);
        }
    }
    return K;
}

// ---------------------------------------------------------------------------------
// LOD emission
// ---------------------------------------------------------------------------------
// R4 trial: reshape three existing library members, without adding cards, limbs or
// changing the stand/LOD policy. The continuous map carries connected wood and
// foliage together; card roll then supplies compound twig planes, not flat shelves.
void RefineHeroShape(FSkeleton& K)
{
    K.bHeroShape = true;
    const bool bOak = K.Species == ESpecies::CoastLiveOak;
    const bool bPine = K.Species == ESpecies::MontereyPine;
    const float H = K.Height, Radius = K.CrownRadius;
    auto Shape = [=](const FVector& P) -> FVector
    {
        // Preserve roots, low bole and gate collision through 4.5 m. The oak's
        // low fork already exists; allow its upper limbs to carry the broad crown.
        const float Active = Smooth01((P.Z - 450.0F) / 350.0F);
        if (Active <= 0) return P;
        const float T = FMath::Clamp(static_cast<float>(P.Z) / H, 0.0F, 1.0F);
        const float R = FVector2D(P.X, P.Y).Size();
        const float Yaw = FMath::Atan2(P.Y, P.X);
        const float Radial = Smooth01(R / FMath::Max(120.0F, Radius * .65F));
        // Unequal lobes shrink within the prior envelope. Twist varies with
        // radius/height so the connected primary limbs are curved, not spokes.
        const float Envelope = bOak ? .90F + .07F * FMath::Sin(Yaw * 3 + .7F)
            : bPine ? .90F + .08F * FMath::Sin(Yaw * 2 + .8F) : .93F + .06F * FMath::Sin(Yaw * 3 - .6F);
        const float Scale = FMath::Lerp(1.0F, Envelope, Active * Radial);
        const float Twist = Active * Radial * (bPine ? .17F : .10F) * FMath::Sin(T * 7.0F + Yaw);
        const float X = P.X * FMath::Cos(Twist) - P.Y * FMath::Sin(Twist);
        const float Y = P.X * FMath::Sin(Twist) + P.Y * FMath::Cos(Twist);
        // Fir boughs droop unevenly; pine upper boughs ascend with asymmetric
        // terminal masses. Oak branches carry unequal crown lobes at depth.
        const float Sag = bOak ? .13F * FMath::Sin(Yaw * 3 + T * 5)
            : bPine ? .10F + .13F * FMath::Sin(Yaw * 2 + T * 6)
            : -.12F + .11F * FMath::Sin(Yaw * 3 + T * 5);
        const float DZ = Active * R * Radial * Sag;
        // Small recovery bends above the protected bole. Keep conservative
        // gameplay collision unchanged; no movement of planted origin/collar.
        const float Bend = Active * (bPine ? 24.0F : bOak ? 12.0F : 10.0F) * FMath::Sin(PI * T);
        return FVector(X * Scale + Bend, Y * Scale + Bend * .55F * FMath::Sin(T * 5), P.Z + DZ);
    };
    for (FVector& P : K.Trunk) P = Shape(P);
    for (FLimb& Limb : K.Limbs) for (FVector& P : Limb.Path) P = Shape(P);
    for (int32 Index = 0; Index < K.Tufts.Num(); ++Index)
    {
        FTuft& Tuft = K.Tufts[Index];
        const FVector Old = Tuft.Origin, At = Shape(Old);
        const FVector Along = (Shape(Old + Tuft.Dir * 10) - At).GetSafeNormal();
        const FVector Wide = Shape(Old + Tuft.Across.GetSafeNormal() * 10) - At;
        FVector Across = (Wide - Along * FVector::DotProduct(Wide, Along)).GetSafeNormal();
        // Both signs and several inclinations break shared planes without
        // rotating whole trees or changing the deterministic placement stream.
        const float Phase = FMath::Sin(Old.X * .027F + Old.Y * .019F + Old.Z * .011F + Index * 2.39996F);
        const float Roll = FMath::DegreesToRadians((Index % 2 ? -1.0F : 1.0F) * (bOak ? 25.0F : 38.0F) + Phase * 25.0F);
        Across = Across.RotateAngleAxis(FMath::RadiansToDegrees(Roll), Along).GetSafeNormal();
        Tuft.Origin = At;
        Tuft.Dir = Along;
        Tuft.Across = Across;
        Tuft.Outward = (At - FVector(0, 0, H * (bOak ? .55F : .65F))).GetSafeNormal();
        // Break coincident edges at the same budget; retain enough area that
        // reduced flatness does not regress into the previous bare-tree failure.
        Tuft.Length *= .94F + .06F * (.5F + .5F * Phase);
    }
}

// ---------------------------------------------------------------------------------
// R5 representative crowns (AAA-R5-TREE-FORM-DIRECTION): foliage rebuilt from connected
// branchlet groups instead of continuous spray shelves along each primary.
// ---------------------------------------------------------------------------------
/** Envelope-lit card: flat (2 tris) or folded along its midline (4 tris). Per-vertex normals follow
 *  the local cluster envelope (vertex - EnvelopeCenter) blended with the true half-face normal. */
void EnvelopeCard(FSection& S, const FVector& Origin, const FVector& Along, const FVector& Across, float Length, const FIsland& Island,
    const FVector& EnvelopeCenter, const FVector4f& Color, float FoldDegrees)
{
    const FVector2f Extent = Island.Max - Island.Min;
    const float Aspect = Island.bAlongU ? Extent.X / Extent.Y : Extent.Y / Extent.X;
    const float Width = Length / FMath::Max(Aspect, .25F);
    const FVector A = Along.GetSafeNormal(), W = (Across - A * FVector::DotProduct(Across, A)).GetSafeNormal();
    const FVector Face = FVector::CrossProduct(W, A).GetSafeNormal();
    const FVector Base = Island.bCentered ? Origin - A * Length * .5F : Origin;
    const float Fold = FMath::DegreesToRadians(FoldDegrees);
    // Both halves tilt toward the same face: a shallow V that gives the spray edge thickness.
    const FVector Left = (-W * FMath::Cos(Fold) + Face * FMath::Sin(Fold)) * Width * .5F;
    const FVector Right = (W * FMath::Cos(Fold) + Face * FMath::Sin(Fold)) * Width * .5F;
    auto Coord = [&Island](float AlongT, float AcrossT)
    {
        return Island.bAlongU ? FVector2f(FMath::Lerp(Island.Min.X, Island.Max.X, AlongT), FMath::Lerp(Island.Min.Y, Island.Max.Y, AcrossT))
            : FVector2f(FMath::Lerp(Island.Min.X, Island.Max.X, AcrossT), FMath::Lerp(Island.Max.Y, Island.Min.Y, AlongT));
    };
    const FVector Tangent = Island.bAlongU ? A : W, Bitangent = Island.bAlongU ? W : -A;
    auto Normal = [&](const FVector& P, const FVector& HalfFace)
    {
        FVector N = HalfFace;
        const FVector Envelope = (P - EnvelopeCenter).GetSafeNormal();
        if (FVector::DotProduct(N, Envelope) < 0) N = -N;
        return (N * .4F + Envelope * .6F).GetSafeNormal();
    };
    const FVector LeftFace = FVector::CrossProduct(Left, A).GetSafeNormal(), RightFace = FVector::CrossProduct(A, Right).GetSafeNormal();
    if (FoldDegrees <= 0.5F)
    {
        const FVector Half = W * Width * .5F;
        const int32 V0 = S.Add(Base - Half, Normal(Base - Half, Face), Tangent, Bitangent, Coord(0, 0), Color);
        const int32 V1 = S.Add(Base + Half, Normal(Base + Half, Face), Tangent, Bitangent, Coord(0, 1), Color);
        const int32 V2 = S.Add(Base + Half + A * Length, Normal(Base + Half + A * Length, Face), Tangent, Bitangent, Coord(1, 1), Color);
        const int32 V3 = S.Add(Base - Half + A * Length, Normal(Base - Half + A * Length, Face), Tangent, Bitangent, Coord(1, 0), Color);
        S.Tri(V0, V1, V2); S.Tri(V0, V2, V3);
        return;
    }
    const FVector Mid = (LeftFace + RightFace).GetSafeNormal();
    const FVector Tip = A * Length;
    const int32 L0 = S.Add(Base + Left, Normal(Base + Left, LeftFace), Tangent, Bitangent, Coord(0, 0), Color);
    const int32 C0 = S.Add(Base, Normal(Base, Mid), Tangent, Bitangent, Coord(0, .5F), Color);
    const int32 R0 = S.Add(Base + Right, Normal(Base + Right, RightFace), Tangent, Bitangent, Coord(0, 1), Color);
    const int32 L1 = S.Add(Base + Left + Tip, Normal(Base + Left + Tip, LeftFace), Tangent, Bitangent, Coord(1, 0), Color);
    const int32 C1 = S.Add(Base + Tip, Normal(Base + Tip, Mid), Tangent, Bitangent, Coord(1, .5F), Color);
    const int32 R1 = S.Add(Base + Right + Tip, Normal(Base + Right + Tip, RightFace), Tangent, Bitangent, Coord(1, 1), Color);
    S.Tri(L0, C0, C1); S.Tri(L0, C1, L1); S.Tri(C0, R0, R1); S.Tri(C0, R1, C1);
}

FVector PathPoint(const FLimb& Limb, float S)
{
    const float F = FMath::Clamp(S, 0.0F, 1.0F) * (Limb.Path.Num() - 1);
    const int32 I0 = FMath::Min(FMath::FloorToInt(F), Limb.Path.Num() - 2);
    return FMath::Lerp(Limb.Path[I0], Limb.Path[I0 + 1], F - I0);
}
FVector Turn(const FVector& D, float YawDegrees, float PitchDegrees)
{
    const FVector Flat = FVector(D.X, D.Y, 0).GetSafeNormal();
    const FVector Base = Flat.IsNearlyZero() ? FVector::ForwardVector : Flat;
    const FVector Yawed = Base.RotateAngleAxis(YawDegrees, FVector::UpVector);
    const float Pitch = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(static_cast<float>(D.Z), -1.0F, 1.0F))) + PitchDegrees;
    return Dir(FMath::Atan2(Yawed.Y, Yawed.X), FMath::DegreesToRadians(Pitch));
}

/** Records a card in cluster Id; envelope/ranks are finalised by FinishClusters. */
void AddClusterCard(FSkeleton& K, int32 Cluster, const FVector& Origin, const FVector& Along, const FVector& Across, float Length,
    int32 Island, float Wind, float Occlusion, float Fold)
{
    // Subtle sway on branchlet cards (QA-MOTION R5a/R6b): weight capped at .15 (~0.4 cm gust).
    FTuft Tuft{Origin, Along.GetSafeNormal(), Across, FVector::UpVector, Length, Occlusion, FMath::Min(Wind, .15F), Island};
    Tuft.Cluster = Cluster; Tuft.Fold = Fold;
    K.Tufts.Add(Tuft);
}
void FinishClusters(FSkeleton& K)
{
    for (int32 Id = 0; Id < K.Clusters.Num(); ++Id)
    {
        FCluster& C = K.Clusters[Id];
        TArray<int32> Members;
        FVector Sum = FVector::ZeroVector;
        for (int32 T = 0; T < K.Tufts.Num(); ++T)
            if (K.Tufts[T].Cluster == Id) { Members.Add(T); Sum += K.Tufts[T].Origin + K.Tufts[T].Dir * K.Tufts[T].Length * .5F; }
        C.Cards = Members.Num();
        if (Members.IsEmpty()) continue;
        C.Center = Sum / Members.Num();
        const FVector Side = FVector::CrossProduct(FVector::UpVector, C.Axis).GetSafeNormal();
        C.Up = FVector::CrossProduct(C.Axis, Side).GetSafeNormal();
        FVector Radii(15);
        for (int32 T : Members)
            for (const float End : {0.0F, 1.0F})
            {
                const FVector P = K.Tufts[T].Origin + K.Tufts[T].Dir * K.Tufts[T].Length * End - C.Center;
                Radii.X = FMath::Max<double>(Radii.X, FMath::Abs(FVector::DotProduct(P, C.Axis)) + 6);
                Radii.Y = FMath::Max<double>(Radii.Y, FMath::Abs(FVector::DotProduct(P, Side)) + 6);
                Radii.Z = FMath::Max<double>(Radii.Z, FMath::Abs(FVector::DotProduct(P, C.Up)) + 6);
            }
        C.Radii = Radii;
        // Rank: outer silhouette cards first, so coarser LODs keep the cluster outline.
        Members.Sort([&](int32 A, int32 B)
        {
            const float DA = FVector::DistSquared(K.Tufts[A].Origin + K.Tufts[A].Dir * K.Tufts[A].Length, C.Center);
            const float DB = FVector::DistSquared(K.Tufts[B].Origin + K.Tufts[B].Dir * K.Tufts[B].Length, C.Center);
            return DA != DB ? DA > DB : A < B;
        });
        for (int32 R = 0; R < Members.Num(); ++R)
        {
            FTuft& T = K.Tufts[Members[R]];
            T.Rank = R;
            T.Outward = C.Center; // envelope centre used by EnvelopeCard normals
        }
    }
}

/** Douglas-fir 1101: drooping primaries carry staggered branchlet groups (thin inner end, two
 *  unequal side sprigs, an offset tip), never a continuous horizontal spray floor. */
void BuildFirBranchlets(FSkeleton& K, FRandomStream& Random)
{
    const float CrownTop = K.Height;
    // Branchlet size follows tree size: saplings get proportionally small groups, veterans larger.
    const float Scale = K.Height < 500 ? .65F : K.Height < 1600 ? 1.2F : FMath::Clamp(K.Height / 2600.0F, 1.0F, 1.3F);
    for (int32 LimbIndex = 0; LimbIndex < K.Limbs.Num(); ++LimbIndex)
    {
        const FLimb& Limb = K.Limbs[LimbIndex];
        if (Limb.Level != 1 && Limb.Level != 2) continue;
        if (Limb.Wind0 == 0 && Limb.Wind1 == 0) continue; // dead stubs
        const float Length = FVector::Distance(Limb.Path[0], Limb.Path.Last());
        if (Length < 60 * Scale) continue;
        const float Start = Limb.Level == 1 ? .30F : .18F;
        int32 Side = Random.FRand() < .5F ? -1 : 1;
        for (float D = Length * Start + Random.FRandRange(0, 20); D < Length * .97F; D += Random.FRandRange(Limb.Level == 1 ? 36.0F : 44.0F, Limb.Level == 1 ? 54.0F : 64.0F) * Scale / K.FoliageDensity)
        {
            const float S = D / Length;
            // Staggered depth: a fraction of groups is skipped, leaving windows to the next layer.
            if (Random.FRand() < .12F / FMath::Max(1.0F, K.FoliageDensity)) { Side = -Side; continue; }
            const FVector Attach = PathPoint(Limb, S);
            const FVector Ahead = (PathPoint(Limb, FMath::Min(1.0F, S + .04F)) - Attach).GetSafeNormal();
            const float U = (Attach.Z - K.CrownBase) / FMath::Max(1.0F, CrownTop - K.CrownBase);
            const float GroupLength = Random.FRandRange(56, 84) * Scale * (Limb.Level == 2 ? .8F : 1.0F) * FMath::Lerp(1.0F, .96F, U);
            // Group axis leaves the limb sideways and droops; tips near the limb end turn up.
            const FVector Axis = Turn(Ahead, Side * Random.FRandRange(28, 58), -Random.FRandRange(10, 26) + (S > .85F ? 18.0F : 0.0F));
            const FVector Lift(0, 0, Side * Random.FRandRange(6, 18) * (Random.FRand() < .5F ? 1 : -1));
            const int32 Id = K.Clusters.Num();
            FCluster C; C.Axis = Axis; C.Island = 1; C.Lobe = FMath::Clamp(FMath::FloorToInt(U * 3), 0, 2) * 4
                + FMath::Clamp(FMath::FloorToInt((FMath::Atan2(Attach.Y, Attach.X) + PI) / (PI * .5F)), 0, 3);
            C.Wind = FMath::Lerp(Limb.Wind0, Limb.Wind1, S);
            C.Occlusion = FMath::Clamp(.45F + .55F * FVector::Dist2D(Attach, FVector::ZeroVector) / FMath::Max(150.0F, K.CrownRadius), .45F, 1.0F);
            K.Clusters.Add(C);
            const FVector Root = Attach + Lift;
            // Sprays roll 35-70 degrees off horizontal (alternating sign): no shared horizontal floor.
            auto Roll = [&](const FVector& Along) { return FVector::CrossProduct(Along, FVector::UpVector).GetSafeNormal().RotateAngleAxis((Random.FRand() < .5F ? -1 : 1) * Random.FRandRange(35, 70), Along); };
            // Core spray from the thin inner end (starts 15% out) to the group tip.
            const FVector Core = Root + Axis * GroupLength * .15F;
            AddClusterCard(K, Id, Core, Axis, Roll(Axis), GroupLength * .85F, Random.RandRange(0, 2), C.Wind, C.Occlusion, 0);
            // Two unequal side sprigs.
            const FVector SideA = Turn(Axis, Side * Random.FRandRange(48, 66), -Random.FRandRange(8, 20));
            AddClusterCard(K, Id, Root + Axis * GroupLength * .35F, SideA, Roll(SideA), GroupLength * Random.FRandRange(.55F, .75F), Random.RandRange(0, 2), C.Wind, C.Occlusion * .95F, 0);
            const FVector SideB = Turn(Axis, -Side * Random.FRandRange(55, 75), -Random.FRandRange(12, 26));
            AddClusterCard(K, Id, Root + Axis * GroupLength * .6F, SideB, Roll(SideB), GroupLength * Random.FRandRange(.32F, .48F), Random.RandRange(0, 2), C.Wind, C.Occlusion * .9F, 0);
            // Offset, slightly upturned tip.
            const FVector TipDir = Turn(Axis, Random.FRandRange(-18, 18), Random.FRandRange(4, 14));
            AddClusterCard(K, Id, Root + Axis * GroupLength * .9F, TipDir, Roll(TipDir), GroupLength * Random.FRandRange(.55F, .78F), Random.RandRange(0, 2), FMath::Min(1.0F, C.Wind + .08F), C.Occlusion, 20);
            Side = -Side;
        }
    }
}

/** Monterey pine 2203: rising limbs end in non-coplanar terminal masses with gaps between them. */
void BuildPineTerminals(FSkeleton& K, FRandomStream& Random)
{
    for (int32 LimbIndex = 0; LimbIndex < K.Limbs.Num(); ++LimbIndex)
    {
        const FLimb& Limb = K.Limbs[LimbIndex];
        if (Limb.Level != 1 && Limb.Level != 2) continue;
        if (Limb.Wind0 == 0 && Limb.Wind1 == 0) continue;
        const FVector Tip = Limb.Path.Last();
        const FVector TipDir = (Tip - PathPoint(Limb, .8F)).GetSafeNormal();
        // Visible gaps: some fork ends stay bare twigs.
        if (Limb.Level == 2 && Random.FRand() < FMath::Max(.04F, 1.0F - .9F * K.FoliageDensity)) continue;
        if (Limb.Level == 1 && K.FoliageDensity < 1.0F && Random.FRand() > K.FoliageDensity) continue;
        const float Mass = (Limb.Level == 1 ? Random.FRandRange(.9F, 1.15F) : Random.FRandRange(.65F, .9F)) * (K.Height < 500 ? .55F : FMath::Clamp(K.Height / 2500.0F, 1.0F, 1.25F));
        const int32 Id = K.Clusters.Num();
        FCluster C; C.Axis = TipDir; C.Island = 2; C.Wind = Limb.Wind1;
        C.Lobe = FMath::Clamp(FMath::FloorToInt((Tip.Z - K.CrownBase) / FMath::Max(1.0F, K.Height - K.CrownBase) * 2), 0, 1) * 4
            + FMath::Clamp(FMath::FloorToInt((FMath::Atan2(Tip.Y, Tip.X) + PI) / (PI * .5F)), 0, 3);
        C.Occlusion = FMath::Clamp(.5F + .5F * FVector::Dist2D(Tip, FVector::ZeroVector) / FMath::Max(150.0F, K.CrownRadius), .5F, 1.0F);
        K.Clusters.Add(C);
        // Mass centre sits just beyond the tip; cards radiate in a hemisphere with jittered,
        // non-coplanar centres (thin where it joins the twig, thick on the outside).
        const FVector Centre = Tip + TipDir * 24.0F * Mass;
        const int32 Cards = FMath::Clamp(FMath::RoundToInt((Limb.Level == 1 ? 6 : 5) * K.FoliageDensity), 2, 9);
        for (int32 Card = 0; Card < Cards; ++Card)
        {
            const float Yaw = Card * 360.0F / Cards + Random.FRandRange(-25, 25);
            const FVector Out = (TipDir * .55F + Turn(TipDir, Yaw, Random.FRandRange(-35, 50)) * .85F + FVector(0, 0, .2F)).GetSafeNormal();
            const FVector At = Centre + Random.VRand() * Random.FRandRange(4, 14) * Mass - Out * 10.0F;
            const FVector Across = FVector::CrossProduct(Out, Random.VRand()).GetSafeNormal();
            AddClusterCard(K, Id, At, Out, Across, Random.FRandRange(92, 124) * Mass, Card == 0 ? 1 : 2, C.Wind, C.Occlusion, Card == 0 ? 20.0F : 0.0F);
        }
        // One radial clump faces outward across the mass.
        // The radial clump stands upright facing out of the crown (horizontal normal), never lying flat.
        FVector Facing = FVector(Tip.X, Tip.Y, 0).GetSafeNormal();
        if (Facing.IsNearlyZero()) Facing = FVector(FMath::Cos(LimbIndex * 2.4F), FMath::Sin(LimbIndex * 2.4F), 0);
        const FVector FacingSide = FVector::CrossProduct(FVector::UpVector, Facing).GetSafeNormal();
        AddClusterCard(K, Id, Centre + TipDir * 6.0F, FVector::CrossProduct(Facing, FacingSide).GetSafeNormal(), FacingSide, 116.0F * Mass, 0, C.Wind, C.Occlusion, 0);
        // A small inner sprig along the limb keeps the twig from reading bare.
        if (Limb.Level == 1 && Random.FRand() < .5F)
        {
            const FVector At = PathPoint(Limb, Random.FRandRange(.55F, .7F));
            const FVector Out = Turn((Tip - At).GetSafeNormal(), Random.FRandRange(-60, 60), Random.FRandRange(5, 30));
            // Its own small cluster, so the terminal mass envelope stays compact.
            FCluster Inner = C; Inner.Axis = Out; Inner.Island = 2;
            const int32 InnerId = K.Clusters.Add(Inner);
            AddClusterCard(K, InnerId, At, Out, FVector::CrossProduct(Out, FVector::UpVector), Random.FRandRange(34, 46), 2, C.Wind * .8F, C.Occlusion * .85F, 18);
        }
    }
}

/** Coast live oak 3307: 3-5 unequal crown lobes (one per stem), connected through real branches,
 *  with window sectors and thinned redundant interiors. */
void BuildOakLobes(FSkeleton& K, FRandomStream& Random)
{
    // Lobe = the stem whose limbs carry the twig; lobe centres from the twig tips themselves.
    TArray<int32> LobeOfLimb; LobeOfLimb.Init(-1, K.Limbs.Num());
    int32 Lobe = -1;
    for (int32 I = 0; I < K.Limbs.Num(); ++I)
    {
        if (K.Limbs[I].Level == 0) ++Lobe;
        LobeOfLimb[I] = FMath::Max(Lobe, 0);
    }
    const int32 LobeCount = FMath::Max(Lobe + 1, 1);
    TArray<FVector> LobeCentre; LobeCentre.Init(FVector::ZeroVector, LobeCount);
    TArray<int32> LobeTips; LobeTips.Init(0, LobeCount);
    for (int32 I = 0; I < K.Limbs.Num(); ++I)
        if (K.Limbs[I].Level == 3) { LobeCentre[LobeOfLimb[I]] += K.Limbs[I].Path.Last(); ++LobeTips[LobeOfLimb[I]]; }
    TArray<float> LobeRadius; LobeRadius.Init(100, LobeCount);
    for (int32 L = 0; L < LobeCount; ++L) if (LobeTips[L]) LobeCentre[L] /= LobeTips[L];
    for (int32 I = 0; I < K.Limbs.Num(); ++I)
        if (K.Limbs[I].Level == 3) LobeRadius[LobeOfLimb[I]] = FMath::Max<float>(LobeRadius[LobeOfLimb[I]], FVector::Distance(K.Limbs[I].Path.Last(), LobeCentre[LobeOfLimb[I]]));
    // One or two window sectors per lobe keep branch forks visible through the crown.
    TArray<float> Window; for (int32 L = 0; L < LobeCount; ++L) Window.Add(Random.FRandRange(-PI, PI));
    int32 Branch = -1;
    for (int32 I = 0; I < K.Limbs.Num(); ++I)
    {
        const FLimb& Twig = K.Limbs[I];
        if (Twig.Level == 2) Branch = I;
        if (Twig.Level != 3) continue;
        const int32 L = LobeOfLimb[I];
        const FVector Tip = Twig.Path.Last(), Rel = Tip - LobeCentre[L];
        const float Radial = Rel.Size() / FMath::Max(1.0F, LobeRadius[L]);
        const float Azimuth = FMath::Atan2(Rel.Y, Rel.X);
        const float ToWindow = FMath::Abs(FMath::UnwindRadians(Azimuth - Window[L]));
        if (ToWindow < .32F && FMath::Abs(Rel.Z) < LobeRadius[L] * .5F) continue; // branch window
        const bool bInterior = Radial < .45F;
        if (bInterior && Random.FRand() < FMath::Max(.1F, 1.0F - .7F * K.FoliageDensity)) continue; // redundant hidden interior
        const FVector TwigDir = (Tip - Twig.Path[0]).GetSafeNormal();
        const int32 Id = K.Clusters.Num();
        FCluster C; C.Axis = (Rel.GetSafeNormal() * .6F + TwigDir * .4F).GetSafeNormal(); C.Lobe = L; C.Wind = Twig.Wind1;
        C.Occlusion = FMath::Clamp(.4F + .6F * Radial, .4F, 1.0F);
        K.Clusters.Add(C);
        const bool bLower = Rel.Z < -LobeRadius[L] * .25F;
        const int32 Cards = bInterior ? 1 : FMath::Max(2, FMath::RoundToInt(5 * K.FoliageDensity));
        for (int32 Card = 0; Card < Cards; ++Card)
        {
            FVector Along = Turn(C.Axis, (Card - 1.5F) * Random.FRandRange(25, 45), bLower ? -Random.FRandRange(25, 45) : Random.FRandRange(-10, 25));
            const FVector At = FMath::Lerp(Twig.Path[0], Tip, Random.FRandRange(.55F, .95F)) + Random.VRand() * 6.0F;
            const int32 Island = bLower ? 3 : Card == 0 ? 0 : Card == 1 ? 1 : 2;
            const FVector Across = FVector::CrossProduct(Along, FVector::UpVector).GetSafeNormal().RotateAngleAxis((Card % 2 ? -1 : 1) * Random.FRandRange(45, 80), Along);
            AddClusterCard(K, Id, At, Along, Across, Random.FRandRange(40, 56), Island, C.Wind, C.Occlusion, Card == 0 ? 22.0F : 0.0F);
        }
    }
    (void)Branch;
}

void BuildHeroFoliage(FSkeleton& K, int32 Seed)
{
    FRandomStream Random(Seed * 7 + 5);
    K.Tufts.Reset();
    K.Clusters.Reset();
    K.bHeroFoliage = true;
    if (K.Species == ESpecies::CoastLiveOak) BuildOakLobes(K, Random);
    else if (K.Species == ESpecies::MontereyPine) BuildPineTerminals(K, Random);
    else BuildFirBranchlets(K, Random);
    FinishClusters(K);
}

/** Cluster-stable LODs: LOD1 keeps each cluster's outer-ranked cards (modest growth), LOD2 one
 *  crossed pair per cluster sized to its envelope, LOD3 two crossed cards per crown lobe. Cards
 *  never exceed their envelope, so silhouettes do not migrate or merge into a roof between levels. */
void EmitHeroFoliage(const FSkeleton& K, int32 Lod, FLod& Out)
{
    if (Lod <= 1)
    {
        for (const FTuft& T : K.Tufts)
        {
            const FCluster& C = K.Clusters[T.Cluster];
            if (Lod == 1 && T.Rank >= FMath::Max(1, FMath::CeilToInt(C.Cards * .45F))) continue;
            const float Grow = Lod == 1 ? 1.25F : 1.0F;
            EnvelopeCard(Out.Leaf, T.Origin - T.Dir * T.Length * (Grow - 1) * .5F, T.Dir, T.Across, T.Length * Grow,
                K.Islands[T.Island % K.IslandCount], C.Center, FVector4f(T.Occlusion, T.Wind, 0, 1), Lod == 0 ? T.Fold : 0.0F);
        }
        return;
    }
    // One envelope-sized card per cluster (LOD2) or for each lobe's outer clusters (LOD3).
    auto ClusterCard = [&](const FCluster& C, int32 Id, float Grow, float RollOffset = 0)
    {
        const FVector Side = FVector::CrossProduct(FVector::UpVector, C.Axis).GetSafeNormal();
        const float Span = FMath::Max3(C.Radii.X, C.Radii.Y, C.Radii.Z);
        const FIsland& Island = K.Islands[C.Island % K.IslandCount];
        const FVector2f Extent = Island.Max - Island.Min;
        const float Aspect = Island.bAlongU ? Extent.X / Extent.Y : Extent.Y / Extent.X;
        // Length chosen so the card's narrow side still spans the cluster's broad side.
        const float Length = FMath::Min(2.0F * Span * Grow * FMath::Clamp(Aspect, .8F, 1.6F) * .75F, 2.0F * Span * Grow);
        // Tilt alternates by stable id; never a horizontal floor. Grow pulls toward the crown axis.
        const float Roll = (Id % 2 ? 50.0F : -40.0F) + RollOffset;
        const FVector Inward = -FVector(C.Center.X, C.Center.Y, 0).GetSafeNormal() * Span * (Grow - 1.0F);
        const FVector Centre = C.Center + Inward;
        EnvelopeCard(Out.Leaf, Centre - C.Axis * Length * .5F, C.Axis, Side.RotateAngleAxis(Roll, C.Axis), Length, Island, Centre, FVector4f(C.Occlusion, C.Wind, 0, 1), 0);
    };
    if (Lod == 2)
    {
        // Terminal masses (pine) need three crossed planes, fir branchlets two, oak twig groups one.
        const int32 Planes = K.Species == ESpecies::MontereyPine && K.Height >= 500 ? 3 : 1;
        for (int32 Id = 0; Id < K.Clusters.Num(); ++Id)
            if (K.Clusters[Id].Cards)
                for (int32 Plane = 0; Plane < (K.Clusters[Id].Cards > 1 ? Planes : 1); ++Plane) ClusterCard(K.Clusters[Id], Id, 1.0F, Plane * 180.0F / Planes);
        return;
    }
    TMap<int32, TArray<int32>> Lobes;
    int32 Total = 0;
    for (int32 Id = 0; Id < K.Clusters.Num(); ++Id) if (K.Clusters[Id].Cards) { Lobes.FindOrAdd(K.Clusters[Id].Lobe).Add(Id); ++Total; }
    TArray<int32> Keys; Lobes.GetKeys(Keys); Keys.Sort();
    const int32 PerLobe = K.Species == ESpecies::CoastLiveOak ? 12 : K.Species == ESpecies::DouglasFir ? 14 : 8;
    const int32 Planes = 1;
    // Each lobe's outermost clusters first; at most 30% of all clusters survive to LOD3.
    TArray<TPair<int32, int32>> Ranked; // (rank within lobe, cluster id)
    for (const int32 Key : Keys)
    {
        TArray<int32> Members = Lobes[Key];
        Members.Sort([&](int32 A, int32 B)
        {
            const float DA = FVector::Dist2D(K.Clusters[A].Center, FVector::ZeroVector), DB = FVector::Dist2D(K.Clusters[B].Center, FVector::ZeroVector);
            return DA != DB ? DA > DB : A < B;
        });
        const int32 Step = FMath::Max(1, Members.Num() / PerLobe);
        for (int32 I = 0, Rank = 0; I < Members.Num() && Rank < PerLobe; I += Step, ++Rank) Ranked.Add({Rank, Members[I]});
    }
    Ranked.Sort([](const TPair<int32, int32>& A, const TPair<int32, int32>& B) { return A.Key != B.Key ? A.Key < B.Key : A.Value < B.Value; });
    // The rounded lobe shells below carry far mass; only a few outer clusters keep their own card.
    const int32 Limit = FMath::FloorToInt(Total * .06F);
    for (int32 I = 0; I < Ranked.Num() && I < Limit; ++I)
        for (int32 Plane = 0; Plane < Planes; ++Plane) ClusterCard(K.Clusters[Ranked[I].Value], Ranked[I].Value, K.Height < 500 ? 1.0F : 1.5F, Plane * 90.0F);
    // R8 far crown mass: each lobe becomes a rounded shell of natural-aspect cards spread over
    // its ellipsoid (Fibonacci directions), lying tangent to the surface with spherical normals.
    // No card is stretched beyond its atlas aspect and none stands as an upright plane, so far
    // crowns read as rounded masses rather than flags against the sky.
    const bool bOakShell = K.Species == ESpecies::CoastLiveOak, bPineShell = K.Species == ESpecies::MontereyPine;
    // Round, dense atlas cells for the shell: pine radial clump, oak rosette, fir widest spray.
    const int32 ShellIsland = bPineShell ? 0 : bOakShell ? 2 : 1;
    const int32 ShellCardsMax = bOakShell ? 10 : bPineShell ? 7 : 6;
    for (const int32 Key : Keys)
    {
        const TArray<int32>& Members = Lobes[Key];
        FBox Box(ForceInit);
        float Wind = 0, Occlusion = 0;
        for (const int32 Id : Members)
        {
            const FCluster& C = K.Clusters[Id];
            Box += C.Center + FVector(C.Radii.X, C.Radii.X, C.Radii.Z) * .6F;
            Box += C.Center - FVector(C.Radii.X, C.Radii.X, C.Radii.Z) * .6F;
            Wind = FMath::Max(Wind, C.Wind); Occlusion += C.Occlusion;
        }
        Occlusion /= Members.Num();
        const FVector Centre = Box.GetCenter(), Radius = Box.GetExtent() * .8F;
        // Small lobes (saplings, young trees) get fewer shell cards so LOD3 stays cheaper than LOD2.
        const int32 ShellCards = FMath::Clamp(FMath::FloorToInt(Members.Num() * .4F), bPineShell && K.Height >= 500 ? 4 : 2, ShellCardsMax);
        const float CardSize = FMath::Clamp(FMath::Min3(Radius.X, Radius.Y, Radius.Z) * 1.3F, 60.0F, 420.0F);
        for (int32 I = 0; I < ShellCards; ++I)
        {
            // Fibonacci sphere, skipping the lowest cap (crown underside is hidden at distance).
            const float Y = 1.0F - 1.8F * (I + .5F) / ShellCards;
            const float R = FMath::Sqrt(FMath::Max(0.0F, 1.0F - Y * Y)), Phi = I * 2.39996F + Key * .7F;
            const FVector Dir(FMath::Cos(Phi) * R, FMath::Sin(Phi) * R, Y);
            const FVector At = Centre + Dir * Radius;
            const FVector Normal = Dir;
            FVector Tangent = FVector::CrossProduct(Normal, FVector::UpVector);
            if (Tangent.IsNearlyZero()) Tangent = FVector::ForwardVector;
            Tangent = Tangent.GetSafeNormal().RotateAngleAxis(I * 47.0F, Normal);
            const FVector Across = FVector::CrossProduct(Normal, Tangent).GetSafeNormal();
            const FIsland& Island = K.Islands[ShellIsland % K.IslandCount];
            const FVector Origin = Island.bCentered ? At : At - Tangent * CardSize * .5F;
            EnvelopeCard(Out.Leaf, Origin, Tangent, Across, CardSize, Island, Centre, FVector4f(FMath::Lerp(.75F, 1.0F, Occlusion), FMath::Min(Wind, .15F), 0, 1), 0);
        }
    }
}

void EmitLod(const FSkeleton& K, int32 Lod, FLod& Out, uint32 Seed, bool bCrossed = true)
{
    const bool bOak = K.Species == ESpecies::CoastLiveOak;
    const int32 TrunkSides = Lod == 0 ? 14 : Lod == 1 ? 8 : Lod == 2 ? 5 : 3;
    TArray<TPair<float, float>> Lobes;
    FRandomStream LobeRandom(Seed);
    const int32 LobeCount = LobeRandom.RandRange(4, 6);
    for (int32 L = 0; L < LobeCount; ++L) Lobes.Add({L * 2 * PI / LobeCount + LobeRandom.FRandRange(-.35F, .35F), LobeRandom.FRandRange(.05F, .07F)});
    // Physical bark tiling: whole repeats around the circumference so the seam closes.
    const float URepeats = FMath::Clamp(FMath::RoundToFloat(2 * PI * K.Breast / K.TileU * 1.6F), 2.0F, 4.0F);
    const float Height = K.Height;
    Sweep(Out.Bark, K.Trunk, [&](int32 Ring, float Theta)
        {
            const float Z = K.Trunk[Ring].Z;
            const float Base = K.TrunkRadius[Ring];
            if (K.Species != ESpecies::CoastLiveOak && K.Height < 500) return Base; // seedlings: no collar
            const float Section = K.bHeroShape ? 1.0F + .035F * Smooth01((Z - 450.0F) / 350.0F) * FMath::Sin(Theta * 2 + Z * .001F) : 1.0F;
            // LOD2+ keep the smooth flare (no buttress lobes) so mid-distance bases never read as cylinders.
            // Far trunks are thinned (LOD2 x0.8, LOD3 x0.55) so from 300 m+ the crowns, not poles, dominate.
            const float Far = Lod == 2 ? .8F : Lod == 3 ? .55F : 1.0F;
            return Section * Far * (Lod >= 2 ? Base * (1.0F + .18F * FMath::Exp(-FMath::Max(Z, 0.0F) / 30.0F)) : CollarRadius(Z, Theta, Base, Lobes));
        }, TrunkSides, URepeats, K.TileV,
        [&](int32 Ring)
        {
            const float Z = K.Trunk[Ring].Z;
            // Soil contact darkening, stand shade on the bole, deeper shade inside the crown;
            // far LODs are darker still so distant trunks never read as pale matchsticks.
            const float Shade = (Z > K.CrownBase ? .62F : .80F) * (Lod >= 2 ? .6F : 1.0F);
            return FVector4f(FMath::Min(FMath::Clamp(Z / 90.0F, 0.0F, 1.0F), Shade), FMath::Square(FMath::Max(Z, 0.0F) / Height) * .12F, 0, 1);
        });
    // R9 surface root swells: low, mostly buried ridges along each buttress lobe that run out and
    // down into the soil (LOD0/1). They read as a rooted base, never as legs (they start inside
    // the collar at <=15 cm height and end 15 cm below grade).
    if (Lod <= 1 && K.Height >= 500)
    {
        const float R0 = K.TrunkRadius.IsValidIndex(1) ? K.TrunkRadius[1] : K.Breast;
        for (const auto& Lobe : Lobes)
        {
            const FVector RootDir(FMath::Cos(Lobe.Key), FMath::Sin(Lobe.Key), 0);
            const float Reach = R0 * FMath::Lerp(1.8F, 2.4F, (Lobe.Value - .05F) / .02F);
            const TArray<FVector> Path = {RootDir * R0 * .55F + FVector(0, 0, 15), RootDir * R0 * 1.15F + FVector(0, 0, 4),
                RootDir * Reach * .8F + FVector(0, 0, -4), RootDir * Reach + FVector(0, 0, -15)};
            Sweep(Out.Bark, Path, [&](int32 Ring, float) { return R0 * FMath::Lerp(.30F, .06F, Ring / 3.0F); },
                Lod == 0 ? 6 : 4, 1.0F, K.TileV, [](int32) { return FVector4f(.1F, 0, 0, 1); });
        }
    }
    for (const FLimb& Limb : K.Limbs)
    {
        if (Lod == 1 && Limb.Level >= (bOak ? 3 : 2)) continue;
        if (Lod == 2 && Limb.Level >= (bOak ? 2 : 1)) continue;
        if (Lod == 3 && Limb.Level >= 1) continue;
        // Thin twigs are sub-pixel at flight distance and alias; foliage cards carry them.
        if ((Limb.Level >= 3 && bOak) || (Limb.Level >= 2 && Limb.R0 < 1.4F)) continue;
        const int32 Sides = Limb.Level == 0 ? (Lod == 0 ? 10 : Lod == 1 ? 6 : 4) : Lod == 0 ? (Limb.R0 > 4 ? 6 : 4) : 3;
        const int32 N = Limb.Path.Num();
        Sweep(Out.Bark, Limb.Path, [&](int32 Ring, float) { return FMath::Lerp(Limb.R0, Limb.R1, Ring / static_cast<float>(N - 1)); },
            Sides, Limb.R0 > 12 ? 2.0F : 1.0F, K.TileV,
            [&](int32 Ring) { return FVector4f(1, FMath::Lerp(Limb.Wind0, Limb.Wind1, Ring / static_cast<float>(N - 1)), 0, 1); });
    }
    if (K.bHeroFoliage) { EmitHeroFoliage(K, Lod, Out); return; }
    // Regular cards carry near detail; mass (clump) cards carry canopy volume to mid/far LODs.
    // Kept cards are scaled up so projected crown coverage survives the LOD transitions.
    const int32 KeepRegular = Lod == 0 ? 1 : Lod == 1 ? 6 : 0;
    const float GrowRegular = Lod == 0 ? 1.0F : 1.9F;
    const int32 KeepMass = Lod == 0 ? (bOak ? 0 : 1) : Lod == 1 ? 1 : Lod == 2 ? (bOak ? 3 : 2) : (bOak ? 9 : 7);
    const float GrowMass = Lod <= 1 ? (bOak && Lod == 1 ? 1.5F : 1.0F) : Lod == 2 ? (bOak ? 2.2F : 1.8F) : (bOak ? 4.0F : 3.5F);
    int32 Regular = 0, Mass = 0;
    for (int32 Index = 0; Index < K.Tufts.Num(); ++Index)
    {
        const FTuft& Tuft = K.Tufts[Index];
        const int32 Keep = Tuft.bMass ? KeepMass : KeepRegular;
        int32& Counter = Tuft.bMass ? Mass : Regular;
        if (Keep <= 0 || (Counter++ * 7919 + Seed) % Keep != 0) continue;
        const float Grow = Tuft.bMass ? GrowMass : GrowRegular;
        const FVector Along = Tuft.Dir;
        Card(Out.Leaf, Tuft.Origin - Along * Tuft.Length * (Grow - 1) * .3F, Along, Tuft.Across, Tuft.Length * Grow,
            K.Islands[Tuft.Island % K.IslandCount], Tuft.Outward, FVector4f(Tuft.Occlusion, Tuft.Wind, 0, 1));
        // Near LODs: a second card crossed at 90 degrees turns each spray into a small volume.
        if (bCrossed && !Tuft.bMass && Lod <= 1)
            Card(Out.Leaf, Tuft.Origin - Along * Tuft.Length * (Grow - 1) * .3F, Along, FVector::CrossProduct(Along, Tuft.Across), Tuft.Length * Grow * .92F,
                K.Islands[(Tuft.Island + 1) % K.IslandCount], Tuft.Outward, FVector4f(Tuft.Occlusion * .92F, Tuft.Wind, 0, 1));
    }
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalHeroShapeBudgetTest, "AeroSim.Coastal.Flora.HeroShapeBudgetAndAnchors",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCoastalHeroShapeBudgetTest::RunTest(const FString& Parameters)
{
    for (const int32 Seed : {1101, 2203, 3307})
    {
        const FSkeleton Before = Seed == 3307 ? MakeOak(Seed) : MakeConifer(Seed == 2203 ? ESpecies::MontereyPine : ESpecies::DouglasFir, EAge::Mature, Seed);
        FSkeleton After = Before;
        RefineHeroShape(After);
        TestEqual(TEXT("Card allocation is preserved"), After.Tufts.Num(), Before.Tufts.Num());
        TestEqual(TEXT("Connected wood allocation is preserved"), After.Limbs.Num(), Before.Limbs.Num());
        TestEqual(TEXT("Placement crown radius is preserved"), After.CrownRadius, Before.CrownRadius);
        for (int32 Ring = 0; Ring < Before.Trunk.Num(); ++Ring)
            if (Before.Trunk[Ring].Z <= 450)
                TestTrue(TEXT("Grounding and low gate bole stay planted"), Before.Trunk[Ring].Equals(After.Trunk[Ring], .001));
        TestEqual(TEXT("Collision allocation is preserved"), After.Collision.Num(), Before.Collision.Num());
        for (int32 Index = 0; Index < Before.Collision.Num(); ++Index)
        {
            const FTrunkSegment& A = Before.Collision[Index]; const FTrunkSegment& B = After.Collision[Index];
            TestTrue(TEXT("Existing gate collision is unchanged"), A.Start.Equals(B.Start, .001F) && A.End.Equals(B.End, .001F) && A.Radius == B.Radius);
        }
        int32 FlatBefore = 0, FlatAfter = 0;
        for (int32 Index = 0; Index < After.Tufts.Num(); ++Index)
        {
            const FTuft& A = Before.Tufts[Index]; const FTuft& B = After.Tufts[Index];
            const FVector Normal = FVector::CrossProduct(B.Dir, B.Across).GetSafeNormal();
            TestTrue(TEXT("Compound twig cards have finite noncollapsed axes"), !B.Origin.ContainsNaN() && !Normal.ContainsNaN() && Normal.SizeSquared() > .99);
            if (FMath::Abs(FVector::CrossProduct(A.Dir, A.Across).GetSafeNormal().Z) > .95) ++FlatBefore;
            if (FMath::Abs(Normal.Z) > .95) ++FlatAfter;
        }
        if (Seed == 1101) TestTrue(TEXT("Fir sprays no longer share mostly horizontal planes"), FlatBefore > 0 && FlatAfter < FlatBefore * .75F);
        for (int32 Lod = 0; Lod < LodCount; ++Lod)
        {
            FLod A, B;
            EmitLod(Before, Lod, A, Seed); EmitLod(After, Lod, B, Seed);
            TestEqual(TEXT("No added wood vertices at any LOD"), B.Bark.P.Num(), A.Bark.P.Num());
            TestEqual(TEXT("No added foliage vertices at any LOD"), B.Leaf.P.Num(), A.Leaf.P.Num());
            TestEqual(TEXT("Wood triangle allocation is unchanged"), B.Bark.I.Num(), A.Bark.I.Num());
            TestEqual(TEXT("Foliage triangle allocation is unchanged"), B.Leaf.I.Num(), A.Leaf.I.Num());
            TestTrue(TEXT("Both reshaped slots retain real finite UV streaming density"), StreamingUVInfo(B.Bark).IsSet() && StreamingUVInfo(B.Leaf).IsSet());
        }
    }
    return true;
}
#endif


struct FSpec { const TCHAR* Name; ESpecies Species; EAge Age; int32 Seed; float Size = 1.0F; };
const FSpec Specs[] = {
    {TEXT("CoastalFloraFirMatureA"), ESpecies::DouglasFir, EAge::Mature, 1101}, {TEXT("CoastalFloraFirMatureB"), ESpecies::DouglasFir, EAge::Mature, 1187},
    {TEXT("CoastalFloraFirMatureC"), ESpecies::DouglasFir, EAge::Mature, 1243}, {TEXT("CoastalFloraFirPole"), ESpecies::DouglasFir, EAge::Pole, 1319},
    {TEXT("CoastalFloraPineMatureA"), ESpecies::MontereyPine, EAge::Mature, 2203}, {TEXT("CoastalFloraPineMatureB"), ESpecies::MontereyPine, EAge::Mature, 2281},
    {TEXT("CoastalFloraPinePole"), ESpecies::MontereyPine, EAge::Pole, 2347},
    {TEXT("CoastalFloraOakA"), ESpecies::CoastLiveOak, EAge::Mature, 3307}, {TEXT("CoastalFloraOakB"), ESpecies::CoastLiveOak, EAge::Mature, 3391},
    {TEXT("CoastalFloraOakC"), ESpecies::CoastLiveOak, EAge::Mature, 3433},
    {TEXT("CoastalFloraSaplingFirA"), ESpecies::DouglasFir, EAge::Sapling, 4409}, {TEXT("CoastalFloraSaplingFirB"), ESpecies::DouglasFir, EAge::Sapling, 4463},
    {TEXT("CoastalFloraSaplingPine"), ESpecies::MontereyPine, EAge::Sapling, 4517},
    {TEXT("CoastalFloraFirVeteran"), ESpecies::DouglasFir, EAge::Mature, 1409, 1.32F}, {TEXT("CoastalFloraPineVeteran"), ESpecies::MontereyPine, EAge::Mature, 2441, 1.25F},
    {TEXT("CoastalFloraFirYoung"), ESpecies::DouglasFir, EAge::Pole, 1523, .68F}, {TEXT("CoastalFloraPineYoung"), ESpecies::MontereyPine, EAge::Pole, 2557, .72F}};
/** The deterministic skeleton + R5 clustered foliage for one archetype spec. */
FSkeleton MakeSkeleton(const FSpec& Spec)
{
    FSkeleton K = Spec.Species == ESpecies::CoastLiveOak ? MakeOak(Spec.Seed) : MakeConifer(Spec.Species, Spec.Age, Spec.Seed, Spec.Size);
    // R4 wood shaping, then R5 connected branchlet clusters on every archetype (R6 spread).
    RefineHeroShape(K);
    // Per-archetype budget: LOD0 foliage never exceeds the R4 (single-card) slot of this same skeleton.
    FLod Legacy;
    EmitLod(K, 0, Legacy, Spec.Seed, false);
    const int32 Budget = Legacy.Leaf.I.Num() / 3;
    // Densest foliage the R4 slot allows: bisection on the branchlet density (deterministic).
    auto TrisAt = [&](float Density)
    {
        K.FoliageDensity = Density;
        BuildHeroFoliage(K, Spec.Seed);
        FLod Hero;
        EmitLod(K, 0, Hero, Spec.Seed);
        return Hero.Leaf.I.Num() / 3;
    };
    float Low = .15F, High = 2.2F;
    if (TrisAt(High) > Budget)
    {
        for (int32 Pass = 0; Pass < 9; ++Pass)
        {
            const float Mid = (Low + High) * .5F;
            (TrisAt(Mid) <= Budget ? Low : High) = Mid;
        }
        TrisAt(Low);
    }
    return K;
}

#if WITH_DEV_AUTOMATION_TESTS
namespace R5Metrics
{
struct FShelf { double HorizontalFraction = 0; double MaxBandContinuity = 0; };
FShelf Shelf(const FSection& S)
{
    FShelf Out; double Total = 0, Horizontal = 0;
    TMap<int32, TSet<int32>> Bands;
    for (int32 I = 0; I + 2 < S.I.Num(); I += 3)
    {
        const FVector3f A = S.P[S.I[I]], B = S.P[S.I[I + 1]], C = S.P[S.I[I + 2]];
        const FVector3f N = FVector3f::CrossProduct(B - A, C - A);
        const double Area = N.Size() * .5;
        if (Area <= 1e-3) continue;
        Total += Area;
        if (FMath::Abs(N.GetSafeNormal().Z) > .9F)
        {
            Horizontal += Area;
            const FVector3f Mid = (A + B + C) / 3.0F;
            if (FVector2f(Mid.X, Mid.Y).Size() > 60)
                Bands.FindOrAdd(FMath::FloorToInt(Mid.Z / 60.0F)).Add(FMath::FloorToInt((FMath::Atan2(Mid.Y, Mid.X) + PI) / (2 * PI) * 16) % 16);
        }
    }
    Out.HorizontalFraction = Total > 0 ? Horizontal / Total : 0;
    for (const auto& Band : Bands) Out.MaxBandContinuity = FMath::Max(Out.MaxBandContinuity, Band.Value.Num() / 16.0);
    return Out;
}
/** Orthographic coverage masks of the foliage on the XZ and YZ planes over fixed bounds. */
TBitArray<> Mask(const FSection& S, const FBox& Bounds, int32 Axis)
{
    // Rasterised at 4x4 supersampling and OR-reduced to 48x48, so thin sprays are not missed.
    constexpr int32 Out = 48, N = Out * 4;
    TBitArray<> Fine(false, N * N);
    const FVector Size = Bounds.GetSize();
    auto To = [&](const FVector3f& P) { return FVector2D((P[Axis] - Bounds.Min[Axis]) / Size[Axis] * N, (P.Z - Bounds.Min.Z) / Size.Z * N); };
    for (int32 I = 0; I + 2 < S.I.Num(); I += 3)
    {
        const FVector2D A = To(S.P[S.I[I]]), B = To(S.P[S.I[I + 1]]), C = To(S.P[S.I[I + 2]]);
        const int32 X0 = FMath::Clamp(FMath::FloorToInt(FMath::Min3(A.X, B.X, C.X)), 0, N - 1), X1 = FMath::Clamp(FMath::CeilToInt(FMath::Max3(A.X, B.X, C.X)), 0, N - 1);
        const int32 Y0 = FMath::Clamp(FMath::FloorToInt(FMath::Min3(A.Y, B.Y, C.Y)), 0, N - 1), Y1 = FMath::Clamp(FMath::CeilToInt(FMath::Max3(A.Y, B.Y, C.Y)), 0, N - 1);
        const double Den = (B.Y - C.Y) * (A.X - C.X) + (C.X - B.X) * (A.Y - C.Y);
        if (FMath::Abs(Den) < 1e-9) continue;
        for (int32 Y = Y0; Y <= Y1; ++Y) for (int32 X = X0; X <= X1; ++X)
        {
            const FVector2D P(X + .5, Y + .5);
            const double W0 = ((B.Y - C.Y) * (P.X - C.X) + (C.X - B.X) * (P.Y - C.Y)) / Den;
            const double W1 = ((C.Y - A.Y) * (P.X - C.X) + (A.X - C.X) * (P.Y - C.Y)) / Den;
            if (W0 >= 0 && W1 >= 0 && W0 + W1 <= 1) Fine[Y * N + X] = true;
        }
    }
    TBitArray<> Bits(false, Out * Out);
    for (int32 Y = 0; Y < N; ++Y) for (int32 X = 0; X < N; ++X) if (Fine[Y * N + X]) Bits[(Y / 4) * Out + X / 4] = true;
    return Bits;
}
double IoU(const TBitArray<>& A, const TBitArray<>& B)
{
    int32 Inter = 0, Union = 0;
    for (int32 I = 0; I < A.Num(); ++I) { Inter += A[I] && B[I]; Union += A[I] || B[I]; }
    return Union ? static_cast<double>(Inter) / Union : 0;
}
FBox Bounds(const FSection& S) { FBox Box(ForceInit); for (const FVector3f& P : S.P) Box += FVector(P); return Box.ExpandBy(1); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalR5RepresentativeCrownTest, "AeroSim.Coastal.Flora.R5RepresentativeCrowns",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCoastalR5RepresentativeCrownTest::RunTest(const FString& Parameters)
{
    using namespace R5Metrics;
    // R6: every archetype uses the R5 branchlet-cluster construction.
    for (const FSpec& Spec : Specs)
    {
        const int32 Seed = Spec.Seed;
        FSkeleton Legacy = Spec.Species == ESpecies::CoastLiveOak ? MakeOak(Seed) : MakeConifer(Spec.Species, Spec.Age, Seed, Spec.Size);
        RefineHeroShape(Legacy);                                      // R4 slot baseline (same skeleton)
        const FSkeleton Hero = MakeSkeleton(Spec);
        TestEqual(TEXT("Wood/collision anchors are unchanged by the foliage rebuild"), Hero.Collision.Num(), Legacy.Collision.Num());
        for (int32 I = 0; I < Hero.Collision.Num(); ++I)
            TestTrue(FString::Printf(TEXT("%s: Collision segment identical"), Spec.Name), Hero.Collision[I].Start.Equals(Legacy.Collision[I].Start, .001F) && Hero.Collision[I].End.Equals(Legacy.Collision[I].End, .001F));
        TestTrue(FString::Printf(TEXT("%s: Crown is organised into many connected clusters"), Spec.Name), Hero.Clusters.Num() >= (Spec.Age == EAge::Sapling ? 8 : Spec.Age == EAge::Pole ? 20 : 40));
        int32 Lobes = 0; TSet<int32> LobeIds; for (const FCluster& C : Hero.Clusters) { LobeIds.Add(C.Lobe); TestTrue(FString::Printf(TEXT("%s: Every cluster owns cards"), Spec.Name), C.Cards >= 1); }
        Lobes = LobeIds.Num();
        if (Spec.Species == ESpecies::CoastLiveOak) TestTrue(FString::Printf(TEXT("%s: Oak crown has 3-5 lobes"), Spec.Name), Lobes >= 3 && Lobes <= 5);
        for (const FTuft& T : Hero.Tufts) TestTrue(FString::Printf(TEXT("%s: Every card belongs to a cluster"), Spec.Name), Hero.Clusters.IsValidIndex(T.Cluster));
        FLod R4, Lods[LodCount];
        EmitLod(Legacy, 0, R4, Seed, false);
        for (int32 Lod = 0; Lod < LodCount; ++Lod) EmitLod(Hero, Lod, Lods[Lod], Seed);
        const int32 R4Tris = R4.Leaf.I.Num() / 3, HeroTris = Lods[0].Leaf.I.Num() / 3;
        TestTrue(FString::Printf(TEXT("%s: LOD0 foliage triangles do not exceed the R4 slot baseline"), Spec.Name), HeroTris <= R4Tris);
        for (int32 Lod = 1; Lod < LodCount; ++Lod) TestTrue(FString::Printf(TEXT("%s: Foliage triangles fall with each LOD"), Spec.Name), Lods[Lod].Leaf.I.Num() < Lods[Lod - 1].Leaf.I.Num());
        const FShelf Before = Shelf(R4.Leaf), After = Shelf(Lods[0].Leaf);
        TestTrue(FString::Printf(TEXT("%s: Horizontal card area at most 70%% of the R4 shelf crown"), Spec.Name), After.HorizontalFraction <= FMath::Max(Before.HorizontalFraction * .7 + .01, .05));
        TestTrue(FString::Printf(TEXT("%s: No height band carries horizontal foliage around more than half the crown"), Spec.Name), After.MaxBandContinuity <= .5);
        const FBox Box = Bounds(Lods[0].Leaf);
        double Stability[LodCount] = {1, 0, 0, 0}, LegacyStability[LodCount] = {1, 0, 0, 0};
        for (int32 Lod = 1; Lod < LodCount; ++Lod)
        {
            Stability[Lod] = FMath::Min(IoU(Mask(Lods[0].Leaf, Box, 0), Mask(Lods[Lod].Leaf, Box, 0)), IoU(Mask(Lods[0].Leaf, Box, 1), Mask(Lods[Lod].Leaf, Box, 1)));
            FLod L0, LK; EmitLod(Legacy, 0, L0, Seed, false); EmitLod(Legacy, Lod, LK, Seed, false);
            const FBox LegacyBox = Bounds(L0.Leaf);
            LegacyStability[Lod] = FMath::Min(IoU(Mask(L0.Leaf, LegacyBox, 0), Mask(LK.Leaf, LegacyBox, 0)), IoU(Mask(L0.Leaf, LegacyBox, 1), Mask(LK.Leaf, LegacyBox, 1)));
            const FBox LodBox = Bounds(Lods[Lod].Leaf);
            TestTrue(FString::Printf(TEXT("%s: Coarser LOD stays inside the LOD0 crown envelope (+15%%)"), Spec.Name), LodBox.GetSize().X <= Box.GetSize().X * 1.15 && LodBox.GetSize().Y <= Box.GetSize().Y * 1.15 && LodBox.GetSize().Z <= Box.GetSize().Z * 1.15);
        }
        {
            const TBitArray<> M0 = Mask(Lods[0].Leaf, Box, 0), M2 = Mask(Lods[2].Leaf, Box, 0), M3 = Mask(Lods[3].Leaf, Box, 0);
            // Same frame for both crowns (union of bounds), both side axes summed.
            const FBox Shared = Bounds(R4.Leaf) + Box;
            const int32 R4Cover = Mask(R4.Leaf, Shared, 0).CountSetBits() + Mask(R4.Leaf, Shared, 1).CountSetBits();
            const int32 HeroCover = Mask(Lods[0].Leaf, Shared, 0).CountSetBits() + Mask(Lods[0].Leaf, Shared, 1).CountSetBits();
            const TBitArray<> MR4 = Mask(R4.Leaf, Bounds(R4.Leaf), 0);
            AddInfo(FString::Printf(TEXT("FLORA_R5_COVERAGE %s seed=%d r4_lod0=%d lod0=%d lod2=%d lod3=%d (of %d)"), Spec.Name, Seed, MR4.CountSetBits(), M0.CountSetBits(), M2.CountSetBits(), M3.CountSetBits(), M0.Num()));
            AddInfo(FString::Printf(TEXT("FLORA_R5_SIDECOVER %s r4=%d r5=%d ratio=%.2f"), Spec.Name, R4Cover, HeroCover, HeroCover / FMath::Max(1.0F, static_cast<float>(R4Cover))));
            TestTrue(FString::Printf(TEXT("%s: Side-view crown coverage stays at least 75%% of the R4 crown (no thinning into bare poles)"), Spec.Name), HeroCover >= R4Cover * .75F);
        }
        TestTrue(FString::Printf(TEXT("%s: LOD1 silhouette IoU >= 0.55"), Spec.Name), Stability[1] >= .55);
        TestTrue(FString::Printf(TEXT("%s: LOD2 silhouette IoU >= 0.45"), Spec.Name), Stability[2] >= .45);
        TestTrue(FString::Printf(TEXT("%s: LOD3 silhouette IoU >= 0.30"), Spec.Name), Stability[3] >= .30);
        TestTrue(FString::Printf(TEXT("%s: UV streaming density stays valid on both slots"), Spec.Name), StreamingUVInfo(Lods[0].Bark).IsSet() && StreamingUVInfo(Lods[0].Leaf).IsSet());
        AddInfo(FString::Printf(TEXT("FLORA_R5 %s seed=%d clusters=%d lobes=%d leaf_tris_r4=%d leaf_tris_lod=%d/%d/%d/%d horizontal=%.3f->%.3f band=%.2f->%.2f iou=%.2f/%.2f/%.2f (r4 %.2f/%.2f/%.2f)"),
            Spec.Name, Seed, Hero.Clusters.Num(), Lobes, R4Tris, HeroTris, Lods[1].Leaf.I.Num() / 3, Lods[2].Leaf.I.Num() / 3, Lods[3].Leaf.I.Num() / 3,
            Before.HorizontalFraction, After.HorizontalFraction, Before.MaxBandContinuity, After.MaxBandContinuity,
            Stability[1], Stability[2], Stability[3], LegacyStability[1], LegacyStability[2], LegacyStability[3]));
    }
    return true;
}
#endif

UStaticMesh* BuildMesh(const FName Name, const TArray<FLod>& Lods, TArray<int32>& OutTriangles)
{
    TArray<FMeshDescription> Descriptions; Descriptions.SetNum(Lods.Num());
    TArray<const FMeshDescription*> Pointers;
    for (int32 Lod = 0; Lod < Lods.Num(); ++Lod)
    {
        FMeshDescription& Description = Descriptions[Lod];
        FStaticMeshAttributes Attributes(Description); Attributes.Register();
        Attributes.GetVertexInstanceUVs().SetNumChannels(1);
        const FSection* Sections[2] = {&Lods[Lod].Bark, &Lods[Lod].Leaf};
        int32 Triangles = 0;
        for (int32 SectionIndex = 0; SectionIndex < 2; ++SectionIndex)
        {
            const FSection& S = *Sections[SectionIndex];
            const FPolygonGroupID Group = Description.CreatePolygonGroup();
            Attributes.GetPolygonGroupMaterialSlotNames()[Group] = SectionIndex == 0 ? TEXT("Bark") : TEXT("Foliage");
            TArray<FVertexInstanceID> Instances; Instances.Reserve(S.P.Num());
            for (int32 V = 0; V < S.P.Num(); ++V)
            {
                const FVertexID Vertex = Description.CreateVertex();
                Attributes.GetVertexPositions()[Vertex] = S.P[V];
                const FVertexInstanceID Instance = Description.CreateVertexInstance(Vertex);
                Attributes.GetVertexInstanceNormals()[Instance] = S.N[V];
                Attributes.GetVertexInstanceTangents()[Instance] = S.T[V];
                Attributes.GetVertexInstanceBinormalSigns()[Instance] = S.Sign[V];
                Attributes.GetVertexInstanceUVs().Set(Instance, 0, S.UV[V]);
                Attributes.GetVertexInstanceColors()[Instance] = S.C[V];
                Instances.Add(Instance);
            }
            for (int32 I = 0; I + 2 < S.I.Num(); I += 3)
                Description.CreateTriangle(Group, {Instances[S.I[I]], Instances[S.I[I + 1]], Instances[S.I[I + 2]]});
            Triangles += S.I.Num() / 3;
        }
        OutTriangles.Add(Triangles);
        Pointers.Add(&Description);
    }
    // An intentionally empty slot (e.g. the litter disc has no bark) publishes a neutral density.
    auto SlotInfo = [](const FSection& S) -> TOptional<FMeshUVChannelInfo> { return S.I.IsEmpty() ? TOptional<FMeshUVChannelInfo>(FMeshUVChannelInfo(100.0F)) : StreamingUVInfo(S); };
    const TOptional<FMeshUVChannelInfo> SlotUV[2] = {SlotInfo(Lods[0].Bark), SlotInfo(Lods[0].Leaf)};
    if (!SlotUV[0].IsSet() || !SlotUV[1].IsSet())
    {
        UE_LOG(LogTemp, Error, TEXT("FLORA_UV_STREAMING_INVALID mesh=%s bark=%d foliage=%d"), *Name.ToString(), SlotUV[0].IsSet(), SlotUV[1].IsSet());
        return nullptr;
    }
    UStaticMesh* Mesh = NewObject<UStaticMesh>(GetTransientPackage(), Name, RF_Transient);
    // Runtime fast-built meshes never request a mesh distance field (no async DF build/render-thread cost).
    Mesh->bGenerateMeshDistanceField = false;
    const FName SlotNames[2] = {TEXT("Bark"), TEXT("Foliage")};
    for (int32 Slot = 0; Slot < 2; ++Slot)
    {
        FStaticMaterial Material(nullptr, SlotNames[Slot]);
        Material.UVChannelData = SlotUV[Slot].GetValue();
        Mesh->GetStaticMaterials().Add(Material);
        UE_LOG(LogTemp, Display, TEXT("FLORA_UV_STREAMING mesh=%s slot=%d name=%s initialized=1 uv0_cm_per_uv=%.6f"),
            *Name.ToString(), Slot, *SlotNames[Slot].ToString(), Material.UVChannelData.LocalUVDensities[0]);
    }
    UStaticMesh::FBuildMeshDescriptionsParams Params;
    Params.bFastBuild = true; Params.bCommitMeshDescription = false; Params.bMarkPackageDirty = false; Params.bBuildSimpleCollision = false;
    Params.bAllowCpuAccess = false;
    Mesh->BuildFromMeshDescriptions(Pointers, Params);
    if (FStaticMeshRenderData* Data = Mesh->GetRenderData())
        for (int32 Lod = 0; Lod < Data->LODResources.Num() && Lod < LodCount; ++Lod) Data->ScreenSize[Lod].Default = LodScreenSize[Lod];
    Mesh->AddToRoot();
    return Mesh;
}

FArchetype MakeArchetype(const FName Name, const FSkeleton& K, EAge Age, uint32 Seed)
{
    FArchetype A; A.Name = Name; A.Species = K.Species; A.Age = Age; A.Height = K.Height; A.CrownBase = K.CrownBase;
    A.CrownRadius = K.CrownRadius; A.Collision = K.Collision; A.BuriedDepth = Buried; A.StemCount = K.Stems;
    TArray<FLod> Lods; Lods.SetNum(LodCount);
    for (int32 Lod = 0; Lod < LodCount; ++Lod) EmitLod(K, Lod, Lods[Lod], Seed);
    // Measure the real LOD0 trunk vertices: collar width, breast radius and stem-near foliage.
    const FSection& Bark = Lods[0].Bark;
    const int32 TrunkVertices = K.Trunk.Num() * 15;
    float Breast = 0; int32 BreastCount = 0;
    A.CollarHeight = 76.0F;
    for (int32 V = 0; V < TrunkVertices && V < Bark.P.Num(); ++V)
    {
        const FVector3f P = Bark.P[V];
        const float Radius = FVector2f(P.X - K.Trunk[V / 15].X, P.Y - K.Trunk[V / 15].Y).Size();
        if (P.Z >= -1 && P.Z <= A.CollarHeight) A.CollarRadius = FMath::Max(A.CollarRadius, Radius);
        if (FMath::IsNearlyEqual(P.Z, 130.0F, 1.0F)) { Breast += Radius; ++BreastCount; }
    }
    A.BreastRadius = BreastCount ? Breast / BreastCount : K.Breast;
    A.LowestFoliageNearStem = K.Height;
    const FSection& Leaf = Lods[0].Leaf;
    for (const FVector3f& P : Leaf.P) if (FVector2f(P.X, P.Y).Size() < 300) A.LowestFoliageNearStem = FMath::Min(A.LowestFoliageNearStem, P.Z);
    for (const FLimb& Limb : K.Limbs)
        if (Limb.Level >= 1 && Limb.Level < 3)
            for (const FVector& P : Limb.Path) if (FVector2D(P.X, P.Y).Size() < 300) A.LowestFoliageNearStem = FMath::Min<float>(A.LowestFoliageNearStem, P.Z);
    A.Mesh = BuildMesh(Name, Lods, A.LodTriangles);
    return A;
}
}

UStaticMesh* GetTrunkLitterMesh()
{
    static UStaticMesh* Mesh = nullptr;
    if (Mesh) return Mesh;
    TArray<FLod> Lods; Lods.SetNum(1);
    FSection& S = Lods[0].Leaf;
    constexpr int32 Segments = 24;
    const int32 Centre = S.Add(FVector::ZeroVector, FVector::UpVector, FVector::ForwardVector, -FVector::RightVector, FVector2f(.5F, .5F), FVector4f(1, 0, 0, 1));
    for (int32 I = 0; I < Segments; ++I)
    {
        const float A = I * 2 * PI / Segments;
        const FVector P(FMath::Cos(A) * 100.0F, FMath::Sin(A) * 100.0F, 0);
        S.Add(P, FVector::UpVector, FVector::ForwardVector, -FVector::RightVector, FVector2f(.5F + P.X / 200.0F, .5F - P.Y / 200.0F), FVector4f(1, 0, 0, 1));
    }
    for (int32 I = 0; I < Segments; ++I) S.Tri(Centre, 1 + I, 1 + (I + 1) % Segments);
    TArray<int32> Triangles;
    Mesh = BuildMesh(TEXT("CoastalFloraTrunkLitterDisc"), Lods, Triangles);
    return Mesh;
}

const TArray<FArchetype>& GetArchetypes()
{
    static TArray<FArchetype> Archetypes;
    if (!Archetypes.IsEmpty()) return Archetypes;
    for (const FSpec& Spec : Specs)
    {
        const FSkeleton K = MakeSkeleton(Spec);
        FArchetype A = MakeArchetype(Spec.Name, K, Spec.Age, Spec.Seed);
        if (Spec.Age == EAge::Sapling) A.Species = ESpecies::Sapling;
        Archetypes.Add(MoveTemp(A));
    }
    return Archetypes;
}

// ---------------------------------------------------------------------------------
// Placement
// ---------------------------------------------------------------------------------
namespace
{
const FVector2D WalkCenter(-9000, 8500);
const FVector2D WalkAlong(FMath::Cos(FMath::DegreesToRadians(18.0F)), FMath::Sin(FMath::DegreesToRadians(18.0F)));
double SegmentDistance(const FVector2D& P, const FVector2D& A, const FVector2D& B)
{
    const FVector2D Delta = B - A;
    const double T = FMath::Clamp(FVector2D::DotProduct(P - A, Delta) / FMath::Max(Delta.SizeSquared(), 1e-6), 0.0, 1.0);
    return FVector2D::Distance(P, A + Delta * T);
}
const TArray<FVector2D>& TrailPolyline(int32 Side)
{
    static TArray<FVector2D> Sides[2];
    TArray<FVector2D>& Out = Sides[Side > 0 ? 1 : 0];
    if (Out.IsEmpty())
    {
        const FVector2D Across(-WalkAlong.Y, WalkAlong.X);
        for (int32 Row = 0; Row <= 120; ++Row)
        {
            const float Distance = 1520 + Row * 55;
            const float Bend = FMath::Square(FMath::Max(0.0F, Distance - 2200) / 6000.0F) * 1600;
            Out.Add(WalkCenter + WalkAlong * (Side * Distance) + Across * Bend);
        }
    }
    return Out;
}
const FVector2D LaunchPath[] = {FVector2D(-400, 350), FVector2D(-4000, 2500), FVector2D(-10500, 5000), FVector2D(-14000, 6900)};
// Training corridor: enters in front of the `forest` review pose, loops through the
// southern stand and exits back onto the open training field.
const FVector2D CorridorControl[] = {FVector2D(-15500, -6600), FVector2D(-15900, -7500), FVector2D(-17600, -10600), FVector2D(-19800, -14200),
    FVector2D(-20600, -18600), FVector2D(-17800, -22200), FVector2D(-12600, -23000), FVector2D(-8200, -20400), FVector2D(-6200, -15600),
    FVector2D(-5600, -10600), FVector2D(-5200, -7300), FVector2D(-5100, -6400)};
const TArray<FCorridorSample>& CorridorSamples()
{
    static TArray<FCorridorSample> Samples;
    if (!Samples.IsEmpty()) return Samples;
    constexpr int32 Count = UE_ARRAY_COUNT(CorridorControl);
    float Distance = 0;
    FVector2D Previous = CorridorControl[0];
    for (int32 Segment = 0; Segment + 1 < Count; ++Segment)
    {
        const FVector2D P0 = CorridorControl[FMath::Max(Segment - 1, 0)], P1 = CorridorControl[Segment];
        const FVector2D P2 = CorridorControl[Segment + 1], P3 = CorridorControl[FMath::Min(Segment + 2, Count - 1)];
        const int32 Steps = FMath::Max(2, FMath::CeilToInt(FVector2D::Distance(P1, P2) / 50.0F));
        for (int32 Step = Segment == 0 ? 0 : 1; Step <= Steps; ++Step)
        {
            const float T = Step / static_cast<float>(Steps), T2 = T * T, T3 = T2 * T;
            const FVector2D Point = .5F * (2 * P1 + (P2 - P0) * T + (2 * P0 - 5 * P1 + 4 * P2 - P3) * T2 + (3 * P1 - P0 - 3 * P2 + P3) * T3);
            Distance += FVector2D::Distance(Point, Previous);
            Previous = Point;
            FCorridorSample Sample; Sample.Point = Point; Sample.Distance = Distance;
            // Clear width 4.2–5.8 m (half 210–290 cm), varying slowly along the run.
            Sample.HalfWidth = 250.0F + 40.0F * FMath::Sin(Distance / 3700.0F) * FMath::Cos(Distance / 9100.0F + 1.3F);
            // A natural clearing mouth (up to 9 m wide) makes the entrance legible from the field.
            Sample.HalfWidth = FMath::Max(Sample.HalfWidth, 450.0F - 200.0F * Smooth01(Distance / 2200.0F));
            Samples.Add(Sample);
        }
    }
    for (int32 Index = 0; Index < Samples.Num(); ++Index)
        Samples[Index].Tangent = (Samples[FMath::Min(Index + 1, Samples.Num() - 1)].Point - Samples[FMath::Max(Index - 1, 0)].Point).GetSafeNormal();
    return Samples;
}

struct FGrid
{
    TMap<FIntPoint, TArray<int32>> Cells;
    static constexpr float Cell = 600.0F;
    FIntPoint Key(const FVector2D& P) const { return FIntPoint(FMath::FloorToInt(P.X / Cell), FMath::FloorToInt(P.Y / Cell)); }
    void Add(const FVector2D& P, int32 Index) { Cells.FindOrAdd(Key(P)).Add(Index); }
    template<class F> void Near(const FVector2D& P, F&& Visit) const
    {
        const FIntPoint K = Key(P);
        for (int32 DX = -1; DX <= 1; ++DX) for (int32 DY = -1; DY <= 1; ++DY)
            if (const TArray<int32>* Items = Cells.Find(K + FIntPoint(DX, DY))) for (int32 Item : *Items) Visit(Item);
    }
};

float NearEastEdge(float Y) { return FMath::Min(10300.0F, APacificCoastGraybox::GetCliffEdgeX(Y) - 2600.0F); }
}

float CorridorDistance(const FVector2D& Point, float* OutHalfWidth, FVector2D* OutTangent)
{
    const TArray<FCorridorSample>& Samples = CorridorSamples();
    double Best = MAX_dbl; int32 BestIndex = 0;
    for (int32 Index = 1; Index < Samples.Num(); ++Index)
    {
        const double D = SegmentDistance(Point, Samples[Index - 1].Point, Samples[Index].Point);
        if (D < Best) { Best = D; BestIndex = Index; }
    }
    if (OutHalfWidth) *OutHalfWidth = Samples[BestIndex].HalfWidth;
    if (OutTangent) *OutTangent = Samples[BestIndex].Tangent;
    return Best;
}

float BoardwalkDistance(const FVector2D& Point)
{
    const FVector2D Relative = Point - WalkCenter;
    const double Along = FVector2D::DotProduct(Relative, WalkAlong);
    const double Across = -Relative.X * WalkAlong.Y + Relative.Y * WalkAlong.X;
    const double DX = FMath::Max(FMath::Abs(Along) - 1300.0, 0.0), DY = FMath::Max(FMath::Abs(Across) - 110.0, 0.0);
    return FMath::Sqrt(DX * DX + DY * DY);
}

bool IsProtected(const FVector2D& Point, float Clearance)
{
    const TArray<FVector>& Route = FPacificCoastGrayboxContract::GetPlateauClearancePoints();
    for (int32 Index = 1; Index < Route.Num(); ++Index)
        if (SegmentDistance(Point, FVector2D(Route[Index - 1]), FVector2D(Route[Index])) < FMath::Max(Clearance, 2600.0F)) return true;
    for (int32 Index = 1; Index < 4; ++Index)
        if (SegmentDistance(Point, LaunchPath[Index - 1], LaunchPath[Index]) < Clearance + 300) return true;
    if (BoardwalkDistance(Point) < FMath::Max(Clearance, 960.0F)) return true;
    for (int32 Side = -1; Side <= 1; Side += 2)
    {
        const TArray<FVector2D>& Trail = TrailPolyline(Side);
        if (FVector2D::DistSquared(Point, Trail[60]) > FMath::Square(6000.0F) && FVector2D::DistSquared(Point, Trail[0]) > FMath::Square(6000.0F)) continue;
        for (int32 Index = 1; Index < Trail.Num(); ++Index)
            if (SegmentDistance(Point, Trail[Index - 1], Trail[Index]) < Clearance) return true;
    }
    return false;
}

bool IsWalkway(const FVector2D& Point, float Clearance)
{
    for (int32 Index = 1; Index < 4; ++Index)
        if (SegmentDistance(Point, LaunchPath[Index - 1], LaunchPath[Index]) < Clearance) return true;
    if (BoardwalkDistance(Point) < Clearance) return true;
    for (int32 Side = -1; Side <= 1; Side += 2)
    {
        const TArray<FVector2D>& Trail = TrailPolyline(Side);
        for (int32 Index = 1; Index < Trail.Num(); ++Index)
            if (SegmentDistance(Point, Trail[Index - 1], Trail[Index]) < Clearance) return true;
    }
    return false;
}

float NearStandDensity(const float X, const float Y)
{
    const float AbsY = FMath::Abs(Y);
    // Irregular macro outline (40-120 m lobes) plus a fine ragged edge; never a parallelogram.
    // The south field edge stays close in front of the `forest` review pose and corridor entry.
    const float EntryKeep = Y < 0 ? FMath::Clamp(FMath::Abs(X + 15800.0F) / 5000.0F - .4F, 0.0F, 1.0F) : 1.0F;
    const float FieldEdge = 7700.0F + 900.0F * Fbm(X, Y > 0 ? 1000.0F : -1000.0F, 5200.0F) - 450.0F
        + EntryKeep * 8000.0F * FMath::Max(0.0F, Fbm(X + 3100.0F, Y > 0 ? 7000.0F : -7000.0F, 15000.0F) - .40F);
    // R9: two-scale outline noise (60-160 m lobes plus 20-50 m bays) so groves never read as blocks.
    const float OuterEdge = 25000.0F + 10000.0F * (Fbm(X + 5000, Y, 16000.0F) - .5F) + 3000.0F * (Fbm(X - 900, Y + 400, 4800.0F) - .5F);
    const float WestEdge = -29000.0F + 11000.0F * (Fbm(Y, X, 14000.0F) - .5F) + 3000.0F * (Fbm(Y + 700, X, 4500.0F) - .5F);
    const float EastEdge = NearEastEdge(Y) - 1800.0F * Fbm(Y, 300.0F, 4200.0F) - 8000.0F * FMath::Max(0.0F, Fbm(Y - 2000.0F, 900.0F, 11000.0F) - .38F);
    // R10: erode the distance-to-edge field itself with 25-60 m noise, so every side (incl. the
    // north grove) gets bays and promontories rather than four straight edges.
    const float Erosion = 3200.0F * (Fbm(X + 11000.0F, Y - 3000.0F, 3600.0F) - .35F) + 1400.0F * (Fbm(X - 2700.0F, Y + 5100.0F, 1500.0F) - .5F);
    const float Inside = FMath::Min(FMath::Min(AbsY - FieldEdge, OuterEdge - AbsY), FMath::Min(X - WestEdge, EastEdge - X)) - (Y > 0 ? 1.4F : 1.0F) * Erosion;
    if (Inside <= 0)
    {
        // Outside the core stands, woodland-class ground (DEV-TERRAIN habitat) carries a looser
        // forest out along shaded slopes and gullies, so stands have no drawn outline.
        if (AbsY < FieldEdge || AbsY > 40000.0F || X < -44000.0F || X > EastEdge) return 0;
        const FCoastHabitatSample Habitat = FPacificCoastHabitat::Sample(X, Y);
        const float Woodland = Habitat.Woodland + .5F * Habitat.ShelteredGrass - .6F * Habitat.Rock;
        const float Reach = 1.0F - Smooth01(-Inside / 9000.0F);
        const float Extension = .72F * Smooth01((Woodland - .32F + .25F * (Fbm(X - 700, Y + 900, 5200.0F) - .5F)) / .28F) * (.35F + .65F * Reach);
        // Feathered edge: scattered outlier trees and small clumps up to ~30 m beyond the outline.
        const float Outlier = .22F * (1.0F - Smooth01(-Inside / 3000.0F)) * Smooth01((Fbm(X + 300, Y - 800, 1900.0F) - .52F) / .1F);
        return FMath::Max(Extension, Outlier);
    }
    // Ragged 6-20 m feather from sparse edge trees into the closed stand.
    const float Feather = .28F + .72F * Smooth01(Inside / (600.0F + 1400.0F * Fbm(X, Y, 3100.0F)));
    // Small natural glades keep openings without thinning the stand everywhere.
    float Glade = Fbm(X + 9100, Y - 4700, 4600.0F);
    // More, larger clearings (~14% of area), but never on the training corridor's flanks.
    if (Glade > .56F) { float Half = 0; if (CorridorDistance(FVector2D(X, Y), &Half) < Half + 1800.0F) Glade = 0; }
    // DEV-TERRAIN habitat: steep rock and strongly sun-exposed dry grass break the stand.
    const FCoastHabitatSample Habitat = FPacificCoastHabitat::Sample(X, Y);
    // Habitat shapes only the outer ~40 m of each stand: the dense training core is never erased.
    const float Hostile = (Habitat.Rock + Habitat.Shore + FMath::Max(0.0F, Habitat.ExposedGrass - .55F) * Smooth01(Habitat.SunExposure / .5F))
        * (1.0F - Smooth01(Inside / 4000.0F));
    // Natural clumping: 0.88-1.0 density modulation at ~25 m scale.
    const float Clump = .88F + .12F * Smooth01((Fbm(X - 4100, Y + 2600, 2600.0F) - .35F) / .3F);
    return Feather * Clump * (1.0F - Smooth01((Glade - .60F) / .06F)) * (1.0F - Smooth01((Hostile - .25F) / .35F));
}

FTransform InstanceTransform(const FPlacedTree& Tree)
{
    const FQuat Lean = FQuat(FVector(-FMath::Sin(Tree.LeanYaw), FMath::Cos(Tree.LeanYaw), 0), Tree.LeanPitch);
    return FTransform(Lean * FQuat(FVector::UpVector, Tree.Yaw), Tree.Base, FVector(Tree.Scale * Tree.Girth, Tree.Scale * Tree.Girth, Tree.Scale));
}

const FLayout& GetLayout()
{
    static FLayout Layout;
    static bool bBuilt = false;
    if (bBuilt) return Layout;
    bBuilt = true;
    const TArray<FArchetype>& Archetypes = GetArchetypes();
    TArray<int32> Firs, Pines, Oaks, Saplings, FirPoles, PinePoles, Veterans;
    for (int32 Index = 0; Index < Archetypes.Num(); ++Index)
    {
        const FArchetype& A = Archetypes[Index];
        if (A.Species == ESpecies::Sapling) Saplings.Add(Index);
        else if (A.Species == ESpecies::CoastLiveOak) Oaks.Add(Index);
        else if (A.Age == EAge::Pole) (A.Species == ESpecies::DouglasFir ? FirPoles : PinePoles).Add(Index);
        else if (A.Name.ToString().Contains(TEXT("Veteran"))) Veterans.Add(Index);
        else (A.Species == ESpecies::DouglasFir ? Firs : Pines).Add(Index);
    }
    Layout.Corridor = CorridorSamples();
    FGrid Grid;
    TArray<float> Radii; // effective trunk-spacing radius per placed tree
    auto Ground = [](const FVector2D& P, float CollarRadius)
    {
        // Sit on the lowest sampled ground under the collar so no side floats; the buried
        // 60 cm trunk/collar continues below the surface on the uphill side.
        float Z = APacificCoastGraybox::GetCoastalLandHeight(P.X, P.Y);
        for (int32 Sample = 0; Sample < 8; ++Sample)
        {
            const float Angle = Sample * PI * .25F;
            Z = FMath::Min(Z, APacificCoastGraybox::GetCoastalLandHeight(P.X + FMath::Cos(Angle) * CollarRadius, P.Y + FMath::Sin(Angle) * CollarRadius));
        }
        return Z - 4.0F;
    };
    float NextGirth = 1.0F; // consumed by the next Place
    auto Place = [&](const FVector2D& P, int32 Archetype, float Scale, float Yaw, float LeanPitch, float LeanYaw, bool bNear, bool bFlank, float SpacingRadius)
    {
        FPlacedTree Tree;
        // Lean tilts the collar: lower by the lift on the uphill side of the tilt.
        Tree.Girth = NextGirth; NextGirth = 1.0F;
        const float Collar = Archetypes[Archetype].CollarRadius * Scale * Tree.Girth;
        Tree.Base = FVector(P.X, P.Y, Ground(P, Collar) - Collar * FMath::Sin(LeanPitch) - 2.0F);
        Tree.Archetype = Archetype; Tree.Scale = Scale; Tree.Yaw = Yaw; Tree.LeanPitch = LeanPitch; Tree.LeanYaw = LeanYaw;
        Tree.bNearForest = bNear; Tree.bCorridorFlank = bFlank;
        const int32 Index = Layout.Trees.Add(Tree);
        Radii.Add(SpacingRadius);
        Grid.Add(P, Index);
    };
    auto Clear = [&](const FVector2D& P, float SpacingRadius)
    {
        bool bClear = true;
        Grid.Near(P, [&](int32 Other)
        {
            if (bClear && FVector2D::DistSquared(P, FVector2D(Layout.Trees[Other].Base)) < FMath::Square(SpacingRadius + Radii[Other])) bClear = false;
        });
        return bClear;
    };
    FRandomStream Random(21630);
    // 1. Corridor gates and flanks: deterministic constrictions (2.6–3.8 m surface gap)
    //    and edge rows so the cleared run reads as a corridor.
    // Corridor trees: varied conifers (incl. veterans) whose live crown stays above drone height.
    TArray<int32> CorridorTrees;
    for (int32 Index : Firs) if (Archetypes[Index].LowestFoliageNearStem >= 520) CorridorTrees.Add(Index);
    for (int32 Index : Pines) if (Archetypes[Index].LowestFoliageNearStem >= 520) CorridorTrees.Add(Index);
    check(!CorridorTrees.IsEmpty());
    const int32 GateArchetype = CorridorTrees[0];
    const float GateRadius = Archetypes[GateArchetype].BreastRadius;
    // The first 60 m stay an open, straight sightline from the `corridor` pose; constrictions start beyond it.
    const float GateDistances[] = {6500, 11500, 17000, 23000, 29500, 36000, 41000};
    for (int32 Gate = 0; Gate < UE_ARRAY_COUNT(GateDistances); ++Gate)
    {
        const FCorridorSample* Sample = Layout.Corridor.FindByPredicate([&](const FCorridorSample& S) { return S.Distance >= GateDistances[Gate]; });
        if (!Sample) continue;
        const float Gap = 260.0F + 120.0F * Gate / (UE_ARRAY_COUNT(GateDistances) - 1.0F) * (Gate % 2 ? .4F : 1.0F);
        // Asymmetric offset: the gap is not always centred, the pilot must line up.
        const float Shift = (Gate % 3 - 1) * 45.0F;
        const FVector2D Normal(-Sample->Tangent.Y, Sample->Tangent.X);
        for (int32 Side = -1; Side <= 1; Side += 2)
        {
            const FVector2D P = Sample->Point + Normal * (Shift + Side * (Gap * .5F + GateRadius));
            Place(P, GateArchetype, 1.0F, Random.FRandRange(0, 2 * PI), 0, 0, true, true, GateRadius + 40);
        }
        Layout.GateGaps.Add(Gap);
    }
    for (int32 Side = -1; Side <= 1; Side += 2)
    {
        float Next = Side < 0 ? 900.0F : 1300.0F;
        for (const FCorridorSample& Sample : Layout.Corridor)
        {
            if (Sample.Distance < Next) continue;
            Next = Sample.Distance + Random.FRandRange(420, 1150); // irregular along-lane spacing
            const int32 Archetype = CorridorTrees[Random.RandRange(0, CorridorTrees.Num() - 1)];
            const float FlankScale = Random.FRandRange(.9F, 1.12F), FlankGirth = Random.FRandRange(.85F, 1.25F);
            const float Radius = Archetypes[Archetype].BreastRadius * FlankScale * FlankGirth;
            const FVector2D Normal(-Sample.Tangent.Y, Sample.Tangent.X);
            // Staggered setback (0.1-1.5 m beyond the lane edge) plus along-lane jitter: no straight rows.
            const FVector2D P = Sample.Point + Normal * Side * (Sample.HalfWidth + Radius + Random.FRandRange(10, 150)) + Sample.Tangent * Random.FRandRange(-120, 120);
            if (NearStandDensity(P.X, P.Y) <= 0 || IsProtected(P, 400)) continue;
            if (!Clear(P, Radius + 120)) continue;
            {
                float Low = MAX_flt, High = -MAX_flt;
                for (int32 S = 0; S < 8; ++S)
                {
                    const float Z = APacificCoastGraybox::GetCoastalLandHeight(P.X + FMath::Cos(S * PI * .25F) * Radius * 1.3F, P.Y + FMath::Sin(S * PI * .25F) * Radius * 1.3F);
                    Low = FMath::Min(Low, Z); High = FMath::Max(High, Z);
                }
                if (High - Low > 35.0F) continue;
            }
            NextGirth = FlankGirth;
            Place(P, Archetype, FlankScale, Random.FRandRange(0, 2 * PI), 0, 0, true, true, Radius + 120);
        }
    }
    // 2. Fixed-density stand fill: jittered 5.0 m lattice with Poisson-style rejection.
    constexpr float Cell = 395.0F;
    for (float GX = -45000; GX < 12500; GX += Cell)
        for (float GY = -41000; GY < 41000; GY += Cell)
        {
            // Full-cell jitter plus up to ~1 m overlap into neighbouring cells hides the lattice.
            const FVector2D P(GX + Random.FRandRange(-.2F, 1.2F) * Cell, GY + Random.FRandRange(-.2F, 1.2F) * Cell);
            const float Density = NearStandDensity(P.X, P.Y);
            const float Roll = Random.FRand(), AgeRoll = Random.FRand(), SpeciesRoll = Random.FRand();
            const float Yaw = Random.FRandRange(0, 2 * PI), LeanYaw = Random.FRandRange(0, 2 * PI), LeanRoll = Random.FRand(), ScaleRoll = Random.FRand();
            if (Density <= 0 || Roll > Density * .97F) continue;
            if (IsProtected(P, 330)) continue;
            // Live oak patches occupy sheltered low ground and parts of the field edge.
            const float Oakness = Fbm(P.X - 2300, P.Y + 7700, 7800.0F) + (FMath::Abs(P.Y) < 11000 ? .08F : 0) - (P.X < -24000 ? .1F : 0);
            const bool bOakPatch = Oakness > .60F;
            int32 Archetype;
            EAge Age = AgeRoll < .62F ? EAge::Mature : AgeRoll < .85F ? EAge::Pole : EAge::Sapling;
            if (Age == EAge::Sapling) Archetype = Saplings[Random.RandRange(0, Saplings.Num() - 1)];
            else if (bOakPatch && Age == EAge::Mature && SpeciesRoll < .55F) Archetype = Oaks[Random.RandRange(0, Oaks.Num() - 1)];
            else
            {
                // Monterey pine favours the seaward (east) side, Douglas-fir the inland slopes.
                const bool bPine = SpeciesRoll < (P.X > -6000 ? .62F : .30F);
                const TArray<int32>& Poles = bPine ? PinePoles : FirPoles;
                Archetype = Age == EAge::Pole ? Poles[Random.RandRange(0, Poles.Num() - 1)]
                    : ScaleRoll < .12F ? Veterans[bPine ? 1 : 0]
                    : bPine ? Pines[Random.RandRange(0, Pines.Num() - 1)] : Firs[Random.RandRange(0, Firs.Num() - 1)];
            }
            const FArchetype& A = Archetypes[Archetype];
            const float Scale = A.Species == ESpecies::CoastLiveOak ? FMath::Lerp(.85F, 1.1F, ScaleRoll) : FMath::Lerp(.86F, 1.14F, ScaleRoll);
            // Girth varies trunk thickness (and crown spread) independently of height: 0.8-1.3x.
            const float Girth = A.Species == ESpecies::Sapling ? 1.0F : FMath::Lerp(.8F, 1.3F, FMath::Frac(ScaleRoll * 7.31F + Yaw));
            const float TrunkRadius = A.CollarRadius * Scale * Girth;
            // Corridor: no trunk surface inside the clear width plus 30 cm; no oak limbs, no sapling crowns.
            float HalfWidth = 0;
            const float ToCorridor = CorridorDistance(P, &HalfWidth);
            // Staggered extra setback (0-1.1 m) so the first fill trees do not line up on one clearance line.
            const float Stagger = 110.0F * FMath::Frac(FMath::Sin(P.X * .013F + P.Y * .017F) * 43758.5453F);
            const float CorridorClearance = A.Species == ESpecies::CoastLiveOak ? 1100.0F : A.Species == ESpecies::Sapling ? HalfWidth + 160.0F : HalfWidth + TrunkRadius + 30.0F + Stagger;
            if (ToCorridor < CorridorClearance) continue;
            // Density falls off toward the lane: a thinned 6 m band frames the corridor edges.
            if (ToCorridor < CorridorClearance + 600.0F && Roll > Density * .97F * (.25F + .75F * (ToCorridor - CorridorClearance) / 600.0F)) continue;
            const float Spacing = A.Species == ESpecies::CoastLiveOak ? 330.0F : A.Species == ESpecies::Sapling ? 70.0F : A.Age == EAge::Pole ? 110.0F : 135.0F;
            if (!Clear(P, Spacing)) continue;
            // Oaks keep conifer trunks out of their low spreading limbs.
            if (A.Species != ESpecies::CoastLiveOak && A.Species != ESpecies::Sapling)
            {
                bool bUnderOak = false;
                Grid.Near(P, [&](int32 Other)
                {
                    const FArchetype& O = Archetypes[Layout.Trees[Other].Archetype];
                    if (O.Species == ESpecies::CoastLiveOak && FVector2D::DistSquared(P, FVector2D(Layout.Trees[Other].Base)) < FMath::Square(O.CrownRadius * .55F)) bUnderOak = true;
                });
                if (bUnderOak) continue;
            }
            // Skip spots where the ground under the collar varies by more than 35 cm (gully banks).
            {
                float Low = MAX_flt, High = -MAX_flt;
                for (int32 S = 0; S < 8; ++S)
                {
                    const float Z = APacificCoastGraybox::GetCoastalLandHeight(P.X + FMath::Cos(S * PI * .25F) * TrunkRadius, P.Y + FMath::Sin(S * PI * .25F) * TrunkRadius);
                    Low = FMath::Min(Low, Z); High = FMath::Max(High, Z);
                }
                if (High - Low > 35.0F) continue;
            }
            const float MaxLean = A.Species == ESpecies::MontereyPine ? 7.0F : A.Species == ESpecies::CoastLiveOak ? 0 : 3.5F;
            NextGirth = Girth;
            Place(P, Archetype, Scale, Yaw, FMath::DegreesToRadians(MaxLean * LeanRoll), LeanYaw, true, false, Spacing);
        }
    // 3. Woodland mosaic beyond the near stands, driven by DEV-TERRAIN habitat classes:
    //    live oak in gullies/drainages, Douglas-fir groves on shaded slopes, Monterey pine
    //    near the coast, open (tree-free) sun-exposed grass. Spacing and density follow the
    //    woodland weight, so edges feather with the habitat instead of a drawn boundary.
    FRandomStream Distant(918437);
    int32 DistantCount = 0;
    for (float GX = -180000; GX < 14000; GX += 900.0F)
        for (float GY = -150000; GY < 150000; GY += 900.0F)
        {
            const FVector2D P(GX + Distant.FRandRange(0, 900), GY + Distant.FRandRange(0, 900));
            const float Roll = Distant.FRand(), Pick = Distant.FRand(), Yaw = Distant.FRandRange(0, 2 * PI), ScaleRoll = Distant.FRand();
            if (DistantCount >= 6000 || NearStandDensity(P.X, P.Y) > 0) continue;
            const bool bMid = P.X > -70000 && FMath::Abs(P.Y) < 70000;
            // Far ridge: coarser 18 m sampling keeps the instance count bounded.
            if (!bMid && (FMath::FloorToInt(GX / 900) % 2 || FMath::FloorToInt(GY / 900) % 2)) continue;
            const FCoastHabitatSample Habitat = FPacificCoastHabitat::Sample(P.X, P.Y);
            const float Woodland = Habitat.Woodland + .25F * Habitat.ShelteredGrass;
            if (Woodland < .30F || Roll > Smooth01((Woodland - .30F) / .45F) * .85F) continue;
            if (IsProtected(P, 400)) continue;
            if (APacificCoastGraybox::GetCoastalLandHeight(P.X, P.Y) < APacificCoastGraybox::SeaLevelCm + 600) continue;
            if (!Clear(P, 300)) continue;
            const bool bOak = Habitat.Drainage > .45F ? Pick < .75F : Habitat.SunExposure > -.1F ? Pick < .55F : Pick < .2F;
            const bool bPine = !bOak && P.X > -25000 && Pick > .5F;
            const int32 Archetype = bOak ? Oaks[Distant.RandRange(0, Oaks.Num() - 1)] : bPine ? Pines[Distant.RandRange(0, Pines.Num() - 1)] : Firs[Distant.RandRange(0, Firs.Num() - 1)];
            Place(P, Archetype, FMath::Lerp(.8F, 1.1F, ScaleRoll), Yaw, 0, 0, false, false, 300);
            ++DistantCount;
        }
    for (const FPlacedTree& Tree : Layout.Trees)
    {
        const FArchetype& A = Archetypes[Tree.Archetype];
        if (!Tree.bNearForest) { ++Layout.DistantTrees; continue; }
        ++Layout.NearTrees;
        if (A.IsOverstory()) { ++Layout.NearOverstoryTrees; Layout.NearOverstoryStems += A.StemCount; }
        else ++Layout.NearSaplings;
    }
    return Layout;
}
}

void APacificCoastGraybox::BuildCoastalTrees(UMaterialInterface* Material)
{
    using namespace CoastFlora;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    // Draft materials are produced by the isolated HabitatDraftImport commandlet; the slice
    // still builds and renders (engine material) before that intake has been run.
    FString Commandlet;
    const bool bImporting = FParse::Value(FCommandLine::Get(), TEXT("run="), Commandlet)
        && (Commandlet.Equals(TEXT("HabitatDraftImport"), ESearchCase::IgnoreCase) || Commandlet.Equals(TEXT("CoastDraftImport"), ESearchCase::IgnoreCase));
    UMaterialInterface* Barks[3] = {Material, Material, Material};
    UMaterialInterface* Leaves[3] = {Material, Material, Material};
    if (!bImporting && FPackageName::DoesPackageExist(TEXT("/Game/Development/CoastDraft/Materials/M_FloraOakLeavesDraft")))
    {
        static ConstructorHelpers::FObjectFinder<UMaterialInterface> FirBark(TEXT("/Game/Development/CoastDraft/Materials/M_FloraFirBarkDraft.M_FloraFirBarkDraft"));
        static ConstructorHelpers::FObjectFinder<UMaterialInterface> PineBark(TEXT("/Game/Development/CoastDraft/Materials/M_FloraPineBarkDraft.M_FloraPineBarkDraft"));
        static ConstructorHelpers::FObjectFinder<UMaterialInterface> OakBark(TEXT("/Game/Development/CoastDraft/Materials/M_FloraOakBarkDraft.M_FloraOakBarkDraft"));
        static ConstructorHelpers::FObjectFinder<UMaterialInterface> FirLeaves(TEXT("/Game/Development/CoastDraft/Materials/M_FloraFirNeedlesDraft.M_FloraFirNeedlesDraft"));
        static ConstructorHelpers::FObjectFinder<UMaterialInterface> PineLeaves(TEXT("/Game/Development/CoastDraft/Materials/M_FloraPineNeedlesDraft.M_FloraPineNeedlesDraft"));
        static ConstructorHelpers::FObjectFinder<UMaterialInterface> OakLeaves(TEXT("/Game/Development/CoastDraft/Materials/M_FloraOakLeavesDraft.M_FloraOakLeavesDraft"));
        Barks[0] = FirBark.Object; Barks[1] = PineBark.Object; Barks[2] = OakBark.Object;
        Leaves[0] = FirLeaves.Object; Leaves[1] = PineLeaves.Object; Leaves[2] = OakLeaves.Object;
    }
    const TArray<FArchetype>& Archetypes = GetArchetypes();
    const FLayout& Layout = GetLayout();
    // Two instanced batches per archetype: near stands (full LOD chain) and the woodland mosaic
    // beyond them (MinLOD 1, culled past 2.2 km) so far stands never submit LOD0 geometry.
    TArray<UHierarchicalInstancedStaticMeshComponent*> Batches;
    for (int32 Index = 0; Index < Archetypes.Num() * 2; ++Index)
    {
        const FArchetype& A = Archetypes[Index % Archetypes.Num()];
        const bool bFar = Index >= Archetypes.Num();
        UHierarchicalInstancedStaticMeshComponent* Batch = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
            bFar ? FName(*(A.Name.ToString() + TEXT("Far"))) : A.Name);
        Batch->SetupAttachment(Root);
        Batch->SetStaticMesh(A.Mesh);
        const bool bPineLike = A.Name.ToString().Contains(TEXT("Pine"));
        const int32 Kind = A.Species == ESpecies::CoastLiveOak ? 2 : bPineLike ? 1 : 0;
        Batch->SetMaterial(0, Barks[Kind]);
        Batch->SetMaterial(1, Leaves[Kind]);
        Batch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Batch->SetCollisionProfileName(TEXT("NoCollision"));
        Batch->SetCastShadow(true);
        Batch->bAffectDistanceFieldLighting = false;
        // Saplings are understory: culled with a fade beyond 160 m.
        if (A.Species == ESpecies::Sapling) Batch->SetCullDistances(13000, 16000);
        if (bFar)
        {
            Batch->bOverrideMinLOD = true;
            Batch->MinLOD = 1;
            if (A.Species != ESpecies::Sapling) Batch->SetCullDistances(200000, 220000);
        }
        Batch->ComponentTags.Add(FName(*FString::Printf(TEXT("CoastalFloraArchetype|%d"), Index % Archetypes.Num())));
        Batches.Add(Batch);
        ++GeneratedPrimitiveCount;
    }
    UHierarchicalInstancedStaticMeshComponent* TrunkProxies = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("CoastalTrunkCollisionProxies"));
    TrunkProxies->SetupAttachment(Root);
    TrunkProxies->SetStaticMesh(CylinderFinder.Object);
    TrunkProxies->SetCollisionProfileName(TEXT("BlockAll"));
    TrunkProxies->SetHiddenInGame(true);
    TrunkProxies->SetVisibility(false);
    TrunkProxies->SetCastShadow(false);
    TArray<FTransform> Transforms[64];
    check(Archetypes.Num() * 2 <= 64);
    for (const FPlacedTree& Tree : Layout.Trees)
    {
        const FTransform Instance = InstanceTransform(Tree);
        const bool bFarBatch = !Tree.bNearForest;
        Transforms[Tree.Archetype + (bFarBatch ? Archetypes.Num() : 0)].Add(Instance);
        // Trunk collision where the drone can fly: near stands and the woodland mosaic around them.
        if (Tree.Base.X < -70000 || FMath::Abs(Tree.Base.Y) > 70000) continue;
        // Simple cylinders follow the actual generated stems/limbs (engine cylinder: r 50, h 100).
        for (const FTrunkSegment& Segment : Archetypes[Tree.Archetype].Collision)
        {
            const FVector Start = Instance.TransformPosition(FVector(Segment.Start)), End = Instance.TransformPosition(FVector(Segment.End));
            const FVector Axis = End - Start;
            const float Radius = Segment.Radius * Tree.Scale * Tree.Girth;
            TrunkProxies->AddInstance(FTransform(FQuat::FindBetweenNormals(FVector::UpVector, Axis.GetSafeNormal()), (Start + End) * .5,
                FVector(Radius * .02F, Radius * .02F, Axis.Size() * .01F)));
        }
    }
    for (int32 Index = 0; Index < Batches.Num(); ++Index) Batches[Index]->AddInstances(Transforms[Index], false);
    CoastalTreeCount = Layout.NearOverstoryTrees;
    TrunkProxies->ComponentTags.Add(FName(*FString::Printf(TEXT("CoastalTrunkProxyCount|%d"), TrunkProxies->GetInstanceCount())));
    ++GeneratedPrimitiveCount;
}
