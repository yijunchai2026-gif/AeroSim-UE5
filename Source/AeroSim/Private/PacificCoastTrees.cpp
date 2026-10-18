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

// Bay Area coastal forest: Douglas-fir and Monterey pine stands with coast live oak
// patches, built as a small library of instanced archetypes instead of unique merged
// geometry. Each archetype is a four-LOD static mesh generated once per process.
namespace CoastFlora
{
namespace
{
constexpr int32 LodCount = 4;
constexpr float LodScreenSize[LodCount] = {1.0F, .60F, .28F, .08F};

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

struct FIsland { FVector2f Min, Max; bool bAlongU; };
// Atlas islands measured from the ART-FLORA alpha maps (only isolated needle/leaf regions;
// the pine_tree_01 baked cone region and searsia bark strip are never sampled).
const FIsland PineSprigs[] = {{FVector2f(.0371F, .041F), FVector2f(.209F, .3887F), false}, {FVector2f(.6934F, .3652F), FVector2f(.9395F, .5215F), true}};
const FIsland FirSprays[] = {{FVector2f(.3027F, .4004F), FVector2f(.6348F, .7793F), false}, {FVector2f(.648F, .039F), FVector2f(.941F, .373F), false},
    {FVector2f(.186F, .043F), FVector2f(.420F, .316F), false}};
const FIsland OakRows[] = {{FVector2f(.012F, .018F), FVector2f(.820F, .514F), false}, {FVector2f(.014F, .574F), FVector2f(.564F, 1.0F), false}};
// Fallback (old 256x512 PineTree01 needle sprig) used only when the habitat import is missing.

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
    const FVector Normal = (Face * .45F + Outward.GetSafeNormal() * .55F).GetSafeNormal();
    auto Coord = [&Island](float AlongT, float AcrossT)
    {
        return Island.bAlongU ? FVector2f(FMath::Lerp(Island.Min.X, Island.Max.X, AlongT), FMath::Lerp(Island.Min.Y, Island.Max.Y, AcrossT))
            : FVector2f(FMath::Lerp(Island.Min.X, Island.Max.X, AcrossT), FMath::Lerp(Island.Max.Y, Island.Min.Y, AlongT));
    };
    const FVector Half = W * Width * .5F;
    const FVector Tangent = Island.bAlongU ? A : W, Bitangent = Island.bAlongU ? W : -A;
    const int32 V0 = S.Add(Origin - Half, Normal, Tangent, Bitangent, Coord(0, 0), Color);
    const int32 V1 = S.Add(Origin + Half, Normal, Tangent, Bitangent, Coord(0, 1), Color);
    const int32 V2 = S.Add(Origin + Half + A * Length, Normal, Tangent, Bitangent, Coord(1, 1), Color);
    const int32 V3 = S.Add(Origin - Half + A * Length, Normal, Tangent, Bitangent, Coord(1, 0), Color);
    S.Tri(V0, V1, V2); S.Tri(V0, V2, V3);
}

// ---------------------------------------------------------------------------------
// Skeletons
// ---------------------------------------------------------------------------------
struct FLimb { TArray<FVector> Path; float R0 = 1, R1 = .5F; int32 Level = 1; float Wind0 = 0, Wind1 = .5F; };
struct FTuft { FVector Origin, Dir, Across, Outward; float Length; float Occlusion; float Wind; int32 Island; };
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
    float Flare = .15F * FMath::Exp(-Zc / 34.0F);
    float Buttress = 0;
    for (const auto& Lobe : Lobes) Buttress += Lobe.Value * FMath::Pow(FMath::Max(0.0F, FMath::Cos(Theta - Lobe.Key)), 6.0F);
    return BaseRadius * (1.0F + Flare + Buttress * FMath::Exp(-Zc / 48.0F));
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
    const float Step0 = bSapling ? 34.0F : bPine ? 58.0F : 30.0F, Step1 = bSapling ? 48.0F : bPine ? 95.0F : 46.0F;
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
            auto Outward = [&Center](const FVector& P) { return (P - Center + FVector(0, 0, 40)).GetSafeNormal(); };
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
                    const int32 Needles = bSapling ? 6 : Random.RandRange(10, 14);
                    for (int32 Needle = 0; Needle < Needles; ++Needle)
                    {
                        FVector D = (TipPair.Value * .75F + Random.VRand() * .85F + FVector(0, 0, .35F)).GetSafeNormal();
                        FTuft Tuft{TipPair.Key - TipPair.Value * 8.0F, D, FVector::CrossProduct(D, Random.VRand()), Outward(TipPair.Key),
                            Random.FRandRange(26, 36) * (bSapling ? .75F : 1.0F), Occl(TipPair.Key), Limb.Wind1, Random.RandRange(0, 1)};
                        K.Tufts.Add(Tuft);
                    }
                    // A second, smaller tuft behind the tip thickens each foliage clump.
                    for (int32 Needle = 0; Needle < (bSapling ? 2 : 5); ++Needle)
                    {
                        const FVector At = TipPair.Key - TipPair.Value * Random.FRandRange(30, 55);
                        FVector D = (Random.VRand() + FVector(0, 0, .5F)).GetSafeNormal();
                        K.Tufts.Add({At, D, FVector::CrossProduct(D, TipPair.Value), Outward(At), Random.FRandRange(24, 32), Occl(At) * .9F, Limb.Wind1 * .8F, Random.RandRange(0, 1)});
                    }
                }
            }
            else
            {
                // Douglas-fir: flat two-ranked sprays along the limb and short side branchlets.
                const FVector Flat = FVector::CrossProduct(FVector::UpVector, (Tip - Base).GetSafeNormal()).GetSafeNormal();
                const float Spacing = bSapling ? 15.0F : 21.0F;
                for (float D = L * .22F; D < L; D += Spacing * Random.FRandRange(.8F, 1.2F))
                {
                    const float S = D / L;
                    const FVector At = PointAt(S), Ahead = (PointAt(FMath::Min(1.0F, S + .05F)) - At).GetSafeNormal();
                    for (int32 Side = -1; Side <= 1; Side += 2)
                    {
                        const FVector Spray = (Ahead * .55F + Flat * Side * .80F + FVector(0, 0, Random.FRandRange(-.05F, .18F))).GetSafeNormal();
                        K.Tufts.Add({At, Spray, FVector::CrossProduct(Spray, FVector::UpVector) + FVector(0, 0, Random.FRandRange(-.3F, .3F)), Outward(At),
                            Random.FRandRange(34, 52) * (bSapling ? .7F : 1.0F) * FMath::Lerp(1.0F, .75F, S), Occl(At), FMath::Lerp(Limb.Wind0, Limb.Wind1, S), Random.RandRange(0, 2)});
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
                        for (float BDist = BL * .2F; BDist < BL; BDist += 17.0F)
                            for (int32 BS = -1; BS <= 1; BS += 2)
                            {
                                const FVector P = At + BD * BDist, Spray = (BD * .5F + Across * BS * .85F + FVector(0, 0, .06F)).GetSafeNormal();
                                K.Tufts.Add({P, Spray, FVector::CrossProduct(Spray, FVector::UpVector), Outward(P), Random.FRandRange(30, 44), Occl(P), Branchlet.Wind1, Random.RandRange(0, 2)});
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

FSkeleton MakeConifer(ESpecies Species, EAge Age, int32 Seed)
{
    FRandomStream Random(Seed);
    FSkeleton K; K.Species = Species;
    const bool bPine = Species == ESpecies::MontereyPine, bSapling = Age == EAge::Sapling;
    K.Islands = bPine ? PineSprigs : FirSprays; K.IslandCount = bPine ? 2 : 3;
    K.TileU = bPine ? 200.0F : 215.0F; K.TileV = K.TileU;
    K.Height = bSapling ? Random.FRandRange(170, 440) : Age == EAge::Pole ? Random.FRandRange(bPine ? 900 : 1100, bPine ? 1400 : 1700)
        : Random.FRandRange(bPine ? 1700 : 2300, bPine ? 2600 : 3400);
    const float H = K.Height;
    K.Breast = bSapling ? 2.5F + H * .006F : H * (bPine ? .0155F : .0125F) + 3.0F;
    K.CrownBase = bSapling ? H * .12F : H * (Age == EAge::Pole ? Random.FRandRange(.30F, .40F) : bPine ? Random.FRandRange(.36F, .46F) : Random.FRandRange(.42F, .55F));
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
    FSkeleton K; K.Species = ESpecies::CoastLiveOak; K.Islands = OakRows; K.IslandCount = 2; K.TileU = 100; K.TileV = 200;
    K.Breast = Random.FRandRange(30, 44);
    const float Fork = Random.FRandRange(70, 170);
    K.Stems = Random.RandRange(2, 4);
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
        const float Length = Random.FRandRange(380, 600);
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
            const float LL = Random.FRandRange(300, 480);
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
        for (int32 Leaf = 0; Leaf < 9; ++Leaf)
        {
            const FVector D = (Outward * .55F + Random.VRand() * .9F).GetSafeNormal();
            const FVector At = Tip + Random.VRand() * Random.FRandRange(0, 28);
            K.Tufts.Add({At, D, FVector::CrossProduct(D, Random.VRand()), Outward, Random.FRandRange(10, 13.5F), Occlusion, .9F, Random.RandRange(0, 1)});
        }
    }
    return K;
}

// ---------------------------------------------------------------------------------
// LOD emission
// ---------------------------------------------------------------------------------
void EmitLod(const FSkeleton& K, int32 Lod, FLod& Out, uint32 Seed)
{
    const bool bOak = K.Species == ESpecies::CoastLiveOak;
    const int32 TrunkSides = Lod == 0 ? 14 : Lod == 1 ? 8 : Lod == 2 ? 5 : 3;
    TArray<TPair<float, float>> Lobes;
    FRandomStream LobeRandom(Seed);
    const int32 LobeCount = LobeRandom.RandRange(4, 6);
    for (int32 L = 0; L < LobeCount; ++L) Lobes.Add({L * 2 * PI / LobeCount + LobeRandom.FRandRange(-.35F, .35F), LobeRandom.FRandRange(.045F, .075F)});
    // Physical bark tiling: whole repeats around the circumference so the seam closes.
    const float URepeats = FMath::Clamp(FMath::RoundToFloat(2 * PI * K.Breast / K.TileU * 1.6F), 2.0F, 4.0F);
    const float Height = K.Height;
    Sweep(Out.Bark, K.Trunk, [&](int32 Ring, float Theta)
        {
            const float Z = K.Trunk[Ring].Z;
            const float Base = K.TrunkRadius[Ring];
            if (K.Species != ESpecies::CoastLiveOak && K.Height < 500) return Base; // seedlings: no collar
            return Lod >= 2 ? Base * (Z < 30 ? 1.12F : 1.0F) : CollarRadius(Z, Theta, Base, Lobes);
        }, TrunkSides, URepeats, K.TileV,
        [&](int32 Ring)
        {
            const float Z = K.Trunk[Ring].Z;
            return FVector4f(FMath::Clamp(.05F + Z / 55.0F, 0.0F, 1.0F), FMath::Square(FMath::Max(Z, 0.0F) / Height) * .12F, 0, 1);
        });
    for (const FLimb& Limb : K.Limbs)
    {
        if (Lod == 1 && Limb.Level >= (bOak ? 3 : 2)) continue;
        if (Lod == 2 && Limb.Level >= (bOak ? 2 : 1)) continue;
        if (Lod == 3 && Limb.Level >= 1) continue;
        const int32 Sides = Limb.Level == 0 ? (Lod == 0 ? 10 : Lod == 1 ? 6 : 4) : Lod == 0 ? (Limb.R0 > 4 ? 6 : 4) : 3;
        const int32 N = Limb.Path.Num();
        Sweep(Out.Bark, Limb.Path, [&](int32 Ring, float) { return FMath::Lerp(Limb.R0, Limb.R1, Ring / static_cast<float>(N - 1)); },
            Sides, Limb.R0 > 12 ? 2.0F : 1.0F, K.TileV,
            [&](int32 Ring) { return FVector4f(1, FMath::Lerp(Limb.Wind0, Limb.Wind1, Ring / static_cast<float>(N - 1)), 0, 1); });
    }
    // Sample the foliage set; scale kept cards up so crown coverage is preserved.
    const int32 Keep = Lod == 0 ? 1 : Lod == 1 ? 5 : Lod == 2 ? 16 : 44;
    const float Grow = Lod == 0 ? 1.0F : Lod == 1 ? 2.05F : Lod == 2 ? 3.4F : 5.4F;
    int32 Emitted = 0;
    for (int32 Index = 0; Index < K.Tufts.Num(); ++Index)
    {
        if ((Index * 7919 + Seed) % Keep != 0 && !(Lod == 3 && Emitted < 8 && Index > K.Tufts.Num() - 8)) continue;
        const FTuft& Tuft = K.Tufts[Index];
        const FVector Along = Tuft.Dir;
        Card(Out.Leaf, Tuft.Origin - Along * Tuft.Length * (Grow - 1) * .3F, Along, Tuft.Across, Tuft.Length * Grow,
            K.Islands[Tuft.Island % K.IslandCount], Tuft.Outward, FVector4f(Tuft.Occlusion, Tuft.Wind, 0, 1));
        ++Emitted;
    }
}

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
    UStaticMesh* Mesh = NewObject<UStaticMesh>(GetTransientPackage(), Name, RF_Transient);
    Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, FName(TEXT("Bark"))));
    Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, FName(TEXT("Foliage"))));
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

