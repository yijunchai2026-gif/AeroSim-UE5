#include "PacificCoastGraybox.h"
#include "PacificCoastFlora.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "UObject/ConstructorHelpers.h"
#include "ProceduralMeshComponent.h"

float APacificCoastGraybox::GetTrainingCorridorWear(const float X, const float Y)
{
    const float Distance = CoastFlora::CorridorDistance(FVector2D(X, Y));
    // 1.6 m trodden centre with a 0.8 m soft shoulder on each side.
    return 1.0F - FMath::SmoothStep(80.0F, 160.0F, Distance);
}

FLinearColor APacificCoastGraybox::GetForestLitterMask(const float X, const float Y)
{
    using namespace CoastFlora;
    struct FCells { TMap<FIntPoint, TArray<int32>> Map; };
    static const FCells Cells = []
    {
        FCells Result;
        const FLayout& Layout = GetLayout();
        for (int32 Index = 0; Index < Layout.Trees.Num(); ++Index)
            Result.Map.FindOrAdd(FIntPoint(FMath::FloorToInt(Layout.Trees[Index].Base.X / 500), FMath::FloorToInt(Layout.Trees[Index].Base.Y / 500))).Add(Index);
        return Result;
    }();
    const FLayout& Layout = GetLayout();
    const TArray<FArchetype>& Archetypes = GetArchetypes();
    float Conifer = 0, Oak = 0;
    const FIntPoint Key(FMath::FloorToInt(X / 500), FMath::FloorToInt(Y / 500));
    for (int32 DX = -1; DX <= 1; ++DX) for (int32 DY = -1; DY <= 1; ++DY)
        if (const TArray<int32>* Items = Cells.Map.Find(Key + FIntPoint(DX, DY)))
            for (int32 Item : *Items)
            {
                const FPlacedTree& Tree = Layout.Trees[Item];
                const FArchetype& A = Archetypes[Tree.Archetype];
                const bool bOak = A.Species == ESpecies::CoastLiveOak;
                // R9: contact-scale reach so neighbouring trees no longer merge into one dark carpet.
                const float Reach = (bOak ? 320.0F : A.Species == ESpecies::Sapling ? 80.0F : 240.0F) * Tree.Scale * Tree.Girth;
                const float D = FVector2D::Distance(FVector2D(X, Y), FVector2D(Tree.Base)) - A.CollarRadius * Tree.Scale * Tree.Girth;
                const float W = FMath::Pow(FMath::Clamp(1.0F - D / Reach, 0.0F, 1.0F), 1.5F);
                (bOak ? Oak : Conifer) = FMath::Max(bOak ? Oak : Conifer, W);
            }
    // Keep the training lane and its worn path free of litter (path stays readable), and break the
    // remaining floor litter into patches so bare duff/soil shows between trees.
    float HalfWidth = 0;
    const float Lane = CorridorDistance(FVector2D(X, Y), &HalfWidth);
    const float LaneClear = FMath::SmoothStep(HalfWidth - 60.0F, HalfWidth + 80.0F, Lane);
    const float CoarsePatch = FMath::PerlinNoise2D(FVector2D(X / 3600.0F + 5.7F, Y / 3600.0F - 2.1F));
    const float FinePatch = FMath::PerlinNoise2D(FVector2D(X / 1400.0F - 8.3F, Y / 1400.0F + 4.6F));
    const float Patch = FMath::SmoothStep(.34F, .68F, .5F + .5F * (.68F * CoarsePatch + .32F * FinePatch));
    const float Keep = LaneClear * FMath::Lerp(.80F, 1.0F, Patch);
    return FLinearColor(Conifer * Keep, Oak * Keep, 0, 1);
}

