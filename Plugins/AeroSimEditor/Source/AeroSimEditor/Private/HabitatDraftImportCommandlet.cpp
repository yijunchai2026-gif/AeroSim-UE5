#include "HabitatDraftImportCommandlet.h"
#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Dom/JsonObject.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Factories/TextureFactory.h"
#include "HAL/PlatformFileManager.h"
#include "IAssetTools.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionPerInstanceFadeAmount.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshCompiler.h"
#include "StaticMeshResources.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace HabitatDraft
{
const FString Destination(TEXT("/Game/Development/CoastDraft/"));
const TCHAR* MeshNames[]={TEXT("SM_CoastGrassSmallDraft"),TEXT("SM_CoastGrassTallDraft"),TEXT("SM_CoastGrassTinyDraft"),TEXT("SM_CoastShrubADraft"),TEXT("SM_CoastShrubBDraft")};
bool Save(UObject* Object)
{
    if(!Object || !Object->GetOutermost()->GetName().StartsWith(Destination))return false;
    const FString Filename=FPackageName::LongPackageNameToFilename(Object->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
    if(!FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(Filename)))return false;
    Object->MarkPackageDirty();FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
    return UPackage::SavePackage(Object->GetOutermost(),Object,*Filename,Args);
}
template<class T>T* Node(UMaterial* M){return CastChecked<T>(UMaterialEditingLibrary::CreateMaterialExpression(M,T::StaticClass()));}
UMaterial* Material(const TCHAR* Name,const TArray<UTexture2D*>& Maps)
{
    UPackage* P=CreatePackage(*(Destination+TEXT("Materials/")+Name));P->FullyLoad();
    UMaterial* M=FindObject<UMaterial>(P,Name);const bool New=!M;
    if(New)M=NewObject<UMaterial>(P,Name,RF_Public|RF_Standalone);
    M->PreEditChange(nullptr);
    TArray<UMaterialExpression*> Old;for(const TObjectPtr<UMaterialExpression>& E:M->GetExpressions())Old.Add(E.Get());
    for(auto* E:Old)UMaterialEditingLibrary::DeleteMaterialExpression(M,E);
    M->BlendMode=BLEND_Masked;M->TwoSided=true;M->SetShadingModel(MSM_TwoSidedFoliage);M->OpacityMaskClipValue=.33F;M->DitheredLODTransition=true;
    bool NeedsRecompile=false;M->SetMaterialUsage(NeedsRecompile,MATUSAGE_InstancedStaticMeshes);
    const TCHAR* Names[]={TEXT("BaseColorTexture"),TEXT("AlphaTexture"),TEXT("NormalTexture"),TEXT("RoughnessTexture")};
    UMaterialExpressionTextureSampleParameter2D* Samples[4];
    for(int32 I=0;I<4;++I){Samples[I]=Node<UMaterialExpressionTextureSampleParameter2D>(M);Samples[I]->ParameterName=Names[I];Samples[I]->Texture=Maps[I];Samples[I]->SamplerType=I==0?SAMPLERTYPE_Color:I==2?SAMPLERTYPE_Normal:SAMPLERTYPE_LinearGrayscale;}
    UMaterialEditingLibrary::ConnectMaterialProperty(Samples[0],TEXT("RGB"),MP_BaseColor);
    UMaterialEditingLibrary::ConnectMaterialProperty(Samples[2],TEXT("RGB"),MP_Normal);
    UMaterialEditingLibrary::ConnectMaterialProperty(Samples[3],TEXT("R"),MP_Roughness);
    auto* Fade=Node<UMaterialExpressionPerInstanceFadeAmount>(M);auto* Mask=Node<UMaterialExpressionMultiply>(M);
    UMaterialEditingLibrary::ConnectMaterialExpressions(Samples[1],TEXT("R"),Mask,TEXT("A"));UMaterialEditingLibrary::ConnectMaterialExpressions(Fade,TEXT(""),Mask,TEXT("B"));
    UMaterialEditingLibrary::ConnectMaterialProperty(Mask,TEXT(""),MP_OpacityMask);
    auto* Half=Node<UMaterialExpressionConstant>(M);Half->R=.32F;auto* Sub=Node<UMaterialExpressionMultiply>(M);
    UMaterialEditingLibrary::ConnectMaterialExpressions(Samples[0],TEXT("RGB"),Sub,TEXT("A"));UMaterialEditingLibrary::ConnectMaterialExpressions(Half,TEXT(""),Sub,TEXT("B"));
    UMaterialEditingLibrary::ConnectMaterialProperty(Sub,TEXT(""),MP_SubsurfaceColor);
    if(New)FAssetRegistryModule::AssetCreated(M);M->PostEditChange();UMaterialEditingLibrary::RecompileMaterial(M);
    return Save(M)?M:nullptr;
}
bool Mesh(const TCHAR* Name,UMaterial* M)
{
    FString Text;const FString File=FPaths::ProjectDir()/TEXT("SourceAssets/EnvironmentDraft/HabitatDraft/")+(FString(Name)+TEXT(".mesh.json"));
    if(!FFileHelper::LoadFileToString(Text,*File) || Text.Len()>2000000)return false;
    TSharedPtr<FJsonObject> Json;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Json)||!Json.IsValid()||Json->GetStringField(TEXT("schema"))!=TEXT("aerosim.habitat-mesh/v1")||Json->GetStringField(TEXT("asset"))!=Name)return false;
    const auto& Points=Json->GetArrayField(TEXT("positions"));const auto& Normals=Json->GetArrayField(TEXT("normals"));const auto& UV=Json->GetArrayField(TEXT("uv"));const auto& Indices=Json->GetArrayField(TEXT("indices"));
    if(Points.Num()<3||Points.Num()>5000||Points.Num()!=Normals.Num()||Points.Num()!=UV.Num()||Indices.Num()>15000||Indices.Num()%3)return false;
    FMeshDescription Description;FStaticMeshAttributes Attributes(Description);Attributes.Register();Attributes.GetVertexInstanceUVs().SetNumChannels(1);
    TArray<FVertexID> Vertices;
    for(const auto& P:Points){const auto& A=P->AsArray();if(A.Num()!=3)return false;const FVector3f Position(A[0]->AsNumber(),A[1]->AsNumber(),A[2]->AsNumber());if(Position.ContainsNaN()||Position.GetAbsMax()>500)return false;FVertexID V=Description.CreateVertex();Attributes.GetVertexPositions()[V]=Position;Vertices.Add(V);}
    const FPolygonGroupID Group=Description.CreatePolygonGroup();Attributes.GetPolygonGroupMaterialSlotNames()[Group]=TEXT("Foliage");
    for(int32 I=0;I<Indices.Num();I+=3)
    {
        TArray<FVertexInstanceID> Corners;
        for(int32 C=0;C<3;++C)
        {
            const int32 Index=Indices[I+C]->AsNumber();if(!Vertices.IsValidIndex(Index))return false;
            const auto& N=Normals[Index]->AsArray();const auto& T=UV[Index]->AsArray();if(N.Num()!=3||T.Num()!=2)return false;
            const FVector3f Normal=FVector3f(N[0]->AsNumber(),N[1]->AsNumber(),N[2]->AsNumber()).GetSafeNormal();
            if(Normal.ContainsNaN()||Normal.IsNearlyZero())return false;
            FVertexInstanceID Instance=Description.CreateVertexInstance(Vertices[Index]);Attributes.GetVertexInstanceNormals()[Instance]=Normal;
            Attributes.GetVertexInstanceUVs().Set(Instance,0,FVector2f(T[0]->AsNumber(),T[1]->AsNumber()));Attributes.GetVertexInstanceColors()[Instance]=FVector4f(1,1,1,1);Corners.Add(Instance);
        }
        Description.CreatePolygon(Group,Corners);
    }
    UPackage* P=CreatePackage(*(Destination+TEXT("Meshes/")+Name));P->FullyLoad();UStaticMesh* S=FindObject<UStaticMesh>(P,Name);const bool New=!S;
    if(New)S=NewObject<UStaticMesh>(P,Name,RF_Public|RF_Standalone);
    S->SetNumSourceModels(0);S->GetStaticMaterials().Reset();S->GetStaticMaterials().Add(FStaticMaterial(M,TEXT("Foliage")));S->SetAutoComputeLODScreenSize(false);
    const int32 LODCount=FString(Name).Contains(TEXT("Tiny"))?1:3;
    for(int32 LOD=0;LOD<LODCount;++LOD)
    {
        auto& Source=S->AddSourceModel();Source.BuildSettings.bRecomputeNormals=false;Source.BuildSettings.bRecomputeTangents=true;Source.BuildSettings.bUseMikkTSpace=true;Source.BuildSettings.bGenerateLightmapUVs=false;
        Source.ReductionSettings.PercentTriangles=LOD==0?1.0F:LOD==1?.5F:.2F;Source.ScreenSize.Default=LOD==0?.10F:LOD==1?.025F:.006F;
    }
    S->CreateMeshDescription(0,MoveTemp(Description));S->CommitMeshDescription(0);S->Build(false);
    TArray<UStaticMesh*> Compiling{S};FStaticMeshCompilingManager::Get().FinishCompilation(Compiling);
    if(S->GetNumLODs()!=LODCount)return false;
    for(int32 LOD=0;LOD<LODCount;++LOD)UE_LOG(LogTemp,Display,TEXT("HABITAT_MESH %s lod=%d triangles=%u"),Name,LOD,S->GetRenderData()->LODResources[LOD].GetNumTriangles());
    if(New)FAssetRegistryModule::AssetCreated(S);S->PostEditChange();return Save(S);
}
}
UHabitatDraftImportCommandlet::UHabitatDraftImportCommandlet(){IsClient=false;IsEditor=true;IsServer=false;LogToConsole=true;}
int32 UHabitatDraftImportCommandlet::Main(const FString& Params)
{
    using namespace HabitatDraft;
    UMaterial* Materials[2]={nullptr,nullptr};auto& Tools=FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
    const TCHAR* Folders[]={TEXT("grass_medium_01"),TEXT("shrub_03")};const TCHAR* Prefixes[]={TEXT("DryGrass"),TEXT("BroadShrub")};const TCHAR* Roles[]={TEXT("BaseColor"),TEXT("Alpha"),TEXT("Normal"),TEXT("Roughness")};const TCHAR* Files[]={TEXT("basecolor.jpg"),TEXT("alpha.jpg"),TEXT("normal.jpg"),TEXT("roughness.jpg")};
    for(int32 Plant=0;Plant<2;++Plant)
    {
        TArray<UTexture2D*> Maps;
        for(int32 Map=0;Map<4;++Map)
        {
            UAssetImportTask* Task=NewObject<UAssetImportTask>();Task->Filename=FPaths::ProjectDir()/TEXT("SourceAssets/EnvironmentDraft/HabitatDraft/")/Folders[Plant]/Files[Map];Task->DestinationPath=Destination+TEXT("Textures");Task->DestinationName=FString(TEXT("T_Coast"))+Prefixes[Plant]+TEXT("_")+Roles[Map];Task->Factory=NewObject<UTextureFactory>();Task->bAutomated=true;Task->bReplaceExisting=true;Task->bReplaceExistingSettings=true;
            Tools.Get().ImportAssetTasks({Task});const auto& Objects=Task->GetObjects();UTexture2D* Texture=Objects.Num()==1?Cast<UTexture2D>(Objects[0]):nullptr;if(!Texture)return 3;
            Texture->SRGB=Map==0;Texture->CompressionSettings=Map==2?TC_Normalmap:Map==0?TC_Default:TC_Grayscale;Texture->bFlipGreenChannel=false;
            Texture->bDoScaleMipsForAlphaCoverage=Map==1;if(Map==1)Texture->AlphaCoverageThresholds=FVector4(.33F,0,0,0);
            if(Texture->AssetImportData){FString Relative=Task->Filename;FPaths::MakePathRelativeTo(Relative,*FPaths::ProjectDir());Texture->AssetImportData->UpdateFilenameOnly(Relative);}
            Texture->PostEditChange();if(!Save(Texture))return 4;Maps.Add(Texture);
        }
        Materials[Plant]=Material(Plant==0?TEXT("M_CoastDryGrassDraft"):TEXT("M_CoastBroadShrubDraft"),Maps);if(!Materials[Plant])return 5;
    }
    for(int32 I=0;I<5;++I)if(!Mesh(MeshNames[I],Materials[I<3?0:1]))return 6;
    UE_LOG(LogTemp,Display,TEXT("HABITAT_IMPORT_COMPLETE textures=8 materials=2 meshes=5 state=DRAFT_NOT_USER_CONFIRMED"));return 0;
}
