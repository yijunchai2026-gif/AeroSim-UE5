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
float BaselineCliffEdgeX(float Y);
float BaselineLandHeight(float X,float Y);
struct FHeroRimProfile { float X, Z, ScanTopX, ScanTopZ; };
FHeroRimProfile HeroRimProfile(float Y);
float HeroEditWeight(float Y);

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

// LOD0 bin SHA256 56c1f67209c0d53e27a0bb744ee8b764201715256f03184c63de6fcb602a8323
// Actual upper open-boundary Y intersections, 25cm samples; sea-side envelope in top35cm; ceil to cm.
const int16 ScanUpperBoundary[161][2] = {
    {-307,948},
    {-297,947},
    {-287,946},
    {-279,943},
    {-273,940},
    {-268,937},
    {-262,935},
    {-256,934},
    {-250,935},
    {-243,934},
    {-235,935},
    {-230,936},
    {-245,935},
    {-283,931},
    {-326,920},
    {-359,912},
    {-389,909},
    {-418,908},
    {-443,911},
    {-466,913},
    {-485,914},
    {-489,916},
    {-485,917},
    {-479,918},
    {-473,917},
    {-466,920},
    {-457,925},
    {-448,934},
    {-439,944},
    {-430,951},
    {-421,957},
    {-410,954},
    {-398,953},
    {-387,950},
    {-376,945},
    {-364,941},
    {-353,938},
    {-342,933},
    {-330,932},
    {-318,931},
    {-305,930},
    {-291,928},
    {-275,928},
    {-260,928},
    {-244,931},
    {-228,940},
    {-213,944},
    {-198,945},
    {-184,943},
    {-170,941},
    {-157,945},
    {-148,950},
    {-142,951},
    {-137,950},
    {-132,948},
    {-125,946},
    {-116,945},
    {-106,941},
    {-94,938},
    {-82,936},
    {-71,933},
    {-62,933},
    {-55,934},
    {-50,936},
    {-45,939},
    {-41,941},
    {-39,944},
    {-37,948},
    {-36,952},
    {-34,953},
    {-31,956},
    {-28,956},
    {-25,957},
    {-22,954},
    {-20,953},
    {-21,954},
    {-25,955},
    {-28,954},
    {-32,952},
    {-34,951},
    {-33,952},
    {-30,955},
    {-27,957},
    {-23,960},
    {-20,963},
    {-18,963},
    {-16,963},
    {-15,961},
    {-13,960},
    {-13,961},
    {-20,961},
    {-34,958},
    {-55,947},
    {-79,942},
    {-106,928},
    {-125,923},
    {-142,921},
    {-162,920},
    {-189,926},
    {-216,930},
    {-243,930},
    {-266,928},
    {-290,927},
    {-302,923},
    {-304,924},
    {-307,925},
    {-309,927},
    {-311,928},
    {-313,929},
    {-315,930},
    {-317,930},
    {-319,930},
    {-321,930},
    {-322,931},
    {-324,933},
    {-327,933},
    {-330,932},
    {-332,931},
    {-335,932},
    {-338,934},
    {-340,937},
    {-335,939},
    {-330,941},
    {-325,942},
    {-320,942},
    {-315,943},
    {-311,943},
    {-312,942},
    {-316,939},
    {-320,939},
    {-324,939},
    {-328,938},
    {-331,936},
    {-330,935},
    {-330,934},
    {-329,934},
    {-328,932},
    {-327,931},
    {-326,931},
    {-326,931},
    {-325,930},
    {-324,930},
    {-323,930},
    {-322,929},
    {-321,927},
    {-319,926},
    {-318,924},
    {-316,923},
    {-315,920},
    {-313,917},
    {-312,914},
    {-310,912},
    {-309,910},
    {-310,910},
    {-310,912},
    {-311,912},
    {-312,910},
    {-313,908},
    {-313,907},
    {-314,908},
    {-315,909}
};

float HeroEditWeight(const float Y)
{
    return 1.0F-Smooth((FMath::Abs(Y-HeroBayY)-2000.0F)/500.0F);
}

