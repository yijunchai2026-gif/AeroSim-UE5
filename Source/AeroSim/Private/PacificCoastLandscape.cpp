#include "PacificCoastGraybox.h"

#include "Components/SceneComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/PackageName.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

void APacificCoastGraybox::BuildCoastalLandscape()
{
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> SolidFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> CloudFinder(TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst"));

    BuildCoastalTerrain(SolidFinder.Object);
    BuildCoastalTrees(SolidFinder.Object);
    BuildCoastalHabitat();
    BuildCoastalOcean(SolidFinder.Object);
    // This isolated development asset set is authored by the offline Editor importer.
    // Bootstrap remains buildable before those fixed packages have been generated.
    FString ImportCommandlet;
    const bool bDraftImport = FParse::Value(FCommandLine::Get(), TEXT("run="), ImportCommandlet)
        && ImportCommandlet.Equals(TEXT("CoastDraftImport"), ESearchCase::IgnoreCase);
    if (!bDraftImport && FPackageName::DoesPackageExist(TEXT("/Game/Development/CoastDraft/Materials/M_CoastGroundDraft")))
    {
        static ConstructorHelpers::FObjectFinder<UMaterialInterface> Ground(TEXT("/Game/Development/CoastDraft/Materials/M_CoastGroundDraft.M_CoastGroundDraft"));
        static ConstructorHelpers::FObjectFinder<UMaterialInterface> Rock(TEXT("/Game/Development/CoastDraft/Materials/M_CoastRockDraft.M_CoastRockDraft"));
        static ConstructorHelpers::FObjectFinder<UMaterialInterface> Needles(TEXT("/Game/Development/CoastDraft/Materials/M_CoastNeedlesDraft.M_CoastNeedlesDraft"));
        static ConstructorHelpers::FObjectFinder<UMaterialInterface> Bark(TEXT("/Game/Development/CoastDraft/Materials/M_CoastBarkDraft.M_CoastBarkDraft"));
        for (int32 Index = 0; Index < CoastalColorParts.Num(); ++Index)
        {
            UMeshComponent* Mesh = CoastalColorParts[Index];
            const FString Name = Mesh->GetName();
            UMaterialInterface* Material = nullptr;
            FLinearColor Tint = FLinearColor::White;
            if (Name == TEXT("CoastalPlateau") || Name == TEXT("CoastalMountainScrub")
                || Name == TEXT("CoastalMountainRock") || Name == TEXT("CoastalDistantRange")) Material = Ground.Object;
            else if (Name == TEXT("CoastalBedrock")
                || Name == TEXT("CoastalFracturedRock") || Name == TEXT("CoastalTalusButtresses")
                || Name == TEXT("CoastalSubtidalShelf") || Name == TEXT("CoastalHeroCliffScan"))
            {
                Material = Rock.Object;
                Tint = FLinearColor(.58F,.64F,.68F);
            }
            else if (Name.StartsWith(TEXT("CoastalNeedles")))
            {
                Material = Needles.Object;
                Tint = FLinearColor(.82F + (Index%3)*.05F,.90F,.82F + (Index%2)*.05F);
            }
            else if (Name == TEXT("CoastalBranchSkeleton")) Material = Bark.Object;
            if (Material != nullptr)
            {
                Mesh->SetMaterial(0, Material);
                CoastalPartColors[Index] = Tint;
            }
        }
    }
    if (!bDraftImport && FPackageName::DoesPackageExist(TEXT("/Game/Development/CoastDraft/Materials/M_CoastWaterDraft")))
    {
        static ConstructorHelpers::FObjectFinder<UMaterialInterface> Water(TEXT("/Game/Development/CoastDraft/Materials/M_CoastWaterDraft.M_CoastWaterDraft"));
        TArray<UProceduralMeshComponent*> Meshes;
        GetComponents(Meshes);
        for (UProceduralMeshComponent* Mesh : Meshes)
            if (Mesh->GetFName() == TEXT("PacificOcean") && Water.Object != nullptr) Mesh->SetMaterial(0, Water.Object);
    }
    if(!bDraftImport && FPackageName::DoesPackageExist(TEXT("/Game/Development/CoastDraft/Materials/M_CoastGravelDraft")))
    {
        static ConstructorHelpers::FObjectFinder<UMaterialInterface> Gravel(TEXT("/Game/Development/CoastDraft/Materials/M_CoastGravelDraft.M_CoastGravelDraft"));
        CoastalGravelMaterial=Gravel.Object;
    }
    CoastalClouds = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("CoastalClouds"));
    CoastalClouds->SetupAttachment(Root);
    CoastalClouds->SetMaterial(CloudFinder.Object);
    CoastalClouds->SetLayerBottomAltitude(1.2F);
    CoastalClouds->SetLayerHeight(3.0F);
    CoastalClouds->SetViewSampleCountScale(.5F);
    CoastalClouds->SetShadowViewSampleCountScale(.25F);
}

