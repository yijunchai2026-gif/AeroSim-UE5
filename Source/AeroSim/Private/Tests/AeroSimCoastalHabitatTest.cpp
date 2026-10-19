#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PacificCoastGraybox.h"
#include "ProceduralMeshComponent.h"
#include "Engine/Texture.h"
#include "Materials/Material.h"
#include "MaterialShared.h"
#include "Misc/App.h"
#include "RHIGlobals.h"

// DEV-TERRAIN (RD-08): habitat field, landform and shoreline invariants for U5/U6.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalHabitatTest, "AeroSim.Environment.CoastalTerrain.HabitatLandformAndShore",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCoastalHabitatTest::RunTest(const FString& Parameters)
{
    // Determinism, normalisation and coverage of the public habitat API.
    int32 Counts[5] = {0, 0, 0, 0, 0};
    int32 LandSamples = 0, Mismatches = 0, BadWeights = 0;
    for (float X = -230000; X <= 15000; X += 4000)
        for (float Y = -210000; Y <= 210000; Y += 4000)
        {
            const FCoastHabitatSample A = FPacificCoastHabitat::Sample(X, Y);
            const FCoastHabitatSample B = FPacificCoastHabitat::Sample(X, Y);
            if (A.Class != B.Class || A.Woodland != B.Woodland || A.Rock != B.Rock || FPacificCoastHabitat::GetHabitatClass(X, Y) != A.Class) ++Mismatches;
            const float Sum = A.ExposedGrass + A.ShelteredGrass + A.Woodland + A.Rock + A.Shore;
            if (!FMath::IsNearlyEqual(Sum, 1.0F, .001F) || FMath::Min(FMath::Min(A.ExposedGrass, A.ShelteredGrass), FMath::Min(A.Woodland, FMath::Min(A.Rock, A.Shore))) < 0) ++BadWeights;
            if (A.Class == ECoastHabitatClass::CliffShore) continue;
            ++LandSamples;
            ++Counts[static_cast<int32>(A.Class)];
        }
    TestEqual(TEXT("Habitat sampling is deterministic"), Mismatches, 0);
    TestEqual(TEXT("Habitat soft weights are non-negative and sum to one"), BadWeights, 0);
    const TCHAR* Names[] = {TEXT("exposed grass"), TEXT("sheltered grass"), TEXT("woodland"), TEXT("steep rock")};
    for (int32 Class = 0; Class < 4; ++Class)
        TestTrue(FString::Printf(TEXT("Habitat class %s covers at least 2%% of the land"), Names[Class]), LandSamples > 0 && Counts[Class] > LandSamples / 50);
    TestTrue(TEXT("Golden exposed grass is the dominant habitat (at least 40% of land)"), Counts[0] * 5 >= LandSamples * 2 && Counts[0] > Counts[1] && Counts[0] > Counts[2]);
    TestTrue(TEXT("Woodland ground stays a minority of the land (under 35%)"), Counts[2] * 100 < LandSamples * 35);
    TestTrue(TEXT("Open sea beyond the cliff is classified as shore"), FPacificCoastHabitat::GetHabitatClass(APacificCoastGraybox::GetCliffEdgeX(0) + 5000, 0) == ECoastHabitatClass::CliffShore);
    for (const FVector& Route : FPacificCoastGrayboxContract::GetPlateauClearancePoints())
    {
        const ECoastHabitatClass Class = FPacificCoastHabitat::GetHabitatClass(Route.X, Route.Y);
        TestTrue(TEXT("Flight route ground is open grass, not woodland or rock"), Class == ECoastHabitatClass::ExposedGrass || Class == ECoastHabitatClass::ShelteredGrass);
        // Route grade preserved: the corridor around every waypoint is exactly level.
        for (const FVector2D Offset : {FVector2D(0, 2000), FVector2D(0, -2000), FVector2D(-2000, 0), FVector2D(-2000, 2000)})
            TestTrue(TEXT("Route corridor keeps its level supported grade"), FMath::Abs(APacificCoastGraybox::GetCoastalLandHeight(Route.X + Offset.X, Route.Y + Offset.Y)) < .5F);
    }

    // No hard classification band edges: weight change between 5 m neighbours on real slopes.
    float MaximumStep = 0;
    int32 Steps = 0, LargeSteps = 0;
    FVector2D WorstStep = FVector2D::ZeroVector;
    for (float Y = -150000; Y <= 150000; Y += 15000)
    {
        FCoastHabitatSample Previous = FPacificCoastHabitat::Sample(-215000, Y);
        for (float X = -214500; X <= -40000; X += 500)
        {
            const FCoastHabitatSample Current = FPacificCoastHabitat::Sample(X, Y);
            if (Current.Shore < .01F && Previous.Shore < .01F)
            {
                const float Step = FMath::Max(FMath::Max(FMath::Abs(Current.ExposedGrass - Previous.ExposedGrass), FMath::Abs(Current.ShelteredGrass - Previous.ShelteredGrass)),
                    FMath::Max(FMath::Abs(Current.Woodland - Previous.Woodland), FMath::Abs(Current.Rock - Previous.Rock)));
                ++Steps;
                if (Step > .15F) ++LargeSteps;
                if (Step > MaximumStep) { MaximumStep = Step; WorstStep = FVector2D(X, Y); }
            }
            Previous = Current;
        }
    }
    // A hard classification edge would change a weight by ~1.0 in one cell; every transition here
    // must spread over at least three 5 m cells, and steep transitions must be rare.
    TestTrue(TEXT("Habitat weights never jump by more than 0.45 across a 5 m cell (no sawtooth band edge)"), Steps > 5000 && MaximumStep < .45F);
    TestTrue(TEXT("Fewer than 4% of 5 m cells change any habitat weight by more than 0.15"), LargeSteps * 25 < Steps);

    // R3: principal drainages are real incised valleys (geometry), each with a tributary.
    {
        struct FProbe { FVector2D Centre, Along; const TCHAR* Name; };
        const FProbe Probes[] = {
            {FVector2D(-100000, 13300), FVector2D(18000, 6000), TEXT("system A trunk")}, {FVector2D(-114000, 0), FVector2D(8000, 19000), TEXT("system A tributary")},
            {FVector2D(-118000, -65000), FVector2D(20000, -10000), TEXT("system B trunk")}, {FVector2D(-131000, -70000), FVector2D(22000, 12000), TEXT("system B tributary")},
            {FVector2D(-99000, 96000), FVector2D(18000, 16000), TEXT("system C trunk")}};
        for (const FProbe& Probe : Probes)
        {
            const FVector2D Across = FVector2D(-Probe.Along.Y, Probe.Along.X).GetSafeNormal();
            float Floor = BIG_NUMBER;
            for (float Offset = -3000; Offset <= 3000; Offset += 250)
            {
                const FVector2D P = Probe.Centre + Across * Offset;
                Floor = FMath::Min(Floor, APacificCoastGraybox::GetCoastalLandHeight(P.X, P.Y));
            }
            const FVector2D L = Probe.Centre - Across * 16000, R = Probe.Centre + Across * 16000;
            const float Banks = .5F * (APacificCoastGraybox::GetCoastalLandHeight(L.X, L.Y) + APacificCoastGraybox::GetCoastalLandHeight(R.X, R.Y));
            TestTrue(FString::Printf(TEXT("Drainage %s is incised at least 15 m below its banks"), Probe.Name), Banks - Floor > 1500);
            AddInfo(FString::Printf(TEXT("Drainage %s depth below banks %.0f cm"), Probe.Name, Banks - Floor));
        }
    }

    // Bay Area landform: aspect controls habitat on the main ridge flanks.
    float SunWood = 0, ShadeWood = 0; int32 SunCount = 0, ShadeCount = 0;
    for (float X = -200000; X <= -60000; X += 3000)
        for (float Y = -160000; Y <= 160000; Y += 3000)
        {
            const FCoastHabitatSample S = FPacificCoastHabitat::Sample(X, Y);
            if (S.Shore > .5F || S.Grade < .15F) continue;
            if (S.SunExposure > .5F) { SunWood += S.Woodland; ++SunCount; }
            if (S.SunExposure < -.5F) { ShadeWood += S.Woodland; ++ShadeCount; }
        }
    TestTrue(TEXT("Shaded aspects carry clearly more woodland than sun-facing aspects"), SunCount > 50 && ShadeCount > 50 && ShadeWood / ShadeCount > SunWood / SunCount + .2F);

    // Shorelines: the north, south and west waterlines are irregular, not ruler-straight.
    auto Spread = [](const TArray<float>& Values) { float Min = BIG_NUMBER, Max = -BIG_NUMBER; for (float V : Values) { Min = FMath::Min(Min, V); Max = FMath::Max(Max, V); } return Max - Min; };
    for (int32 Side = -1; Side <= 1; Side += 2)
    {
        TArray<float> Waterline;
        for (float X = -200000; X <= -20000; X += 2500)
            for (float Y = 220000; Y > 120000; Y -= 250)
                if (APacificCoastGraybox::GetCoastalLandHeight(X, Side * Y) > APacificCoastGraybox::SeaLevelCm) { Waterline.Add(Y); break; }
        TestTrue(TEXT("North/south waterline wanders by more than 60 m"), Waterline.Num() > 60 && Spread(Waterline) > 6000);
    }
    TArray<float> WestWaterline;
    for (float Y = -150000; Y <= 150000; Y += 2500)
        for (float X = -240000; X < -150000; X += 250)
            if (APacificCoastGraybox::GetCoastalLandHeight(X, Y) > APacificCoastGraybox::SeaLevelCm) { WestWaterline.Add(X); break; }
    TestTrue(TEXT("West waterline wanders by more than 60 m"), WestWaterline.Num() > 100 && Spread(WestWaterline) > 6000);

    // Distant receding ridgelines: rooted below sea at their perimeter, taller than the main summit.
    const APacificCoastGraybox* Coast = GetDefault<APacificCoastGraybox>();
    TArray<UProceduralMeshComponent*> Meshes; Coast->GetComponents(Meshes);
    bool bRange = false;
    for (UProceduralMeshComponent* Mesh : Meshes)
    {
        if (Mesh->GetFName() != TEXT("CoastalDistantRange")) continue;
        const FProcMeshSection* Section = Mesh->GetProcMeshSection(0);
        if (!TestNotNull(TEXT("Distant range has geometry"), Section)) return false;
        bRange = true;
        const FBox Box = Section->SectionLocalBox;
        float PerimeterMax = -BIG_NUMBER, Top = -BIG_NUMBER;
        for (const FProcMeshVertex& V : Section->ProcVertexBuffer)
        {
            Top = FMath::Max(Top, static_cast<float>(V.Position.Z));
            if (V.Position.X > Box.Max.X - 1 || V.Position.X < Box.Min.X + 1 || FMath::Abs(V.Position.Y) > Box.Max.Y - 1)
                PerimeterMax = FMath::Max(PerimeterMax, static_cast<float>(V.Position.Z));
        }
        TestTrue(TEXT("Distant range perimeter is rooted below sea level"), PerimeterMax < APacificCoastGraybox::SeaLevelCm - 3000);
        TestTrue(TEXT("Distant range starts behind the playable land"), Box.Max.X < -228000);
        TestTrue(TEXT("Receding ridgelines rise above the near main summit"), Top > 40000 && Top < 70000);
        AddInfo(FString::Printf(TEXT("Distant range top %.0f cm; perimeter max %.0f cm; %d triangles"), Top, PerimeterMax, Section->ProcIndexBuffer.Num() / 3));
    }
    TestTrue(TEXT("Distant receding ridgeline mesh exists"), bRange);

    // Talus foot: supported blocks rest on the real cliff foot at the waterline; several emerge.
    for (UProceduralMeshComponent* Mesh : Meshes)
    {
        if (Mesh->GetFName() != TEXT("CoastalTalusButtresses")) continue;
        const FProcMeshSection* Section = Mesh->GetProcMeshSection(0);
        if (!TestNotNull(TEXT("Talus has geometry"), Section)) break;
        const int32 Blocks = Section->ProcVertexBuffer.Num() / 16;
        int32 Emergent = 0, NearWaterline = 0;
        for (int32 Block = 0; Block < Blocks; ++Block)
        {
            const FVector Top = Section->ProcVertexBuffer[Block * 16 + 15].Position;
            if (Top.Z > APacificCoastGraybox::SeaLevelCm + 100) ++Emergent;
            if (FMath::Abs(Top.X - APacificCoastGraybox::GetCoastalShoreX(Top.Y)) < 2500) ++NearWaterline;
        }
        TestEqual(TEXT("Sixty-four talus blocks"), Blocks, 64);
        TestTrue(TEXT("At least a quarter of the talus blocks break the sea surface"), Emergent * 4 >= Blocks);
        TestTrue(TEXT("Talus blocks sit at the actual waterline, not buried behind the cliff"), NearWaterline * 10 >= Blocks * 9);
        AddInfo(FString::Printf(TEXT("Talus: %d blocks, %d emergent, %d within 25 m of the waterline"), Blocks, Emergent, NearWaterline));
    }

    // R5a: forest-floor litter (DEV-FLORA trunk mask) reaches the terrain as 1 - vertex alpha.
    {
        int32 LitterVertices = 0, OpenLandLitter = 0;
        for (UProceduralMeshComponent* Mesh : Meshes)
        {
            if (Mesh->GetFName() != TEXT("CoastalPlateau")) continue;
            if (const FProcMeshSection* Section = Mesh->GetProcMeshSection(0))
                for (const FProcMeshVertex& V : Section->ProcVertexBuffer)
                {
                    if (V.Color.A < 160) ++LitterVertices;
                    if (V.Color.A < 250 && FMath::IsNearlyZero(FPacificCoastHabitat::GetForestLitter(V.Position.X, V.Position.Y))) ++OpenLandLitter;
                }
        }
        TestTrue(TEXT("Forest stands carry trunk-base litter in the ground vertex alpha"), LitterVertices > 200);
        TestEqual(TEXT("Litter appears only where the flora mask places it"), OpenLandLitter, 0);
        AddInfo(FString::Printf(TEXT("Litter vertices %d"), LitterVertices));
    }

    // Cliff face never overhangs: along every sampled cliff column, X grows monotonically with depth.
    {
        TMap<int32, TArray<FVector>> Columns;
        for (UProceduralMeshComponent* Mesh : Meshes)
        {
            if (Mesh->GetFName() != TEXT("CoastalPlateau")) continue;
            if (const FProcMeshSection* Section = Mesh->GetProcMeshSection(0))
                for (const FProcMeshVertex& V : Section->ProcVertexBuffer)
                    if (FMath::Abs(V.Position.Y) < 60000 && V.Position.Z > -14001 && V.Position.X >= APacificCoastGraybox::GetCliffEdgeX(V.Position.Y) - .5)
                        Columns.FindOrAdd(FMath::RoundToInt(V.Position.Y)).Add(V.Position);
        }
        int32 Overhangs = 0, Checked = 0;
        for (auto& Column : Columns)
        {
            Column.Value.Sort([](const FVector& A, const FVector& B) { return A.Z > B.Z; });
            for (int32 Index = 1; Index < Column.Value.Num(); ++Index, ++Checked)
                if (Column.Value[Index].X < Column.Value[Index - 1].X - 1.0) ++Overhangs;
        }
        TestTrue(TEXT("Cliff columns were sampled"), Checked > 5000);
        TestEqual(TEXT("Sea cliff has no overhanging step anywhere in the core coast"), Overhangs, 0);
    }
    for (float DX : {-1000.0F, -500.0F, 0.0F, 500.0F})
    {
        const FCoastHabitatSample W = FPacificCoastHabitat::Sample(WorstStep.X + DX, WorstStep.Y);
        AddInfo(FString::Printf(TEXT("Worst-step neighbourhood x%+.0f: h %.0f grade %.3f sun %.3f drain %.3f | E %.2f S %.2f W %.2f R %.2f Sh %.2f"), DX,
            APacificCoastGraybox::GetCoastalLandHeight(WorstStep.X + DX, WorstStep.Y), W.Grade, W.SunExposure, W.Drainage, W.ExposedGrass, W.ShelteredGrass, W.Woodland, W.Rock, W.Shore));
    }
    // U5 ground: true-scale CC0 habitat layers, detail and macro inputs on the shared land function.
    for (const TCHAR* Surface : {TEXT("CoastalPlateau"), TEXT("CoastalForestTrail")})
    {
        UMaterialInterface* Ground = nullptr;
        for (UProceduralMeshComponent* Mesh : Meshes) if (Mesh->GetName() == Surface) Ground = Mesh->GetMaterial(0);
        if (!TestNotNull(TEXT("Terrain surface uses an imported draft ground material"), Ground)) continue;
        const TPair<const TCHAR*, const TCHAR*> Layers[] = {
            {TEXT("ExposedGrassColor"), TEXT("T_CoastWitheredGrass_BaseColor")}, {TEXT("ShelteredGrassColor"), TEXT("T_CoastSparseGrass_BaseColor")},
            {TEXT("WoodlandLitterColor"), TEXT("T_CoastLeafLitter_BaseColor")}, {TEXT("StonyGroundColor"), TEXT("T_CoastStonyGround_BaseColor")},
            {TEXT("DetailNormal"), TEXT("T_CoastDetailDirt_Normal")}, {TEXT("DetailNormalB"), TEXT("T_CoastDetailDirt_Normal")},
            {TEXT("MacroVariation"), TEXT("T_CoastMacroGrassRock_BaseColor")}};
        for (const auto& Layer : Layers)
        {
            UTexture* Texture = nullptr;
            TestTrue(FString::Printf(TEXT("%s uses %s"), Layer.Key, Layer.Value), Ground->GetTextureParameterValue(FMaterialParameterInfo(Layer.Key), Texture) && Texture && Texture->GetFName() == Layer.Value);
        }
#if WITH_EDITOR
        if (FApp::CanEverRender())
        {
            FMaterialResource* Resource = Ground->GetMaterial()->GetMaterialResource(GMaxRHIShaderPlatform);
            if (TestNotNull(TEXT("Ground material has a render resource"), Resource))
            {
                Resource->FinishCompilation();
                for (const FString& Error : Resource->GetCompileErrors()) AddError(FString::Printf(TEXT("Ground shader compile error: %s"), *Error));
                TestTrue(TEXT("Ground material shader map compiled"), Resource->GetGameThreadShaderMap() != nullptr);
            }
        }
        else AddInfo(TEXT("Ground shader compile check NOT_RUN under NullRHI"));
#else
        AddInfo(TEXT("Ground shader compile check NOT_RUN outside editor builds"));
#endif
    }
    // Cliff rock: worn sandstone upper face and fractured lower cliff layer, compiled for the real RHI.
    for (UProceduralMeshComponent* Mesh : Meshes)
    {
        if (Mesh->GetFName() != TEXT("CoastalBedrock")) continue;
        UMaterialInterface* Rock = Mesh->GetMaterial(0);
        if (!TestNotNull(TEXT("Cliff uses the imported rock material"), Rock)) break;
        UTexture* Upper = nullptr; UTexture* LowerFace = nullptr;
        TestTrue(TEXT("Upper cliff face uses worn_rock_natural_01"), Rock->GetTextureParameterValue(FMaterialParameterInfo(TEXT("BaseColorTexture")), Upper) && Upper && Upper->GetFName() == TEXT("T_CoastWornRock_BaseColor"));
        TestTrue(TEXT("Lower cliff uses rock_face_03"), Rock->GetTextureParameterValue(FMaterialParameterInfo(TEXT("LowerCliffColor")), LowerFace) && LowerFace && LowerFace->GetFName() == TEXT("T_CoastRockFace_BaseColor"));
#if WITH_EDITOR
        if (FApp::CanEverRender())
            if (FMaterialResource* Resource = Rock->GetMaterial()->GetMaterialResource(GMaxRHIShaderPlatform))
            {
                Resource->FinishCompilation();
                for (const FString& Error : Resource->GetCompileErrors()) AddError(FString::Printf(TEXT("Rock shader compile error: %s"), *Error));
                TestTrue(TEXT("Rock material shader map compiled"), Resource->GetGameThreadShaderMap() != nullptr);
            }
#endif
    }
    AddInfo(FString::Printf(TEXT("Habitat coverage of %d land samples: exposed %d sheltered %d woodland %d rock %d"), LandSamples, Counts[0], Counts[1], Counts[2], Counts[3]));
    AddInfo(FString::Printf(TEXT("Habitat 5 m step max %.3f at (%.0f, %.0f); >0.15 in %d of %d cells; woodland mean sun %.3f shade %.3f"),
        MaximumStep, WorstStep.X, WorstStep.Y, LargeSteps, Steps, SunCount ? SunWood / SunCount : 0, ShadeCount ? ShadeWood / ShadeCount : 0));
    return true;
}
#endif