FHeroRimProfile HeroRimProfile(const float Y)
{
    const float OriginalX=BaselineCliffEdgeX(Y),OriginalZ=BaselineLandHeight(OriginalX,Y);
    const float U=FMath::Clamp((Y-ScanY+ScanHalfWidth)/25.0F,0.0F,160.0F);
    const int32 I=FMath::Min(FMath::FloorToInt(U),159);const float T=U-I;
    const float TopX=ScanX+FMath::Lerp(float(ScanUpperBoundary[I][0]),float(ScanUpperBoundary[I+1][0]),T);
    const float TopZ=ScanZ+FMath::Lerp(float(ScanUpperBoundary[I][1]),float(ScanUpperBoundary[I+1][1]),T);
    const float South=1.0F-Smooth(FMath::Abs(Y-(HeroBayY-1150.0F))/750.0F);
    const float Channel=1.0F-Smooth(FMath::Abs(Y-(HeroBayY-450.0F))/550.0F);
    const float Cover=150.0F+25.0F*South-25.0F*Channel;
    // Keep the existing global scan-top burial gate, while following the actual top footprint.
    const float RimZ=FMath::Max(TopZ+Cover,ScanZ+967.5003F+55.0F);
    const float Weight=HeroEditWeight(Y);
    return {FMath::Lerp(OriginalX,TopX+30.0F,Weight),FMath::Lerp(OriginalZ,RimZ,Weight),TopX,TopZ};
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

// R5a/R6 (PMO+QA: the cove cap reads as one smooth draped U-sheet with curtain folds): the land
// cap of every cove uses a sharper, irregular lateral profile (steep side walls, flatter floor) and
// descends through an irregular headscarp and a hummocky slump body to the rim. Scarp and
// walls exceed the ground material's rock threshold, so rock breaks through instead of one sheet.
// The cliff itself still uses CoastalErosion; the rim depth at each cove centre is unchanged.
float CoveCapDepth(const float Y)
{
    const float Centers[] = {-128000,-47000,-19000,19000,54000,112000};
    const float Widths[] = {18000,16000,12500,15000,12000,22000};
    const float Depths[] = {4200,4700,5400,4200,3500,4800};
    float Depth=0;
    for(int32 Bay=0;Bay<UE_ARRAY_COUNT(Centers);++Bay)
    {
        const float Lateral=1.0F-FMath::Abs(Y-Centers[Bay])/Widths[Bay];
        const float Wall=Smooth(1.9F*Lateral-.18F+.22F*Noise(Y,Bay*5300.0F,2600.0F));
        Depth=FMath::Max(Depth,Depths[Bay]*Wall);
    }
    return Depth;
}

// R10 (QA: cove headscarp reads as flat 7.6 m planks): the coastal land columns are redistributed,
// not added - 57 of the 77 core columns span the flat plateau (~8.8 m) and 20 span the last 70 m
// before the rim (~3.5 m), so the headscarp, slump and lip carry real relief. The last column
// remains exactly the rim, so the cliff seam and every rim reconstruction are unchanged.
float CoastColumnX(const float V, const float Edge)
{
    constexpr float Split = 57.0F / 77.0F;
    const float BandStart = Edge - 7000.0F;
    return V < Split ? FMath::Lerp(-40000.0F, BandStart, V / Split) : FMath::Lerp(BandStart, Edge, (V - Split) / (1.0F - Split));
}

float SteppedCoveBlend(const float Run, const float Y, const float X)
{
    // A slumped bluff rather than a draped sheet: an irregular rock headscarp high on the slope
    // (its position wanders along the coast), a hummocky slump body below it, then the cove floor.
    const float Scarp=.16F+.08F*Noise(Y,-8100.0F,3300.0F);
    const float Head=Smooth((Run-Scarp)/.16F);
    // R8: the R7 blocky slump steps sat on the ~7.6 m land grid and rendered as identical faceted
    // trays; the body is a smooth, irregular slump again and rock breakup comes from the steep-face
    // and habitat rock layers instead.
    const float Body=Smooth((Run-Scarp-.12F)/.75F+.06F*Noise(X,Y-2700.0F,2300.0F));
    // R10: 2-5 m blocky relief on the slump and scarp now that the band has ~3.5 m columns.
    const float Rubble=.035F*Noise(X*1.4F,Y,420.0F)*Smooth(Run/.12F)*(1.0F-Smooth((Run-.9F)/.1F));
    // R11: stronger multi-scale relief on the steep cap band (headscarp and upper slump) so its 5.2 m
    // rows do not read as planes; zero near the rim and inside the hero-scan reach (its rim/cover
    // contract is fitted to the scan).
    const float CapBand=Smooth((Run-.06F)/.1F)*(1.0F-Smooth((Run-.62F)/.2F))*Smooth((FMath::Abs(Y-ScanY)-2600.0F)/900.0F);
    const float CapRelief=CapBand*(.045F*Noise(X*1.2F,Y+1900.0F,900.0F)+.03F*FMath::Abs(Noise(X,Y-3700.0F,330.0F))-.012F);
    const float Hummocks=Rubble+CapRelief+.06F*Noise(X,Y+4100.0F,1500.0F)*Smooth(Run/.3F)*(1.0F-Smooth((Run-.8F)/.2F));
    return FMath::Clamp(.42F*Head+.58F*Body+Hummocks,0.0F,1.0F);
}

// 1 where coast rows are ~5 m apart (|Y| < 580 m), 0 where they are ~21 m apart.
float RowDetail(const float Y)
{
    return 1.0F - Smooth((FMath::Abs(Y) - 58000.0F) / 3000.0F);
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
    const float FractureA = FMath::Clamp(.5F + .5F * Noise(Y + 1700.0F, 6200.0F, 1800.0F), 0.0F, 1.0F);
    const float FractureB = FMath::Clamp(.5F + .5F * Noise(Y - 900.0F, 11100.0F, 3300.0F), 0.0F, 1.0F);
    const float RouteClear = Smooth((FMath::Abs(Y + 6000.0F) - 2500.0F) / 2000.0F);
    Outcrop += RouteClear * (90.0F + 290.0F * FractureB)
        * Smooth((Depth - (.18F + .10F * FractureA)) / .14F);
    for (int32 Bed = 0; Bed < UE_ARRAY_COUNT(Thresholds); ++Bed)
    {
        // R9: stronger, shorter dip so beds are not read as level contour bands on low headlands.
        // R12: on the 20.8 m outer rows, bed dip and thickness vary only over >150 m so bands stay level
        // instead of zig-zagging across the row triangulation (north-toe chevrons).
        const float Far = 1.0F - RowDetail(Y);
        const float Dip = .055F * Noise(Y + Bed * 4300.0F, 13000.0F + Bed * 3700.0F, 6800.0F + 12000.0F * Far) + .07F * RowDetail(Y) * Noise(Y, Bed * 2700.0F - 5000.0F, 2400.0F);
        const float BedHeight = Thickness[Bed] * (1.0F + .25F * Noise(Y, Bed * 9100.0F, 4700.0F + 12000.0F * Far));
        Outcrop += BedHeight * Smooth((Depth - Thresholds[Bed] - Dip) / Widths[Bed]);
    }
    // Every term is non-decreasing with depth, so the face never overhangs: blocks vary along the
    // coast only and ramp in once below the rim.
    const float Interior = Smooth(Depth / .07F);
    // No sub-7 m term: short-wavelength block noise serrated the waterline row by row.
    const float Blocks = 450.0F + 380.0F * Noise(Y, 14000.0F, 2100.0F + 14000.0F * (1.0F - RowDetail(Y)));
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

// R7 (QA: the procedural cliff face above the scan band shades as a smooth plane): fracture
// benches. Going down from the rim the face steps outward in 4-7 irregular ledges with steep
// risers; the rim (land edge) is receded by the total step-out BenchTotal(Y), whose own variation
// along the coast forms 4-12 m buttresses and chutes. The step-out reaches exactly BenchTotal at
// still water, so the waterline (shore, foam, DEV-FLIGHT C2/C3 references) is unchanged; only
// the scan footprint is excluded (its proxy face is fitted to the scan).
float BenchFootprint(const float Y)
{
    return 1.0F - Smooth((FMath::Abs(Y - ScanY) - ScanHalfWidth) / 600.0F);
}

float BenchTotal(const float Y)
{
    const float Buttress = 420.0F + 260.0F * Noise(Y, 2900.0F, 1300.0F + 2700.0F * (1.0F - RowDetail(Y))) + 380.0F * RowDetail(Y) * FMath::Abs(Noise(Y, -6100.0F, 470.0F));
    return FMath::Max(0.0F, Buttress) * (1.0F - BenchFootprint(Y));
}

float BenchStepOut(const float Y, const float Z, const float Top)
{
    const float Total = BenchTotal(Y);
    if (Total <= 0) return 0.0F;
    const float Height = FMath::Max(100.0F, Top - APacificCoastGraybox::SeaLevelCm);
    const float Down = FMath::Clamp((Top - Z) / Height, 0.0F, 1.0F);
    // R8 (QA: ledges read as stacked identical trays / contour bands): fewer ledges whose height
    // wanders strongly along the coast, unequal tread widths and softer, rounded risers.
    const float Count = 3.0F + FMath::Floor(3.0F * (.5F + .5F * Noise(Y, 7300.0F, 5200.0F)));
    // R10 (QA: north-toe chevrons): beyond |Y| = 600 m coast rows are 20.8 m apart, so variation
    // shorter than ~50 m aliases into chevrons; it is faded out there.
    const float Fine = RowDetail(Y);
    const float Wander = 1.1F * Noise(Y, 5100.0F, 1700.0F + 3300.0F * (1.0F - Fine)) + .5F * Fine * Noise(Y, -900.0F, 640.0F);
    const float T = FMath::Clamp(Down * Count + Wander * Down * (1.0F - Down) * 4.0F, 0.0F, Count);
    const float Ledge = FMath::Clamp(FMath::FloorToFloat(T), 0.0F, Count - 1.0F);
    const float Tread = .45F + .3F * Noise(Y, Ledge * 3100.0F, 2900.0F);
    const float Riser = Smooth((T - Ledge - Tread) / .45F);
    // Unequal ledge widths: each ledge contributes its own share of the total step-out.
    float Weights[6], Sum = 0;
    for (int32 Index = 0; Index < 6; ++Index) { Weights[Index] = .55F + .45F * (.5F + .5F * Noise(Y, Index * 4700.0F, 4100.0F)); if (Index < Count) Sum += Weights[Index]; }
    float Done = 0;
    for (int32 Index = 0; Index < static_cast<int32>(Ledge); ++Index) Done += Weights[Index];
    const float Share = FMath::Clamp((Done + Weights[static_cast<int32>(Ledge)] * Riser) / Sum, 0.0F, 1.0F);
    // The full step-out is reached exactly at still water, so the waterline never moves.
    return Total * FMath::Lerp(Share, 1.0F, Smooth((Down - .9F) / .1F));
}

// Match the actual last-column float arithmetic, not only the analytic edge coordinate.
FVector BaselineRimPosition(const float Y)
{
    const float X=CoastColumnX(1.0F,BaselineCliffEdgeX(Y));
    return FVector(X,Y,BaselineLandHeight(X,Y));
}
FVector RimPosition(const float Y)
{
    const float OldEdge=BaselineCliffEdgeX(Y),Edge=APacificCoastGraybox::GetCliffEdgeX(Y);
    const float Start=FMath::Max(10500.0F,OldEdge-4500.0F);
    const float OldX=CoastColumnX(1.0F,OldEdge);
    const float X=OldX+Smooth((OldX-Start)/(OldEdge-Start))*(Edge-OldEdge);
    return FVector(X,Y,APacificCoastGraybox::GetCoastalLandHeight(X,Y));
}

// Cliff rows: station 0 is the shared rim vertex. Sixteen stations span rim -> just below still
// water (fine where the face is seen) and twenty span the submerged foot. Within the scan reach
// the face follows ScanFace above water and a 0.9:1 talus below the scan base, fading back into
// the ordinary profile ~8-33 m under water. A running max keeps every row free of overhangs.
void BaselineCliffStations(const float RowY, const FVector& Rim, FVector (&Out)[CliffDepthSteps + 1])
{
    // R7: 26 stations rim -> just below still water (fracture benches need ~2.5 m resolution on a
    // 65 m face), 10 for the submerged foot; total unchanged.
    constexpr int32 Upper = 26;
    const float Top = static_cast<float>(Rim.Z);
    const float Split = Top > APacificCoastGraybox::SeaLevelCm + 300.0F ? APacificCoastGraybox::SeaLevelCm - 200.0F : FMath::Lerp(Top, CliffFoot, .4F);
    const float Weight = ScanWeight(RowY);
    Out[0] = Rim;
    for (int32 Step = 1; Step <= CliffDepthSteps; ++Step)
    {
        const float Z = Step <= Upper ? FMath::Lerp(Top, Split, Step / static_cast<float>(Upper))
            : FMath::Lerp(Split, CliffFoot, (Step - Upper) / static_cast<float>(CliffDepthSteps - Upper));
        const float Depth = FMath::Clamp((Top - Z) / (Top - CliffFoot), 0.0F, 1.0F);
        const float Bench = BenchStepOut(RowY, Z, Top);
        // R9 (QA: ledge band reads as planar flat-shaded facets; flat bands under water): 1-6 m rock
        // relief breaks every station plane. It fades out within 1-4 m of still water so the
        // waterline (shore query, foam, DEV-FLIGHT references) is unchanged.
        const float Above = Z - APacificCoastGraybox::SeaLevelCm, Below = -Above;
        const float ReliefFade = FMath::Max(Smooth((Above - 40.0F) / 120.0F), Smooth((Below - 80.0F) / 200.0F));
        const float Relief = ReliefFade * RowDetail(RowY) * (90.0F * Noise(RowY, Z * 1.6F, 560.0F) + 55.0F * Noise(RowY + 700.0F, Z * 2.1F, 230.0F) + 35.0F * Noise(RowY - 300.0F, Z, 110.0F));
        float X = static_cast<float>(Rim.X) + CliffOutcrop(RowY, Depth) + Bench + Relief * (1.0F - BenchFootprint(RowY));
        if (Weight > 0)
        {
            const float Local = Z - ScanZ;
            const float Base = ScanX + ScanFaceLocalX(RowY, -50.0F);
            const float Hero = Local >= -50.0F ? ScanX + ScanFaceLocalX(RowY, Local) : Base + (-50.0F - Local) * .9F;
            const float Fade = 1.0F - Smooth((-50.0F - Local - 800.0F) / 2500.0F);
            X = FMath::Lerp(X, Hero - BenchTotal(RowY) + Bench, Weight * Fade);
        }
        Out[Step] = FVector(FMath::Max(X, static_cast<float>(Out[Step - 1].X)), RowY, Z);
    }
}

// Preserve the original sea-intersection cells and neighbouring vertices exactly. Only the
// supported upper face connects to the new thin cap; the frozen anchor owns water/foam geometry.
void CliffStations(const float RowY,const FVector& Rim,FVector (&Out)[CliffDepthSteps+1])
{
    const FVector OldRim=BaselineRimPosition(RowY);const float Edge=OldRim.X;
    BaselineCliffStations(RowY,OldRim,Out);
    if(HeroEditWeight(RowY)<=0)return;
    int32 Anchor=1;
    while(Anchor<CliffDepthSteps && Out[Anchor].Z>APacificCoastGraybox::SeaLevelCm+450.0F)++Anchor;
    // This anchor is at least one cell above every sea crossing in the hero reach.
    const FVector Fixed=Out[Anchor];
    Out[0]=Rim;
    for(int32 Step=1;Step<Anchor;++Step)
    {
        const float Z=FMath::Lerp(float(Rim.Z),float(Fixed.Z),Step/float(Anchor));
        const float Local=Z-ScanZ;
        const float BaselineDepth=FMath::Clamp((float(OldRim.Z)-Z)/(float(OldRim.Z)-CliffFoot),0.0F,1.0F);
        float X=Edge+CliffOutcrop(RowY,BaselineDepth);
        const float Base=ScanX+ScanFaceLocalX(RowY,-50.0F);
        const float Hero=Local>=-50.0F?ScanX+ScanFaceLocalX(RowY,Local):Base+(-50.0F-Local)*.9F;
        const float Fade=1.0F-Smooth((-50.0F-Local-800.0F)/2500.0F);
        X=FMath::Lerp(X,Hero,ScanWeight(RowY)*Fade);
        // The original monotone envelope is an upper bound: the seam cannot invert or overhang.
        X=FMath::Min(float(Fixed.X),FMath::Max(float(Out[Step-1].X),X));
        Out[Step]=FVector(X,RowY,Z);
    }
}

bool IntersectCliffFaceYZ(const float Y,const float Z,float& OutX)
{
    const float CY=FMath::Clamp(Y,-North,North);const TArray<float>& Rows=CoastRowsY();
    int32 Low=0,High=Rows.Num()-1;
    while(Low+1<High){const int32 M=(Low+High)/2;if(Rows[M]<=CY)Low=M;else High=M;}
    const int32 Row=FMath::Min(Low,Rows.Num()-2);
    FVector A[CliffDepthSteps+1],B[CliffDepthSteps+1];
    CliffStations(Rows[Row],RimPosition(Rows[Row]),A);
    CliffStations(Rows[Row+1],RimPosition(Rows[Row+1]),B);
    bool Found=false;double X=-BIG_NUMBER;
    for(int32 Step=0;Step<CliffDepthSteps;++Step)
    {
        const FVector Faces[][3]={{A[Step],B[Step],A[Step+1]},{A[Step+1],B[Step],B[Step+1]}};
        for(const auto& Face:Faces)
        {
            const FVector U=Face[1]-Face[0],V=Face[2]-Face[0];const double Det=U.Y*V.Z-U.Z*V.Y;
            if(FMath::Abs(Det)<1.e-8)continue;
            const double DY=CY-Face[0].Y,DZ=Z-Face[0].Z;
            const double S=(DY*V.Z-DZ*V.Y)/Det,T=(U.Y*DZ-U.Z*DY)/Det;
            if(S>=-1.e-6&&T>=-1.e-6&&S+T<=1.000001){Found=true;X=FMath::Max(X,Face[0].X+S*U.X+T*V.X);}
        }
    }
    if(Found)OutX=static_cast<float>(X);return Found;
}

#if WITH_DEV_AUTOMATION_TESTS
// Private test audit: exact R5 stations, never used to define the new query result.
void GetR5CliffReference(const float Y,TArray<FVector>& Result)
{
    FVector P[CliffDepthSteps+1];BaselineCliffStations(Y,BaselineRimPosition(Y),P);
    Result.Reset();Result.Append(P,CliffDepthSteps+1);
}
#endif

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

float CoastalTerrain::BaselineCliffEdgeX(const float Y)
{
    // Several non-repeating spatial frequencies produce coves and projecting headlands.
    const float Headland=20000.0F + 3800.0F * CoastalTerrain::Noise(Y, 12730.0F, 7200.0F)
        + 1600.0F * CoastalTerrain::Noise(Y, -7300.0F, 1900.0F)
        + 2600.0F * FMath::Sin(Y / 14500.0F);
    const float Edge = FMath::Max(14500.0F,Headland-1500.0F*CoastalTerrain::CoastalErosion(Y)/5400.0F);
    // The rim inside the scan reach is the top of the rebuilt closed face behind the scan.
    using namespace CoastalTerrain;
    return FMath::Lerp(Edge, ScanX + ScanFaceLocalX(Y, 1000.0F), ScanWeight(Y)) - BenchTotal(Y);
}

float CoastalTerrain::BaselineLandHeight(const float X, const float Y)
{
    using namespace CoastalTerrain;
    // The complete ridge descends to the submerged perimeter over a broad shoulder. The collar's
    // waterline is displaced by low-frequency noise (vanishing at the outer boundary) so the
    // north, south and west shores are irregular instead of ruler-straight.
    float WestDistance = X - West, NorthDistance = North - FMath::Abs(Y);
    const float YSide = Y < 0 ? -1.0F : 1.0F;
    WestDistance += (9000.0F * Noise(Y, 3100.0F, 52000.0F) + 3500.0F * Noise(Y, -4100.0F, 17000.0F)) * FMath::Clamp(WestDistance / 15000.0F, 0.0F, 1.0F);
    // R5a: stronger, multi-scale displacement (QA: N/S shores still read ruler-straight from 2.5 km).
    // R7: 3x stronger at 50-200 m (coves and points along the north/south shores).
    // The strong short terms act only in the low collar near the waterline (not on the ridges).
    const float ShoreBand = FMath::Clamp(NorthDistance / 8000.0F, 0.0F, 1.0F) * (1.0F - Smooth((NorthDistance - 20000.0F) / 20000.0F));
    NorthDistance += 19000.0F * Noise(X, 9100.0F * YSide, 47000.0F) * FMath::Clamp(NorthDistance / 15000.0F, 0.0F, 1.0F)
        + (30000.0F * Noise(X, -5300.0F * YSide, 17000.0F) + 10500.0F * Noise(X, 2900.0F * YSide, 6100.0F)) * ShoreBand;
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
    const float CoastBlend=SteppedCoveBlend(FMath::Clamp((X-10500.0F)/FMath::Max(1000.0F,BaselineCliffEdgeX(Y)-10500.0F),0.0F,1.0F),Y,X);
    // Inside the scan reach the cove floor is 6 m shallower so the land cap buries the scan's top edge.
    const float ScanCentre=1.0F-Smooth((FMath::Abs(Y-ScanY)-1800.0F)/800.0F); // flat: keeps the runnel/bank relief intact
    const float CoastHeight=LandHeight-(CoveCapDepth(Y)-600.0F*ScanCentre)*CoastBlend*Interior;
    // A shallow seasonal swale gives the short woodland bridge an actual crossing to span.
    // Its 16 m bank-to-bank width spans several near-field samples; both bridge landings remain at grade.
    return CoastHeight - 180.0F * ChannelWeight(X, Y) + HeroBayLandRelief(X,Y,BaselineCliffEdgeX(Y));
}

float APacificCoastGraybox::GetCliffEdgeX(const float Y)
{
    return CoastalTerrain::HeroEditWeight(Y)>0?CoastalTerrain::HeroRimProfile(Y).X:CoastalTerrain::BaselineCliffEdgeX(Y);
}

float APacificCoastGraybox::GetCoastalLandHeight(const float X,const float Y)
{
    using namespace CoastalTerrain;
    const float Original=BaselineLandHeight(X,Y),Weight=HeroEditWeight(Y);
    const float Start=FMath::Max(10500.0F,BaselineCliffEdgeX(Y)-4500.0F);
    if(Weight<=0||X<=Start||X>BaselineCliffEdgeX(Y))return Original;
    const FHeroRimProfile Rim=HeroRimProfile(Y);
    const float U=FMath::Clamp((X-Start)/(Rim.X-Start),0.0F,1.0F);
    const float Connection=Smooth(U);
    const float RimDelta=Rim.Z-BaselineLandHeight(Rim.X,Y);
    const float Center=HeroBayY-450.0F+300.0F*FMath::Square(1.0F-U);
    const float Width=450.0F+150.0F*(1.0F-U);
    const float Channel=1.0F-Smooth(FMath::Abs(Y-Center)/Width);
    const float South=1.0F-Smooth(FMath::Abs(Y-(Center-950.0F))/650.0F);
    const float NorthShoulder=1.0F-Smooth(FMath::Abs(Y-(Center+1150.0F))/900.0F);
    const float Inland=FMath::Square(FMath::Sin(PI*U));
    return Original+Connection*RimDelta+Weight*Inland*(-250.0F*Channel+100.0F*South+60.0F*NorthShoulder);
}

float APacificCoastGraybox::GetCoastalShoreX(float Y)
{
    float X;if(CoastalTerrain::IntersectCliffFaceYZ(Y,SeaLevelCm,X))return X;
    using namespace CoastalTerrain;
    Y=FMath::Clamp(Y,-North,North);const float Edge=GetCliffEdgeX(Y),Top=GetCoastalLandHeight(Edge,Y);
    return Edge+CliffOutcrop(Y,FMath::Clamp((Top-SeaLevelCm)/(Top-CliffFoot),0.0F,1.0F));
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

float FPacificCoastHabitat::GetCliffFaceX(const float Y,const float Z)
{
    float X;if(CoastalTerrain::IntersectCliffFaceYZ(Y,Z,X))return X;
    const float Edge=APacificCoastGraybox::GetCliffEdgeX(Y);
    return Z>=APacificCoastGraybox::GetCoastalLandHeight(Edge,Y)?Edge:Edge+CoastalTerrain::CliffOutcrop(Y,1.0F);
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
    struct FTopSample { float Parameter; int32 Index; };
    TArray<TArray<FTopSample>> TopRows;TopRows.SetNum(Rows);
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
        const float Edge = GetCliffEdgeX(Y),OldEdge=BaselineCliffEdgeX(Y);
        const float EditStart=FMath::Max(10500.0F,OldEdge-4500.0F);
        for (int32 Column = 0; Column < Columns; ++Column)
        {
            const float U = Column / 192.0F;
            const float OldX = U <= .6F ? FMath::Lerp(West, -40000.0F, U / .6F) : CoastColumnX((U - .6F) / .4F, OldEdge);
            const float X=OldX+Smooth((OldX-EditStart)/(OldEdge-EditStart))*(Edge-OldEdge);
            Vertex(FVector(X, Y, GetCoastalLandHeight(X, Y)));
        }
    }
    // Only hero rows gain samples: core vertices and every other coast row stay in place.
    // A zipper joins neighbouring rows with unequal sample counts; no T-junction or extra coast ring.
    for(int32 Row=0;Row<Rows;++Row)
    {
        const float Y=CoastY(Row),Edge=GetCliffEdgeX(Y);
        for(int32 Column=0;Column<Columns;++Column)
        {
            const int32 A=Row*Columns+Column;TopRows[Row].Add({float(Column),A});
            if(Column+1==Columns||HeroEditWeight(Y)<=0||Vertices[A].X<FMath::Max(10500.0F,Edge-4500.0F))continue;
            const float XA=Vertices[A].X,XB=Vertices[A+1].X;
            const int32 Splits=FMath::Max(1,FMath::CeilToInt((XB-XA)/140.0F));
            for(int32 Split=1;Split<Splits;++Split)
            {
                const float T=Split/float(Splits),X=FMath::Lerp(XA,XB,T);
                TopRows[Row].Add({Column+T,Vertex(FVector(X,Y,GetCoastalLandHeight(X,Y)))});
            }
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
    for(int32 Row=0;Row<Rows-1;++Row)
    {
        const auto& A=TopRows[Row];const auto& B=TopRows[Row+1];int32 I=0,J=0;
        while(I+1<A.Num()||J+1<B.Num())
        {
            const float NextA=I+1<A.Num()?A[I+1].Parameter:BIG_NUMBER;
            const float NextB=J+1<B.Num()?B[J+1].Parameter:BIG_NUMBER;
            if(FMath::IsNearlyEqual(NextA,NextB,1.e-5F))
            {
                LandTriangle(A[I].Index,B[J].Index,A[I+1].Index);
                LandTriangle(A[I+1].Index,B[J].Index,B[J+1].Index);++I;++J;
            }
            else if(NextA<NextB){LandTriangle(A[I].Index,B[J].Index,A[I+1].Index);++I;}
            else {LandTriangle(A[I].Index,B[J].Index,B[J+1].Index);++J;}
        }
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
    // R7 (QA: stretched streaks along the rim lip and rock ledges): the shared rim vertices used to
    // average land (up) and cliff (sideways) normals, so the cliff's world-aligned projection picked
    // its top plane on vertical rim facets. Land and rock sections now shade with normals from their
    // own family of faces; positions and indices stay shared, so the closed topology is unchanged.
    TArray<FVector> Normals, RockNormals;
    Normals.Init(FVector::ZeroVector, Vertices.Num()); RockNormals.Init(FVector::ZeroVector, Vertices.Num());
    for (int32 GroupIndex = 0; GroupIndex < 5; ++GroupIndex)
        for (int32 Index = 0; Index < Triangles[GroupIndex].Num(); Index += 3)
        {
            const TArray<int32>& Group = Triangles[GroupIndex];
            const int32 A = Group[Index], B = Group[Index + 1], C = Group[Index + 2];
            const FVector N = FVector::CrossProduct(Vertices[C] - Vertices[A], Vertices[B] - Vertices[A]);
            TArray<FVector>& Target = GroupIndex < 3 ? Normals : RockNormals;
            Target[A] += N; Target[B] += N; Target[C] += N;
        }
    for (int32 Index = 0; Index < Vertices.Num(); ++Index)
    {
        if (Normals[Index].IsNearlyZero()) Normals[Index] = RockNormals[Index];
        if (RockNormals[Index].IsNearlyZero()) RockNormals[Index] = Normals[Index];
        Normals[Index].Normalize(); RockNormals[Index].Normalize();
    }
    const TCHAR* Names[] = {TEXT("CoastalPlateau"), TEXT("CoastalMountainRock"), TEXT("CoastalMountainScrub"), TEXT("CoastalBedrock"), TEXT("CoastalFracturedRock")};
    const FLinearColor Colors[] = {FLinearColor(.115F,.155F,.065F), FLinearColor(.13F,.153F,.087F), FLinearColor(.12F,.155F,.075F), FLinearColor(.19F,.175F,.14F), FLinearColor(.215F,.20F,.165F)};
    for (int32 Group = 0; Group < 5; ++Group)
    {
        UProceduralMeshComponent* Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(Names[Group]);
        Mesh->SetupAttachment(Root);
        Mesh->bUseAsyncCooking = true;
        Mesh->SetCollisionProfileName(TEXT("BlockAll"));
        Mesh->CreateMeshSection_LinearColor(0, Vertices, Triangles[Group], Group < 3 ? Normals : RockNormals, UV, CoverColors, TArray<FProcMeshTangent>(), true);
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
    // R12 (QA: cove boulders were removed rather than rebuilt): 64 coast blocks plus a field of 24
    // rounded 4-6 m rocks along the hero cove waterline, on both flanks of the scan.
    for (int32 Rock = 0; Rock < 88; ++Rock)
    {
        float Y = Rock >= 64 ? ScanY + (Rock % 2 == 0 ? -1.0F : 1.0F) * RockRandom.FRandRange(ScanHalfWidth + 300.0F, 9000.0F) : Rock % 2 == 0 ? FMath::Clamp(CoveCentres[(Rock / 2) % 4] + RockRandom.FRandRange(-6500.0F, 6500.0F), -57000.0F, 57000.0F)
            : RockRandom.FRandRange(-57000.0F, 57000.0F);
        // Keep the scanned hero face clear: blocks that fall in front of it move to its flanks.
        if (FMath::Abs(Y - ScanY) < ScanHalfWidth + 700.0F) Y = ScanY + (Y < ScanY ? -1.0F : 1.0F) * (ScanHalfWidth + 700.0F + FMath::Abs(Y - ScanY) * .8F);
        const float Edge = GetCliffEdgeX(Y);
        // R10 (QA: a ~15 m flat-faced boulder wedge beside the scan): blocks near the hero cove are
        // capped at 4-6 m across; elsewhere they stay under ~16 m.
        const float HeroDistance = FMath::Abs(Y - ScanY);
        const float Radius = HeroDistance < 9000.0F ? RockRandom.FRandRange(200.0F, 300.0F) : RockRandom.FRandRange(500.0F, 1200.0F);
        const bool bSmall = Radius <= 300.0F;
        // Small cove rocks are rounded and sit at the waterline (crown 0.6-3.2 m above still water).
        const float Peak = bSmall ? APacificCoastGraybox::SeaLevelCm + RockRandom.FRandRange(60.0F, 320.0F) : RockRandom.FRandRange(-7400.0F, -4600.0F);
        const float FootZ = bSmall ? APacificCoastGraybox::SeaLevelCm - 450.0F : -11100.0F;
        // Blocks sit on the real sloping cliff foot at the waterline (previously buried behind it).
        const float Foot = GetCoastalShoreX(Y) + RockRandom.FRandRange(-300.0F, 700.0F) - Radius * .3F;
        const int32 BaseIndex = TalusVertices.Num();
        const float Rotation = RockRandom.FRandRange(0.0F, 2.0F * PI);
        // R9 (QA: waterline rocks read as smooth pyramids/cones): irregular boulders - four rings of
        // nine jittered corners with a bulging waist, a leaning, chamfered and flattened crown, and
        // a footing sunk well below the waterline. 38 vertices per block; the crown is the last.
        constexpr int32 Corners = 9, Rings = 4;
        TalusVertices.Add(bSmall ? FVector(Foot - 100.0F, Y, APacificCoastGraybox::SeaLevelCm - 900.0F) : FVector(Edge - 300.0F, Y, -15500.0F));
        const FVector2D Lean(RockRandom.FRandRange(-.35F, .35F) * Radius, RockRandom.FRandRange(-.35F, .35F) * Radius);
        const float RingT[Rings] = {0.0F, RockRandom.FRandRange(.38F, .52F), RockRandom.FRandRange(.72F, .82F), RockRandom.FRandRange(.9F, .96F)};
        const float RingScale[Rings] = {1.05F, RockRandom.FRandRange(1.0F, 1.25F), RockRandom.FRandRange(bSmall ? .82F : .72F, .9F), RockRandom.FRandRange(bSmall ? .5F : .38F, bSmall ? .62F : .55F)};
        float Jitter[Corners];
        for (int32 Corner = 0; Corner < Corners; ++Corner) Jitter[Corner] = RockRandom.FRandRange(bSmall ? .85F : .68F, bSmall ? 1.1F : 1.2F);
        for (int32 Ring = 0; Ring < Rings; ++Ring)
            for (int32 Corner = 0; Corner < Corners; ++Corner)
            {
                const float Angle = Rotation + (Corner + RockRandom.FRandRange(-.3F, .3F)) * (2.0F * PI / Corners);
                const float R = Radius * RingScale[Ring] * FMath::Lerp(Jitter[Corner], RockRandom.FRandRange(.7F, 1.2F), .35F);
                const float Height = FMath::Lerp(FootZ, Peak, RingT[Ring]) + (Ring > 0 ? RockRandom.FRandRange(-120.0F, 120.0F) : 0.0F);
                const FVector2D Shift = Lean * RingT[Ring];
                TalusVertices.Add(FVector(Foot + Shift.X + FMath::Cos(Angle) * R, Y + Shift.Y + FMath::Sin(Angle) * R * 1.3F, Height));
            }
        TalusVertices.Add(FVector(Foot + Lean.X + RockRandom.FRandRange(-200.0F, 200.0F), Y + Lean.Y + RockRandom.FRandRange(-200.0F, 200.0F), Peak));
        const int32 Crown = BaseIndex + 1 + Rings * Corners;
        for (int32 Corner = 0; Corner < Corners; ++Corner)
        {
            const int32 Next = (Corner + 1) % Corners;
            TalusIndices.Append({BaseIndex, BaseIndex + 1 + Corner, BaseIndex + 1 + Next});
            for (int32 Ring = 0; Ring + 1 < Rings; ++Ring)
            {
                const int32 A = BaseIndex + 1 + Ring * Corners + Corner, B = BaseIndex + 1 + Ring * Corners + Next;
                const int32 C = A + Corners, D = B + Corners;
                TalusIndices.Append({A, C, B, B, C, D});
            }
            TalusIndices.Append({BaseIndex + 1 + (Rings - 1) * Corners + Corner, Crown, BaseIndex + 1 + (Rings - 1) * Corners + Next});
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
