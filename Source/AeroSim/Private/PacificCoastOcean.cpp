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
        {18600, 28.0F, -158, .30F}, {14100, 24.0F, -178, .30F}, {10700, 19.0F, 166, .32F},
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
        // Analytic normal detail fades at ~30-120 wavelengths: enough slope variance for a bounded
        // sun-glint path (R2 lost it at 15-60), while texture detail fades by 120 m (moire).
        // Far-field cap (R5 player view): beyond ~1.2-4 km every component, including the long
        // swell, hands over to distance roughness so grazing perspective cannot compress swell
        // crests into ruler-parallel horizon stripes. Near-shore travelling swell is unchanged.
        const float Near = FMath::Min(W.WavelengthCm * (bOffset ? 40.0F : 30.0F), bOffset ? 200000.0F : 120000.0F);
        const float Far = FMath::Min(W.WavelengthCm * (bOffset ? 160.0F : 120.0F), bOffset ? 600000.0F : 400000.0F);
        Code += FString::Printf(TEXT("{float2 d=float2(%s,%s),w=float2(%s,%s),e=float2(%s,%s);"),
            *Num(W.Direction.X), *Num(W.Direction.Y), *Num(D.Warp.X), *Num(D.Warp.Y), *Num(D.Group.X), *Num(D.Group.Y));
        Code += FString::Printf(TEXT("float wa=dot(P.xy,w)*%s+%s; float th=dot(P.xy,d)*%s-T*%s+%s+%s*sin(wa);"),
            *Num(D.WarpK), *Num(W.Phase * 1.7F), *Num(D.K), *Num(W.AngularFrequency), *Num(W.Phase), *Num(D.WarpRad));
        Code += FString::Printf(TEXT("float ga=dot(P.xy,e)*%s+T*%s+%s; float env=.55+.45*sin(ga);"),
            *Num(D.GroupK), *Num(D.GroupOmega), *Num(W.Phase * 2.3F));
        const FString Fade = bSwellOnly ? FString(TEXT("1")) : FString::Printf(TEXT("1-smoothstep(%s,%s,dist)"), *Num(Near), *Num(Far));
        Code += FString::Printf(TEXT("float f=%s; float a=A*%s*f;"), *Fade, *Num(W.AmplitudeCm));
        Code += TEXT("h+=a*env*sin(th);");
        if (bOffset) Code += FString::Printf(TEXT("o+=A*%s*f*env*cos(th)*d;"), *Num(D.HorizontalCm));
        if (bGradient)
            Code += FString::Printf(TEXT("g+=a*(env*cos(th)*(d*%s+%s*cos(wa)*w*%s)+.45*cos(ga)*e*%s*sin(th));"),
                *Num(D.K), *Num(D.WarpRad), *Num(D.WarpK), *Num(D.GroupK));
        Code += TEXT("}\n");
    }
    return Code;
}

