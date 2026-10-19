#include "PacificCoastGraybox.h"

#include "Components/SceneComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

// DEV-SEASKY: one analytic sea-state contract. The same spectrum drives the CPU
// evaluation used by tests/diagnostics and the HLSL emitted into M_CoastWaterDraft
// by the editor importer. It is a sum of travelling Gerstner components with deep-water
// dispersion, not a spectral FFT ocean, fluid solver or breaking-wave simulation.
namespace CoastalSea
{
constexpr float Gravity = 981.0F;                // cm/s^2
constexpr float GeometryMinWavelength = 1200.0F; // shorter components live only in the normal
constexpr float SeaFloorZ = -24000.0F;
constexpr float DeepFloorDepth = APacificCoastGraybox::SeaLevelCm - SeaFloorZ;
using FWave = APacificCoastGraybox::FCoastalWaveComponent;

struct FDerived
{
    float K, HorizontalCm;
    FVector2D Warp, Group;      // perpendicular warp and group-envelope directions
    float WarpK, WarpRad, GroupK, GroupOmega;
};

FVector2D FromYaw(const float YawDeg)
{
    const float R = FMath::DegreesToRadians(YawDeg);
    return FVector2D(FMath::Cos(R), FMath::Sin(R));
}

TArray<FWave> BuildSpectrum()
{
    // Wavelength (cm), amplitude (cm), propagation yaw (deg), Gerstner steepness.
    // +X is offshore/west, so yaw 180 runs straight at the cliff. Three long WNW Pacific
    // swell bands (107–186 m), a weaker SW cross-swell and a NW afternoon wind sea spread
    // over ~110 degrees. Wavelength ratios ~1.36 and golden-angle phases mean no two
    // components share a crest line, so the sum never forms one parallel band.
    struct FRow { float Lambda, Amp, Yaw, Q; };
    const FRow Rows[] = {
        {18600, 26.0F, -158, .30F}, {14100, 22.0F, -178, .30F}, {10700, 18.0F, 166, .32F},
        {12300,  9.0F,  132, .28F}, { 7700,  6.0F,  141, .30F},
        { 4600,  9.5F, -128, .45F}, { 3370,  7.0F, -152, .48F}, { 2490,  5.2F, -113, .50F},
        { 1830,  3.8F, -141, .52F}, { 1360,  2.7F, -100, .55F}, { 1010,  1.9F, -160, .55F},
        {  730,  1.35F,-122, .55F}, {  540,  .95F, -170, .55F}, {  390,  .65F,  -95, .55F},
        {  290,  .45F, -147, .55F}, {  210,  .30F, -118, .55F}};
    TArray<FWave> Result;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Rows); ++Index)
    {
        const FRow& Row = Rows[Index];
        const float K = 2.0F * PI / Row.Lambda;
        Result.Add({FromYaw(Row.Yaw), Row.Lambda, Row.Amp, Row.Q, FMath::Sqrt(Gravity * K),
            FMath::Fmod(Index * 2.39996323F + .47F, 2.0F * PI)});
    }
    return Result;
}

FDerived Derive(const FWave& Wave, const int32 Index, const int32 Count)
{
    FDerived D;
    D.K = 2.0F * PI / Wave.WavelengthCm;
    D.HorizontalCm = Wave.Steepness / (D.K * Count);
    // Warp phase along the crest (~5.5 wavelengths) bends every crest line; group
    // envelopes travel slowly across a rotated axis so amplitude varies in space and time.
    D.Warp = FVector2D(-Wave.Direction.Y, Wave.Direction.X);
    const float Rotation = (Index % 2 == 0 ? 35.0F : -35.0F) + Index * 3.0F;
    D.Group = Wave.Direction.GetRotated(Rotation);
    D.WarpK = D.K * .18F;
    D.WarpRad = Index < 5 ? 1.5F : .9F; // stronger crest bending on the long, most regular swell
    D.GroupK = D.K * .11F;
    D.GroupOmega = Wave.AngularFrequency * .055F;
    return D;
}

