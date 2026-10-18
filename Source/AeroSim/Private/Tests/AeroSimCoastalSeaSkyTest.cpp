#if WITH_DEV_AUTOMATION_TESTS
// DEV-SEASKY: invariants for the Pacific sea state, sky/sun and weather derivation.
// These are engineering contracts only; they do not establish visual or user acceptance.
#include "Misc/AutomationTest.h"
#include "PacificCoastGraybox.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

namespace SeaSkyTest
{
template <typename T> T* Find(const AActor* Actor)
{
    TArray<T*> Items; Actor->GetComponents(Items);
    return Items.IsEmpty() ? nullptr : Items[0];
}
UMeshComponent* FindPart(const AActor* Actor, const FName Name)
{
    TArray<UMeshComponent*> Items; Actor->GetComponents(Items);
    for (UMeshComponent* Item : Items) if (Item->GetFName() == Name) return Item;
    return nullptr;
}
struct FSnapshot
{
    FRotator Sun; float Fog = 0, SecondFog = 0, Falloff = 0, Mie = 0, Aerial = 0, CloudBottom = 0, Light = 0;
    bool operator==(const FSnapshot& O) const
    {
        return Sun.Equals(O.Sun, 1e-4F) && Fog == O.Fog && SecondFog == O.SecondFog && Falloff == O.Falloff
            && Mie == O.Mie && Aerial == O.Aerial && CloudBottom == O.CloudBottom && Light == O.Light;
    }
};
FSnapshot Capture(const APacificCoastGraybox* Coast)
{
    FSnapshot S;
    const auto* Key = Find<UDirectionalLightComponent>(Coast);
    const auto* Fog = Find<UExponentialHeightFogComponent>(Coast);
    const auto* Sky = Find<USkyAtmosphereComponent>(Coast);
    const auto* Clouds = Find<UVolumetricCloudComponent>(Coast);
    if (Key) { S.Sun = Key->GetRelativeRotation(); S.Light = Key->Intensity; }
    if (Fog) { S.Fog = Fog->FogDensity; S.SecondFog = Fog->SecondFogData.FogDensity; S.Falloff = Fog->FogHeightFalloff; }
    if (Sky) { S.Mie = Sky->MieScatteringScale; S.Aerial = Sky->AerialPespectiveViewDistanceScale; }
    if (Clouds) S.CloudBottom = Clouds->LayerBottomAltitude;
    return S;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalSunTest, "AeroSim.Environment.SeaSky.ClearSunCaliforniaOctober",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCoastalSunTest::RunTest(const FString& Parameters)
{
    const FEnvironmentState Clear;
    TestTrue(TEXT("Baseline is Clear 16:30 seed 21630"), Clear.IsValid() && Clear.Weather == EAeroSimWeather::Clear);
    const FVector2D Sun = APacificCoastGraybox::GetCoastalSunElevationAzimuth(Clear);
    AddInfo(FString::Printf(TEXT("Clear 16:30 PDT sun elevation %.2f deg, azimuth %.2f deg"), Sun.X, Sun.Y));
    TestTrue(TEXT("Late-afternoon October sun is ~20 deg high"), Sun.X > 18.0 && Sun.X < 24.0);
    TestTrue(TEXT("Sun is in the south-west"), Sun.Y > 228.0 && Sun.Y < 245.0);
    // +X is offshore/west, +Y north: the direction toward the sun must be over the ocean, southward.
    const FVector ToSun = -APacificCoastGraybox::GetCoastalSunRotation(Clear).Vector();
    TestTrue(TEXT("Sun is over the ocean (west) and to the south, above the horizon"), ToSun.X > .5 && ToSun.Y < -.3 && ToSun.Z > .3 && ToSun.Z < .42);

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    APacificCoastGraybox* Coast = World ? World->SpawnActor<APacificCoastGraybox>() : nullptr;
    if (TestNotNull(TEXT("Coast spawns"), Coast))
    {
        Coast->ApplyEnvironmentState(Clear);
        const auto* Key = SeaSkyTest::Find<UDirectionalLightComponent>(Coast);
        TestTrue(TEXT("Key light is the atmosphere sun at the computed angle"), Key && Key->IsUsedAsAtmosphereSunLight()
            && Key->GetRelativeRotation().Equals(APacificCoastGraybox::GetCoastalSunRotation(Clear), .01F));
        const auto* Fog = SeaSkyTest::Find<UExponentialHeightFogComponent>(Coast);
        TestTrue(TEXT("Clear fog sits at sea level and keeps the near sea readable"), Fog && FMath::IsNearlyEqual(Fog->GetRelativeLocation().Z, APacificCoastGraybox::SeaLevelCm)
            && Fog->StartDistance > 0 && Fog->SecondFogData.FogDensity > 0);
        const auto* Clouds = SeaSkyTest::Find<UVolumetricCloudComponent>(Coast);
        if (TestNotNull(TEXT("Volumetric clouds exist"), Clouds))
        {
            const UMaterialInterface* Material = Clouds->GetMaterial();
            float Coverage = -1, Wind = -1;
            const bool bCoverage = Material && Material->GetScalarParameterValue(FMaterialParameterInfo(TEXT("Cloud_GlobalCoverage")), Coverage);
            const bool bWind = Material && Material->GetScalarParameterValue(FMaterialParameterInfo(TEXT("WindMultiplier")), Wind);
            AddInfo(FString::Printf(TEXT("Cloud params: coverage %d %.3f wind %d %.3f; layer %.2f km + %.2f km"),
                bCoverage, Coverage, bWind, Wind, Clouds->LayerBottomAltitude, Clouds->LayerHeight));
            TestTrue(TEXT("Clear cloud layer is a shallow fair-weather band"), Clouds->LayerBottomAltitude >= 1.2F && Clouds->LayerHeight <= 2.0F);
        }
    }
    if (World) World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalEnvironmentDeterminismTest, "AeroSim.Environment.SeaSky.WeatherDerivesFromOneState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCoastalEnvironmentDeterminismTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    APacificCoastGraybox* A = World ? World->SpawnActor<APacificCoastGraybox>() : nullptr;
    APacificCoastGraybox* B = World ? World->SpawnActor<APacificCoastGraybox>() : nullptr;
    if (TestTrue(TEXT("Two coasts spawn"), A && B))
    {
        SeaSkyTest::FSnapshot Previous;
        const EAeroSimWeather Order[] = {EAeroSimWeather::Clear, EAeroSimWeather::Overcast, EAeroSimWeather::Rain};
        TArray<SeaSkyTest::FSnapshot> Shots;
        for (const EAeroSimWeather Weather : Order)
        {
            FEnvironmentState State; State.SetWeather(Weather);
            A->ApplyEnvironmentState(State);
            B->ApplyEnvironmentState(State);
            const SeaSkyTest::FSnapshot SA = SeaSkyTest::Capture(A), SB = SeaSkyTest::Capture(B);
            TestTrue(TEXT("Same Environment State gives identical sky/fog/cloud/light"), SA == SB);
            TestTrue(TEXT("Applied fog is exactly the state's fog"), FMath::IsNearlyEqual(SA.Fog, State.GetFogDensity()));
            Shots.Add(SA);
        }
        // Re-applying Clear after Rain returns exactly to the Clear baseline (no hidden state).
        FEnvironmentState Clear; A->ApplyEnvironmentState(Clear);
        TestTrue(TEXT("Clear baseline is reproducible after weather changes"), SeaSkyTest::Capture(A) == Shots[0]);
        TestTrue(TEXT("Weather never moves the sun"), Shots[0].Sun.Equals(Shots[1].Sun) && Shots[0].Sun.Equals(Shots[2].Sun));
        TestTrue(TEXT("Haze, aerosol and cloud base progress consistently Clear->Overcast->Rain"),
            Shots[0].Mie < Shots[1].Mie && Shots[1].Mie < Shots[2].Mie && Shots[0].CloudBottom > Shots[1].CloudBottom
            && Shots[1].CloudBottom > Shots[2].CloudBottom && Shots[0].Light > Shots[1].Light && Shots[1].Light > Shots[2].Light);
    }
    if (World) World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalWaveSpectrumTest, "AeroSim.Environment.SeaSky.WaveSpectrumNonDegenerate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCoastalWaveSpectrumTest::RunTest(const FString& Parameters)
{
    using FWave = APacificCoastGraybox::FCoastalWaveComponent;
    const TArray<FWave>& Waves = APacificCoastGraybox::GetCoastalWaveSpectrum();
    TestTrue(TEXT("Multi-scale spectrum has at least 12 components"), Waves.Num() >= 12);
    float MinLambda = TNumericLimits<float>::Max(), MaxLambda = 0, MaxSpread = 0, Pinch = 0;
    int32 SwellBands = 0;
    for (int32 I = 0; I < Waves.Num(); ++I)
    {
        const FWave& W = Waves[I];
        TestTrue(TEXT("Direction is a unit vector"), FMath::IsNearlyEqual(W.Direction.Size(), 1.0, 1e-3));
        TestTrue(TEXT("Deep-water dispersion"), FMath::IsNearlyEqual(W.AngularFrequency, FMath::Sqrt(981.0F * 2 * PI / W.WavelengthCm), 1e-3F));
        MinLambda = FMath::Min(MinLambda, W.WavelengthCm); MaxLambda = FMath::Max(MaxLambda, W.WavelengthCm);
        if (W.WavelengthCm >= 3000 && W.WavelengthCm <= 12000) ++SwellBands;
        Pinch += W.Steepness / Waves.Num();
        for (int32 J = I + 1; J < Waves.Num(); ++J)
        {
            const float Cos = FVector2D::DotProduct(W.Direction, Waves[J].Direction);
            MaxSpread = FMath::Max(MaxSpread, FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Cos, -1.0F, 1.0F))));
            const float Ratio = FMath::Max(W.WavelengthCm, Waves[J].WavelengthCm) / FMath::Min(W.WavelengthCm, Waves[J].WavelengthCm);
            TestFalse(TEXT("No two components share both wavelength and direction (parallel bands)"), Ratio < 1.12F && Cos > .985F);
        }
    }
    AddInfo(FString::Printf(TEXT("%d components, %.0f-%.0f cm, direction spread %.1f deg, %d bands in 30-120 m"), Waves.Num(), MinLambda, MaxLambda, MaxSpread, SwellBands));
    TestTrue(TEXT("Spans metre ripples to >100 m swell"), MinLambda < 400 && MaxLambda > 10000);
    TestTrue(TEXT("At least two swell bands of 30-120 m"), SwellBands >= 2);
    TestTrue(TEXT("Directions are genuinely spread"), MaxSpread > 60);
    TestTrue(TEXT("Gerstner crests never loop"), Pinch < 1.0F);