// R9: the visible waterline taken from the actual terrain/cliff/talus triangles. Terrain
// rounds (ledges, slumps, wiggled shores) moved the rock face away from the analytic shore
// function, so foam keyed only to GetCoastalShoreX missed the real rock contact.
struct FWaterlineGrid
{
    static constexpr float Cell = 1000.0F;
    TMap<FIntPoint, TArray<FVector2D>> Grid;
    int32 Num = 0;
    static FIntPoint Key(const FVector2D& P) { return FIntPoint(FMath::FloorToInt(P.X / Cell), FMath::FloorToInt(P.Y / Cell)); }
    void Add(const FVector2D& P) { Grid.FindOrAdd(Key(P)).Add(P); ++Num; }
    float Nearest(const FVector2D& P, const float MaxDistance) const
    {
        float Best = MaxDistance;
        const FIntPoint C = Key(P);
        const int32 R = FMath::CeilToInt(MaxDistance / Cell);
        for (int32 DX = -R; DX <= R; ++DX)
            for (int32 DY = -R; DY <= R; ++DY)
                if (const TArray<FVector2D>* Points = Grid.Find(C + FIntPoint(DX, DY)))
                    for (const FVector2D& Q : *Points) Best = FMath::Min(Best, static_cast<float>(FVector2D::Distance(P, Q)));
        return Best;
    }
};

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
        const float Envelope = .55F + .45F * FMath::Sin(Ga); // strong groups break long parallel crests
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
        // Inputs P,T,A,C,N1,N2,N3 (detail/far normal samples). World-space normal.
        return EmitWaves(false, true, false)
            + TEXT("float detail=1-smoothstep(1500,12000,dist);")
            // Far field (R7b "flat"): an isotropic ~550 m texture normal fades in from 0.8 km, so
            // the distant sea keeps soft broad modulation without directional crest stripes.
            + TEXT("float far=smoothstep(80000,300000,dist)*(1-smoothstep(1500000,4000000,dist));")
            + TEXT("return normalize(float3(-g+(N1.xy*.15+N2.xy*.06)*detail+N3.xy*.10*far,1));");
    case ECoastalWaterShader::Foam:
        // Inputs P,T,A,C,FoamTex (texture object),Mask (RGB),MaskA (vertex alpha; Custom inputs take only RGB). Mask r=distance to waterline/100 m, g=rock contact energy,
        // b=depth/40 m, a=shoal (reef) factor. Foam follows the swell crest that actually
        // reaches the rock and drifts shoreward with it; open water never receives foam
        // because every term is gated by rock contact or shoal depth.
        // R11 duty cycle: (1) a persistent lacy fringe at every rock/shore contact, independent
        // of the swell phase, so foam reads in any still; (2) marbled residue that lingers 3-6 s
        // after each surge - the swell crest is also evaluated 1.5/3/4.5 s in the past and the
        // residue keeps the decayed maximum; (3) residue advects shoreward along the swell travel
        // direction (it does not churn in place); (4) surge peak denser but still marbled.
        // Everything is a pure function of position and time: nothing accumulates at runtime.
        return NoiseHelpers()
            + TEXT("struct SwellField { float H(float3 P, float T, float A, float3 C) {")
            + EmitWaves(false, false, true)
            + TEXT(" return h; } }; SwellField sw;")
            + TEXT("float d=Mask.r*10000; float contact=Mask.g; float shoal=MaskA; float k=1.0/(A*42+1);")
            + TEXT("float crest=saturate(sw.H(P,T,A,C)*k*.5+.5);")
            + TEXT("float cycle=smoothstep(.15,.85,.5+.5*sin(T*1.496+P.y*.00008));")
            + TEXT("float c1=saturate(sw.H(P,T-1.5,A,C)*k*.5+.5), c2=saturate(sw.H(P,T-3.0,A,C)*k*.5+.5), c3=saturate(sw.H(P,T-4.5,A,C)*k*.5+.5);")
            + TEXT("float linger=max(max(smoothstep(.55,.85,c1)*.85,smoothstep(.55,.85,c2)*.60),smoothstep(.55,.85,c3)*.35);")
            // Near-rock lace and residue share a 1.2 m/s shore-dominant current.
            + TEXT("float2 drift=float2(-.95,-.31)*T*120; float2 shore=float2(-.95,-.31)*T*120;")
            + TEXT("float cells=noise.f((P.xy+drift)*.0025);")
            + TEXT("float lace=smoothstep(.30,.70,noise.f((P.xy+drift)*.007)*.55+cells*.45);")
            + TEXT("float gaps=smoothstep(.32,.62,noise.f((P.xy+drift)*float2(.0004,.0011)));")
            + TEXT("float r1=1-abs(2*noise.f((P.xy+drift)*.0042)-1); float r2=1-abs(2*noise.f((P.xy+drift*1.15)*.011+3.7)-1);")
            + TEXT("float marble=saturate(smoothstep(.72,.94,r1)+smoothstep(.78,.96,r2)*.8);")
            + TEXT("float s1=1-abs(2*noise.f((P.xy-shore)*.0038+5.1)-1); float s2=1-abs(2*noise.f((P.xy-shore)*.010+9.3)-1);")
            + TEXT("float smarble=saturate(smoothstep(.60,.90,s1)+smoothstep(.70,.94,s2)*.8);")
            + TEXT("float fringe=contact*exp(-d/320)*(.55+.45*marble)*(.60+.40*gaps)*(.20+.80*cycle)*2.2;")
            + TEXT("float reach=300+1100*crest*contact;")
            + TEXT("float surge=contact*exp(-d/reach)*(.30+.70*crest)*(.25+.75*cycle)*lerp(saturate(marble*1.35),1,.25*exp(-d/100));")
            + TEXT("float patches=contact*exp(-d/650)*marble*gaps*.40;")
            + TEXT("float shallow=1-smoothstep(.05,.50,Mask.b);")
            + TEXT("float breaker=max(shoal,shallow*contact)*smoothstep(.70,.88,crest)*(.25+.75*cycle)*(.30+.70*saturate(marble*1.5))*1.2;")
            + TEXT("float residue=contact*max(shallow,exp(-d/900))*linger*(.35+.65*(1-cycle))*smarble*1.5;")
            + TEXT("float swash=contact*(1-smoothstep(0,.18,Mask.b))*(.35+.65*crest)*(.25+.75*cycle)*marble*.7;")
            + TEXT("float streak=contact*exp(-d/1400)*smoothstep(.58,.90,noise.f((P.xy-shore)*float2(.0016,.0035)))*smarble*.5;")
            // Foam002 (ambientCG CC0) opacity, advected with the same current, gives bubbly edges.
            + TEXT("float tex=Texture2DSample(FoamTex,FoamTexSampler,(P.xy+drift)/220.0).r;")
            + TEXT("float f=fringe+surge+patches+breaker+residue+swash+streak;")
            // Sharpening only up close; far away the mip-averaged value is used directly, so distant
            // foam cannot pop on/off between frames (R8 overview flicker at the cliff base).
            + TEXT("float x=f*(.50+.80*tex); return saturate(lerp(smoothstep(.12,.75,x),x,smoothstep(3000,20000,length(P-C))));");
    case ECoastalWaterShader::Scattering:
        // Input Mask. Greener shallows over the platform, deep blue-grey offshore (per cm); a
        // turbid surf-zone term keeps the waterline from reading as clear glass over rock.
        return TEXT("float s=smoothstep(0,1,Mask.b); float surf=1-smoothstep(0,.12,Mask.b); return lerp(float3(.00007,.00020,.00019),float3(.00005,.00012,.00022),s)+float3(.00025,.00030,.00026)*surf;");
    case ECoastalWaterShader::Absorption:
        return TEXT("float s=smoothstep(0,1,Mask.b); return lerp(float3(.0028,.00072,.00062),float3(.0034,.00090,.00046),s);");
    case ECoastalWaterShader::Roughness:
        // Inputs R (base), F (foam), P, C. Slightly rougher with distance so the far sea
        // stays calm and the sun path stays a bounded cone rather than silver glitter.
        return TEXT("float dist=length(P-C); float r=lerp(R,R+.14,smoothstep(3000,120000,dist))+.08*smoothstep(150000,500000,dist); return lerp(r,.62,F);");
    case ECoastalWaterShader::RockWetness:
        // For the rock/ground material owner (DEV-TERRAIN): inputs P (world pos), T, A, C.
        // Returns 0..1 wetness: fully wet below still water, a ragged 0.5-2 m splash band
        // above it that rises as the same swell crests arrive, dry above.
        return EmitWaves(false, false, true) + NoiseHelpers()
            + FString::Printf(TEXT("float above=P.z-(%s);"), *Num(SeaLevelCm))
            + TEXT("float crest=saturate(h/(A*42+1)*.5+.5);")
            + TEXT("float top=lerp(110,210,crest)+60*(noise.f(P.xy*.004+float2(P.z*.002,0))-.5);")
            + TEXT("return 1-smoothstep(top*.35,top,above);");
    case ECoastalWaterShader::Cirrus:
        // Inputs P (dome world pos), C (camera), S (direction toward the sun). Returns 0..1.
        // Cirrus uncinus: thin fibres at three scales with hooked ends, low-amplitude warp,
        // clumped/broken coverage, horizon/sun masked, no time input.
        // R10 mapping: angular (azimuth, elevation) coordinates so every stroke shares one gently
        // tilted, near-horizontal direction in every view. A perspective plane made parallel
        // streaks fan out from a vanishing point (starburst); a direction-dome map stretched
        // them vertically near the horizon. Two azimuth origins (seams due south / due north)
        // are blended by the north-south component so no seam is ever visible.
        return NoiseHelpers()
            + TEXT("struct CirrusField { float eval(float2 q, CoastNoise noise) {")
            + TEXT(" float2 p=q+(float2(noise.f(q*.5+1.3),noise.f(q*.5+8.1))-.5)*.25;")
            + TEXT(" float2 dir=float2(.985,.17); float2 r=float2(dot(p,dir),dot(p,float2(-dir.y,dir.x)));")
            + TEXT(" float seg=frac(r.x*.45+noise.f(float2(r.y*1.3,7.7))*.8); float hook=pow(smoothstep(.62,1.0,seg),2.0); r.y+=hook*.18;")
            + TEXT(" float fib=noise.f(float2(r.x*.6,r.y*12.0)); float fib2=noise.f(float2(r.x*1.4+4.0,r.y*26.0)); float fib3=noise.f(float2(r.x*3.0+9.0,r.y*55.0));")
            + TEXT(" float body=noise.f(float2(r.x*.35,r.y*2.2)+2.7); float clump=smoothstep(.48,.72,noise.f(q*.18+7.3));")
            + TEXT(" float breaks=smoothstep(.30,.70,noise.f(float2(r.x*2.5,r.y*.8)+5.5));")
            + TEXT(" return smoothstep(.50,.88,body*.30+fib*.30+fib2*.25+fib3*.15)*clump*breaks*(.75+.25*hook); } };")
            + TEXT("float3 v=normalize(P-C); float up=smoothstep(.03,.20,v.z); float el=asin(saturate(v.z));")
            + TEXT("CirrusField cf; float c1=cf.eval(float2(atan2(v.x,v.y)*3.0,el*9.0),noise);")
            + TEXT("float c2=cf.eval(float2(atan2(-v.x,-v.y)*3.0+11.0,el*9.0),noise);")
            + TEXT("float c=lerp(c2,c1,smoothstep(-.3,.3,v.y));")
            + TEXT("float sun=1-smoothstep(.975,.995,dot(v,normalize(S)));")
            + TEXT("return saturate(c*up*sun);");
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
    CoastalSea::FWaterlineGrid Waterline;
    for (UMeshComponent* Part : CoastalColorParts)
    {
        UProceduralMeshComponent* Land = Cast<UProceduralMeshComponent>(Part);
        // Near-field land only: the distant backdrop range is several km away and needs no surf.
        const FProcMeshSection* Section = Land && Land->GetName().StartsWith(TEXT("Coastal"))
            && !Land->GetName().Contains(TEXT("Distant")) ? Land->GetProcMeshSection(0) : nullptr;
        if (Section == nullptr) continue;
        const TArray<FProcMeshVertex>& V = Section->ProcVertexBuffer;
        const TArray<uint32>& I = Section->ProcIndexBuffer;
        for (int32 T = 0; T + 2 < I.Num(); T += 3)
            for (int32 E = 0; E < 3; ++E)
            {
                const FVector A = V[I[T + E]].Position, B = V[I[T + (E + 1) % 3]].Position;
                if ((A.Z - SeaLevelCm) * (B.Z - SeaLevelCm) >= 0) continue;
                const FVector X = FMath::Lerp(A, B, (SeaLevelCm - A.Z) / (B.Z - A.Z));
                Waterline.Add(FVector2D(X.X, X.Y));
            }
    }
    WaterlinePointCount = Waterline.Num;
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
                // Swash energy must also sit on the land-side vertices of the waterline triangles:
                // otherwise contact interpolates to ~0 exactly at the waterline (R7b south: no surf).
                const float Swash = .55F + .25F * FMath::PerlinNoise2D(FVector2D(X / 13000.0F + 1.7F, Y / 13000.0F - 4.2F));
                if (Land >= SeaLevelCm)
                {
                    Distance = 0.0F;
                    if (Land < SeaLevelCm + 2500.0F) VertexContact = FMath::Max(VertexContact * (bCliffCoast ? 1.0F : 0.0F), Swash);
                }
                else if (Land > SeaLevelCm - 2500.0F)
                {
                    const float GX = (GetCoastalLandHeight(X + 300.0F, Y) - GetCoastalLandHeight(X - 300.0F, Y)) / 600.0F;
                    const float GY = (GetCoastalLandHeight(X, Y + 300.0F) - GetCoastalLandHeight(X, Y - 300.0F)) / 600.0F;
                    const float Grade = FMath::Max(FMath::Sqrt(GX * GX + GY * GY), .02F);
                    Distance = FMath::Min(Distance, (SeaLevelCm - Land) / Grade);
                    VertexContact = FMath::Max(VertexContact * (bCliffCoast ? 1.0F : 0.0F), Swash);
                }
            }
            if (!bCliffCoast && VertexContact == Contact) VertexContact = 0.0F;
            if (Distance > 0.0F && Waterline.Num > 0 && FMath::Abs(X) < 320000.0F && FMath::Abs(Y) < 260000.0F)
            {
                const float Actual = Waterline.Nearest(FVector2D(X, Y), 3000.0F);
                if (Actual < 3000.0F && Actual < Distance) // Nearest returns the search radius when nothing is found
                {
                    Distance = Actual;
                    // Rock actually touches the sea here: guarantee surf energy even where the
                    // analytic cliff row says otherwise (perimeter or moved ledges).
                    VertexContact = FMath::Max(VertexContact, .6F);
                }
            }
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
