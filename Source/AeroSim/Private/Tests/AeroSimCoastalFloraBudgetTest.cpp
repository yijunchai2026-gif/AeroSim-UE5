#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AeroSimGameMode.h"
#include "PacificCoastGraybox.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ConvexVolume.h"
#include "Engine/StaticMesh.h"
#include "ProceduralMeshComponent.h"
#include "SceneManagement.h"
#include "StaticMeshResources.h"

namespace CoastalFloraBudget
{
// CPU-side estimate of the geometry submitted for one fixed review camera:
// component/instance frustum culling, draw distances and static-mesh LOD
// selection by projected screen size. No occlusion culling is assumed, so the
// figures are an upper bound. Metal draw calls cannot be measured (see ADR).
struct FViewBudget { int64 Triangles = 0, FloraTriangles = 0; int32 Components = 0, FloraComponents = 0, FloraInstances = 0; };

bool IsFlora(const UPrimitiveComponent* Component)
{
    const FString Name = Component->GetName();
    return Name == TEXT("CoastalBranchSkeleton") || Name.StartsWith(TEXT("CoastalNeedles")) || Name.StartsWith(TEXT("CoastalFlora"));
}

FViewBudget Measure(const AActor* Coast, const FString& View, const float VerticalFovDegrees = 60.0F, const float Aspect = 16.0F / 9.0F)
{
    FViewBudget Result;
    FVector Location; FRotator Rotation;
    if (!AAeroSimGameMode::ResolveReviewView(View, 8, Location, Rotation)) return Result;
    const float HalfHorizontal = FMath::Atan(FMath::Tan(FMath::DegreesToRadians(VerticalFovDegrees * .5F)) * Aspect);
    const FMatrix ViewMatrix = FInverseRotationMatrix(Rotation) * FMatrix(FPlane(0,0,1,0), FPlane(1,0,0,0), FPlane(0,1,0,0), FPlane(0,0,0,1));
    const FMatrix Projection = FReversedZPerspectiveMatrix(HalfHorizontal, Aspect, 1.0F, 10.0F);
    const FMatrix ViewProjection = FTranslationMatrix(-Location) * ViewMatrix * Projection;
    FConvexVolume Frustum;
    GetViewFrustumBounds(Frustum, ViewProjection, false);
    TArray<UPrimitiveComponent*> Components;
    Coast->GetComponents(Components);
    for (UPrimitiveComponent* Component : Components)
    {
        if (!Component->IsVisible() || Component->bHiddenInGame) continue;
        const bool bFlora = IsFlora(Component);
        const float MaxDraw = Component->LDMaxDrawDistance > 0 ? Component->LDMaxDrawDistance : MAX_flt;
        const FTransform ComponentTransform = Component->GetRelativeTransform();
        int64 Added = 0;
        int32 VisibleInstances = 0;
        if (const UInstancedStaticMeshComponent* Instanced = Cast<UInstancedStaticMeshComponent>(Component))
        {
            const UStaticMesh* Mesh = Instanced->GetStaticMesh();
            if (Mesh == nullptr || Mesh->GetRenderData() == nullptr) continue;
            const FStaticMeshRenderData* Data = Mesh->GetRenderData();
            const FBoxSphereBounds LocalBounds = Mesh->GetBounds();
            const float EndCull = Instanced->InstanceEndCullDistance > 0 ? Instanced->InstanceEndCullDistance : MAX_flt;
            for (int32 Index = 0; Index < Instanced->GetInstanceCount(); ++Index)
            {
                FTransform Instance;
                Instanced->GetInstanceTransform(Index, Instance, false);
                const FBoxSphereBounds Bounds = LocalBounds.TransformBy(Instance * ComponentTransform);
                const double Distance = FVector::Distance(Bounds.Origin, Location);
                if (Distance - Bounds.SphereRadius > FMath::Min(EndCull, MaxDraw)) continue;
                if (!Frustum.IntersectBox(Bounds.Origin, Bounds.BoxExtent)) continue;
                const float ScreenSize = ComputeBoundsScreenSize(FVector4(Bounds.Origin, 1), Bounds.SphereRadius, FVector4(Location, 1), Projection);
                // Mirrors ComputeStaticMeshLOD: the coarsest LOD whose threshold exceeds the projected size.
                int32 Lod = 0;
                for (int32 Candidate = 1; Candidate < Data->LODResources.Num(); ++Candidate)
                    if (ScreenSize <= Data->ScreenSize[Candidate].Default) Lod = Candidate;
                Added += Data->LODResources[Lod].GetNumTriangles();
                ++VisibleInstances;
            }
        }
        else if (const UProceduralMeshComponent* Procedural = Cast<UProceduralMeshComponent>(Component))
        {
            for (int32 Section = 0; Section < Procedural->GetNumSections(); ++Section)
            {
                const FProcMeshSection* Data = const_cast<UProceduralMeshComponent*>(Procedural)->GetProcMeshSection(Section);
                if (Data == nullptr || !Data->bSectionVisible || Data->ProcIndexBuffer.IsEmpty()) continue;
                const FBox Box = Data->SectionLocalBox.TransformBy(ComponentTransform);
                if (FVector::Distance(Box.GetCenter(), Location) - Box.GetExtent().Size() > MaxDraw) continue;
                if (Frustum.IntersectBox(Box.GetCenter(), Box.GetExtent())) Added += Data->ProcIndexBuffer.Num() / 3;
            }
        }
        else if (const UStaticMeshComponent* Static = Cast<UStaticMeshComponent>(Component))
        {
            const UStaticMesh* Mesh = Static->GetStaticMesh();
            if (Mesh == nullptr || Mesh->GetRenderData() == nullptr || Mesh->GetRenderData()->LODResources.IsEmpty()) continue;
            const FBoxSphereBounds Bounds = Mesh->GetBounds().TransformBy(ComponentTransform);
            if (Frustum.IntersectBox(Bounds.Origin, Bounds.BoxExtent)) Added += Mesh->GetRenderData()->LODResources[0].GetNumTriangles();
        }
        if (Added == 0) continue;
        Result.Triangles += Added;
        ++Result.Components;
        if (bFlora && View == TEXT("forest")) UE_LOG(LogTemp, Display, TEXT("FLORA_FRUSTUM_PART view=%s component=%s instances=%d triangles=%lld"), *View, *Component->GetName(), VisibleInstances, Added);
        if (bFlora) { Result.FloraTriangles += Added; ++Result.FloraComponents; Result.FloraInstances += VisibleInstances; }
    }
    return Result;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalFloraFrustumBudgetTest,
    "AeroSim.Coastal.Flora.FrustumGeometryBudget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoastalFloraFrustumBudgetTest::RunTest(const FString& Parameters)
{
    const APacificCoastGraybox* Coast = GetDefault<APacificCoastGraybox>();
    if (!TestNotNull(TEXT("Generated coast exists"), Coast)) return false;
    const TCHAR* Views[] = {TEXT("forest"), TEXT("overview"), TEXT("mountain"), TEXT("bridge"), TEXT("launch-low"), TEXT("ground")};
    for (const TCHAR* View : Views)
    {
        const CoastalFloraBudget::FViewBudget Budget = CoastalFloraBudget::Measure(Coast, View);
        AddInfo(FString::Printf(TEXT("FLORA_FRUSTUM view=%s components=%d triangles=%lld flora_components=%d flora_instances=%d flora_triangles=%lld"),
            View, Budget.Components, Budget.Triangles, Budget.FloraComponents, Budget.FloraInstances, Budget.FloraTriangles));
        TestTrue(TEXT("View sees some generated geometry"), Budget.Triangles > 0);
        // R0 baseline (merged procedural batches): 3,451,597 triangles in the forest view,
        // 2,941,043 of them tree batches visible from every view regardless of distance.
        TestTrue(TEXT("Flora frustum geometry is below the merged-mesh baseline"), Budget.FloraTriangles < 2941043 * 6 / 10);
    }
    return true;
}
#endif