    // Height statistics and motion from the CPU mirror of the shader displacement.
    double Sum = 0, SumSq = 0, Moved = 0; int32 Samples = 0;
    for (float X = 25000; X < 125000; X += 1730)
        for (float Y = -50000; Y < 50000; Y += 1910)
        {
            const FVector P0 = APacificCoastGraybox::GetCoastalOceanDisplacement(X, Y, 100.0F);
            const FVector P1 = APacificCoastGraybox::GetCoastalOceanDisplacement(X, Y, 100.3F);
            const double H = P0.Z - APacificCoastGraybox::SeaLevelCm;
            Sum += H; SumSq += H * H; Moved += FMath::Abs(P1.Z - P0.Z); ++Samples;
        }
    const double Rms = FMath::Sqrt(SumSq / Samples - FMath::Square(Sum / Samples));
    AddInfo(FString::Printf(TEXT("Sea RMS height %.1f cm, mean |dz| over 0.3 s %.2f cm"), Rms, Moved / Samples));
    TestTrue(TEXT("Calm-day Pacific swell: RMS height 20-70 cm (Hs ~1-2.5 m)"), Rms > 20 && Rms < 70);
    TestTrue(TEXT("Surface visibly moves between consecutive 300 ms frames"), Moved / Samples > 1.0);
    // Travelling, not standing: the main swell crest advances toward shore over time.
    const FWave& Main = Waves[0];
    const float Speed = Main.AngularFrequency / (2 * PI / Main.WavelengthCm);
    TestTrue(TEXT("Main swell travels toward the cliff (-X) at deep-water phase speed"), Main.Direction.X < -.9F && Speed > 1200 && Speed < 2000);