// Forest-floor understory for the near stands: sword-fern-like clumps under closed
// canopy, coyote-brush-like shrubs and dry grass at feathered edges and glades.
// Every plant is an instance of an imported CC0 draft mesh (no merged geometry).
// The training corridor, flight route, trails, boardwalk and launch path stay clear.
void APacificCoastGraybox::BuildCoastalHabitat()
{
    using namespace CoastFlora;
    FString Commandlet;
    const bool bImporting = FParse::Value(FCommandLine::Get(), TEXT("run="), Commandlet)
        && (Commandlet.Equals(TEXT("HabitatDraftImport"), ESearchCase::IgnoreCase) || Commandlet.Equals(TEXT("CoastDraftImport"), ESearchCase::IgnoreCase));
    if (bImporting || !FPackageName::DoesPackageExist(TEXT("/Game/Development/CoastDraft/Meshes/SM_FloraFernADraft"))) return;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> FernA(TEXT("/Game/Development/CoastDraft/Meshes/SM_FloraFernADraft.SM_FloraFernADraft"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> FernB(TEXT("/Game/Development/CoastDraft/Meshes/SM_FloraFernBDraft.SM_FloraFernBDraft"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> BrushA(TEXT("/Game/Development/CoastDraft/Meshes/SM_FloraRooibosADraft.SM_FloraRooibosADraft"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> BrushB(TEXT("/Game/Development/CoastDraft/Meshes/SM_FloraRooibosBDraft.SM_FloraRooibosBDraft"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ShrubA(TEXT("/Game/Development/CoastDraft/Meshes/SM_CoastShrubADraft.SM_CoastShrubADraft"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ShrubB(TEXT("/Game/Development/CoastDraft/Meshes/SM_CoastShrubBDraft.SM_CoastShrubBDraft"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ForbA(TEXT("/Game/Development/CoastDraft/Meshes/SM_CoastAnnualForbADraft.SM_CoastAnnualForbADraft"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ForbB(TEXT("/Game/Development/CoastDraft/Meshes/SM_CoastAnnualForbBDraft.SM_CoastAnnualForbBDraft"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ForbC(TEXT("/Game/Development/CoastDraft/Meshes/SM_CoastAnnualForbCDraft.SM_CoastAnnualForbCDraft"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> GrassTall(TEXT("/Game/Development/CoastDraft/Meshes/SM_CoastGrassTallDraft.SM_CoastGrassTallDraft"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> GrassSmall(TEXT("/Game/Development/CoastDraft/Meshes/SM_CoastGrassSmallDraft.SM_CoastGrassSmallDraft"));
    struct FKind { const TCHAR* Name; UStaticMesh* Mesh; float MinScale, MaxScale, CullStart, CullEnd; bool bShadow; float MinClearHalfWidthOffset; };
    const FKind Kinds[] = {
        {TEXT("CoastalFloraUnderstoryFernA"), FernA.Object, 1.3F, 2.0F, 4000, 5500, true, 40},
        {TEXT("CoastalFloraUnderstoryFernB"), FernB.Object, 1.4F, 2.0F, 5000, 7000, true, 40},
        {TEXT("CoastalFloraUnderstoryBrushA"), BrushA.Object, 2.0F, 3.4F, 4500, 6000, true, 140},
        {TEXT("CoastalFloraUnderstoryBrushB"), BrushB.Object, 2.0F, 3.0F, 7000, 9000, true, 140},
        {TEXT("CoastalFloraUnderstoryShrubA"), ShrubA.Object, 2.4F, 3.8F, 6000, 8000, true, 120},
        {TEXT("CoastalFloraUnderstoryShrubB"), ShrubB.Object, 2.4F, 3.6F, 6000, 8000, true, 120},
        {TEXT("CoastalFloraGroundGrassTall"), GrassTall.Object, 1.6F, 2.8F, 2500, 3500, false, -400},
        {TEXT("CoastalFloraGroundGrassSmall"), GrassSmall.Object, 2.0F, 3.4F, 2200, 3000, false, -400},
        {TEXT("CoastalFloraAnnualForbA"), ForbA.Object, 2.0F, 3.0F, 1000, 1600, false, -400},
        {TEXT("CoastalFloraAnnualForbB"), ForbB.Object, 2.2F, 3.2F, 1000, 1600, false, -400},
        {TEXT("CoastalFloraAnnualForbC"), ForbC.Object, 2.4F, 3.5F, 1000, 1600, false, -400}};
    constexpr int32 KindCount = UE_ARRAY_COUNT(Kinds);
    TArray<FTransform> Instances[KindCount];
    // Trunk exclusion uses the real placed trees.
    const FLayout& Layout = GetLayout();
    const TArray<FArchetype>& Archetypes = GetArchetypes();
    TMap<FIntPoint, TArray<int32>> TreeCells;
    for (int32 Index = 0; Index < Layout.Trees.Num(); ++Index)
        if (Layout.Trees[Index].bNearForest)
            TreeCells.FindOrAdd(FIntPoint(FMath::FloorToInt(Layout.Trees[Index].Base.X / 500), FMath::FloorToInt(Layout.Trees[Index].Base.Y / 500))).Add(Index);
    auto NearTrunk = [&](const FVector2D& P, float Margin)
    {
        const FIntPoint Key(FMath::FloorToInt(P.X / 500), FMath::FloorToInt(P.Y / 500));
        for (int32 DX = -1; DX <= 1; ++DX) for (int32 DY = -1; DY <= 1; ++DY)
            if (const TArray<int32>* Items = TreeCells.Find(Key + FIntPoint(DX, DY)))
                for (int32 Item : *Items)
                {
                    const FPlacedTree& Tree = Layout.Trees[Item];
                    if (FVector2D::Distance(P, FVector2D(Tree.Base)) < Archetypes[Tree.Archetype].CollarRadius * Tree.Scale * Tree.Girth + Margin) return true;
                }
        return false;
    };
    FRandomStream Random(55021);
    // ~1 shrub/fern per 10 m² of stand (QA R0 target), grass only where light reaches the floor.
    constexpr float Cell = 300.0F;
    for (float GX = -52000; GX < 12500; GX += Cell)
        for (float GY = -48000; GY < 48000; GY += Cell)
        {
            const FVector2D P(GX + Random.FRandRange(0, Cell), GY + Random.FRandRange(0, Cell));
            const float Pick = Random.FRand(), Roll = Random.FRand(), ScaleRoll = Random.FRand(), Yaw = Random.FRandRange(0, 360);
            const float Density = NearStandDensity(P.X, P.Y);
            const FCoastHabitatSample Habitat = FPacificCoastHabitat::Sample(P.X, P.Y);
            // Grassland annual forbs stay on open or feathered, moderately drained ground;
            // closed redwood/Douglas-fir proxy canopy remains fern and duff dominated.
            const bool bForbGround = Habitat.Rock + Habitat.Shore < .25F && Habitat.Drainage < .72F;
            const int32 ForbKind = 8 + Random.RandRange(0, 2);
            // Grass fringe extends ~12 m out from the stand edge into the field.
            const float Fringe = Density > 0 ? 0 : NearStandDensity(P.X, P.Y + (P.Y > 0 ? 1200 : -1200));
            int32 Kind;
            if (Density <= 0 && Fringe <= 0)
            {
                // Sparse exposed-grass tufts continue across open ground so large fields retain
                // parallax without turning the whole landscape into a costly dense instance carpet.
                if (Roll > .04F) continue;
                Kind = bForbGround && Pick > .62F ? ForbKind : 7;
            }
            else if (Density <= 0)
            {
                if (Roll > .55F) continue;
                Kind = bForbGround && Pick > .64F ? ForbKind : Pick < .34F ? 6 : 7;
            }
            else if (Density > .75F)
            {   // Closed stand: ferns dominate, scattered broad shrubs, little grass.
                if (Roll > .92F) continue;
                Kind = Pick < .62F ? (Pick < .36F ? 0 : 1) : Pick < .80F ? (Pick < .71F ? 4 : 5) : Pick < .88F ? 2 : 7;
            }
            else
            {   // Feathered edge and glades: coyote-brush-like shrubs and dry grass.
                if (Roll > .95F) continue;
                Kind = bForbGround && Pick > .82F ? ForbKind
                    : Pick < .40F ? (Pick < .24F ? 2 : 3) : Pick < .55F ? 0 : Pick < .78F ? 6 : 7;
            }
            const FKind& K = Kinds[Kind];
            if (!K.Mesh) continue;
            if (IsProtected(P, Kind >= 6 ? 250 : 330)) continue;
            float HalfWidth = 0;
            if (CorridorDistance(P, &HalfWidth) < HalfWidth + K.MinClearHalfWidthOffset) continue;
            if (NearTrunk(P, Kind >= 6 ? 15 : 45)) continue;
            const float Scale = FMath::Lerp(K.MinScale, K.MaxScale, ScaleRoll);
            const FVector Base(P.X, P.Y, GetCoastalLandHeight(P.X, P.Y) - 3.0F);
            Instances[Kind].Add(FTransform(FRotator(0, Yaw, 0), Base, FVector(Scale)));
        }
    // Near-camera ground cover around the launch worksite and route plateau (deferred to
    // DEV-FLORA by DEV-TERRAIN): dense tufts within ~25 m thinning out to 60 m, kept off the
    // apron/mat, the access path, trails and boardwalk; species follow the habitat weights.
    FRandomStream Ground(77113);
    constexpr float GrassCell = 65.0F;
    for (float GX = -6000; GX < 6000; GX += GrassCell)
        for (float GY = -6000; GY < 6000; GY += GrassCell)
        {
            const FVector2D P(GX + Ground.FRandRange(0, GrassCell), GY + Ground.FRandRange(0, GrassCell));
            const float Roll = Ground.FRand(), Pick = Ground.FRand(), ScaleRoll = Ground.FRand(), Yaw = Ground.FRandRange(0, 360);
            const float Distance = P.Size();
            if (Distance > 6000 || Distance < 950) continue; // keep the 14 x 12 m work surface clear
            if (Roll > FMath::Lerp(.88F, .24F, FMath::Clamp((Distance - 2500.0F) / 3500.0F, 0.0F, 1.0F))) continue;
            if (P.X > GetCliffEdgeX(P.Y) - 400 || IsWalkway(P, 180) || IsProtected(P, 250)) continue;
            float HalfWidth = 0;
            if (CorridorDistance(P, &HalfWidth) < HalfWidth + 160) continue;
            const FCoastHabitatSample Habitat = FPacificCoastHabitat::Sample(P.X, P.Y);
            if (Habitat.Rock + Habitat.Shore > .4F) continue;
            const float ScrubChance = FMath::SmoothStep(1800.0F, 5200.0F, Distance) * FMath::Lerp(.08F, .20F, 1.0F - Habitat.ExposedGrass);
            const bool bScrub = Pick < ScrubChance;
            const bool bAnnualForb = !bScrub && Habitat.Rock + Habitat.Shore < .22F && Habitat.Drainage < .72F && Pick > .82F;
            const int32 Kind = bScrub ? (Ground.RandRange(0, 1) == 0 ? 2 : Ground.RandRange(3, 5))
                : bAnnualForb ? 8 + Ground.RandRange(0, 2) : Pick < Habitat.ExposedGrass * .8F ? 6 : 7;
            const FKind& K = Kinds[Kind];
            if (!K.Mesh) continue;
            const float Scale = bScrub ? FMath::Lerp(K.MinScale * .68F, K.MaxScale * .88F, ScaleRoll)
                : FMath::Lerp(K.MinScale * 1.08F, K.MaxScale * 1.18F, ScaleRoll);
            Instances[Kind].Add(FTransform(FRotator(0, Yaw, 0), FVector(P.X, P.Y, GetCoastalLandHeight(P.X, P.Y) - 3.0F), FVector(Scale)));
        }
    // Worn corridor floor: a terrain-following path ribbon (1.6 m core, soft 0.8 m shoulders)
    // down the training lane so the route reads from the `corridor` pose. Same gravel/earth
    // blend material as the forest trail; vertex alpha 0 at the shoulders, 255 on the core.
    if (FPackageName::DoesPackageExist(TEXT("/Game/Development/CoastDraft/Materials/M_CoastGravelDraft")))
    {
        static ConstructorHelpers::FObjectFinder<UMaterialInterface> Gravel(TEXT("/Game/Development/CoastDraft/Materials/M_CoastGravelDraft.M_CoastGravelDraft"));
        TArray<FVector> Positions; TArray<int32> Triangles; TArray<FVector> Normals; TArray<FVector2D> UV; TArray<FLinearColor> Colors;
        const float Offsets[] = {-160, -80, 0, 80, 160};
        int32 Rows = 0;
        float Previous = -100;
        for (const FCorridorSample& Sample : Layout.Corridor)
        {
            if (Sample.Distance - Previous < 45.0F) continue;
            Previous = Sample.Distance;
            const FVector2D Normal(-Sample.Tangent.Y, Sample.Tangent.X);
            // The path narrows and wanders slightly so it does not read as a paved strip.
            const float Wobble = 18.0F * FMath::Sin(Sample.Distance / 900.0F) + 10.0F * FMath::Sin(Sample.Distance / 310.0F + 1.7F);
            const float Width = .85F + .20F * FMath::Sin(Sample.Distance / 1300.0F + .4F);
            for (int32 Column = 0; Column < 5; ++Column)
            {
                const FVector2D P = Sample.Point + Normal * (Offsets[Column] * Width + Wobble);
                const bool bCore = Column >= 1 && Column <= 3;
                Positions.Add(FVector(P.X, P.Y, GetCoastalLandHeight(P.X, P.Y) + (bCore ? .8F : -.2F)));
                const float DX = (GetCoastalLandHeight(P.X + 10, P.Y) - GetCoastalLandHeight(P.X - 10, P.Y)) / 20;
                const float DY = (GetCoastalLandHeight(P.X, P.Y + 10) - GetCoastalLandHeight(P.X, P.Y - 10)) / 20;
                Normals.Add(FVector(-DX, -DY, 1).GetSafeNormal());
                UV.Add(FVector2D(P.X / 4000.0F, P.Y / 4000.0F));
                // RGB: the exact habitat ground weights beneath (shoulders vanish into the terrain);
                // A: worn core (same convention as the forest trail and launch apron).
                FLinearColor Cover = FPacificCoastHabitat::GetGroundVertexColor(P.X, P.Y);
                Cover.A = bCore ? (Column == 2 ? 1.0F : .72F) : 0.0F;
                Colors.Add(Cover);
            }
            if (Rows > 0)
                for (int32 Column = 0; Column < 4; ++Column)
                {
                    const int32 A = (Rows - 1) * 5 + Column, B = A + 1, C = A + 5, D = C + 1;
                    // Upward-facing in Unreal's clockwise convention.
                    if (FVector::CrossProduct(Positions[C] - Positions[A], Positions[B] - Positions[A]).Z > 0) { Triangles.Append({A, B, C}); Triangles.Append({B, D, C}); }
                    else { Triangles.Append({A, C, B}); Triangles.Append({B, C, D}); }
                }
            ++Rows;
        }
        UProceduralMeshComponent* Path = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("CoastalFloraCorridorPath"));
        Path->SetupAttachment(Root);
        Path->CreateMeshSection_LinearColor(0, Positions, Triangles, Normals, UV, Colors, TArray<FProcMeshTangent>(), false);
        Path->SetMaterial(0, Gravel.Object);
        Path->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Path->SetCastShadow(false);
        // R9: brighter, warmer worn-earth core so the path reads under the closed canopy
        // (ApplyCoastalMaterials turns this tint into the material's Color parameter).
        CoastalColorParts.Add(Path); CoastalPartColors.Add(FLinearColor(1.45F, 1.30F, 1.05F));
        ++GeneratedPrimitiveCount;
    }
    // Trunk-base litter rings (needles under conifers, leaf litter under oaks): a terrain-aligned
    // masked disc whose radial alpha fades into the ground. DEV-TERRAIN may also blend the ground
    // itself via GetForestLitterMask.
    if (FPackageName::DoesPackageExist(TEXT("/Game/Development/CoastDraft/Materials/M_FloraTrunkLitterDraft")))
    {
        static ConstructorHelpers::FObjectFinder<UMaterialInterface> LitterMaterial(TEXT("/Game/Development/CoastDraft/Materials/M_FloraTrunkLitterDraft.M_FloraTrunkLitterDraft"));
        UHierarchicalInstancedStaticMeshComponent* Litter = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("CoastalFloraTrunkLitter"));
        Litter->SetupAttachment(Root);
        Litter->SetStaticMesh(GetTrunkLitterMesh());
        Litter->SetMaterial(0, LitterMaterial.Object);
        Litter->SetMaterial(1, LitterMaterial.Object);
        Litter->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Litter->SetCollisionProfileName(TEXT("NoCollision"));
        Litter->SetCastShadow(false);
        Litter->bAffectDistanceFieldLighting = false;
        Litter->SetCullDistances(6000, 8000);
        TArray<FTransform> Rings;
        FRandomStream LitterRandom(91921);
        for (const FPlacedTree& Tree : Layout.Trees)
        {
            const FArchetype& A = Archetypes[Tree.Archetype];
            if (!Tree.bNearForest || !A.IsOverstory()) continue;
            {
                float LaneHalf = 0;
                if (CorridorDistance(FVector2D(Tree.Base), &LaneHalf) < LaneHalf + 150.0F) continue; // no rug in the lane
            }
            // Contact-scale rings only (DEV-TERRAIN's ground litter mask carries the continuous forest floor).
            const float Radius = (A.Species == ESpecies::CoastLiveOak ? LitterRandom.FRandRange(300, 480) : LitterRandom.FRandRange(220, 380)) * Tree.Scale * Tree.Girth;
            const float H = Radius * .5F;
            const float DX = (GetCoastalLandHeight(Tree.Base.X + H, Tree.Base.Y) - GetCoastalLandHeight(Tree.Base.X - H, Tree.Base.Y)) / (2 * H);
            const float DY = (GetCoastalLandHeight(Tree.Base.X, Tree.Base.Y + H) - GetCoastalLandHeight(Tree.Base.X, Tree.Base.Y - H)) / (2 * H);
            const FVector Normal = FVector(-DX, -DY, 1).GetSafeNormal();
            const FQuat Align = FQuat::FindBetweenNormals(FVector::UpVector, Normal) * FQuat(FVector::UpVector, LitterRandom.FRandRange(0, 2 * PI));
            const FVector Centre(Tree.Base.X, Tree.Base.Y, GetCoastalLandHeight(Tree.Base.X, Tree.Base.Y) + 2.0F);
            // Irregular outline: unequal X/Y radii per ring.
            Rings.Add(FTransform(Align, Centre, FVector(Radius / 100.0F, Radius / 100.0F * LitterRandom.FRandRange(.7F, 1.0F), 1)));
        }
        Litter->AddInstances(Rings, false);
        Litter->ComponentTags.Add(TEXT("CoastalFloraHabitat"));
        ++GeneratedPrimitiveCount;
    }
    int32 Total = 0;
    for (int32 Kind = 0; Kind < KindCount; ++Kind)
    {
        if (!Kinds[Kind].Mesh || Instances[Kind].IsEmpty()) continue;
        UHierarchicalInstancedStaticMeshComponent* Batch = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(Kinds[Kind].Name);
        Batch->SetupAttachment(Root);
        Batch->SetStaticMesh(Kinds[Kind].Mesh);
        Batch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Batch->SetCollisionProfileName(TEXT("NoCollision"));
        Batch->SetCastShadow(Kinds[Kind].bShadow);
        Batch->bAffectDistanceFieldLighting = false;
        Batch->SetCullDistances(Kinds[Kind].CullStart, Kinds[Kind].CullEnd);
        Batch->AddInstances(Instances[Kind], false);
        Batch->ComponentTags.Add(TEXT("CoastalFloraHabitat"));
        Total += Instances[Kind].Num();
        ++GeneratedPrimitiveCount;
    }
    UE_LOG(LogTemp, Verbose, TEXT("Coastal habitat instances: %d"), Total);
}
