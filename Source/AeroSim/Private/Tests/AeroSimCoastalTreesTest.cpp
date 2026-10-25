#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AeroSimGameMode.h"
#include "PacificCoastGraybox.h"
#include "PacificCoastFlora.h"
#include "ProceduralMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInterface.h"

namespace CoastalTreeTest
{
const UHierarchicalInstancedStaticMeshComponent* FindBatch(const AActor* Coast, const FName Name)
{
    TArray<UHierarchicalInstancedStaticMeshComponent*> Batches;
    Coast->GetComponents(Batches);
    for (const auto* Batch : Batches) if (Batch->GetFName() == Name) return Batch;
    return nullptr;
}
struct FCylinder { FVector A, B; float Radius; };
TArray<FCylinder> Proxies(const AActor* Coast)
{
    TArray<FCylinder> Result;
    const auto* Batch = FindBatch(Coast, TEXT("CoastalTrunkCollisionProxies"));
    if (!Batch) return Result;
    for (int32 Index = 0; Index < Batch->GetInstanceCount(); ++Index)
    {
        FTransform T; Batch->GetInstanceTransform(Index, T, false);
        Result.Add({T.TransformPosition(FVector(0, 0, -50)), T.TransformPosition(FVector(0, 0, 50)), static_cast<float>(T.GetScale3D().X * 50.0)});
    }
    return Result;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalTreeArchetypeTest, "AeroSim.Coastal.Flora.ArchetypesRootCollarAndLods",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCoastalTreeArchetypeTest::RunTest(const FString& Parameters)
{
    using namespace CoastFlora;
    const TArray<FArchetype>& Archetypes = GetArchetypes();
    TestEqual(TEXT("Seventeen deterministic archetypes (fir, pine, oak, saplings, veterans, young poles)"), Archetypes.Num(), 17);
    float MinBreast = MAX_flt, MaxBreast = 0;
    for (const FArchetype& A : Archetypes) if (A.IsOverstory()) { MinBreast = FMath::Min(MinBreast, A.BreastRadius); MaxBreast = FMath::Max(MaxBreast, A.BreastRadius); }
    TestTrue(TEXT("Overstory DBH spans young poles (<=0.35 m) to veterans (>=1.1 m)"), MinBreast * 2 <= 35 && MaxBreast * 2 >= 110);
    int32 Oaks = 0, Pines = 0, Firs = 0, Saplings = 0;
    for (const FArchetype& A : Archetypes)
    {
        Oaks += A.Species == ESpecies::CoastLiveOak; Pines += A.Species == ESpecies::MontereyPine; Firs += A.Species == ESpecies::DouglasFir; Saplings += A.Species == ESpecies::Sapling;
        if (!TestNotNull(TEXT("Archetype mesh was built"), A.Mesh) || !A.Mesh->GetRenderData()) return false;
        const FStaticMeshRenderData* Data = A.Mesh->GetRenderData();
        TestEqual(TEXT("Four LODs including a cheap far representation"), Data->LODResources.Num(), 4);
        for (int32 Lod = 0; Lod < Data->LODResources.Num(); ++Lod)
        {
            TestEqual(TEXT("Render LOD matches generated triangles"), static_cast<int32>(Data->LODResources[Lod].GetNumTriangles()), A.LodTriangles[Lod]);
            if (Lod > 0)
            {
                TestTrue(TEXT("Each LOD is cheaper than the previous"), A.LodTriangles[Lod] < A.LodTriangles[Lod - 1]);
                TestTrue(TEXT("LOD screen sizes decrease"), Data->ScreenSize[Lod].Default < Data->ScreenSize[Lod - 1].Default);
            }
        }
        TestTrue(TEXT("LOD0 within the ART-FLORA budget (trees <=20k, saplings <=4k)"), A.LodTriangles[0] <= (A.Species == ESpecies::Sapling ? 4000 : 20000));
        TestTrue(TEXT("Far LOD is a few hundred triangles"), A.LodTriangles[3] <= 600);
        // U1: welded flaring root collar, no strut legs.
        // Saplings (<5 m) are seedlings without a buttressed collar; their thin stem is exempt.
        if (A.Species != ESpecies::Sapling)
        {
            TestTrue(TEXT("Root collar flares visibly"), A.CollarRadius >= 1.20F * A.BreastRadius);
            TestTrue(TEXT("Root collar stays within ~1.3x the trunk radius"), A.CollarRadius <= 1.30F * A.BreastRadius);
        }
        TestTrue(TEXT("Trunk continues below grade so no gap can open on slopes"), A.BuriedDepth >= 50);
        if (A.IsOverstory()) TestTrue(TEXT("Overstory trunks carry simple collision"), A.Collision.Num() >= 2);
        AddInfo(FString::Printf(TEXT("FLORA_ARCHETYPE %s height=%.0f breast_r=%.1f collar_r=%.1f ratio=%.2f crown_base=%.0f stems=%d lod_tris=%d/%d/%d/%d"),
            *A.Name.ToString(), A.Height, A.BreastRadius, A.CollarRadius, A.CollarRadius / A.BreastRadius, A.CrownBase, A.StemCount,
            A.LodTriangles[0], A.LodTriangles[1], A.LodTriangles[2], A.LodTriangles[3]));
    }
    TestTrue(TEXT("Douglas-fir, Monterey pine, coast live oak and saplings present"), Oaks >= 3 && Pines >= 3 && Firs >= 4 && Saplings >= 3);
    for (const FArchetype& A : Archetypes) if (A.Species == ESpecies::CoastLiveOak) TestTrue(TEXT("Live oaks have multiple leaning stems"), A.StemCount >= 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalTreePlacementTest, "AeroSim.Coastal.Flora.DensityGroundingAndClearance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCoastalTreePlacementTest::RunTest(const FString& Parameters)
{
    using namespace CoastFlora;
    const APacificCoastGraybox* Coast = GetDefault<APacificCoastGraybox>();
    if (!TestNotNull(TEXT("Generated coast exists"), Coast)) return false;
    const FLayout& Layout = GetLayout();
    const TArray<FArchetype>& Archetypes = GetArchetypes();
    TestTrue(TEXT("Layout is deterministic across calls"), &Layout == &GetLayout());
    // Density over the closed stand interior (density field >= .85, i.e. past the feathered edge and
    // within the 0.88-1.0 clumping band), measured on a 5 m grid.
    int32 InteriorCells = 0;
    for (float X = -31000; X < 12500; X += 500) for (float Y = -28000; Y < 28000; Y += 500)
        if (NearStandDensity(X + 250, Y + 250) >= .85F) ++InteriorCells;
    int32 InteriorTrees = 0, InteriorStems = 0, NearAll = 0, Mature = 0, Pole = 0, Sapling = 0;
    int32 Floating = 0, BuriedTrees = 0, RouteIntrusions = 0, CliffIntrusions = 0, BoardwalkIntrusions = 0;
    for (const FPlacedTree& Tree : Layout.Trees)
    {
        if (!Tree.bNearForest) continue;
        const FArchetype& A = Archetypes[Tree.Archetype];
        ++NearAll;
        (A.Age == EAge::Mature ? Mature : A.Age == EAge::Pole ? Pole : Sapling)++;
        if (A.IsOverstory() && NearStandDensity(Tree.Base.X, Tree.Base.Y) >= .85F) { ++InteriorTrees; InteriorStems += A.StemCount; }
        // Root grounding: around the actual collar the terrain lies between the visible collar
        // bottom and the buried trunk end, i.e. the trunk always passes through the surface.
        const FTransform T = InstanceTransform(Tree);
        for (int32 Ring = 0; Ring < 3; ++Ring)
            for (int32 Angle = 0; Angle < 12; ++Angle)
            {
                const float Radius = A.CollarRadius * (Ring == 0 ? .0F : Ring == 1 ? .6F : 1.0F);
                const FVector Local(FMath::Cos(Angle * PI / 6) * Radius, FMath::Sin(Angle * PI / 6) * Radius, 0);
                const FVector World = T.TransformPosition(Local);
                const double Ground = APacificCoastGraybox::GetCoastalLandHeight(World.X, World.Y);
                const double BuriedEnd = T.TransformPosition(Local - FVector(0, 0, A.BuriedDepth)).Z;
                if (Ground < World.Z - .5) ++Floating;
                // Uphill burial up to the collar top (~60 cm) still leaves the trunk passing through the surface.
                if (Ground > World.Z + 60 || Ground < BuriedEnd + 10) ++BuriedTrees;
            }
        if (!A.IsOverstory()) continue;
        const FVector2D P(Tree.Base);
        const TArray<FVector>& Route = FPacificCoastGrayboxContract::GetPlateauClearancePoints();
        for (int32 Segment = 1; Segment < Route.Num(); ++Segment)
        {
            const FVector2D RA(Route[Segment - 1]), RB(Route[Segment]), AB = RB - RA;
            const double S = FMath::Clamp(FVector2D::DotProduct(P - RA, AB) / AB.SizeSquared(), 0.0, 1.0);
            if (FVector2D::Distance(P, RA + AB * S) < 2500) ++RouteIntrusions;
        }
        if (APacificCoastGraybox::GetCliffEdgeX(P.Y) - P.X <= 2000) ++CliffIntrusions;
        if (BoardwalkDistance(P) <= 950) ++BoardwalkIntrusions;
    }
    const double InteriorHa = InteriorCells * 25.0 / 10000.0;
    const double TreesPerHa = InteriorTrees / FMath::Max(InteriorHa, .01);
    AddInfo(FString::Printf(TEXT("FLORA_DENSITY interior_ha=%.2f overstory_trees=%d trees_per_ha=%.0f stems_per_ha=%.0f near_total=%d mature=%d pole=%d sapling=%d distant=%d"),
        InteriorHa, InteriorTrees, TreesPerHa, InteriorStems / FMath::Max(InteriorHa, .01), NearAll, Mature, Pole, Sapling, Layout.DistantTrees));
    TestTrue(TEXT("Closed stand reaches the fixed 300-400 overstory trees/ha"), TreesPerHa >= 300 && TreesPerHa <= 400);
    TestTrue(TEXT("Forest zones cover several hectares"), InteriorHa > 4);
    TestTrue(TEXT("Age mix includes mature, pole and sapling trees"), Mature > Pole && Pole > 0 && Sapling > 0);
    TestEqual(TEXT("No collar floats above the terrain"), Floating, 0);
    TestEqual(TEXT("Terrain always crosses the trunk between collar and buried end"), BuriedTrees, 0);
    TestEqual(TEXT("Overstory trees clear the flight route by 25 m"), RouteIntrusions, 0);
    TestEqual(TEXT("Trees stand at least 20 m inland of the cliff"), CliffIntrusions, 0);
    TestEqual(TEXT("Trees clear the boardwalk footprint"), BoardwalkIntrusions, 0);
    TestEqual(TEXT("Graybox reports the near overstory count"), Coast->GetCoastalTreeCount(), Layout.NearOverstoryTrees);
    // Instancing replaces the former merged procedural batches.
    TArray<UProceduralMeshComponent*> Procedural; Coast->GetComponents(Procedural);
    for (const auto* Mesh : Procedural)
        TestFalse(TEXT("No merged procedural tree batches remain"), Mesh->GetName().StartsWith(TEXT("CoastalNeedles")) || Mesh->GetFName() == TEXT("CoastalBranchSkeleton"));
    int32 Instances = 0;
    for (const FArchetype& A : Archetypes)
    {
        const auto* Batch = CoastalTreeTest::FindBatch(Coast, A.Name);
        const auto* FarBatch = CoastalTreeTest::FindBatch(Coast, FName(*(A.Name.ToString() + TEXT("Far"))));
        if (!TestNotNull(TEXT("Each archetype has a near instanced component"), Batch) || !TestNotNull(TEXT("Each archetype has a far instanced component"), FarBatch)) continue;
        TestTrue(TEXT("Far woodland never renders LOD0"), FarBatch->bOverrideMinLOD && FarBatch->MinLOD >= 1);
        Instances += Batch->GetInstanceCount() + FarBatch->GetInstanceCount();
        TestEqual(TEXT("Visual tree instances never cook collision"), Batch->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
        TestTrue(TEXT("Bark and foliage material slots"), Batch->GetNumMaterials() == 2);
    }
    TestEqual(TEXT("Every placed tree is rendered exactly once"), Instances, Layout.Trees.Num());
    // Imported CC0 draft materials: masked two-sided foliage cards and photographed bark.
    for (const FArchetype& A : Archetypes)
    {
        const auto* Batch = CoastalTreeTest::FindBatch(Coast, A.Name);
        if (!Batch) continue;
        const UMaterialInterface* Bark = Batch->GetMaterial(0);
        const UMaterialInterface* Leaves = Batch->GetMaterial(1);
        if (!TestNotNull(TEXT("Bark material resolves"), Bark) || !TestNotNull(TEXT("Foliage material resolves"), Leaves)) continue;
        UTexture* BarkColor = nullptr; UTexture* Mask = nullptr;
        TestTrue(TEXT("Bark uses an imported CC0 bark scan"), Bark->GetTextureParameterValue(FMaterialParameterInfo(TEXT("BaseColorTexture")), BarkColor)
            && BarkColor && BarkColor->GetName().StartsWith(TEXT("T_Flora")) && BarkColor->GetName().Contains(TEXT("Bark")));
        TestEqual(TEXT("Foliage gaps use masked blending"), Leaves->GetBlendMode(), BLEND_Masked);
        TestTrue(TEXT("Thin cards render both sides"), Leaves->IsTwoSided());
        TestTrue(TEXT("Foliage alpha texture connected"), Leaves->GetTextureParameterValue(FMaterialParameterInfo(TEXT("OpacityTexture")), Mask) && Mask);
        if (Mask) TestFalse(TEXT("Opacity data is linear"), Mask->SRGB);
        float Coverage = 0;
        TestTrue(TEXT("Distance coverage bias stays below the clip value"), Leaves->GetScalarParameterValue(FMaterialParameterInfo(TEXT("DistantNeedleCoverage")), Coverage)
            && Coverage > 0 && Coverage < Leaves->GetOpacityMaskClipValue());
    }
    // Trunk-base litter: one terrain-aligned ring per near overstory tree, never colliding.
    if (const auto* Litter = CoastalTreeTest::FindBatch(Coast, TEXT("CoastalFloraTrunkLitter")))
    {
        TestTrue(TEXT("Litter rings at most one per near overstory tree, none in the corridor lane"), Litter->GetInstanceCount() <= Layout.NearOverstoryTrees && Litter->GetInstanceCount() > Layout.NearOverstoryTrees * 8 / 10);
        for (const FCorridorSample& Sample : Layout.Corridor)
            if (FMath::Fmod(Sample.Distance, 500.0F) < 60.0F)
                TestTrue(TEXT("No forest-floor litter on the corridor path"), APacificCoastGraybox::GetForestLitterMask(Sample.Point.X, Sample.Point.Y).R + APacificCoastGraybox::GetForestLitterMask(Sample.Point.X, Sample.Point.Y).G < .05F);
        TestEqual(TEXT("Litter rings never collide"), Litter->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
        int32 RingsAbove = 0;
        for (int32 Index = 0; Index < Litter->GetInstanceCount(); Index += 7)
        {
            FTransform T; Litter->GetInstanceTransform(Index, T, false);
            for (int32 Angle = 0; Angle < 8; ++Angle)
            {
                const FVector Edge = T.TransformPosition(FVector(FMath::Cos(Angle * PI / 4) * 70, FMath::Sin(Angle * PI / 4) * 70, 0));
                if (Edge.Z - APacificCoastGraybox::GetCoastalLandHeight(Edge.X, Edge.Y) > 12) ++RingsAbove;
            }
        }
        TestTrue(TEXT("Litter rings hug the ground (<2% of sampled points >12 cm above it)"), RingsAbove * 50 < Litter->GetInstanceCount() / 7 * 8 + 1);
        AddInfo(FString::Printf(TEXT("FLORA_LITTER rings=%d floating_samples=%d"), Litter->GetInstanceCount(), RingsAbove));
    }
    else AddError(TEXT("Trunk-base litter batch missing (run HabitatDraftImport)"));
    const auto* ProxyBatch = CoastalTreeTest::FindBatch(Coast, TEXT("CoastalTrunkCollisionProxies"));
    if (TestNotNull(TEXT("Trunk collision batch exists"), ProxyBatch))
    {
        TestFalse(TEXT("Collision proxies never render"), ProxyBatch->IsVisible());
        TestEqual(TEXT("Trunk proxies block collision"), ProxyBatch->GetCollisionEnabled(), ECollisionEnabled::QueryAndPhysics);
        TestTrue(TEXT("Every near overstory tree has at least two trunk collision segments"), ProxyBatch->GetInstanceCount() >= 2 * (Layout.NearOverstoryTrees));
        AddInfo(FString::Printf(TEXT("FLORA_COLLISION proxies=%d near_overstory=%d"), ProxyBatch->GetInstanceCount(), Layout.NearOverstoryTrees));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalTreeCorridorTest, "AeroSim.Coastal.Flora.TrainingCorridorClearance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCoastalTreeCorridorTest::RunTest(const FString& Parameters)
{
    using namespace CoastFlora;
    const APacificCoastGraybox* Coast = GetDefault<APacificCoastGraybox>();
    if (!TestNotNull(TEXT("Generated coast exists"), Coast)) return false;
    const FLayout& Layout = GetLayout();
    const TArray<CoastalTreeTest::FCylinder> Cylinders = CoastalTreeTest::Proxies(Coast);
    TestTrue(TEXT("Corridor has a real length (>300 m)"), Layout.Corridor.Num() > 2 && Layout.Corridor.Last().Distance > 30000);
    // Measure actual trunk-collision gaps across the corridor at 1.5 m and 4 m above ground.
    TArray<float> Gaps;
    int32 Blocked = 0, Constrictions = 0, Enclosed = 0;
    bool bInConstriction = false;
    for (const FCorridorSample& Sample : Layout.Corridor)
    {
        if (NearStandDensity(Sample.Point.X, Sample.Point.Y) <= .3F) continue; // in the open field
        const FVector2D Normal(-Sample.Tangent.Y, Sample.Tangent.X);
        float SampleGap = MAX_flt;
        for (const float Height : {150.0F, 400.0F})
        {
            const FVector P(Sample.Point.X, Sample.Point.Y, APacificCoastGraybox::GetCoastalLandHeight(Sample.Point.X, Sample.Point.Y) + Height);
            float Left = 1000, Right = 1000;
            for (const auto& C : Cylinders)
            {
                if (FVector::DistSquaredXY(C.A, P) > FMath::Square(1600.0F)) continue;
                const FVector Closest = FMath::ClosestPointOnSegment(P, C.A, C.B);
                if (FMath::Abs(Closest.Z - P.Z) > 1.0) continue; // segment does not span this height
                const FVector2D Offset(Closest.X - P.X, Closest.Y - P.Y);
                // Only trunks abreast of this sample define its cross-corridor gap.
                if (FMath::Abs(FVector2D::DotProduct(Offset, Sample.Tangent)) > 150) continue;
                const float Surface = Offset.Size() - C.Radius;
                if (Surface <= 0) ++Blocked;
                if (FVector2D::DotProduct(Offset, Normal) > 0) Left = FMath::Min(Left, Surface); else Right = FMath::Min(Right, Surface);
            }
            if (Left < 1000 && Right < 1000) SampleGap = FMath::Min(SampleGap, Left + Right);
        }
        if (SampleGap == MAX_flt) { bInConstriction = false; continue; }
        ++Enclosed;
        Gaps.Add(SampleGap);
        if (SampleGap <= 400 && !bInConstriction) ++Constrictions;
        bInConstriction = SampleGap <= 400;
    }
    if (!TestTrue(TEXT("Trunks enclose the corridor on both sides"), Gaps.Num() > 40)) return false;
    Gaps.Sort();
    const float Min = Gaps[0], Median = Gaps[Gaps.Num() / 2];
    AddInfo(FString::Printf(TEXT("FLORA_CORRIDOR length_m=%.0f enclosed_samples=%d min_gap_cm=%.0f median_gap_cm=%.0f max_gap_cm=%.0f constrictions_le_4m=%d blocked=%d"),
        Layout.Corridor.Last().Distance / 100.0F, Gaps.Num(), Min, Median, Gaps.Last(), Constrictions, Blocked));
    TestEqual(TEXT("No trunk collision intrudes into the corridor centreline"), Blocked, 0);
    TestTrue(TEXT("Minimum trunk gap is flyable (>= 2.5 m)"), Min >= 250);
    TestTrue(TEXT("Minimum trunk gap is challenging (<= 4 m)"), Min <= 400);
    TestTrue(TEXT("Several deliberate constrictions"), Constrictions >= 4);
    TestTrue(TEXT("Typical corridor width reads as 4-6 m between trunks"), Median >= 380 && Median <= 700);
    // Low crowns: the corridor's flanking conifers keep foliage above 4.5 m near the stem.
    const TArray<FArchetype>& Archetypes = GetArchetypes();
    for (const FPlacedTree& Tree : Layout.Trees)
        if (Tree.bCorridorFlank)
            TestTrue(TEXT("Corridor flank trees have a raised crown base"), Archetypes[Tree.Archetype].LowestFoliageNearStem * Tree.Scale >= 450);
    // Suggested `corridor` review pose: 3 m inside the mouth, 2.5 m above ground, looking down the run.
    {
        const FCorridorSample* Mouth = Layout.Corridor.FindByPredicate([](const FCorridorSample& S) { return S.Distance >= 300; });
        const FCorridorSample* Ahead = Layout.Corridor.FindByPredicate([](const FCorridorSample& S) { return S.Distance >= 2400; });
        if (Mouth && Ahead)
        {
            const FVector2D Look = (Ahead->Point - Mouth->Point).GetSafeNormal();
            AddInfo(FString::Printf(TEXT("FLORA_CORRIDOR_POSE location=(%.0f,%.0f,%.0f) pitch=-3 yaw=%.1f"), Mouth->Point.X, Mouth->Point.Y,
                APacificCoastGraybox::GetCoastalLandHeight(Mouth->Point.X, Mouth->Point.Y) + 250.0F, FMath::RadiansToDegrees(FMath::Atan2(Look.Y, Look.X))));
        }
    }
    // Legibility from the `corridor` review pose: a clear straight sightline and a thinned lane edge.
    {
        FVector Pose; FRotator PoseRotation;
        if (TestTrue(TEXT("Corridor review pose resolves"), AAeroSimGameMode::ResolveReviewView(TEXT("corridor"), 8, Pose, PoseRotation)))
        {
            const FVector2D Look(PoseRotation.Vector());
            float Sightline = 0;
            for (float Along = 100; Along <= 6000; Along += 50)
            {
                const FVector2D P = FVector2D(Pose) + Look.GetSafeNormal() * Along;
                bool bHit = false;
                for (const auto& C : Cylinders)
                    if (FVector2D::Distance(FVector2D(C.A), P) < C.Radius + 120) { bHit = true; break; }
                if (bHit) break;
                Sightline = Along;
            }
            AddInfo(FString::Printf(TEXT("FLORA_CORRIDOR_SIGHTLINE clear_m=%.1f (2.4 m wide probe, trunk collision)"), Sightline / 100.0F));
            TestTrue(TEXT("At least 40 m of open lane straight ahead of the corridor pose"), Sightline >= 4000);
        }
        int32 Edge = 0, EdgeCells = 0, Inner = 0, InnerCells = 0;
        for (const FCorridorSample& Sample : Layout.Corridor)
        {
            if (FMath::Fmod(Sample.Distance, 1000.0F) > 60 || NearStandDensity(Sample.Point.X, Sample.Point.Y) < .85F) continue;
            const FVector2D Normal(-Sample.Tangent.Y, Sample.Tangent.X);
            for (const FPlacedTree& Tree : Layout.Trees)
            {
                if (!Archetypes[Tree.Archetype].IsOverstory() || Tree.bCorridorFlank) continue;
                const FVector2D D = FVector2D(Tree.Base) - Sample.Point;
                if (FMath::Abs(FVector2D::DotProduct(D, Sample.Tangent)) > 500) continue;
                const float Across = FMath::Abs(FVector2D::DotProduct(D, Normal));
                if (Across > Sample.HalfWidth && Across < Sample.HalfWidth + 600) ++Edge;
                else if (Across > Sample.HalfWidth + 900 && Across < Sample.HalfWidth + 1500) ++Inner;
            }
            ++EdgeCells; ++InnerCells;
        }
        AddInfo(FString::Printf(TEXT("FLORA_CORRIDOR_EDGE fill_trees_edge_band=%d stand_band=%d"), Edge, Inner));
        TestTrue(TEXT("Stand fill thins toward the lane edge"), EdgeCells > 0 && Edge < Inner * .8F);
        TestTrue(TEXT("Worn path weight is 1 on the centreline and 0 off the lane"),
            FMath::IsNearlyEqual(APacificCoastGraybox::GetTrainingCorridorWear(Layout.Corridor[200].Point.X, Layout.Corridor[200].Point.Y), 1.0F)
            && APacificCoastGraybox::GetTrainingCorridorWear(Layout.Corridor[200].Point.X + 2000, Layout.Corridor[200].Point.Y + 2000) == 0);
        TArray<UProceduralMeshComponent*> Paths; Coast->GetComponents(Paths);
        TestTrue(TEXT("Worn corridor path geometry exists"), Paths.ContainsByPredicate([](UProceduralMeshComponent* M) { return M->GetFName() == TEXT("CoastalFloraCorridorPath") && M->GetProcMeshSection(0) && M->GetProcMeshSection(0)->ProcIndexBuffer.Num() > 1000; }));
    }
    // Trunk-base litter mask for the ground material (DEV-TERRAIN).
    {
        // A stand fir away from the corridor (lane trees deliberately carry no litter).
        const FPlacedTree* Fir = Layout.Trees.FindByPredicate([&](const FPlacedTree& T) { return T.bNearForest && !T.bCorridorFlank && Archetypes[T.Archetype].Species == ESpecies::DouglasFir && CorridorDistance(FVector2D(T.Base)) > 1500; });
        if (TestNotNull(TEXT("A near fir exists"), Fir))
        {
            const FLinearColor AtBase = APacificCoastGraybox::GetForestLitterMask(Fir->Base.X + Archetypes[Fir->Archetype].CollarRadius, Fir->Base.Y);
            // Patchy by design (0.55-1.0 of full strength), zero on the corridor path.
            TestTrue(TEXT("Conifer litter is present at the trunk base"), AtBase.R > .5F || APacificCoastGraybox::GetTrainingCorridorWear(Fir->Base.X, Fir->Base.Y) > 0);
        }
        TestEqual(TEXT("No litter in the open launch field"), APacificCoastGraybox::GetForestLitterMask(0, 0).R + APacificCoastGraybox::GetForestLitterMask(0, 0).G, 0.0F);
    }
    // The entry is visible from the fixed `forest` review pose.
    FVector Camera; FRotator Rotation;
    if (TestTrue(TEXT("Forest review pose resolves"), AAeroSimGameMode::ResolveReviewView(TEXT("forest"), 8, Camera, Rotation)))
    {
        const FVector2D Forward(Rotation.Vector());
        int32 Visible = 0;
        for (const FCorridorSample& Sample : Layout.Corridor)
        {
            const FVector2D To = Sample.Point - FVector2D(Camera);
            if (To.Size() < 500 || To.Size() > 4000) continue;
            if (FVector2D::DotProduct(To.GetSafeNormal(), Forward.GetSafeNormal()) > FMath::Cos(FMath::DegreesToRadians(40.0F))) ++Visible;
        }
        TestTrue(TEXT("Corridor entry lies inside the forest review frustum"), Visible > 20);
    }
    return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Materials/Material.h"
#include "MaterialShared.h"
#include "RHIGlobals.h"

// Shader gate: every DEV-FLORA draft material actually bound to the generated coast compiles
// for the running RHI (Metal when the editor runs without -NullRHI) and keeps ISM usage.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalFloraMaterialCompileTest, "AeroSim.Coastal.Flora.MaterialsCompileForRunningRHI",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCoastalFloraMaterialCompileTest::RunTest(const FString& Parameters)
{
    const APacificCoastGraybox* Coast = GetDefault<APacificCoastGraybox>();
    if (!TestNotNull(TEXT("Generated coast exists"), Coast)) return false;
    TSet<UMaterial*> Materials;
    TArray<UHierarchicalInstancedStaticMeshComponent*> Batches; Coast->GetComponents(Batches);
    for (const auto* Batch : Batches)
        if (Batch->GetName().StartsWith(TEXT("CoastalFlora")))
            for (int32 Slot = 0; Slot < Batch->GetNumMaterials(); ++Slot)
                if (UMaterialInterface* Interface = Batch->GetMaterial(Slot))
                    if (UMaterial* Material = Interface->GetMaterial(); Material && Material->GetName().Contains(TEXT("Draft"))) Materials.Add(Material);
    TestTrue(TEXT("All ten flora draft materials are bound"), Materials.Num() >= 10);
    const bool bNullRHI = !GDynamicRHI || FCString::Strcmp(GDynamicRHI->GetName(), TEXT("Null")) == 0;
    AddInfo(FString::Printf(TEXT("FLORA_SHADER_GATE rhi=%s"), GDynamicRHI ? GDynamicRHI->GetName() : TEXT("none")));
    int32 Compiled = 0;
    for (UMaterial* Material : Materials)
    {
        TestTrue(TEXT("Instanced static mesh usage is baked in"), Material->bUsedWithInstancedStaticMeshes);
        if (bNullRHI) continue;
        FMaterialResource* Resource = Material->GetMaterialResource(GMaxRHIShaderPlatform);
        if (!TestNotNull(TEXT("Material resource exists for the running shader platform"), Resource)) continue;
        Resource->FinishCompilation();
        for (const FString& Error : Resource->GetCompileErrors()) AddError(FString::Printf(TEXT("Failed to compile Material %s: %s"), *Material->GetName(), *Error));
        TestTrue(TEXT("Material has a complete game-thread shader map"), Resource->GetGameThreadShaderMap() != nullptr);
        ++Compiled;
    }
    AddInfo(FString::Printf(TEXT("FLORA_SHADER_GATE compiled=%d of %d (0 under NullRHI)"), Compiled, Materials.Num()));
    return true;
}
#endif