// Shoreline polyline sampled from the actual cliff-triangle waterline once per process.
struct FShoreSamples
{
    static constexpr float MinY = -212000.0F;
    static constexpr float Step = 250.0F;
    TArray<float> X;
    FShoreSamples()
    {
        const int32 Count = FMath::CeilToInt(-2.0F * MinY / Step) + 1;
        X.SetNumUninitialized(Count);
        for (int32 Index = 0; Index < Count; ++Index) X[Index] = APacificCoastGraybox::GetCoastalShoreX(MinY + Index * Step);
    }
    float At(const float Y) const
    {
        const float F = FMath::Clamp((Y - MinY) / Step, 0.0F, static_cast<float>(X.Num() - 1));
        const int32 I = FMath::Min(FMath::FloorToInt(F), X.Num() - 2);
        return FMath::Lerp(X[I], X[I + 1], F - I);
    }
    /** True 2D distance from a water point to the waterline (cm), searched over +/-40 m. */
    float Distance(const float PX, const float PY) const
    {
        float Best = FMath::Abs(PX - At(PY));
        for (float DY = -4000.0F; DY <= 4000.0F; DY += Step)
            Best = FMath::Min(Best, FMath::Sqrt(FMath::Square(PX - At(PY + DY)) + DY * DY));
        return Best;
    }
};
const FShoreSamples& Shore()
{
    static const FShoreSamples Samples;
    return Samples;
}

float SeabedDepth(const float X, const float Y, const float ShoreX)
{
    // A broad 2–18 m rock platform (clearer, greener shallows and reef heads where swell
    // breaks) before the drop to the 175 m floor.
    const float T = FMath::Clamp((X - (ShoreX - 700.0F)) / 15000.0F, 0.0F, 1.0F);
    float Depth = T <= .3F ? 250.0F + 1500.0F * FMath::Pow(T / .3F, 1.2F)
        : 1750.0F + (DeepFloorDepth - 1750.0F) * FMath::Pow((T - .3F) / .7F, 1.6F);
    const float Near = 1.0F - FMath::SmoothStep(.05F, .38F, T);
    const float Reef = FMath::Max(0.0F, FMath::PerlinNoise2D(FVector2D(Y / 2600.0F + 3.1F, T * 7.0F)));
    Depth -= 520.0F * Reef * Near;
    Depth += 120.0F * FMath::PerlinNoise2D(FVector2D(Y / 2300.0F, T * 4.0F)) * (1.0F - T) * T;
    return FMath::Max(Depth, 70.0F);
}

struct FDepthSample
{
    float DepthCm = DeepFloorDepth;
    float LandZ = SeaFloorZ;
    bool bLandSurface = false;
};

FDepthSample SampleDepth(const float X, const float Y, const float ShoreX, const float CliffEdge)
{
    FDepthSample Result;
    // Only the actual shelf footprint can replace the deep ocean floor. A point on the
    // landward side of the cliff is not automatically shallow: beyond the land's ends
    // it is open ocean. The former unconditional 250 cm branch painted a huge rectangle.
    if (FMath::Abs(Y) < 212000.0F && X >= ShoreX - 700.0F)
    {
        const float ShelfWeight = 1.0F - FMath::SmoothStep(165000.0F, 210000.0F, FMath::Abs(Y));
        Result.DepthCm = FMath::Lerp(DeepFloorDepth, SeabedDepth(X, Y, ShoreX), ShelfWeight);
    }
    // These are the sampled land mesh bounds in PacificCoastTerrain.cpp. Its height
    // function extrapolates beyond them, so querying that extrapolation is not seabed evidence.
    Result.bLandSurface = X >= -240000.0F && X <= CliffEdge && FMath::Abs(Y) <= 220000.0F;
    if (Result.bLandSurface)
    {
        Result.LandZ = APacificCoastGraybox::GetCoastalLandHeight(X, Y);
        Result.DepthCm = FMath::Min(Result.DepthCm, FMath::Max(0.0F, APacificCoastGraybox::SeaLevelCm - Result.LandZ));
    }
    return Result;
}