const TArray<FArchetype>& GetArchetypes()
{
    static TArray<FArchetype> Archetypes;
    if (!Archetypes.IsEmpty()) return Archetypes;
    struct FSpec { const TCHAR* Name; ESpecies Species; EAge Age; int32 Seed; };
    const FSpec Specs[] = {
        {TEXT("CoastalFloraFirMatureA"), ESpecies::DouglasFir, EAge::Mature, 1101}, {TEXT("CoastalFloraFirMatureB"), ESpecies::DouglasFir, EAge::Mature, 1187},
        {TEXT("CoastalFloraFirMatureC"), ESpecies::DouglasFir, EAge::Mature, 1243}, {TEXT("CoastalFloraFirPole"), ESpecies::DouglasFir, EAge::Pole, 1319},
        {TEXT("CoastalFloraPineMatureA"), ESpecies::MontereyPine, EAge::Mature, 2203}, {TEXT("CoastalFloraPineMatureB"), ESpecies::MontereyPine, EAge::Mature, 2281},
        {TEXT("CoastalFloraPinePole"), ESpecies::MontereyPine, EAge::Pole, 2347},
        {TEXT("CoastalFloraOakA"), ESpecies::CoastLiveOak, EAge::Mature, 3307}, {TEXT("CoastalFloraOakB"), ESpecies::CoastLiveOak, EAge::Mature, 3391},
        {TEXT("CoastalFloraOakC"), ESpecies::CoastLiveOak, EAge::Mature, 3433},
        {TEXT("CoastalFloraSaplingFirA"), ESpecies::DouglasFir, EAge::Sapling, 4409}, {TEXT("CoastalFloraSaplingFirB"), ESpecies::DouglasFir, EAge::Sapling, 4463},
        {TEXT("CoastalFloraSaplingPine"), ESpecies::MontereyPine, EAge::Sapling, 4517}};
    for (const FSpec& Spec : Specs)
    {
        FSkeleton K = Spec.Species == ESpecies::CoastLiveOak ? MakeOak(Spec.Seed) : MakeConifer(Spec.Species, Spec.Age, Spec.Seed);
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
    const TArray<FVector>& Route = FPacificCoastGrayboxContract::GetRoutePoints();
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

float NearStandDensity(const float X, const float Y)
{
    const float AbsY = FMath::Abs(Y);
    // Feathered irregular boundaries (not parallelograms): every edge is displaced by noise.
    const float FieldEdge = 7700.0F + 900.0F * Fbm(X, Y > 0 ? 1000.0F : -1000.0F, 5200.0F) - 450.0F;
    const float OuterEdge = 26000.0F + 1500.0F * (Fbm(X + 5000, Y, 7000.0F) - .5F);
    const float WestEdge = -30000.0F + 1800.0F * (Fbm(Y, X, 6000.0F) - .5F);
    const float EastEdge = NearEastEdge(Y) - 900.0F * Fbm(Y, 300.0F, 4200.0F);
    const float Inside = FMath::Min(FMath::Min(AbsY - FieldEdge, OuterEdge - AbsY), FMath::Min(X - WestEdge, EastEdge - X));
    if (Inside <= 0) return 0;
    // Ragged 6–20 m feather from sparse edge trees into the closed stand.
    const float Feather = .28F + .72F * Smooth01(Inside / (600.0F + 1400.0F * Fbm(X, Y, 3100.0F)));
    // Small natural glades (~8% of area) keep openings without thinning the stand everywhere.
    const float Glade = Fbm(X + 9100, Y - 4700, 4600.0F);
    return Feather * (1.0F - Smooth01((Glade - .64F) / .06F));
}

FTransform InstanceTransform(const FPlacedTree& Tree)
{
    const FQuat Lean = FQuat(FVector(-FMath::Sin(Tree.LeanYaw), FMath::Cos(Tree.LeanYaw), 0), Tree.LeanPitch);
    return FTransform(Lean * FQuat(FVector::UpVector, Tree.Yaw), Tree.Base, FVector(Tree.Scale));
}

const FLayout& GetLayout()
{
    static FLayout Layout;
    static bool bBuilt = false;
    if (bBuilt) return Layout;
    bBuilt = true;
    const TArray<FArchetype>& Archetypes = GetArchetypes();
    TArray<int32> Firs, Pines, Oaks, Saplings, FirPoles, PinePoles;
    for (int32 Index = 0; Index < Archetypes.Num(); ++Index)
    {
        const FArchetype& A = Archetypes[Index];
        if (A.Species == ESpecies::Sapling) Saplings.Add(Index);
        else if (A.Species == ESpecies::CoastLiveOak) Oaks.Add(Index);
        else if (A.Age == EAge::Pole) (A.Species == ESpecies::DouglasFir ? FirPoles : PinePoles).Add(Index);
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
    auto Place = [&](const FVector2D& P, int32 Archetype, float Scale, float Yaw, float LeanPitch, float LeanYaw, bool bNear, bool bFlank, float SpacingRadius)
    {
        FPlacedTree Tree;
        // Lean tilts the collar: lower by the lift on the uphill side of the tilt.
        Tree.Base = FVector(P.X, P.Y, Ground(P, Archetypes[Archetype].CollarRadius * Scale) - Archetypes[Archetype].CollarRadius * Scale * FMath::Sin(LeanPitch) - 2.0F);
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
    const int32 GateArchetype = Firs[0];
    const float GateRadius = Archetypes[GateArchetype].BreastRadius;
    const float GateDistances[] = {2600, 7800, 13600, 19800, 26500, 33200, 40500};
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
            Next = Sample.Distance + Random.FRandRange(750, 1150);
            const bool bPine = Random.FRand() < .35F;
            const int32 Archetype = bPine ? Pines[Random.RandRange(0, Pines.Num() - 1)] : Firs[Random.RandRange(0, Firs.Num() - 1)];
            const float Radius = Archetypes[Archetype].BreastRadius;
            const FVector2D Normal(-Sample.Tangent.Y, Sample.Tangent.X);
            const FVector2D P = Sample.Point + Normal * Side * (Sample.HalfWidth + Radius + Random.FRandRange(10, 70));
            if (NearStandDensity(P.X, P.Y) <= 0 || IsProtected(P, 400)) continue;
            if (!Clear(P, Radius + 120)) continue;
            Place(P, Archetype, 1.0F, Random.FRandRange(0, 2 * PI), 0, 0, true, true, Radius + 120);
        }
    }
    // 2. Fixed-density stand fill: jittered 5.0 m lattice with Poisson-style rejection.
    constexpr float Cell = 470.0F;
    for (float GX = -31000; GX < 12500; GX += Cell)
        for (float GY = -27500; GY < 27500; GY += Cell)
        {
            const FVector2D P(GX + Random.FRandRange(.1F, .9F) * Cell, GY + Random.FRandRange(.1F, .9F) * Cell);
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
                Archetype = Age == EAge::Pole ? (bPine ? PinePoles[0] : FirPoles[0])
                    : bPine ? Pines[Random.RandRange(0, Pines.Num() - 1)] : Firs[Random.RandRange(0, Firs.Num() - 1)];
            }
            const FArchetype& A = Archetypes[Archetype];
            const float Scale = A.Species == ESpecies::CoastLiveOak ? FMath::Lerp(.85F, 1.1F, ScaleRoll) : FMath::Lerp(.86F, 1.14F, ScaleRoll);
            const float TrunkRadius = A.CollarRadius * Scale;
            // Corridor: no trunk surface inside the clear width plus 30 cm; no oak limbs, no sapling crowns.
            float HalfWidth = 0;
            const float ToCorridor = CorridorDistance(P, &HalfWidth);
            const float CorridorClearance = A.Species == ESpecies::CoastLiveOak ? 1100.0F : A.Species == ESpecies::Sapling ? HalfWidth + 160.0F : HalfWidth + TrunkRadius + 30.0F;
            if (ToCorridor < CorridorClearance) continue;
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
            const float MaxLean = A.Species == ESpecies::MontereyPine ? 7.0F : A.Species == ESpecies::CoastLiveOak ? 0 : 3.5F;
            Place(P, Archetype, Scale, Yaw, FMath::DegreesToRadians(MaxLean * LeanRoll), LeanYaw, true, false, Spacing);
        }
    // 3. Inland woodland mosaic on the ridge: oak in sheltered drainages, conifers above.
    FRandomStream Distant(918437);
    FVector2D GroveCenter = FVector2D::ZeroVector; int32 GroveRemaining = 0;
    int32 DistantCount = 0;
    for (int32 Attempt = 0; Attempt < 40000 && DistantCount < 1400; ++Attempt)
    {
        if (GroveRemaining <= 0)
        {
            GroveCenter = FVector2D(Distant.FRandRange(-170000, -36000), Distant.FRandRange(-110000, 110000));
            GroveRemaining = Distant.RandRange(18, 34);
        }
        const float Angle = Distant.FRandRange(0, 2 * PI), Radius = FMath::Sqrt(Distant.FRand()) * 3200.0F;
        const FVector2D P = GroveCenter + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
        const float Roll = Distant.FRand(), Pick = Distant.FRand(), Yaw = Distant.FRandRange(0, 2 * PI), ScaleRoll = Distant.FRand();
        --GroveRemaining;
        if (NearStandDensity(P.X, P.Y) > 0 || FMath::Abs(P.Y) < 30000 && P.X > -32000) continue;
        const float Cover = APacificCoastGraybox::GetCoastalVegetationCover(P.X, P.Y);
        if (Cover < .55F || Roll > Cover * Cover) continue;
        if (APacificCoastGraybox::GetCoastalLandHeight(P.X, P.Y) < 200) continue;
        if (!Clear(P, 280)) continue;
        const bool bOak = Cover > .72F && Pick < .6F;
        const int32 Archetype = bOak ? Oaks[Distant.RandRange(0, Oaks.Num() - 1)] : Pick < .82F ? Firs[Distant.RandRange(0, Firs.Num() - 1)] : Pines[Distant.RandRange(0, Pines.Num() - 1)];
        Place(P, Archetype, FMath::Lerp(.8F, 1.1F, ScaleRoll), Yaw, 0, 0, false, false, 280);
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
    TArray<UHierarchicalInstancedStaticMeshComponent*> Batches;
    for (int32 Index = 0; Index < Archetypes.Num(); ++Index)
    {
        const FArchetype& A = Archetypes[Index];
        UHierarchicalInstancedStaticMeshComponent* Batch = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(A.Name);
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
        Batch->ComponentTags.Add(FName(*FString::Printf(TEXT("CoastalFloraArchetype|%d"), Index)));
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
    for (const FPlacedTree& Tree : Layout.Trees)
    {
        const FTransform Instance = InstanceTransform(Tree);
        Transforms[Tree.Archetype].Add(Instance);
        // Simple cylinders follow the actual generated stems/limbs (engine cylinder: r 50, h 100).
        for (const FTrunkSegment& Segment : Archetypes[Tree.Archetype].Collision)
        {
            const FVector Start = Instance.TransformPosition(FVector(Segment.Start)), End = Instance.TransformPosition(FVector(Segment.End));
            const FVector Axis = End - Start;
            const float Radius = Segment.Radius * Tree.Scale;
            TrunkProxies->AddInstance(FTransform(FQuat::FindBetweenNormals(FVector::UpVector, Axis.GetSafeNormal()), (Start + End) * .5,
                FVector(Radius * .02F, Radius * .02F, Axis.Size() * .01F)));
        }
    }
    for (int32 Index = 0; Index < Batches.Num(); ++Index) Batches[Index]->AddInstances(Transforms[Index], false);
    CoastalTreeCount = Layout.NearOverstoryTrees;
    TrunkProxies->ComponentTags.Add(FName(*FString::Printf(TEXT("CoastalTrunkProxyCount|%d"), TrunkProxies->GetInstanceCount())));
    ++GeneratedPrimitiveCount;
}