    // Generated shader matches the spectrum: every geometric component is in the WPO.
    const FString Offset = APacificCoastGraybox::GetCoastalWaterShaderCode(APacificCoastGraybox::ECoastalWaterShader::Offset);
    const FString Normal = APacificCoastGraybox::GetCoastalWaterShaderCode(APacificCoastGraybox::ECoastalWaterShader::Normal);
    int32 Geometric = 0; for (const FWave& W : Waves) Geometric += W.WavelengthCm >= 1200 ? 1 : 0;
    auto Count = [](const FString& Code) { int32 N = 0, From = 0; while ((From = Code.Find(TEXT("float th="), ESearchCase::CaseSensitive, ESearchDir::FromStart, From)) != INDEX_NONE) { ++N; ++From; } return N; };
    TestEqual(TEXT("WPO contains every geometric component"), Count(Offset), Geometric);
    TestEqual(TEXT("Normal contains every component"), Count(Normal), Waves.Num());
    TestTrue(TEXT("WPO displaces horizontally and vertically"), Offset.Contains(TEXT("return float3(o,h);")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalShoreMaskTest, "AeroSim.Environment.SeaSky.DepthAndContactMasks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCoastalShoreMaskTest::RunTest(const FString& Parameters)
{
    const float Shore = APacificCoastGraybox::GetCoastalShoreX(0);
    TestTrue(TEXT("Shallow platform at the waterline (<10 m)"), APacificCoastGraybox::GetCoastalSeabedDepthCm(Shore + 1000, 0) < 1000);
    TestTrue(TEXT("Deep water offshore (>120 m)"), APacificCoastGraybox::GetCoastalSeabedDepthCm(Shore + 14000, 0) > 12000);
    const APacificCoastGraybox* Coast = GetDefault<APacificCoastGraybox>();
    auto* Ocean = Cast<UProceduralMeshComponent>(SeaSkyTest::FindPart(Coast, TEXT("PacificOcean")));
    const FProcMeshSection* Section = Ocean ? Ocean->GetProcMeshSection(0) : nullptr;
    if (!TestNotNull(TEXT("Ocean mesh exists"), Section)) return false;
    int32 OffshoreFoamGates = 0, Offshore = 0, Contact = 0, Shallow = 0;
    bool bFlat = true;
    for (const FProcMeshVertex& V : Section->ProcVertexBuffer)
    {
        bFlat &= FMath::IsNearlyEqual(V.Position.Z, APacificCoastGraybox::SeaLevelCm);
        const float LocalShore = APacificCoastGraybox::GetCoastalShoreX(FMath::Clamp(V.Position.Y, -60000.0, 60000.0));
        if (FMath::Abs(V.Position.Y) < 60000 && V.Position.X > LocalShore + 25000)
        {
            ++Offshore;
            const FLinearColor C = V.Color.ReinterpretAsLinear();
            // Foam terms are gated by contact*exp(-d/...) and shoal; offshore must be deep, far and non-shoal.
            if (C.A > .01F || C.R < .99F || C.B < .99F) ++OffshoreFoamGates;
        }
        if (FMath::Abs(V.Position.Y) < 60000 && V.Position.X > LocalShore && V.Position.X < LocalShore + 600)
        {
            const FLinearColor C = V.Color.ReinterpretAsLinear();
            Contact += C.G > .1F && C.R < .07F ? 1 : 0;
            Shallow += C.B < .25F ? 1 : 0;
        }
    }
    AddInfo(FString::Printf(TEXT("offshore %d (gated %d), contact %d, shallow %d"), Offshore, OffshoreFoamGates, Contact, Shallow));
    TestTrue(TEXT("Water mesh is flat: swell is travelling material displacement"), bFlat);
    TestEqual(TEXT("Open water never receives contact/shoal foam"), OffshoreFoamGates, 0);
    TestTrue(TEXT("Water touching the rock carries contact energy and shallow depth"), Contact > 20 && Shallow > 20);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalRainWetnessTest, "AeroSim.Environment.SeaSky.RainWetsRealSurfaces",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCoastalRainWetnessTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    APacificCoastGraybox* Coast = World ? World->SpawnActor<APacificCoastGraybox>() : nullptr;
    if (TestNotNull(TEXT("Coast spawns"), Coast))
    {
        TArray<UStaticMeshComponent*> Boxes; Coast->GetComponents(Boxes);
        TestFalse(TEXT("Legacy floating wet boxes are gone"), Boxes.ContainsByPredicate([](const UStaticMeshComponent* M) { return M->GetName().StartsWith(TEXT("RainWetCoast_")); }));
        auto Roughness = [Coast](FName Name)
        {
            float Value = -1;
            const UMeshComponent* Mesh = SeaSkyTest::FindPart(Coast, Name);
            if (Mesh && Mesh->GetMaterial(0)) Mesh->GetMaterial(0)->GetScalarParameterValue(FMaterialParameterInfo(TEXT("Roughness")), Value);
            return Value;
        };
        FEnvironmentState State; State.SetWeather(EAeroSimWeather::Rain);
        Coast->ApplyEnvironmentState(State);
        TestEqual(TEXT("Rain wetness reaches the coastal surfaces"), Coast->GetAppliedSurfaceWetness(), 1.0F);
        const float WetRock = Roughness(TEXT("CoastalBedrock")), WetLand = Roughness(TEXT("CoastalPlateau"));
        State.SetWeather(EAeroSimWeather::Clear);
        Coast->ApplyEnvironmentState(State);
        const float DryRock = Roughness(TEXT("CoastalBedrock")), DryLand = Roughness(TEXT("CoastalPlateau"));
        AddInfo(FString::Printf(TEXT("rock roughness wet %.2f dry %.2f; land wet %.2f dry %.2f"), WetRock, DryRock, WetLand, DryLand));
        TestTrue(TEXT("Rain makes actual rock and land glossy"), WetRock >= 0 && WetRock < .5F && WetLand >= 0 && WetLand < .5F);
        TestTrue(TEXT("Clear dries them again"), DryRock > .9F && DryLand > .9F && Coast->GetAppliedSurfaceWetness() == 0.0F);
    }
    if (World) World->DestroyWorld(false);
    return true;
}
#endif