FString Num(const double Value) { return FString::Printf(TEXT("%.9g"), Value); }

// Emits one HLSL block per component. Inputs: P (world pos), T (s), A (amplitude scale),
// C (camera pos). Accumulates h (height), o (xy Gerstner offset) and g (height gradient).
FString EmitWaves(const bool bOffset, const bool bGradient, const bool bSwellOnly)
{
    const TArray<FWave>& Spectrum = APacificCoastGraybox::GetCoastalWaveSpectrum();
    FString Code = TEXT("float h=0; float2 o=float2(0,0); float2 g=float2(0,0); float dist=length(P-C);\n");
    for (int32 Index = 0; Index < Spectrum.Num(); ++Index)
    {
        const FWave& W = Spectrum[Index];
        if (bOffset && W.WavelengthCm < GeometryMinWavelength) continue;
        if (bSwellOnly && Index >= 5) continue;
        const FDerived D = Derive(W, Index, Spectrum.Num());
        // Short components fade with distance before they alias into glitter.
        // Normal detail fades at ~15-60 wavelengths so grazing low-altitude views do not moire.
        const float Near = W.WavelengthCm * (bOffset ? 40.0F : 15.0F);
        const float Far = W.WavelengthCm * (bOffset ? 160.0F : 60.0F);
        Code += FString::Printf(TEXT("{float2 d=float2(%s,%s),w=float2(%s,%s),e=float2(%s,%s);"),
            *Num(W.Direction.X), *Num(W.Direction.Y), *Num(D.Warp.X), *Num(D.Warp.Y), *Num(D.Group.X), *Num(D.Group.Y));
        Code += FString::Printf(TEXT("float wa=dot(P.xy,w)*%s+%s; float th=dot(P.xy,d)*%s-T*%s+%s+%s*sin(wa);"),
            *Num(D.WarpK), *Num(W.Phase * 1.7F), *Num(D.K), *Num(W.AngularFrequency), *Num(W.Phase), *Num(D.WarpRad));
        Code += FString::Printf(TEXT("float ga=dot(P.xy,e)*%s+T*%s+%s; float env=.68+.32*sin(ga);"),
            *Num(D.GroupK), *Num(D.GroupOmega), *Num(W.Phase * 2.3F));
        const FString Fade = bSwellOnly ? FString(TEXT("1")) : FString::Printf(TEXT("1-smoothstep(%s,%s,dist)"), *Num(Near), *Num(Far));
        Code += FString::Printf(TEXT("float f=%s; float a=A*%s*f;"), *Fade, *Num(W.AmplitudeCm));
        Code += TEXT("h+=a*env*sin(th);");
        if (bOffset) Code += FString::Printf(TEXT("o+=A*%s*f*env*cos(th)*d;"), *Num(D.HorizontalCm));
        if (bGradient)
            Code += FString::Printf(TEXT("g+=a*(env*cos(th)*(d*%s+%s*cos(wa)*w*%s)+.32*cos(ga)*e*%s*sin(th));"),
                *Num(D.K), *Num(D.WarpRad), *Num(D.WarpK), *Num(D.GroupK));
        Code += TEXT("}\n");
    }
    return Code;
}

FString NoiseHelpers()
{
    return TEXT("struct CoastNoise { float hash(float2 p) { return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453); }")
        TEXT(" float n(float2 p) { float2 i=floor(p),f=frac(p); f=f*f*(3-2*f); return lerp(lerp(hash(i),hash(i+float2(1,0)),f.x),lerp(hash(i+float2(0,1)),hash(i+1),f.x),f.y); }")
        TEXT(" float f(float2 p) { return .57*n(p)+.28*n(p*2.03+11)+.15*n(p*4.11-7); } }; CoastNoise noise;\n");
}
}

