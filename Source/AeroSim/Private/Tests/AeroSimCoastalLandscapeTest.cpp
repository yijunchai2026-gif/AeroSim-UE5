#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PacificCoastGraybox.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Texture.h"
#include "ProceduralMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalSceneSupportTest, "AeroSim.Environment.CoastalCliff.SupportAndRouteSetback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCoastalSceneSupportTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Sea lies 65 m below the plateau"), FMath::IsNearlyEqual(APacificCoastGraybox::SeaLevelCm, -6500.0F));
    for (float Y = -60000; Y <= 60000; Y += 500)
    {
        const float Edge = APacificCoastGraybox::GetCliffEdgeX(Y);
        TestTrue(TEXT("Cliff boundary is finite and east of all route gates"), FMath::IsFinite(Edge) && Edge > 12500);
    }
    for (const FVector& Point : FPacificCoastGrayboxContract::GetRoutePoints())
    {
        TestTrue(TEXT("Flight route retains a level supported plateau"), FMath::IsNearlyZero(APacificCoastGraybox::GetCoastalLandHeight(Point.X, Point.Y)));
        TestTrue(TEXT("Route remains at least 30 m inland"), APacificCoastGraybox::GetCliffEdgeX(Point.Y) - Point.X > 3000);
    }
    const APacificCoastGraybox* Coast = GetDefault<APacificCoastGraybox>();
    // DEV-FLORA: tree count, density and instancing are asserted in AeroSim.Coastal.Flora.*
    TestTrue(TEXT("Coastal forest contains generated trees"), Coast != nullptr && Coast->GetCoastalTreeCount() > 240);
    if (Coast == nullptr) return false;
    TArray<UProceduralMeshComponent*> Meshes;
    Coast->GetComponents(Meshes);
    UMaterialInterface* Material = Meshes.IsEmpty() ? nullptr : Meshes[0]->GetMaterial(0);
    for (UProceduralMeshComponent* Mesh : Meshes)
    {
        if (Mesh->GetFName() != TEXT("CoastalPlateau")) continue;
        FProcMeshSection* Section = Mesh->GetProcMeshSection(0);
        if (!TestTrue(TEXT("Plateau has real topology"), Section != nullptr && Section->ProcIndexBuffer.Num() >= 3)) return false;
        const auto& A = Section->ProcVertexBuffer[Section->ProcIndexBuffer[0]];
        const auto& B = Section->ProcVertexBuffer[Section->ProcIndexBuffer[1]];
        const auto& C = Section->ProcVertexBuffer[Section->ProcIndexBuffer[2]];
        TestTrue(TEXT("Land is front-facing from above using Unreal winding"), FVector::CrossProduct(B.Position-A.Position,C.Position-A.Position).Z < 0);
        TestTrue(TEXT("Land lighting normal points upward"), A.Normal.Z > 0);
    }
    for (UProceduralMeshComponent* Mesh : Meshes)
    {
        if (Mesh->GetFName() != TEXT("PacificOcean")) continue;
        const UMaterialInterface* Water = Mesh->GetMaterial(0);
        if (!TestNotNull(TEXT("Cook-reference water material actually resolves"), Water)) return false;
        TestTrue(TEXT("Ocean uses the actual SingleLayerWater volume shading model"), Water->GetMaterial()->GetShadingModels().HasShadingModel(MSM_SingleLayerWater));
        UTexture* WindNormalA = nullptr;
        UTexture* WindNormalB = nullptr;
        float WaterRoughness;
        TestTrue(TEXT("Ocean has its first real wind normal texture"), Water->GetTextureParameterValue(FMaterialParameterInfo(TEXT("WaterNormalA")),WindNormalA) && WindNormalA != nullptr);
        TestTrue(TEXT("Ocean has its second real wind normal texture"), Water->GetTextureParameterValue(FMaterialParameterInfo(TEXT("WaterNormalB")),WindNormalB) && WindNormalB != nullptr);
        TestTrue(TEXT("Ocean has controllable physical roughness"), Water->GetScalarParameterValue(FMaterialParameterInfo(TEXT("Roughness")),WaterRoughness));
        const FProcMeshSection* Section = Mesh->GetProcMeshSection(0);
        TestTrue(TEXT("Water extends beyond all terrain boundaries"), Section != nullptr && Section->SectionLocalBox.Min.X < -1900000 && Section->SectionLocalBox.Max.Y > 1900000);
    }
    TArray<UStaticMeshComponent*> StaticMeshes;Coast->GetComponents(StaticMeshes);
    int32 Supports=0,Ramps=0;bool DeckFound=false;
    for(const auto* Mesh:StaticMeshes)
    {
        if(Mesh->GetFName()==TEXT("BoardwalkDeck"))
        {
            DeckFound=true;
            TestTrue(TEXT("Short crossing is26m long and2.2m wide"),Mesh->GetRelativeScale3D().Equals(FVector(26,2.2F,.16F),.001F));
        }
        if(Mesh->GetName().StartsWith(TEXT("WalkRamp_")))
        {
            ++Ramps;
            const FTransform T=Mesh->GetRelativeTransform();
            const double A=T.TransformPosition(FVector(-50,0,50)).Z;
            const double B=T.TransformPosition(FVector(50,0,50)).Z;
            TestTrue(TEXT("Actual sloped ramp meets the deck and dirt path"), FMath::Abs(FMath::Max(A,B)-34.5)<.2 && FMath::Abs(FMath::Min(A,B)-1)<.2);
        }
        if(Mesh->GetName().StartsWith(TEXT("WalkPost_")))
        {
            ++Supports;const FVector P=Mesh->GetRelativeLocation();
            const double Bottom=P.Z-Mesh->GetRelativeScale3D().Z*50;
            TestTrue(TEXT("Bridge supports penetrate the real swale bed"),Bottom<APacificCoastGraybox::GetCoastalLandHeight(P.X,P.Y)-30);
            const double Top=P.Z+Mesh->GetRelativeScale3D().Z*50;
            TestTrue(TEXT("Guardrail top stays at human scale"),Top<150&&Top>120);
        }
    }
    TestEqual(TEXT("Both bridge approaches have actual sloped ramps"),Ramps,2);
    TestTrue(TEXT("Actual short boardwalk deck exists"),DeckFound);TestEqual(TEXT("Two rows of eight grounded posts"),Supports,16);
    bool TrailFound=false;
    for(auto* Mesh:Meshes)
    {
        if(Mesh->GetFName()!=TEXT("CoastalForestTrail"))continue;
        TrailFound=true;
        TestNotNull(TEXT("Trail uses the scanned terrain material"),Mesh->GetMaterial(0));
        const FProcMeshSection* Section=Mesh->GetProcMeshSection(0);
        if(!TestNotNull(TEXT("Continuous trail contains real mesh geometry"),Section))return false;
        for(const auto& Vertex:Section->ProcVertexBuffer)
        {
            const auto& P=Vertex.Position;
            const double Offset=P.Z-APacificCoastGraybox::GetCoastalLandHeight(P.X,P.Y);
            if(Offset<-.25||Offset>.85){AddError(TEXT("Trail floats above or cuts through the ground"));break;}
        }
    }
    TestTrue(TEXT("Bridge connects to an at-grade continuous trail"),TrailFound);
    // DEV-FLORA: foliage material checks moved to AeroSim.Coastal.Flora.FoliageMaterials (instanced archetypes).
    float Roughness;
    TestTrue(TEXT("Solid coast material exposes physical roughness control"), Material != nullptr && Material->GetScalarParameterValue(FMaterialParameterInfo(TEXT("Roughness")),Roughness));
    FLinearColor Color;
    TestTrue(TEXT("Actual engine prototype material exposes the Color parameter"), Material != nullptr && Material->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Color")), Color));
    return true;
}
#endif