void APacificCoastGraybox::ApplyCoastalMaterials()
{
    for (int32 Index = 0; Index < CoastalColorParts.Num(); ++Index)
    {
        UMeshComponent* Mesh = CoastalColorParts[Index];
        if (Mesh == nullptr || Mesh->GetMaterial(0) == nullptr) continue;
        const bool bWorkedGround=Mesh->GetFName()==TEXT("CoastalLaunchApron")||Mesh->GetFName()==TEXT("CoastalLaunchAccessTrail")||Mesh->GetFName()==TEXT("CoastalForestTrail");
        if(CoastalGravelMaterial && bWorkedGround)
            Mesh->SetMaterial(0,CoastalGravelMaterial);
        if(bWorkedGround)
        {
            // Shoulders must blend into exactly the habitat layers of the terrain beneath them:
            // rewrite their RGB habitat weights from the shared field, keeping the worn-core alpha.
            UProceduralMeshComponent* Surface=Cast<UProceduralMeshComponent>(Mesh);
            if(FProcMeshSection* Section=Surface ? Surface->GetProcMeshSection(0) : nullptr)
            {
                for(FProcMeshVertex& Vertex:Section->ProcVertexBuffer)
                {
                    FColor Habitat=FPacificCoastHabitat::GetGroundVertexColor(Vertex.Position.X,Vertex.Position.Y).ToFColor(false);
                    Habitat.A=Vertex.Color.A;Vertex.Color=Habitat;
                }
                Surface->SetProcMeshSection(0,*Section);
            }
        }
        UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Mesh->GetMaterial(0), this);
        Material->SetVectorParameterValue(TEXT("Color"), Mesh->GetMaterial(0)==CoastalGravelMaterial ? FLinearColor(.72F,.74F,.72F) : CoastalPartColors[Index]);
        Material->SetScalarParameterValue(TEXT("Roughness"), .95F);
        // Trails read as compacted earth paths with grass shoulders; the launch apron stays gravel.
        Material->SetScalarParameterValue(TEXT("PathEarth"), Mesh->GetFName()==TEXT("CoastalLaunchApron") ? 0.0F : 1.0F);
        Mesh->SetMaterial(0, Material);
    }
    TArray<UProceduralMeshComponent*> Meshes;
    GetComponents(Meshes);
    for (UProceduralMeshComponent* Mesh : Meshes)
    {
        if (Mesh->GetFName() != TEXT("PacificOcean") || Mesh->GetMaterial(0) == nullptr) continue;
        UMaterialInstanceDynamic* Water = UMaterialInstanceDynamic::Create(Mesh->GetMaterial(0), this);
        FLinearColor ExistingColor;
        if (Water->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Color")), ExistingColor))
            Water->SetVectorParameterValue(TEXT("Color"), FLinearColor(.009F,.065F,.105F));
        Water->SetScalarParameterValue(TEXT("Roughness"), .24F);
        Mesh->SetMaterial(0, Water);
        CoastalWaterMaterial = Water;
        if (!CoastalColorParts.IsEmpty())
        {
            UMaterialInstanceDynamic* Fallback = UMaterialInstanceDynamic::Create(CoastalWaterFallback != nullptr ? CoastalWaterFallback.Get() : Mesh->GetMaterial(0), this);
            Fallback->SetVectorParameterValue(TEXT("Color"), FLinearColor(.008F,.045F,.065F));
            Fallback->SetScalarParameterValue(TEXT("Roughness"), .4F);
            CoastalWaterFallback = Fallback;
        }
    }
}

void APacificCoastGraybox::SetCoastalQuality(const bool bHigh)
{
    if (CoastalClouds != nullptr) CoastalClouds->SetVisibility(bHigh);
    TArray<UProceduralMeshComponent*> Meshes;
    GetComponents(Meshes);
    for (UProceduralMeshComponent* Mesh : Meshes)
    {
        if (Mesh->GetFName() == TEXT("PacificOcean") && CoastalWaterMaterial != nullptr)
            Mesh->SetMaterial(0, bHigh || CoastalWaterFallback == nullptr ? CoastalWaterMaterial.Get() : CoastalWaterFallback.Get());
        if (Mesh->GetName().StartsWith(TEXT("CoastalNeedles"))) Mesh->SetCastShadow(bHigh);
    }
}
