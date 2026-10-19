#include "PacificCoastGraybox.h"

#include "Components/SceneComponent.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "UObject/ConstructorHelpers.h"

namespace CoastalTerrain
{
constexpr float West = -240000.0F;
constexpr float North = 220000.0F;
constexpr float SubmergedRim = -12000.0F;
constexpr float CliffFoot = -14000.0F;
constexpr float BedrockBase = -18000.0F;
constexpr int32 CoastRows = 385;
constexpr int32 CliffDepthSteps = 36;
constexpr float HeroBayY = -19000.0F;
constexpr float HeroBayHalfWidth = 2500.0F;

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

float JoinRidge(float A, float B)
{
    // Smooth only the seam between neighboring shoulders; the ridge network stays connected.
    // A 30 m blend rounds the valley floors where neighbouring ridge flanks meet (no crease).
    constexpr float SeamHeight = 3000.0F;
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

float BaseCoastY(const int32 Row)
{
    const float T = Row / static_cast<float>(CoastRows - 1) * 2.0F - 1.0F;
    const float Magnitude = FMath::Abs(T);
    return FMath::Sign(T) * (Magnitude <= .6F ? Magnitude * 100000.0F : 60000.0F + (Magnitude - .6F) * 400000.0F);
}

const TArray<float>& CoastRowsY()
{
    static const TArray<float> Samples=[]
    {
        TArray<float> Result;
        for(int32 Row=0;Row<CoastRows;++Row)
        {
            const float Y=BaseCoastY(Row);Result.Add(Y);
            if(Row+1==CoastRows)continue;
            const float Next=BaseCoastY(Row+1);
            if(Next>HeroBayY-HeroBayHalfWidth && Y<HeroBayY+HeroBayHalfWidth)
                for(int32 Split=1;Split<4;++Split)Result.Add(FMath::Lerp(Y,Next,Split*.25F));
        }
        return Result;
    }();
    return Samples;
}

float CoastY(const int32 Row) { return CoastRowsY()[Row]; }

float HeroBayWeight(const float Y)
{
    return 1.0F-Smooth((FMath::Abs(Y-HeroBayY)-1600.0F)/900.0F);
}

// R5 hero-cove scan (Poly Haven coastal_cliff_01, CC0, fixed 40 m crop imported as
// SM_CoastHeroCliffScanDraft). The scan is placed rigidly (scale 1, no rotation) at this origin;
// its lowest vertex is 20 cm above still water. A rigid fit to the original sloping cove apron
// left 0-6% of the scan visible, so inside this single reach the closed cliff face is rebuilt to
// sit just behind the scan: ScanFace holds, per local (y, z), the X of the closed terrain face,
// derived offline from the scan's own LOD0 front surface (2.5 m lower envelope, 40 cm margin,
// perimeter pulled forward so every cut edge is buried, monotone with depth = no overhang).
constexpr float ScanX = 17520.0F, ScanY = HeroBayY, ScanZ = -6446.6F, ScanHalfWidth = 2000.0F;
const int16 ScanFace[22][41] = {
    {536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536},
    {536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536,536},
    {536,536,480,428,402,396,387,384,381,376,372,370,370,373,379,385,390,391,394,395,397,398,395,394,394,402,408,420,425,432,441,446,447,448,456,466,475,494,514,536,536},
    {536,536,426,324,272,260,242,233,227,218,211,208,208,215,227,239,248,268,279,287,296,300,272,257,260,273,286,309,318,332,349,359,361,364,380,398,415,448,492,536,536},
    {536,536,399,236,160,141,112,98,98,122,143,160,170,186,201,218,248,268,279,287,296,300,272,230,201,184,188,214,228,244,266,282,290,296,318,338,354,397,469,536,536},
    {536,536,399,202,94,81,80,85,98,122,143,160,170,186,201,218,248,268,279,287,296,300,272,230,201,184,172,171,173,180,200,229,247,258,268,276,283,339,443,536,536},
    {536,536,399,202,94,81,80,85,98,122,143,160,170,186,201,218,248,268,279,287,296,300,272,230,201,184,172,171,173,180,200,229,247,258,265,266,267,326,437,536,536},
    {536,536,399,202,80,80,80,85,98,122,143,160,170,186,201,218,248,268,279,287,296,300,272,230,201,184,172,171,173,180,200,227,241,247,250,249,249,313,430,536,536},
    {536,536,399,197,80,80,80,80,92,122,143,160,170,186,201,218,248,268,279,287,296,300,272,230,201,184,172,171,173,180,197,216,224,227,227,226,227,296,421,536,536},
    {536,536,399,192,80,80,80,80,80,118,143,160,170,186,201,218,248,268,279,287,296,300,272,230,201,184,172,171,173,180,187,196,200,204,200,197,199,275,410,536,536},
    {536,536,398,187,80,80,80,80,80,98,137,159,170,186,201,218,248,268,279,287,296,300,272,229,199,177,167,167,170,172,167,169,173,178,170,166,167,249,397,536,536},
    {536,536,394,178,80,80,80,80,80,80,115,144,159,182,201,218,248,268,279,287,296,300,272,222,186,158,150,151,155,153,144,142,146,151,141,137,138,223,384,536,536},
    {536,536,386,165,80,80,80,80,80,80,87,121,141,169,197,218,248,268,279,287,296,300,272,214,165,133,125,126,134,134,122,117,119,124,115,110,108,197,370,536,536},
    {536,536,376,151,80,80,80,80,80,80,80,99,126,156,192,218,248,268,279,287,296,300,272,208,144,110,101,102,112,113,100,93,93,96,89,82,80,170,358,536,536},
    {536,536,368,140,80,80,80,80,80,80,80,80,112,146,186,218,248,268,279,287,296,300,272,204,130,92,81,81,88,86,80,80,80,80,80,80,80,148,347,536,536},
    {536,536,360,130,80,80,80,80,80,80,80,80,97,138,177,212,247,268,279,287,296,297,272,203,121,80,80,80,80,80,80,80,80,80,80,80,80,130,337,536,536},
    {536,536,345,101,80,80,80,80,80,80,80,80,80,117,153,185,222,247,263,271,276,277,261,191,95,80,80,80,80,80,80,80,80,80,80,80,80,96,321,536,536},
    {536,536,311,80,80,80,80,80,80,80,80,80,80,80,85,113,144,165,190,203,204,203,197,137,80,80,80,80,80,80,80,80,80,80,80,80,80,80,290,536,536},
    {536,536,268,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,97,84,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,260,536,536},
    {536,536,243,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,246,536,536},
    {536,536,237,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,243,536,536},
    {536,536,236,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,80,242,536,536}
};

float ScanWeight(const float Y)
{
    // R5a: 25 m fade (was 6 m) — the abrupt return to the cove apron made a 6-10 m shadowed side
    // wall ("knife wedge") at each end of the scan.
    return 1.0F - Smooth((FMath::Abs(Y - ScanY) - ScanHalfWidth) / 2500.0F);
}

// Y is world; LocalZ is height above the scan origin (ScanZ).
float ScanFaceLocalX(const float Y, const float LocalZ)
{
    const float U = FMath::Clamp((Y - ScanY + ScanHalfWidth) / 100.0F, 0.0F, 40.0F);
    const float V = FMath::Clamp((LocalZ + 50.0F) / 50.0F, 0.0F, 21.0F);
    const int32 U0 = FMath::Min(FMath::FloorToInt(U), 39), V0 = FMath::Min(FMath::FloorToInt(V), 20);
    const float FU = U - U0, FV = V - V0;
    return FMath::Lerp(FMath::Lerp(static_cast<float>(ScanFace[V0][U0]), static_cast<float>(ScanFace[V0][U0 + 1]), FU),
        FMath::Lerp(static_cast<float>(ScanFace[V0 + 1][U0]), static_cast<float>(ScanFace[V0 + 1][U0 + 1]), FU), FV);
}

float HeroBayLandRelief(const float X,const float Y,const float Edge)
{
    if(X<=Edge-3500.0F || X>Edge)return 0;
    const float Run=(X-(Edge-3500.0F))/3500.0F;
    const float Center=HeroBayY+450.0F*(Run*Run-.5F);
    const float Runnel=1.0F-Smooth(FMath::Abs(Y-Center)/(520.0F+120.0F*Run));
    const float RockShoulder=1.0F-Smooth(FMath::Abs(Y-(Center-1050.0F))/600.0F);
    // A curved drainage cut and an unequal adjoining bedrock shoulder. Both die into
    // the existing soil upslope and the unchanged cove outside this single 50 m reach.
    return HeroBayWeight(Y)*Smooth(Run)*(-300.0F*Runnel*(.4F+.6F*Run)
        +220.0F*RockShoulder*Smooth((Run-.12F)/.5F));
}

float CoastalErosion(const float Y)
{
    const float Centers[] = {-128000,-47000,-19000,19000,54000,112000};
    const float Widths[] = {18000,16000,12500,15000,12000,22000};
    const float Depths[] = {4200,4700,5400,4200,3500,4800};
    float Depth=0;
    for(int32 Bay=0;Bay<UE_ARRAY_COUNT(Centers);++Bay)
        Depth=FMath::Max(Depth,Depths[Bay]*Smooth(1.0F-FMath::Abs(Y-Centers[Bay])/Widths[Bay]));
    return Depth;
}

float CliffOutcrop(const float Y, const float Depth)
{
    // Unequal gently dipping beds form supported shelves instead of one smooth bow with vertical ribs.
    const float Thresholds[] = {.09F, .27F, .46F, .69F, .85F};
    // R2: beds are sloping benches (wide transitions, thinner offsets), not stair-step ledges.
    // R3 (QA stair-stepped slabs): broader, thinner benches.
    const float Widths[] = {.15F, .13F, .17F, .16F, .14F};
    const float Thickness[] = {120.0F, 220.0F, 170.0F, 280.0F, 150.0F};
    const float Collapse=CoastalErosion(Y)/5400.0F;
    // Low eroded coves have a wide supported rubble apron; intact headlands retain a steeper face.
    float Outcrop = Depth * 1500.0F + Collapse * 5500.0F * Smooth(Depth/.30F);
    // A continuous rubble/talus foot slopes through the waterline everywhere, so the sea meets a
    // supported apron rather than a vertical wall; its reach varies irregularly along the coast.
    const float TalusReach = 3600.0F + 1800.0F * Noise(Y, 4400.0F, 9300.0F) + 700.0F * Noise(Y, -2600.0F, 3100.0F);
    Outcrop += TalusReach * FMath::Square(Smooth((Depth - .32F) / .55F));
    for (int32 Bed = 0; Bed < UE_ARRAY_COUNT(Thresholds); ++Bed)
    {
        const float Dip = .055F * Noise(Y + Bed * 4300.0F, 13000.0F + Bed * 3700.0F, 6800.0F);
        const float BedHeight = Thickness[Bed] * (1.0F + .25F * Noise(Y, Bed * 9100.0F, 4700.0F));
        Outcrop += BedHeight * Smooth((Depth - Thresholds[Bed] - Dip) / Widths[Bed]);
    }
    // Every term is non-decreasing with depth, so the face never overhangs: blocks vary along the
    // coast only and ramp in once below the rim.
    const float Interior = Smooth(Depth / .07F);
    // No sub-7 m term: short-wavelength block noise serrated the waterline row by row.
    const float Blocks = 450.0F + 380.0F * Noise(Y, 14000.0F, 2100.0F);
    // Only the hero cove receives these finite, unequal bedrock ribs and their supported
    // rubble tongue. Every displacement ramps monotonically with depth: no floating blocks
    // or overhanging support fiction. Different onsets prevent a row of repeated folds.
    const float Hero=HeroBayWeight(Y);
    const float NorthernRib=1.0F-Smooth(FMath::Abs(Y-(HeroBayY+950.0F))/600.0F);
    const float SouthernRib=1.0F-Smooth(FMath::Abs(Y-(HeroBayY-1050.0F))/850.0F);
    const float RubbleTongue=1.0F-Smooth(FMath::Abs(Y-(HeroBayY-300.0F))/1500.0F);
    const float HeroRock=Hero*(320.0F*NorthernRib*Smooth((Depth-.025F)/.085F)
        +520.0F*SouthernRib*Smooth((Depth-.09F)/.17F)
        +380.0F*RubbleTongue*Smooth((Depth-.055F)/.30F));
    return Outcrop + Interior * Blocks + HeroRock * (1.0F - ScanWeight(Y));
}

// Cliff rows: station 0 is the shared rim vertex. Sixteen stations span rim -> just below still
// water (fine where the face is seen) and twenty span the submerged foot. Within the scan reach
// the face follows ScanFace above water and a 0.9:1 talus below the scan base, fading back into
// the ordinary profile ~8-33 m under water. A running max keeps every row free of overhangs.
void CliffStations(const float RowY, const FVector& Rim, FVector (&Out)[CliffDepthSteps + 1])
{
    constexpr int32 Upper = 16;
    const float Top = static_cast<float>(Rim.Z);
    const float Split = Top > APacificCoastGraybox::SeaLevelCm + 300.0F ? APacificCoastGraybox::SeaLevelCm - 200.0F : FMath::Lerp(Top, CliffFoot, .4F);
    const float Weight = ScanWeight(RowY);
    Out[0] = Rim;
    for (int32 Step = 1; Step <= CliffDepthSteps; ++Step)
    {
        const float Z = Step <= Upper ? FMath::Lerp(Top, Split, Step / static_cast<float>(Upper))
            : FMath::Lerp(Split, CliffFoot, (Step - Upper) / static_cast<float>(CliffDepthSteps - Upper));
        const float Depth = FMath::Clamp((Top - Z) / (Top - CliffFoot), 0.0F, 1.0F);
        float X = static_cast<float>(Rim.X) + CliffOutcrop(RowY, Depth);
        if (Weight > 0)
        {
            const float Local = Z - ScanZ;
            const float Base = ScanX + ScanFaceLocalX(RowY, -50.0F);
            const float Hero = Local >= -50.0F ? ScanX + ScanFaceLocalX(RowY, Local) : Base + (-50.0F - Local) * .9F;
            const float Fade = 1.0F - Smooth((-50.0F - Local - 800.0F) / 2500.0F);
            X = FMath::Lerp(X, Hero, Weight * Fade);
        }
        Out[Step] = FVector(FMath::Max(X, static_cast<float>(Out[Step - 1].X)), RowY, Z);
    }
}

// --- Bay Area ridge network (DEV-TERRAIN U6) -------------------------------------------------
// Compositional controls inspired by Santa Cruz Mountains / Diablo Range structure: one broad,
// rounded main crest, unequal descending spurs, lower receding back ridgelines separated by
// saddles, rolling foothill knolls, and branching V-section gullies cut into every flank.
float RoundRidge(const FVector2D& Point, const FRidgeNode& A, const FRidgeNode& B, const float Crest)
{
    const FVector2D Direction = B.Position - A.Position;
    const float T = FMath::Clamp(static_cast<float>(FVector2D::DotProduct(Point - A.Position, Direction) / Direction.SizeSquared()), 0.0F, 1.0F);
    const float Distance = static_cast<float>((Point - FMath::Lerp(A.Position, B.Position, T)).Size());
    const float Rounded = FMath::Sqrt(Distance * Distance + Crest * Crest) - Crest;
    const float S = FMath::Clamp(1.0F - Rounded / FMath::Lerp(A.Width, B.Width, T), 0.0F, 1.0F);
    // Convex shoulders near the crest and a long concave footslope; no cone or knife edge.
    return FMath::Lerp(A.Height, B.Height, T) * (.6F * S * S * (3.0F - 2.0F * S) + .4F * FMath::Pow(S, 1.8F));
}

struct FChain final { const FRidgeNode* Nodes; int32 Count; };
const FRidgeNode MainCrest[] = {
    {FVector2D(-186000,-170000), 9000, 50000}, {FVector2D(-161000,-88000), 20500, 50000},
    {FVector2D(-147000,-46000), 24200, 52000}, {FVector2D(-138000,-12000), 25600, 53000},
    {FVector2D(-131000,36000), 22400, 50000}, {FVector2D(-121000,108000), 14000, 50000},
    {FVector2D(-133000,172000), 7000, 46000}};
const FRidgeNode SpurA[] = {{FVector2D(-155000,-66000),17000,30000},{FVector2D(-112000,-96000),9800,27000},{FVector2D(-72000,-126000),2500,22000}};
const FRidgeNode SpurB[] = {{FVector2D(-140000,-26000),22000,30000},{FVector2D(-108000,-40000),13200,26000},{FVector2D(-74000,-55000),4200,22000}};
const FRidgeNode SpurC[] = {{FVector2D(-133000,30000),16500,29000},{FVector2D(-100000,62000),10400,26000},{FVector2D(-66000,104000),2800,23000}};
const FRidgeNode SpurD[] = {{FVector2D(-123000,100000),12500,27000},{FVector2D(-96000,140000),6400,24000},{FVector2D(-70000,168000),1200,21000}};
const FRidgeNode BackRidge[] = {
    {FVector2D(-205000,-150000),8500,36000},{FVector2D(-196000,-60000),13500,38000},{FVector2D(-208000,10000),11000,36000},
    {FVector2D(-198000,80000),15500,38000},{FVector2D(-206000,160000),9000,34000}};
const FRidgeNode SaddleA[] = {{FVector2D(-150000,-60000),15000,26000},{FVector2D(-196000,-60000),12000,26000}};
const FRidgeNode SaddleB[] = {{FVector2D(-127000,70000),15500,26000},{FVector2D(-198000,80000),13500,26000}};
const FChain RidgeChains[] = {{MainCrest,7},{SpurA,3},{SpurB,3},{SpurC,3},{SpurD,3},{BackRidge,5},{SaddleA,2},{SaddleB,2}};
constexpr int32 GulliedChains = 6; // main crest, four spurs and the back ridge carry gully systems
const FRidgeNode Knolls[][2] = {
    {{FVector2D(-78000,-18000),3600,21000},{FVector2D(-69000,-13000),2880,21000}},
    {{FVector2D(-62000,42000),2700,18000},{FVector2D(-53000,47000),2160,18000}},
    {{FVector2D(-90000,82000),4200,22000},{FVector2D(-81000,87000),3360,22000}},
    {{FVector2D(-56000,-88000),2200,17000},{FVector2D(-47000,-83000),1760,17000}}};

struct FGully final { FRidgeNode A, B; FVector2D Min, Max; };
const TArray<FGully>& GullyNetwork()
{
    // Deterministic table built once: each flank segment receives trunk gullies that start just
    // below the crest, deepen mid-slope and fade into the footslope, each joined by a tributary.
    static const TArray<FGully> Table = []
    {
        TArray<FGully> Out;
        FRandomStream Random(7041);
        auto Add = [&Out](const FRidgeNode& A, const FRidgeNode& B)
        {
            const double Margin = FMath::Max(A.Width, B.Width);
            Out.Add({A, B, FVector2D(FMath::Min(A.Position.X, B.Position.X) - Margin, FMath::Min(A.Position.Y, B.Position.Y) - Margin),
                FVector2D(FMath::Max(A.Position.X, B.Position.X) + Margin, FMath::Max(A.Position.Y, B.Position.Y) + Margin)});
        };
        for (int32 ChainIndex = 0; ChainIndex < GulliedChains; ++ChainIndex)
        {
            const FChain& Chain = RidgeChains[ChainIndex];
            for (int32 Node = 0; Node + 1 < Chain.Count; ++Node)
            {
                const FRidgeNode& A = Chain.Nodes[Node];
                const FRidgeNode& B = Chain.Nodes[Node + 1];
                const FVector2D Along = (B.Position - A.Position).GetSafeNormal();
                const int32 Count = FMath::Max(1, static_cast<int32>((B.Position - A.Position).Size() / 19000.0));
                for (int32 Index = 0; Index < Count; ++Index)
                {
                    const float T = (Index + .5F + Random.FRandRange(-.25F, .25F)) / Count;
                    const FVector2D Crest = FMath::Lerp(A.Position, B.Position, T);
                    const float Height = FMath::Lerp(A.Height, B.Height, T), Width = FMath::Lerp(A.Width, B.Width, T);
                    for (int32 Side = -1; Side <= 1; Side += 2)
                    {
                        if (Random.FRand() < .12F) continue;
                        const float Angle = FMath::Atan2(static_cast<float>(-Along.X * Side), static_cast<float>(Along.Y * Side)) + Random.FRandRange(-.35F, .35F);
                        const FVector2D Normal(FMath::Cos(Angle), FMath::Sin(Angle));
                        const FVector2D Head = Crest + Normal * (Width * .10F);
                        const float Length = Width * Random.FRandRange(.6F, .95F);
                        const float GullyWidth = Random.FRandRange(.17F, .24F) * Width;
                        const float Depth = FMath::Min(FMath::Clamp(Height * Random.FRandRange(.11F, .17F), 600.0F, 3400.0F), .24F * GullyWidth);
                        const FVector2D Mid = Head + Normal * (Length * .5F), Mouth = Head + Normal * Length;
                        Add({Head, 0, GullyWidth * .6F}, {Mid, Depth, GullyWidth});
                        Add({Mid, Depth, GullyWidth}, {Mouth, 0, GullyWidth * 1.2F});
                        const float TributaryAngle = Angle + Side * (Random.FRand() < .5F ? -1.0F : 1.0F) * Random.FRandRange(.6F, .9F);
                        const float TributaryLength = Length * Random.FRandRange(.3F, .45F);
                        const FVector2D TributaryHead = Mid - FVector2D(FMath::Cos(TributaryAngle), FMath::Sin(TributaryAngle)) * TributaryLength;
                        Add({TributaryHead, 0, GullyWidth * .45F}, {Mid, Depth * .65F, GullyWidth * .7F});
                    }
                }
            }
        }
        return Out;
    }();
    return Table;
}

float GullyIncision(const FVector2D& Point)
{
    float Incision = 0;
    for (const FGully& Gully : GullyNetwork())
    {
        if (Point.X < Gully.Min.X || Point.Y < Gully.Min.Y || Point.X > Gully.Max.X || Point.Y > Gully.Max.Y) continue;
        const FVector2D Direction = Gully.B.Position - Gully.A.Position;
        const float T = FMath::Clamp(static_cast<float>(FVector2D::DotProduct(Point - Gully.A.Position, Direction) / Direction.SizeSquared()), 0.0F, 1.0F);
        const float Distance = static_cast<float>((Point - FMath::Lerp(Gully.A.Position, Gully.B.Position, T)).Size());
        // The V floor is rounded over ~12 m so the channel has a natural thalweg, not a mesh crease.
        const float Floor = FMath::Sqrt(Distance * Distance + 1200.0F * 1200.0F) - 1200.0F;
        const float S = FMath::Clamp(1.0F - Floor / FMath::Lerp(Gully.A.Width, Gully.B.Width, T), 0.0F, 1.0F);
        // V-section relief carried by real geometry, not a normal map.
        Incision = FMath::Max(Incision, FMath::Lerp(Gully.A.Height, Gully.B.Height, T) * S * FMath::Sqrt(S) );
    }
    return Incision;
}

// R3: three principal drainage systems between the spurs, facing the mountain/overview poses. Each
// trunk deepens mid-slope and fades into the foothills; tributaries join it obliquely from upslope.
// They are carved deeper than the minor gully network (cap 55% of local relief vs 42%).
const FRidgeNode MajorA[] = {{FVector2D(-127000,8000),0,9000},{FVector2D(-110000,10000),4600,12000},{FVector2D(-92000,16000),5200,14000},{FVector2D(-72000,22000),3600,15000},{FVector2D(-52000,30000),0,16000}};
const FRidgeNode MajorA1[] = {{FVector2D(-118000,-9000),0,7000},{FVector2D(-110000,10000),3100,9000}};
const FRidgeNode MajorA2[] = {{FVector2D(-112000,34000),0,7000},{FVector2D(-92000,16000),3500,10000}};
const FRidgeNode MajorA3[] = {{FVector2D(-99000,-6000),0,6500},{FVector2D(-80000,19000),2600,9000}};
const FRidgeNode MajorB[] = {{FVector2D(-148000,-52000),0,9000},{FVector2D(-128000,-60000),4300,12000},{FVector2D(-108000,-70000),5000,14000},{FVector2D(-88000,-80000),3800,15000},{FVector2D(-66000,-92000),0,16000}};
const FRidgeNode MajorB1[] = {{FVector2D(-142000,-76000),0,7000},{FVector2D(-120000,-64000),3100,9500}};
const FRidgeNode MajorB2[] = {{FVector2D(-117000,-50000),0,7000},{FVector2D(-100000,-74000),3100,9500}};
const FRidgeNode MajorC[] = {{FVector2D(-125000,72000),0,8500},{FVector2D(-108000,88000),4100,11500},{FVector2D(-90000,104000),4600,13000},{FVector2D(-70000,120000),0,15000}};
const FRidgeNode MajorC1[] = {{FVector2D(-113000,108000),0,6500},{FVector2D(-96000,99000),2900,9000}};
const FChain MajorDrainages[] = {{MajorA,5},{MajorA1,2},{MajorA2,2},{MajorA3,2},{MajorB,5},{MajorB1,2},{MajorB2,2},{MajorC,4},{MajorC1,2}};

float MajorIncision(const FVector2D& Point)
{
    float Incision = 0;
    for (const FChain& Chain : MajorDrainages)
        for (int32 Node = 0; Node + 1 < Chain.Count; ++Node)
        {
            const FRidgeNode& A = Chain.Nodes[Node];
            const FRidgeNode& B = Chain.Nodes[Node + 1];
            const FVector2D Direction = B.Position - A.Position;
            const float T = FMath::Clamp(static_cast<float>(FVector2D::DotProduct(Point - A.Position, Direction) / Direction.SizeSquared()), 0.0F, 1.0F);
            const float Distance = static_cast<float>((Point - FMath::Lerp(A.Position, B.Position, T)).Size());
            const float Floor = FMath::Sqrt(Distance * Distance + 1500.0F * 1500.0F) - 1500.0F;
            const float S = FMath::Clamp(1.0F - Floor / FMath::Lerp(A.Width, B.Width, T), 0.0F, 1.0F);
            Incision = FMath::Max(Incision, FMath::Lerp(A.Height, B.Height, T) * S * FMath::Sqrt(S));
        }
    return Incision;
}

// Distant receding ridgelines beyond the playable land (3–9 km west). They rise from below sea
// level behind the land collar so no channel of sea separates them, and grow taller with distance.
constexpr float RangeEast = -232000.0F, RangeWest = -900000.0F, RangeHalfY = 760000.0F;
const FRidgeNode RangeNear[] = {{FVector2D(-330000,-520000),14000,90000},{FVector2D(-310000,-210000),26000,95000},{FVector2D(-345000,60000),21000,90000},{FVector2D(-320000,330000),28000,95000},{FVector2D(-350000,600000),15000,90000}};
const FRidgeNode RangeMid[] = {{FVector2D(-470000,-640000),26000,120000},{FVector2D(-455000,-300000),39000,125000},{FVector2D(-490000,-20000),33000,120000},{FVector2D(-460000,260000),42000,125000},{FVector2D(-480000,640000),27000,120000}};
const FRidgeNode RangeFar[] = {{FVector2D(-640000,-700000),38000,150000},{FVector2D(-620000,-380000),52000,160000},{FVector2D(-660000,-60000),47000,150000},{FVector2D(-630000,300000),56000,160000},{FVector2D(-650000,700000),40000,150000}};

float DistantRangeHeight(const float X, const float Y)
{
    const float Edge = FMath::Min(FMath::Min(RangeEast - X, X - RangeWest), RangeHalfY - FMath::Abs(Y));
    const float Interior = Smooth(Edge / 90000.0F);
    const FVector2D Point(X + 9000.0F * Noise(X, Y, 120000.0F), Y + 12000.0F * Noise(X + 5000.0F, Y, 140000.0F));
    float Range = 0;
    for (const FRidgeNode* Chain : {RangeNear, RangeMid, RangeFar})
        for (int32 Node = 0; Node < 4; ++Node)
        {
            const float Ridge = RoundRidge(Point, Chain[Node], Chain[Node + 1], 6000.0F);
            constexpr float Seam = 2500.0F;
            const float H = FMath::Clamp(.5F + .5F * (Range - Ridge) / Seam, 0.0F, 1.0F);
            Range = FMath::Lerp(Ridge, Range, H) + Seam * H * (1.0F - H);
        }
    Range = Range * (1.0F + .12F * Noise(X, Y, 60000.0F)) + 900.0F * Noise(X, Y, 33000.0F);
    return FMath::Lerp(SubmergedRim, FMath::Max(Range, 0.0F), Interior);
}
}

float APacificCoastGraybox::GetCliffEdgeX(const float Y)
{
    // Several non-repeating spatial frequencies produce coves and projecting headlands.
    const float Headland=20000.0F + 3800.0F * CoastalTerrain::Noise(Y, 12730.0F, 7200.0F)
        + 1600.0F * CoastalTerrain::Noise(Y, -7300.0F, 1900.0F)
        + 2600.0F * FMath::Sin(Y / 14500.0F);
    const float Edge = FMath::Max(14500.0F,Headland-1500.0F*CoastalTerrain::CoastalErosion(Y)/5400.0F);
    // The rim inside the scan reach is the top of the rebuilt closed face behind the scan.
    using namespace CoastalTerrain;
    return FMath::Lerp(Edge, ScanX + ScanFaceLocalX(Y, 1000.0F), ScanWeight(Y));
}

float APacificCoastGraybox::GetCoastalLandHeight(const float X, const float Y)
{
    using namespace CoastalTerrain;
    // The complete ridge descends to the submerged perimeter over a broad shoulder. The collar's
    // waterline is displaced by low-frequency noise (vanishing at the outer boundary) so the
    // north, south and west shores are irregular instead of ruler-straight.
    float WestDistance = X - West, NorthDistance = North - FMath::Abs(Y);
    const float YSide = Y < 0 ? -1.0F : 1.0F;
    WestDistance += (9000.0F * Noise(Y, 3100.0F, 52000.0F) + 3500.0F * Noise(Y, -4100.0F, 17000.0F)) * FMath::Clamp(WestDistance / 15000.0F, 0.0F, 1.0F);
    // R5a: stronger, multi-scale displacement (QA: N/S shores still read ruler-straight from 2.5 km).
    NorthDistance += (19000.0F * Noise(X, 9100.0F * YSide, 47000.0F) + 10000.0F * Noise(X, -5300.0F * YSide, 17000.0F) + 3500.0F * Noise(X, 2900.0F * YSide, 6100.0F)) * FMath::Clamp(NorthDistance / 15000.0F, 0.0F, 1.0F);
    const float Interior = Smooth(FMath::Min(WestDistance, NorthDistance) / 56000.0F);
    const float RouteDistance = FMath::Max(FMath::Max(-X - 23000.0F, FMath::Abs(Y) - 17000.0F), 0.0F);
    const float Rise = Smooth(RouteDistance / 24000.0F);
    const FVector2D Point(X + 1800.0F * Noise(X, Y, 43000.0F), Y + 2400.0F * Noise(X + 13000.0F, Y, 51000.0F));
    float Mountain = 0;
    for (int32 ChainIndex = 0; ChainIndex < UE_ARRAY_COUNT(RidgeChains); ++ChainIndex)
    {
        const FChain& Chain = RidgeChains[ChainIndex];
        for (int32 Node = 0; Node + 1 < Chain.Count; ++Node)
            Mountain = JoinRidge(Mountain, RoundRidge(Point, Chain.Nodes[Node], Chain.Nodes[Node + 1], ChainIndex == 0 ? 3500.0F : 2600.0F));
    }
    for (const auto& Knoll : Knolls)
        Mountain = JoinRidge(Mountain, RoundRidge(Point, Knoll[0], Knoll[1], 3000.0F));
    // Branching gullies remain subordinate to the ridge mass they cut.
    Mountain -= FMath::Max(FMath::Min(GullyIncision(Point), Mountain * .42F), FMath::Min(MajorIncision(Point), Mountain * .55F));
    // Only broad (170 m) coherent undulation; no short-wavelength ripple over the slopes.
    Mountain += Smooth(Mountain / 7000.0F) * 260.0F * Noise(X, Y, 17000.0F);
    const float Hills = 650.0F + 700.0F * Noise(X, Y, 26000.0F) + 60.0F * Noise(X, Y, 9500.0F);
    const float LandHeight = FMath::Lerp(SubmergedRim, Rise * FMath::Max(0.0F, Hills + Mountain), Interior);
    // Inland worksite, routes and trails (X <= 105 m) keep their existing supported grade.
    // Only the outer coastal apron descends into unequal coves between surviving high sea headlands.
    const float CoastBlend=Smooth((X-10500.0F)/FMath::Max(1000.0F,GetCliffEdgeX(Y)-10500.0F));
    // Inside the scan reach the cove floor is 6 m shallower so the land cap buries the scan's top edge.
    const float ScanCentre=1.0F-Smooth((FMath::Abs(Y-ScanY)-1800.0F)/800.0F); // flat: keeps the runnel/bank relief intact
    const float CoastHeight=LandHeight-(CoastalErosion(Y)-600.0F*ScanCentre)*CoastBlend*Interior;
    // A shallow seasonal swale gives the short woodland bridge an actual crossing to span.
    // Its 16 m bank-to-bank width spans several near-field samples; both bridge landings remain at grade.
    return CoastHeight - 180.0F * ChannelWeight(X, Y) + HeroBayLandRelief(X,Y,GetCliffEdgeX(Y));
}

float APacificCoastGraybox::GetCoastalShoreX(float Y)
{
    using namespace CoastalTerrain;
    Y = FMath::Clamp(Y, -North, North);
    const TArray<float>& Samples=CoastRowsY();
    int32 Low=0,High=Samples.Num()-1;
    while(Low+1<High)
    {
        const int32 Middle=(Low+High)/2;
        if(Samples[Middle]<=Y)Low=Middle;else High=Middle;
    }
    const int32 Row=FMath::Min(Low,Samples.Num()-2);
    const float YA = CoastY(Row), YB = CoastY(Row + 1);
    const float EdgeA = GetCliffEdgeX(YA), EdgeB = GetCliffEdgeX(YB);
    const float TopA = GetCoastalLandHeight(EdgeA, YA), TopB = GetCoastalLandHeight(EdgeB, YB);
    const float Alpha = (Y - YA) / (YB - YA);
    const float Top = FMath::Lerp(TopA, TopB, Alpha);
    FVector StationsA[CliffDepthSteps + 1], StationsB[CliffDepthSteps + 1];
    CliffStations(YA, FVector(EdgeA, YA, TopA), StationsA);
    CliffStations(YB, FVector(EdgeB, YB, TopB), StationsB);
    // Intersect the actual two triangles of the sampled cliff rather than an analytic approximation.
    for (int32 Step = 0; Step < CliffDepthSteps; ++Step)
    {
        const FVector A = StationsA[Step], B = StationsA[Step + 1];
        const FVector C = StationsB[Step], D = StationsB[Step + 1];
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
    (void)Top;
    return FMath::Lerp(EdgeA, EdgeB, Alpha) + CliffOutcrop(Y, FMath::Clamp((Top - SeaLevelCm) / (Top - CliffFoot), 0.0F, 1.0F));
}

namespace CoastalTerrain
{
float FlightAndBridgeClearance(const float X, const float Y)
{
    // Protect the actual takeoff/route corridor; its edges blend into the surrounding woodland.
    const float FlightClearance = FMath::Max(FMath::Max(-X - 8000.0F, X - 12500.0F), FMath::Abs(Y) - 5500.0F);
    const FVector2D Bridge = BridgeCoordinates(X, Y);
    const float BridgeClearance = FMath::Max(FMath::Abs(Bridge.X) - 2100.0F, FMath::Abs(Bridge.Y) - 300.0F);
    return Smooth(FlightClearance / 4500.0F) * Smooth(BridgeClearance / 350.0F);
}
}

FCoastHabitatSample FPacificCoastHabitat::Sample(const float X, const float Y)
{
    using namespace CoastalTerrain;
    FCoastHabitatSample Result;
    if (!FMath::IsFinite(X) || !FMath::IsFinite(Y) || X < West || FMath::Abs(Y) > North) return Result;
    const float Edge = APacificCoastGraybox::GetCliffEdgeX(Y);
    if (X > Edge) return Result;
    auto Height = [](float SX, float SY) { return APacificCoastGraybox::GetCoastalLandHeight(SX, SY); };
    const float H = Height(X, Y);
    if (H < APacificCoastGraybox::SeaLevelCm + 300.0F) return Result;
    // Local slope (12 m) decides rock; aspect uses a broader 60 m gradient so it follows landforms,
    // not individual mesh facets. Two-scale concavity marks gully floors and swales.
    constexpr float Near = 1200.0F, Ring = 3000.0F, Broad = 6000.0F, Wide = 12000.0F;
    const float NX = (Height(X + Near, Y) - Height(X - Near, Y)) / (2.0F * Near);
    const float NY = (Height(X, Y + Near) - Height(X, Y - Near)) / (2.0F * Near);
    const float BW = Height(X - Broad, Y), BE = Height(X + Broad, Y), BS = Height(X, Y - Broad), BN = Height(X, Y + Broad);
    const float WW = Height(X - Wide, Y), WE = Height(X + Wide, Y), WS = Height(X, Y - Wide), WN = Height(X, Y + Wide);
    Result.Grade = FMath::Sqrt(NX * NX + NY * NY);
    const FVector2D BroadGradient((BE - BW) / (2.0F * Broad), (BN - BS) / (2.0F * Broad));
    const float BroadGrade = static_cast<float>(BroadGradient.Size());
    // Scene convention (shared with GetCoastalVegetationCover since v4): +Y is north and the ocean
    // side (+X) stands in for California's west. A slope faces its downhill direction; south and
    // south-west (ocean-ward) aspects are the dry, sun-exposed ones.
    const FVector2D SunFacing = FVector2D(.45F, -.89F).GetSafeNormal();
    const FVector2D Facing = BroadGrade > 1e-4F ? -BroadGradient / BroadGrade : FVector2D::ZeroVector;
    Result.SunExposure = static_cast<float>(FVector2D::DotProduct(Facing, SunFacing)) * Smooth(BroadGrade / .14F);
    // Compare against a 30 m ring mean rather than the single centre height, so crossing a channel
    // thalweg does not flip the classification within one mesh cell.
    const float Centre = .25F * (Height(X - Ring, Y) + Height(X + Ring, Y) + Height(X, Y - Ring) + Height(X, Y + Ring));
    const float Concavity = .5F * (.25F * (BW + BE + BS + BN) - Centre) + .5F * (.25F * (WW + WE + WS + WN) - Centre);
    Result.Drainage = Smooth((Concavity + 100.0F) / 1500.0F);
    // Two irregular noise octaves displace every threshold so transitions are broad and patchy,
    // never contour bands or a sawtooth along mesh rows.
    const float Patch = .55F * Noise(X, Y + 3000.0F, 9000.0F) + .3F * Noise(X - 7000.0F, Y, 3100.0F);
    float Rock = Smooth((Result.Grade - .42F + .14F * Patch) / .62F);
    // Moisture index: shade and drainage favour woodland; ocean bluffs are wind-pruned grassland.
    const float Bluff = Smooth((X - (Edge - 5000.0F)) / 3500.0F);
    // Golden grass dominates open Bay Area hills; woodland keeps to shaded aspects and drainages.
    const float Moisture = .30F - .45F * Result.SunExposure + .66F * Result.Drainage + .2F * Patch - .35F * Bluff;
    float Woodland = Smooth((Moisture - .5F) / .45F) * FlightAndBridgeClearance(X, Y);
    float Sheltered = Smooth((Moisture - .18F) / .42F) * (1.0F - Woodland);
    const float Shore = Smooth((X - (Edge - 700.0F)) / 700.0F) + (1.0F - Smooth((H - APacificCoastGraybox::SeaLevelCm - 300.0F) / 600.0F));
    const float Land = 1.0F - FMath::Clamp(Shore, 0.0F, 1.0F);
    Rock *= Land;
    const float Soil = Land - Rock;
    Result.Woodland = Soil * Woodland;
    Result.ShelteredGrass = Soil * Sheltered;
    Result.ExposedGrass = FMath::Max(0.0F, Soil - Result.Woodland - Result.ShelteredGrass);
    Result.Rock = Rock;
    Result.Shore = 1.0F - Land;
    const float Weights[] = {Result.ExposedGrass, Result.ShelteredGrass, Result.Woodland, Result.Rock, Result.Shore};
    int32 Best = 0;
    for (int32 Index = 1; Index < UE_ARRAY_COUNT(Weights); ++Index) if (Weights[Index] > Weights[Best]) Best = Index;
    Result.Class = static_cast<ECoastHabitatClass>(Best);
    return Result;
}

ECoastHabitatClass FPacificCoastHabitat::GetHabitatClass(const float X, const float Y)
{
    return Sample(X, Y).Class;
}

FLinearColor FPacificCoastHabitat::GetGroundVertexColor(const float X, const float Y)
{
    const FCoastHabitatSample Habitat = Sample(X, Y);
    // The exposed bed of the seasonal swale reads as damp woodland earth.
    const float Bed = .9F * CoastalTerrain::ChannelWeight(X, Y);
    return FLinearColor(Habitat.ExposedGrass * (1.0F - Bed), Habitat.ShelteredGrass * (1.0F - Bed),
        FMath::Lerp(Habitat.Woodland, 1.0F - Habitat.Rock - Habitat.Shore, Bed), 1.0F - GetForestLitter(X, Y));
}

float FPacificCoastHabitat::GetForestLitter(const float X, const float Y)
{
    // DEV-FLORA trunk-base litter (R conifer needles, G oak leaves) from the placed trees; the
    // ground material reads it as 1 - vertex alpha and height-blends leaf/needle litter.
    if (!FMath::IsFinite(X) || !FMath::IsFinite(Y) || X > APacificCoastGraybox::GetCliffEdgeX(Y)) return 0.0F;
    const FLinearColor Litter = APacificCoastGraybox::GetForestLitterMask(X, Y);
    return FMath::Clamp(FMath::Max(Litter.R, Litter.G), 0.0F, 1.0F);
}

float FPacificCoastHabitat::GetCliffFaceX(const float Y, const float Z)
{
    using namespace CoastalTerrain;
    // Mirrors BuildCoastalTerrain: stations of the two bracketing coast rows, linear in Z and Y.
    const float ClampedY = FMath::Clamp(Y, -North, North);
    const TArray<float>& Samples = CoastRowsY();
    int32 Low = 0, High = Samples.Num() - 1;
    while (Low + 1 < High) { const int32 Middle = (Low + High) / 2; if (Samples[Middle] <= ClampedY) Low = Middle; else High = Middle; }
    const int32 Row = FMath::Min(Low, Samples.Num() - 2);
    auto RowFace = [Z](const float RowY)
    {
        const float Edge = APacificCoastGraybox::GetCliffEdgeX(RowY);
        FVector Stations[CliffDepthSteps + 1];
        CliffStations(RowY, FVector(Edge, RowY, APacificCoastGraybox::GetCoastalLandHeight(Edge, RowY)), Stations);
        if (Z >= Stations[0].Z) return static_cast<float>(Stations[0].X);
        for (int32 Step = 0; Step < CliffDepthSteps; ++Step)
            if (Z >= Stations[Step + 1].Z)
                return FMath::Lerp(static_cast<float>(Stations[Step].X), static_cast<float>(Stations[Step + 1].X),
                    static_cast<float>((Stations[Step].Z - Z) / (Stations[Step].Z - Stations[Step + 1].Z)));
        return static_cast<float>(Stations[CliffDepthSteps].X);
    };
    const float YA = Samples[Row], YB = Samples[Row + 1];
    return FMath::Lerp(RowFace(YA), RowFace(YB), FMath::Clamp((ClampedY - YA) / (YB - YA), 0.0F, 1.0F));
}

float FPacificCoastHabitat::GetDistantRangeHeight(const float X, const float Y)
{
    return CoastalTerrain::DistantRangeHeight(X, Y);
}

float APacificCoastGraybox::GetCoastalVegetationCover(const float X, const float Y)
{
    using namespace CoastalTerrain;
    if (!FMath::IsFinite(X) || !FMath::IsFinite(Y) || X < West || FMath::Abs(Y) > North || X > GetCliffEdgeX(Y)) return 0;
    // Coverage derives from the same habitat field as the ground material, so trees, shrubs and
    // ground surfaces agree: woodland cover plus partial cover on sheltered grass.
    const FCoastHabitatSample Habitat = FPacificCoastHabitat::Sample(X, Y);
    const float Cover = Habitat.Woodland + .45F * Habitat.ShelteredGrass + .1F * Habitat.ExposedGrass;
    return FMath::Clamp(Cover * FlightAndBridgeClearance(X, Y), 0.0F, 1.0F);
}

void APacificCoastGraybox::BuildCoastalTerrain(UMaterialInterface* Material)
{
    using namespace CoastalTerrain;
    const int32 Rows = CoastRowsY().Num();
    constexpr int32 Columns = 193;
    TArray<FVector> Vertices;
    TArray<FVector2D> UV;
    TArray<FLinearColor> CoverColors;
    TArray<int32> Triangles[5];
    auto Vertex = [&Vertices, &UV, &CoverColors](FVector Position)
    {
        UV.Add(FVector2D(Position.X / 4000.0F, Position.Y / 4000.0F));
        // Habitat weights drive the ground material layers (see FPacificCoastHabitat).
        CoverColors.Add(FPacificCoastHabitat::GetGroundVertexColor(Position.X, Position.Y));
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
        FVector Stations[CliffDepthSteps + 1];
        CliffStations(static_cast<float>(Vertices[Rim].Y), Vertices[Rim], Stations);
        for (int32 Depth = 1; Depth <= DepthSteps; ++Depth) Cliff.Add(Vertex(Stations[Depth]));
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

    // Distant receding ridgelines (3–9 km): a separate low-density shell that starts below sea level
    // under the west collar and rises westward, giving layered skylines for atmospheric haze.
    {
        constexpr int32 RangeColumns = 141, RangeRows = 161;
        TArray<FVector> RangeVertices, RangeNormals;
        TArray<FVector2D> RangeUV;
        TArray<FLinearColor> RangeColors;
        TArray<int32> RangeIndices;
        for (int32 Row = 0; Row < RangeRows; ++Row)
            for (int32 Column = 0; Column < RangeColumns; ++Column)
            {
                const float X = FMath::Lerp(RangeEast + 2000.0F, RangeWest, Column / static_cast<float>(RangeColumns - 1));
                const float Y = FMath::Lerp(-RangeHalfY, RangeHalfY, Row / static_cast<float>(RangeRows - 1));
                const float Z = DistantRangeHeight(X, Y);
                const float GX = (DistantRangeHeight(X + 3000.0F, Y) - DistantRangeHeight(X - 3000.0F, Y)) / 6000.0F;
                const float GY = (DistantRangeHeight(X, Y + 3000.0F) - DistantRangeHeight(X, Y - 3000.0F)) / 6000.0F;
                RangeVertices.Add(FVector(X, Y, Z));
                RangeNormals.Add(FVector(-GX, -GY, 1.0F).GetSafeNormal());
                RangeUV.Add(FVector2D(X / 4000.0F, Y / 4000.0F));
                // Same aspect rule as the land habitat: dry grass on sun-facing slopes, dark cover on shaded ones.
                const float Grade = FMath::Sqrt(GX * GX + GY * GY);
                const float Facing = Grade > 1e-4F ? (-GX * .45F + GY * .89F) / Grade : 0.0F;
                const float Shade = Smooth(.5F + .5F * Facing + .2F * Noise(X, Y, 40000.0F)) * Smooth(Grade / .1F);
                const float Rock = Smooth((Grade - .65F) / .3F);
                RangeColors.Add(FLinearColor((1.0F - Shade) * (1.0F - Rock), .35F * Shade * (1.0F - Rock), .65F * Shade * (1.0F - Rock), 1.0F));
            }
        for (int32 Row = 0; Row < RangeRows - 1; ++Row)
            for (int32 Column = 0; Column < RangeColumns - 1; ++Column)
            {
                const int32 A = Row * RangeColumns + Column, B = A + 1, C = A + RangeColumns, D = C + 1;
                RangeIndices.Append({A, C, B, B, C, D});
            }
        UProceduralMeshComponent* Range = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("CoastalDistantRange"));
        Range->SetupAttachment(Root);
        Range->bUseAsyncCooking = true;
        Range->SetCollisionProfileName(TEXT("BlockAll"));
        Range->CreateMeshSection_LinearColor(0, RangeVertices, RangeIndices, RangeNormals, RangeUV, RangeColors, TArray<FProcMeshTangent>(), true);
        Range->SetMaterial(0, Material);
        Range->SetCastShadow(true);
        CoastalColorParts.Add(Range); CoastalPartColors.Add(FLinearColor(.16F,.17F,.10F));
        ++GeneratedPrimitiveCount;
    }

    // R5: the single scanned hero face, rigidly placed (scale 1, no rotation, one instance) in front
    // of the rebuilt closed face; LOD0 triangles are its collision (complex-as-simple), so the
    // Pawn sweep meets the rock that is drawn. Skipped while the offline draft importer runs.
    {
        FString ImportCommandlet;
        const bool bDraftImport = FParse::Value(FCommandLine::Get(), TEXT("run="), ImportCommandlet)
            && (ImportCommandlet.Equals(TEXT("CoastDraftImport"), ESearchCase::IgnoreCase) || ImportCommandlet.Equals(TEXT("HabitatDraftImport"), ESearchCase::IgnoreCase));
        if (!bDraftImport && FPackageName::DoesPackageExist(TEXT("/Game/Development/CoastDraft/Meshes/SM_CoastHeroCliffScanDraft")))
        {
            static ConstructorHelpers::FObjectFinder<UStaticMesh> HeroScan(TEXT("/Game/Development/CoastDraft/Meshes/SM_CoastHeroCliffScanDraft.SM_CoastHeroCliffScanDraft"));
            if (HeroScan.Object != nullptr)
            {
                UStaticMeshComponent* Scan = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoastalHeroCliffScan"));
                Scan->SetupAttachment(Root);
                Scan->SetStaticMesh(HeroScan.Object);
                Scan->SetRelativeLocation(FVector(ScanX, ScanY, ScanZ));
                Scan->SetCollisionProfileName(TEXT("BlockAll"));
                Scan->SetCastShadow(true);
                Scan->SetMaterial(0, Material);
                CoastalColorParts.Add(Scan); CoastalPartColors.Add(FLinearColor(.58F, .64F, .68F));
                ++GeneratedPrimitiveCount;
            }
        }
    }

    // Angular fallen buttresses intersect the supported cliff foot, with submerged bases.
    // Unequal spacing, rotation and proportions avoid the former row of identical cubes.
    TArray<FVector> TalusVertices, TalusNormals;
    TArray<int32> TalusIndices;
    FRandomStream RockRandom(914823);
    // R3: 64 unequal blocks; half gather at the eroded cove centres as a supported talus/rubble
    // foot, several breaking the surface (peaks up to ~19 m above the sea).
    const float CoveCentres[] = {-47000.0F, -19000.0F, 19000.0F, 54000.0F};
    for (int32 Rock = 0; Rock < 64; ++Rock)
    {
        float Y = Rock % 2 == 0 ? FMath::Clamp(CoveCentres[(Rock / 2) % 4] + RockRandom.FRandRange(-6500.0F, 6500.0F), -57000.0F, 57000.0F)
            : RockRandom.FRandRange(-57000.0F, 57000.0F);
        // Keep the scanned hero face clear: blocks that fall in front of it move to its flanks.
        if (FMath::Abs(Y - ScanY) < ScanHalfWidth + 700.0F) Y = ScanY + (Y < ScanY ? -1.0F : 1.0F) * (ScanHalfWidth + 700.0F + FMath::Abs(Y - ScanY) * .8F);
        const float Edge = GetCliffEdgeX(Y);
        const float Radius = RockRandom.FRandRange(650.0F, 2100.0F);
        const float Peak = RockRandom.FRandRange(-7400.0F, -4600.0F);
        // Blocks sit on the real sloping cliff foot at the waterline (previously buried behind it).
        const float Foot = GetCoastalShoreX(Y) + RockRandom.FRandRange(-300.0F, 700.0F) - Radius * .3F;
        const int32 BaseIndex = TalusVertices.Num();
        const float Rotation = RockRandom.FRandRange(0.0F, 2.0F * PI);
        TalusVertices.Add(FVector(Edge - 300.0F, Y, -15500.0F));
        for (int32 Ring = 0; Ring < 2; ++Ring)
            for (int32 Corner = 0; Corner < 7; ++Corner)
            {
                const float Angle = Rotation + Corner * (2.0F * PI / 7.0F);
                const float R = Radius * RockRandom.FRandRange(.65F, 1.15F) * (Ring == 0 ? 1.0F : .72F);
                TalusVertices.Add(FVector(Foot + FMath::Cos(Angle) * R, Y + FMath::Sin(Angle) * R * 1.35F,
                    Ring == 0 ? -11100.0F : Peak - RockRandom.FRandRange(900.0F, 1800.0F)));
            }
        TalusVertices.Add(FVector(Foot + RockRandom.FRandRange(-250.0F, 350.0F), Y + Radius * .2F, Peak));
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