const TArray<APacificCoastGraybox::FCoastalWaveComponent>& APacificCoastGraybox::GetCoastalWaveSpectrum()
{
    static const TArray<FCoastalWaveComponent> Spectrum = CoastalSea::BuildSpectrum();
    return Spectrum;
}

FVector APacificCoastGraybox::GetCoastalOceanDisplacement(const float X, const float Y, const float TimeSeconds, const float AmplitudeScale)
{
    // Mirrors the emitted WorldPositionOffset at the camera (no distance fade).
    const TArray<FCoastalWaveComponent>& Spectrum = GetCoastalWaveSpectrum();
    const FVector2D P(X, Y);
    FVector Result(0, 0, SeaLevelCm);
    for (int32 Index = 0; Index < Spectrum.Num(); ++Index)
    {
        const FCoastalWaveComponent& W = Spectrum[Index];
        if (W.WavelengthCm < CoastalSea::GeometryMinWavelength) continue;
        const CoastalSea::FDerived D = CoastalSea::Derive(W, Index, Spectrum.Num());
        const float Wa = FVector2D::DotProduct(P, D.Warp) * D.WarpK + W.Phase * 1.7F;
        const float Theta = FVector2D::DotProduct(P, W.Direction) * D.K - TimeSeconds * W.AngularFrequency + W.Phase + D.WarpRad * FMath::Sin(Wa);
        const float Ga = FVector2D::DotProduct(P, D.Group) * D.GroupK + TimeSeconds * D.GroupOmega + W.Phase * 2.3F;
        const float Envelope = .68F + .32F * FMath::Sin(Ga);
        Result.Z += AmplitudeScale * W.AmplitudeCm * Envelope * FMath::Sin(Theta);
        const FVector2D Offset = W.Direction * (AmplitudeScale * D.HorizontalCm * Envelope * FMath::Cos(Theta));
        Result.X += Offset.X;
        Result.Y += Offset.Y;
    }
    return Result;
}

float APacificCoastGraybox::GetCoastalSeabedDepthCm(const float X, const float Y)
{
    return CoastalSea::SampleDepth(X, Y, GetCoastalShoreX(Y), GetCliffEdgeX(Y)).DepthCm;
}

