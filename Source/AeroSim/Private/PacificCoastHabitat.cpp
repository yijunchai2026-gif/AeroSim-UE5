#include "PacificCoastGraybox.h"
#include "PacificCoastFlora.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "UObject/ConstructorHelpers.h"

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
    static ConstructorHelpers::FObjectFinder<UStaticMesh> GrassTall(TEXT("/Game/Development/CoastDraft/Meshes/SM_CoastGrassTallDraft.SM_CoastGrassTallDraft"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> GrassSmall(TEXT("/Game/Development/CoastDraft/Meshes/SM_CoastGrassSmallDraft.SM_CoastGrassSmallDraft"));
    struct FKind { const TCHAR* Name; UStaticMesh* Mesh; float MinScale, MaxScale, CullStart, CullEnd; bool bShadow; float MinClearHalfWidthOffset; };
    const FKind Kinds[] = {
        {TEXT("CoastalFloraUnderstoryFernA"), FernA.Object, 1.3F, 2.0F, 4000, 5500, true, 40},
        {TEXT("CoastalFloraUnderstoryFernB"), FernB.Object, 1.4F, 2.0F, 5000, 7000, true, 40},
        {TEXT("CoastalFloraUnderstoryBrushA"), BrushA.Object, 2.0F, 3.4F, 4500, 6000, true, 60},
        {TEXT("CoastalFloraUnderstoryBrushB"), BrushB.Object, 2.0F, 3.0F, 7000, 9000, true, 60},
        {TEXT("CoastalFloraUnderstoryShrubA"), ShrubA.Object, 2.4F, 3.8F, 6000, 8000, true, 60},
        {TEXT("CoastalFloraUnderstoryShrubB"), ShrubB.Object, 2.4F, 3.6F, 6000, 8000, true, 60},
        {TEXT("CoastalFloraGroundGrassTall"), GrassTall.Object, 1.6F, 2.8F, 2500, 3500, false, -400},
        {TEXT("CoastalFloraGroundGrassSmall"), GrassSmall.Object, 2.0F, 3.4F, 2200, 3000, false, -400}};
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
                    if (FVector2D::Distance(P, FVector2D(Tree.Base)) < Archetypes[Tree.Archetype].CollarRadius * Tree.Scale + Margin) return true;
                }
        return false;
    };
    FRandomStream Random(55021);
    // ~1 shrub/fern per 10 m² of stand (QA R0 target), grass only where light reaches the floor.
    constexpr float Cell = 300.0F;
    for (float GX = -31000; GX < 12500; GX += Cell)
        for (float GY = -28000; GY < 28000; GY += Cell)
        {
            const FVector2D P(GX + Random.FRandRange(0, Cell), GY + Random.FRandRange(0, Cell));
            const float Pick = Random.FRand(), Roll = Random.FRand(), ScaleRoll = Random.FRand(), Yaw = Random.FRandRange(0, 360);
            const float Density = NearStandDensity(P.X, P.Y);
            // Grass fringe extends ~12 m out from the stand edge into the field.
            const float Fringe = Density > 0 ? 0 : NearStandDensity(P.X, P.Y + (P.Y > 0 ? 1200 : -1200));
            if (Density <= 0 && Fringe <= 0) continue;
            int32 Kind;
            if (Density <= 0) { if (Roll > .55F) continue; Kind = Pick < .55F ? 6 : 7; }
            else if (Density > .75F)
            {   // Closed stand: ferns dominate, scattered broad shrubs, little grass.
                if (Roll > .92F) continue;
                Kind = Pick < .62F ? (Pick < .36F ? 0 : 1) : Pick < .80F ? (Pick < .71F ? 4 : 5) : Pick < .88F ? 2 : 7;
            }
            else
            {   // Feathered edge and glades: coyote-brush-like shrubs and dry grass.
                if (Roll > .95F) continue;
                Kind = Pick < .40F ? (Pick < .24F ? 2 : 3) : Pick < .55F ? 0 : Pick < .78F ? 6 : 7;
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