FString APacificCoastGraybox::GetCoastalWaterShaderCode(const ECoastalWaterShader Kind)
{
    using namespace CoastalSea;
    switch (Kind)
    {
    case ECoastalWaterShader::Offset:
        // Inputs P,T,A,C. Real travelling vertex displacement: the silhouette moves.
        return EmitWaves(true, false, false) + TEXT("return float3(o,h);");
    case ECoastalWaterShader::Normal:
        // Inputs P,T,A,C,N1,N2 (detail normal samples). World-space normal.
        return EmitWaves(false, true, false)
            + TEXT("float detail=1-smoothstep(1500,12000,dist);")
            + TEXT("return normalize(float3(-g+(N1.xy*.15+N2.xy*.06)*detail,1));");
    case ECoastalWaterShader::Foam:
        // Inputs P,T,A,C,Mask (RGB),MaskA (vertex alpha; Custom inputs take only RGB). Mask r=distance to waterline/100 m, g=rock contact energy,
        // b=depth/40 m, a=shoal (reef) factor. Foam follows the swell crest that actually
        // reaches the rock and drifts shoreward with it; open water never receives foam
        // because every term is gated by rock contact or shoal depth.
        return EmitWaves(false, false, true) + NoiseHelpers()
            + TEXT("float d=Mask.r*10000; float contact=Mask.g; float shoal=MaskA;")
            + TEXT("float crest=saturate(h/(A*42+1)*.5+.5);")
            + TEXT("float2 drift=float2(-1,-.30)*T*110;")
            + TEXT("float cells=noise.f((P.xy+drift)*.0045);")
            + TEXT("float lace=smoothstep(.40,.78,noise.f((P.xy+drift*1.3)*.013)*.6+cells*.4);")
            + TEXT("float gaps=smoothstep(.32,.62,noise.f(float2(P.y*.0011,P.x*.0004)+float2(0,T*.004)));")
            + TEXT("float reach=180+820*crest*contact;")
            + TEXT("float surge=contact*exp(-d/reach)*lace*(.30+.70*gaps)*(.35+.65*crest);")
            + TEXT("float churn=contact*exp(-d/80)*(.45+.55*cells)*(.6+.4*gaps);")
            + TEXT("float breaker=shoal*smoothstep(.55,.88,crest)*lace*smoothstep(.30,.70,cells);")
            + TEXT("float streak=contact*exp(-d/1400)*smoothstep(.64,.92,noise.f((P.xy+drift*.6)*float2(.0016,.0035)))*.32;")
            + TEXT("return saturate(surge+churn*.65+breaker+streak);");
    case ECoastalWaterShader::Scattering:
        // Input Mask. Greener shallows over the platform, deep blue-grey offshore (per cm); a
        // turbid surf-zone term keeps the waterline from reading as clear glass over rock.
        return TEXT("float s=smoothstep(0,1,Mask.b); float surf=1-smoothstep(0,.12,Mask.b); return lerp(float3(.00007,.00020,.00019),float3(.00005,.00012,.00022),s)+float3(.00025,.00030,.00026)*surf;");
    case ECoastalWaterShader::Absorption:
        return TEXT("float s=smoothstep(0,1,Mask.b); return lerp(float3(.0028,.00072,.00062),float3(.0034,.00090,.00046),s);");
    case ECoastalWaterShader::Roughness:
        // Inputs R (base), F (foam), P, C. Slightly rougher with distance so the far sea
        // stays calm and the sun path stays a bounded cone rather than silver glitter.
        return TEXT("float dist=length(P-C); float r=lerp(R,R+.10,smoothstep(8000,250000,dist)); return lerp(r,.62,F);");
    case ECoastalWaterShader::RockWetness:
        // For the rock/ground material owner (DEV-TERRAIN): inputs P (world pos), T, A, C.
        // Returns 0..1 wetness: fully wet below still water, a ragged 0.5-2 m splash band
        // above it that rises as the same swell crests arrive, dry above.
        return EmitWaves(false, false, true) + NoiseHelpers()
            + FString::Printf(TEXT("float above=P.z-(%s);"), *Num(SeaLevelCm))
            + TEXT("float crest=saturate(h/(A*42+1)*.5+.5);")
            + TEXT("float top=lerp(110,210,crest)+60*(noise.f(P.xy*.004+float2(P.z*.002,0))-.5);")
            + TEXT("return 1-smoothstep(top*.35,top,above);");
    default:
        return TEXT("return 0;");
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
    Positions.Reserve(Size*Size); Normals.Reserve(Size*Size); UV.Reserve(Size*Size); ShoreMasks.Reserve(Size*Size);
    // Fine vertices around the headland, progressively larger cells toward a 20 km
    // horizon. The mesh is flat at sea level: all swell is travelling material displacement.
    auto Axis = [](int32 Index)
    {
        const float T = (Index - (Size-1)*.5F) / ((Size-1)*.5F);
        return T * 60000.0F + FMath::Sign(T) * 1940000.0F * FMath::Pow(FMath::Abs(T), 5.0F);
    };
    const CoastalSea::FShoreSamples& ShoreLine = CoastalSea::Shore();
    for (int32 Row = 0; Row < Size; ++Row)
    {
        const float Y = Axis(Row);
        const float ShoreX = ShoreLine.At(Y);
        const float CliffEdge = GetCliffEdgeX(Y);
        const float Top = GetCoastalLandHeight(CliffEdge, Y);
        const float Active = Top > SeaLevelCm + 100 && FMath::Abs(Y) < 210000 ? 1.0F : 0.0F;
        // Headlands concentrate refracted swell energy; recessed coves stay calmer.
        const float Headland = FMath::Clamp((ShoreX - .5F * (ShoreLine.At(Y - 4500) + ShoreLine.At(Y + 4500))) / 1800.0F, -1.0F, 1.0F);
        const float Variation = FMath::PerlinNoise2D(FVector2D(Y / 17000.0F + 5.3F, 2.7F));
        const float Contact = Active * FMath::Clamp(.62F + .30F * Headland + .25F * Variation, .15F, 1.0F);
        for (int32 Column = 0; Column < Size; ++Column)
        {
            const float X = 15000.0F + Axis(Column);
            Positions.Add(FVector(X, Y, SeaLevelCm));
            Normals.Add(FVector::UpVector);
            UV.Add(FVector2D(X/1000.0F, Y/1000.0F));
            // Cliff coast: true distance to the cliff-triangle waterline. The shelf fades to the
            // 175 m floor beyond |Y| 165-210 km exactly as the shelf mesh does, so depth colour
            // never claims shallow water over deep floor (R1b south-view hard navy patch).
            const bool bCliffCoast = FMath::Abs(Y) < 212000.0F;
            const bool bNear = bCliffCoast && FMath::Abs(X - ShoreX) < 16000.0F;
            float Distance = bNear ? ShoreLine.Distance(X, Y) : (bCliffCoast ? FMath::Abs(X - ShoreX) : 1.0e6F);
            const CoastalSea::FDepthSample DepthSample = CoastalSea::SampleDepth(X, Y, ShoreX, CliffEdge);
            const float Depth = DepthSample.DepthCm;
            float VertexContact = Contact;
            // North/south/west perimeter: the land collar slopes under the sea. Its submerged
            // surface is the real seabed there; estimate distance to its waterline from depth/grade
            // so the long straight shores also get surf, shoal colour and swash foam.
            if (DepthSample.bLandSurface)
            {
                const float Land = DepthSample.LandZ;
                if (Land >= SeaLevelCm) { Distance = 0.0F; }
                else if (Land > SeaLevelCm - 2500.0F)
                {
                    const float GX = (GetCoastalLandHeight(X + 300.0F, Y) - GetCoastalLandHeight(X - 300.0F, Y)) / 600.0F;
                    const float GY = (GetCoastalLandHeight(X, Y + 300.0F) - GetCoastalLandHeight(X, Y - 300.0F)) / 600.0F;
                    const float Grade = FMath::Max(FMath::Sqrt(GX * GX + GY * GY), .02F);
                    Distance = FMath::Min(Distance, (SeaLevelCm - Land) / Grade);
                    const float Swash = .55F + .25F * FMath::PerlinNoise2D(FVector2D(X / 13000.0F + 1.7F, Y / 13000.0F - 4.2F));
                    VertexContact = FMath::Max(VertexContact * (bCliffCoast ? 1.0F : 0.0F), Swash);
                }
            }
            if (!bCliffCoast && VertexContact == Contact) VertexContact = 0.0F;
            const float Shoal = (1.0F - FMath::SmoothStep(260.0F, 900.0F, Depth)) * FMath::SmoothStep(200.0F, 500.0F, Distance)
                * (VertexContact > 0.0F ? 1.0F : 0.0F);
            ShoreMasks.Add(FLinearColor(FMath::Clamp(Distance / 10000.0F, 0.0F, 1.0F), VertexContact,
                FMath::Clamp(Depth / 2500.0F, 0.0F, 1.0F), Shoal));
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

    // A submerged rock platform supplies actual shallow depth to the water volume.
    UProceduralMeshComponent* Shelf=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("CoastalSubtidalShelf"));
    Shelf->SetupAttachment(Root);Shelf->SetCollisionProfileName(TEXT("NoCollision"));
    TArray<FVector> ShelfPositions,ShelfNormals;TArray<FVector2D> ShelfUV;TArray<int32> ShelfIndices;
    constexpr int32 ShelfRows=385,ShelfColumns=49;
    for(int32 Row=0;Row<ShelfRows;++Row)
    {
        const float Y=-210000+Row*(420000.0F/(ShelfRows-1));
        const float ShoreX=ShoreLine.At(Y);
        for(int32 Col=0;Col<ShelfColumns;++Col)
        {
            const float T=Col/static_cast<float>(ShelfColumns-1),X=ShoreX-700+T*15000;
            // The shelf joins the actual deep seabed continuously; no translucent hard outer ledge.
            const float EndFade=1-FMath::SmoothStep(165000.0F,210000.0F,FMath::Abs(Y));
            const float Profile=SeaLevelCm-CoastalSea::SeabedDepth(X,Y,ShoreX);
            const float Z=FMath::Lerp(CoastalSea::SeaFloorZ,Profile,EndFade);
            ShelfPositions.Add(FVector(X,Y,Z));ShelfNormals.Add(FVector::UpVector);ShelfUV.Add(FVector2D(X/1800,Y/1800));
        }
    }
    for(int32 Row=0;Row<ShelfRows-1;++Row)for(int32 Col=0;Col<ShelfColumns-1;++Col)
    {const int32 A=Row*ShelfColumns+Col,B=A+1,C=A+ShelfColumns,D=C+1;ShelfIndices.Append({A,C,B,B,C,D});}
    // Reef heads are not flat: derive lighting normals from the actual shelf surface.
    for(int32 Row=0;Row<ShelfRows;++Row)for(int32 Col=0;Col<ShelfColumns;++Col)
    {
        const FVector DX=ShelfPositions[Row*ShelfColumns+FMath::Min(Col+1,ShelfColumns-1)]-ShelfPositions[Row*ShelfColumns+FMath::Max(Col-1,0)];
        const FVector DY=ShelfPositions[FMath::Min(Row+1,ShelfRows-1)*ShelfColumns+Col]-ShelfPositions[FMath::Max(Row-1,0)*ShelfColumns+Col];
        const FVector N=FVector::CrossProduct(DX,DY).GetSafeNormal();
        ShelfNormals[Row*ShelfColumns+Col]=N.Z<0?-N:N;
    }
    Shelf->CreateMeshSection_LinearColor(0,ShelfPositions,ShelfIndices,ShelfNormals,ShelfUV,TArray<FLinearColor>(),TArray<FProcMeshTangent>(),false);
    Shelf->SetMaterial(0,SolidMaterial);CoastalColorParts.Add(Shelf);CoastalPartColors.Add(FLinearColor(.13F,.16F,.13F));++GeneratedPrimitiveCount;

    // Explicit sea floor under the entire visible ocean closes the environment.
    UProceduralMeshComponent* Floor = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("PacificSeaFloor"));
    Floor->SetupAttachment(Root); Floor->SetCollisionProfileName(TEXT("NoCollision")); Floor->SetCastShadow(false);
    const TArray<FVector> Bottom = {FVector(-2100000,-2100000,CoastalSea::SeaFloorZ),FVector(2100000,-2100000,CoastalSea::SeaFloorZ),
        FVector(-2100000,2100000,CoastalSea::SeaFloorZ),FVector(2100000,2100000,CoastalSea::SeaFloorZ)};
    Floor->CreateMeshSection_LinearColor(0,Bottom,TArray<int32>{0,2,1,1,2,3},
        TArray<FVector>{FVector::UpVector,FVector::UpVector,FVector::UpVector,FVector::UpVector},
        TArray<FVector2D>{FVector2D(0,0),FVector2D(1,0),FVector2D(0,1),FVector2D(1,1)},
        TArray<FLinearColor>(),TArray<FProcMeshTangent>(),false);
    Floor->SetMaterial(0,SolidMaterial);
    CoastalColorParts.Add(Floor);CoastalPartColors.Add(FLinearColor(.025F,.045F,.040F));
    ++GeneratedPrimitiveCount;
}
