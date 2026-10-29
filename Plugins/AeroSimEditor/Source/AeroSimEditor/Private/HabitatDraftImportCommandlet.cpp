#include "HabitatDraftImportCommandlet.h"
#include "HeroCliffDraftImport.h"
#include "Misc/Parse.h"
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
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionPerInstanceFadeAmount.h"
#include "Materials/MaterialExpressionPerInstanceRandom.h"
#include "Materials/MaterialExpressionPixelDepth.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionWorldPosition.h"
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

// Fixed, whitelisted CC0 intake for the coastal habitat and tree archetypes.
// Every output is an isolated DRAFT_NOT_USER_CONFIRMED development package under
// /Game/Development/CoastDraft; source paths and names are compile-time constants.
namespace HabitatDraft
{
const FString Destination(TEXT("/Game/Development/CoastDraft/"));
enum class ERole : uint8 { Color, Alpha, Normal, Gray, Masks };
struct FHabitatTextureSource { const TCHAR* Path; const TCHAR* Name; ERole Role; };
const FHabitatTextureSource Textures[] = {
    {TEXT("HabitatDraft/grass_medium_01/basecolor.jpg"), TEXT("T_CoastDryGrass_BaseColor"), ERole::Color},
    {TEXT("HabitatDraft/grass_medium_01/alpha.jpg"), TEXT("T_CoastDryGrass_Alpha"), ERole::Alpha},
    {TEXT("HabitatDraft/grass_medium_01/normal.jpg"), TEXT("T_CoastDryGrass_Normal"), ERole::Normal},
    {TEXT("HabitatDraft/grass_medium_01/roughness.jpg"), TEXT("T_CoastDryGrass_Roughness"), ERole::Gray},
    {TEXT("HabitatDraft/shrub_03/basecolor.jpg"), TEXT("T_CoastBroadShrub_BaseColor"), ERole::Color},
    {TEXT("HabitatDraft/shrub_03/alpha.jpg"), TEXT("T_CoastBroadShrub_Alpha"), ERole::Alpha},
    {TEXT("HabitatDraft/shrub_03/normal.jpg"), TEXT("T_CoastBroadShrub_Normal"), ERole::Normal},
    {TEXT("HabitatDraft/shrub_03/roughness.jpg"), TEXT("T_CoastBroadShrub_Roughness"), ERole::Gray},
    // Low annual-forb proxy for open coastal grassland; CC0 Poly Haven weed_plant_02.
    {TEXT("HabitatDraft/weed_plant_02/basecolor.jpg"), TEXT("T_CoastAnnualForb_BaseColor"), ERole::Color},
    {TEXT("HabitatDraft/weed_plant_02/alpha.jpg"), TEXT("T_CoastAnnualForb_Alpha"), ERole::Alpha},
    {TEXT("HabitatDraft/weed_plant_02/normal.jpg"), TEXT("T_CoastAnnualForb_Normal"), ERole::Normal},
    {TEXT("HabitatDraft/weed_plant_02/roughness.jpg"), TEXT("T_CoastAnnualForb_Roughness"), ERole::Gray},
    {TEXT("HabitatDraft/dry_branches_medium_01/basecolor.jpg"), TEXT("T_CoastDeadwood_BaseColor"), ERole::Color},
    {TEXT("HabitatDraft/dry_branches_medium_01/normal.jpg"), TEXT("T_CoastDeadwood_Normal"), ERole::Normal},
    {TEXT("HabitatDraft/dry_branches_medium_01/roughness.jpg"), TEXT("T_CoastDeadwood_Roughness"), ERole::Gray},
    {TEXT("HabitatDraft/shrub_02/basecolor.jpg"), TEXT("T_CoastalScrub_BaseColor"), ERole::Color},
    {TEXT("HabitatDraft/shrub_02/alpha.jpg"), TEXT("T_CoastalScrub_Alpha"), ERole::Alpha},
    {TEXT("HabitatDraft/shrub_02/normal.jpg"), TEXT("T_CoastalScrub_Normal"), ERole::Normal},
    {TEXT("HabitatDraft/shrub_02/roughness.jpg"), TEXT("T_CoastalScrub_Roughness"), ERole::Gray},
    {TEXT("FloraDraft/pine_bark/pine_bark_diff_2k.jpg"), TEXT("T_FloraPineBark_BaseColor"), ERole::Color},
    {TEXT("FloraDraft/pine_bark/pine_bark_nor_dx_2k.jpg"), TEXT("T_FloraPineBark_Normal"), ERole::Normal},
    {TEXT("FloraDraft/pine_bark/pine_bark_rough_2k.jpg"), TEXT("T_FloraPineBark_Roughness"), ERole::Gray},
    // Douglas-fir: ambientCG Bark014 (photogrammetry, tagged fir) supersedes the bark_willow_02 stand-in.
    {TEXT("FloraDraft/Bark014/Bark014_2K-JPG_Color.jpg"), TEXT("T_FloraFirBark_BaseColor"), ERole::Color},
    {TEXT("FloraDraft/Bark014/Bark014_2K-JPG_NormalDX.jpg"), TEXT("T_FloraFirBark_Normal"), ERole::Normal},
    {TEXT("FloraDraft/Bark014/Bark014_2K-JPG_Roughness.jpg"), TEXT("T_FloraFirBark_Roughness"), ERole::Gray},
    {TEXT("FloraDraft/jolcham_oak_bark_01/jolcham_oak_bark_01_diff_1k.jpg"), TEXT("T_FloraOakBark_BaseColor"), ERole::Color},
    {TEXT("FloraDraft/jolcham_oak_bark_01/jolcham_oak_bark_01_nor_dx_1k.jpg"), TEXT("T_FloraOakBark_Normal"), ERole::Normal},
    {TEXT("FloraDraft/jolcham_oak_bark_01/jolcham_oak_bark_01_rough_1k.jpg"), TEXT("T_FloraOakBark_Roughness"), ERole::Gray},
    // Monterey pine: ART-FLORA T_PineClumpCards (dense composites of the real pine_tree_01 sprigs).
    {TEXT("FloraDraft/Derived/T_PineClumpCards_BaseColor.png"), TEXT("T_FloraPineTwig_BaseColor"), ERole::Color},
    {TEXT("FloraDraft/Derived/T_PineClumpCards_Alpha.png"), TEXT("T_FloraPineTwig_Alpha"), ERole::Alpha},
    {TEXT("FloraDraft/Derived/T_PineClumpCards_Normal.png"), TEXT("T_FloraPineTwig_Normal"), ERole::Normal},
    {TEXT("FloraDraft/Derived/T_PineClumpCards_Roughness.png"), TEXT("T_FloraPineTwig_Roughness"), ERole::Gray},
    // Douglas-fir sprays: ambientCG LeafSet019 (three dense conifer sprays).
    {TEXT("FloraDraft/LeafSet019/LeafSet019_2K-JPG_Color.jpg"), TEXT("T_FloraFirTwig_BaseColor"), ERole::Color},
    {TEXT("FloraDraft/LeafSet019/LeafSet019_2K-JPG_Opacity.jpg"), TEXT("T_FloraFirTwig_Alpha"), ERole::Alpha},
    {TEXT("FloraDraft/LeafSet019/LeafSet019_2K-JPG_NormalDX.jpg"), TEXT("T_FloraFirTwig_Normal"), ERole::Normal},
    {TEXT("FloraDraft/LeafSet019/LeafSet019_2K-JPG_Roughness.jpg"), TEXT("T_FloraFirTwig_Roughness"), ERole::Gray},
    // Coast live oak: ART-FLORA T_LiveOakTwigCards (LeafSet024 leaves at 3-6 cm on twigs, pre-darkened olive).
    {TEXT("FloraDraft/Derived/T_LiveOakTwigCards_BaseColor.png"), TEXT("T_FloraOakLeaf_BaseColor"), ERole::Color},
    {TEXT("FloraDraft/Derived/T_LiveOakTwigCards_Alpha.png"), TEXT("T_FloraOakLeaf_Alpha"), ERole::Alpha},
    {TEXT("FloraDraft/Derived/T_LiveOakTwigCards_Normal.png"), TEXT("T_FloraOakLeaf_Normal"), ERole::Normal},
    {TEXT("FloraDraft/Derived/T_LiveOakTwigCards_Roughness.png"), TEXT("T_FloraOakLeaf_Roughness"), ERole::Gray},
    {TEXT("FloraDraft/fern_02/textures/fern_02_diff_1k.jpg"), TEXT("T_FloraFern_BaseColor"), ERole::Color},
    {TEXT("FloraDraft/fern_02/fern_02_alpha_1k.jpg"), TEXT("T_FloraFern_Alpha"), ERole::Alpha},
    {TEXT("FloraDraft/fern_02/fern_02_nor_dx_1k.jpg"), TEXT("T_FloraFern_Normal"), ERole::Normal},
    {TEXT("FloraDraft/fern_02/textures/fern_02_arm_1k.jpg"), TEXT("T_FloraFern_ARM"), ERole::Masks},
    {TEXT("FloraDraft/wild_rooibos_bush/textures/wild_rooibos_bush_diff_1k.jpg"), TEXT("T_FloraRooibos_BaseColor"), ERole::Color},
    {TEXT("FloraDraft/wild_rooibos_bush/wild_rooibos_bush_alpha_1k.jpg"), TEXT("T_FloraRooibos_Alpha"), ERole::Alpha},
    {TEXT("FloraDraft/wild_rooibos_bush/wild_rooibos_bush_nor_dx_1k.jpg"), TEXT("T_FloraRooibos_Normal"), ERole::Normal},
    {TEXT("FloraDraft/wild_rooibos_bush/textures/wild_rooibos_bush_arm_1k.jpg"), TEXT("T_FloraRooibos_ARM"), ERole::Masks},
    // Trunk-base litter ring (ART-FLORA derived from forest_leaves_04, radial alpha fading to 0).
    {TEXT("FloraDraft/Derived/T_TrunkBaseLitter_BaseColor.png"), TEXT("T_FloraTrunkLitter_BaseColor"), ERole::Color},
    {TEXT("FloraDraft/Derived/T_TrunkBaseLitter_Alpha.png"), TEXT("T_FloraTrunkLitter_Alpha"), ERole::Alpha},
    {TEXT("FloraDraft/Derived/T_TrunkBaseLitter_Normal.png"), TEXT("T_FloraTrunkLitter_Normal"), ERole::Normal},
    {TEXT("FloraDraft/Derived/T_TrunkBaseLitter_Roughness.png"), TEXT("T_FloraTrunkLitter_Roughness"), ERole::Gray},
};

bool Save(UObject* Object)
{
    if(!Object || !Object->GetOutermost()->GetName().StartsWith(Destination))return false;
    const FString Filename=FPackageName::LongPackageNameToFilename(Object->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
    if(!FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(Filename)))return false;
    Object->MarkPackageDirty();FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
    return UPackage::SavePackage(Object->GetOutermost(),Object,*Filename,Args);
}
template<class T>T* Node(UMaterial* M){return CastChecked<T>(UMaterialEditingLibrary::CreateMaterialExpression(M,T::StaticClass()));}
struct FIn{const TCHAR* Name;UMaterialExpression* Expr;int32 Output;};
UMaterialExpressionCustom* Custom(UMaterial* M,const TCHAR* Code,ECustomMaterialOutputType Type,std::initializer_list<FIn> Inputs)
{
    auto* C=Node<UMaterialExpressionCustom>(M);C->Code=Code;C->OutputType=Type;C->Inputs.Reset();
    for(const FIn& In:Inputs){FCustomInput Input;Input.InputName=In.Name;Input.Input.Connect(In.Output,In.Expr);C->Inputs.Add(Input);}
    return C;
}
UMaterialExpressionTextureSampleParameter2D* Sample(UMaterial* M,UTexture2D* Texture,const TCHAR* Name,EMaterialSamplerType Type,UMaterialExpression* UV=nullptr)
{
    auto* S=Node<UMaterialExpressionTextureSampleParameter2D>(M);S->ParameterName=Name;S->Texture=Texture;S->SamplerType=Type;
    if(UV)S->Coordinates.Connect(0,UV);
    return S;
}
// Shared deterministic wind: broad gust plus small flutter, weighted by vertex colour G
// (0 on trunks, 1 at branch tips). The same field drives bark and foliage so cards stay attached.
const TCHAR* WindCode=TEXT("float ph=dot(P.xy,float2(.0011,.0007));float g=sin(T*.9+ph)*.65+sin(T*2.3+ph*2.1)*.35;"
    "float f=sin(T*5.1+dot(P.xyz,float3(.031,.021,.017)));return float3(.82,.57,0)*(g*W*2.5)+float3(0,0,1)*(f*W*W*.5);");

UMaterial* Begin(const TCHAR* Name,bool& bNew)
{
    UPackage* P=CreatePackage(*(Destination+TEXT("Materials/")+Name));P->FullyLoad();
    UMaterial* M=FindObject<UMaterial>(P,Name);bNew=!M;
    if(bNew)M=NewObject<UMaterial>(P,Name,RF_Public|RF_Standalone);
    M->PreEditChange(nullptr);
    TArray<UMaterialExpression*> Old;for(const TObjectPtr<UMaterialExpression>& E:M->GetExpressions())Old.Add(E.Get());
    for(auto* E:Old)UMaterialEditingLibrary::DeleteMaterialExpression(M,E);
    bool NeedsRecompile=false;M->SetMaterialUsage(NeedsRecompile,MATUSAGE_InstancedStaticMeshes);
    M->DitheredLODTransition=true;
    return M;
}
UMaterial* Finish(UMaterial* M,bool bNew,const TCHAR* Name)
{
    UMaterialEditingLibrary::LayoutMaterialExpressions(M);
    if(bNew)FAssetRegistryModule::AssetCreated(M);M->PostEditChange();
    const TArray<FString> Errors=UMaterialEditingLibrary::RecompileMaterial(M);
    for(const FString& Error:Errors)UE_LOG(LogTemp,Error,TEXT("HABITAT_MATERIAL_ERROR %s %s"),Name,*Error);
    return Errors.IsEmpty()&&Save(M)?M:nullptr;
}
/** Masked two-sided foliage. Maps: base colour, alpha, DirectX normal, roughness (channel). */
UMaterial* Foliage(const TCHAR* Name,UTexture2D* Base,UTexture2D* Alpha,UTexture2D* Normal,UTexture2D* Rough,int32 RoughChannel,float Clip,const FLinearColor& Tint)
{
    bool bNew=false;UMaterial* M=Begin(Name,bNew);
    M->BlendMode=BLEND_Masked;M->TwoSided=true;M->SetShadingModel(MSM_TwoSidedFoliage);M->OpacityMaskClipValue=Clip;
    auto* Color=Sample(M,Base,TEXT("BaseColorTexture"),SAMPLERTYPE_Color);
    auto* Mask=Sample(M,Alpha,TEXT("OpacityTexture"),SAMPLERTYPE_LinearGrayscale);
    auto* Nrm=Sample(M,Normal,TEXT("NormalTexture"),SAMPLERTYPE_Normal);
    auto* Rgh=Sample(M,Rough,TEXT("RoughnessTexture"),Rough&&Rough->CompressionSettings==TC_Masks?SAMPLERTYPE_Masks:SAMPLERTYPE_LinearGrayscale);
    auto* TintParam=Node<UMaterialExpressionVectorParameter>(M);TintParam->ParameterName=TEXT("Color");TintParam->DefaultValue=Tint;
    auto* RoughParam=Node<UMaterialExpressionScalarParameter>(M);RoughParam->ParameterName=TEXT("Roughness");RoughParam->DefaultValue=1.0F;
    auto* Coverage=Node<UMaterialExpressionScalarParameter>(M);Coverage->ParameterName=TEXT("DistantNeedleCoverage");Coverage->DefaultValue=.20F;
    auto* Random=Node<UMaterialExpressionPerInstanceRandom>(M);auto* Vertex=Node<UMaterialExpressionVertexColor>(M);
    auto* Depth=Node<UMaterialExpressionPixelDepth>(M);auto* Fade=Node<UMaterialExpressionPerInstanceFadeAmount>(M);
    auto* World=Node<UMaterialExpressionWorldPosition>(M);auto* Time=Node<UMaterialExpressionTime>(M);
    // Per-instance hue/brightness spread and inner-crown occlusion (vertex colour R).
    auto* Surface=Custom(M,TEXT("float3 c=C*Tint.rgb*lerp(.80,1.12,R)*float3(lerp(1.04,.94,R),1,lerp(.96,1.06,R));return c*(.62+.38*V.r);"),CMOT_Float3,
        {{TEXT("C"),Color,0},{TEXT("Tint"),TintParam,0},{TEXT("R"),Random,0},{TEXT("V"),Vertex,0}});
    // The bias stays below the clip value so dilated atlas background never becomes opaque.
    // The distance coverage bias is for the camera only: in shadow-depth passes (depth is measured
    // from the light) it would turn every canopy into a solid shadow caster and black out the floor.
    auto* Opacity=Custom(M,TEXT("#if defined(SHADOW_DEPTH_SHADER) && SHADOW_DEPTH_SHADER\nreturn A*F;\n#else\nreturn A*F+saturate((D-1500.0)/3500.0)*B;\n#endif"),CMOT_Float1,
        {{TEXT("A"),Mask,1},{TEXT("D"),Depth,0},{TEXT("B"),Coverage,0},{TEXT("F"),Fade,0}});
    auto* Roughness=Custom(M,TEXT("return saturate(Rg*K);"),CMOT_Float1,{{TEXT("Rg"),Rgh,1+RoughChannel},{TEXT("K"),RoughParam,0}});
    auto* Subsurface=Custom(M,TEXT("return S*.38;"),CMOT_Float3,{{TEXT("S"),Surface,0}});
    auto* Wind=Custom(M,WindCode,CMOT_Float3,{{TEXT("P"),World,0},{TEXT("T"),Time,0},{TEXT("W"),Vertex,2}});
    bool bOk=UMaterialEditingLibrary::ConnectMaterialProperty(Surface,TEXT(""),MP_BaseColor);
    bOk&=UMaterialEditingLibrary::ConnectMaterialProperty(Nrm,TEXT("RGB"),MP_Normal);
    bOk&=UMaterialEditingLibrary::ConnectMaterialProperty(Roughness,TEXT(""),MP_Roughness);
    bOk&=UMaterialEditingLibrary::ConnectMaterialProperty(Opacity,TEXT(""),MP_OpacityMask);
    bOk&=UMaterialEditingLibrary::ConnectMaterialProperty(Subsurface,TEXT(""),MP_SubsurfaceColor);
    bOk&=UMaterialEditingLibrary::ConnectMaterialProperty(Wind,TEXT(""),MP_WorldPositionOffset);
    return bOk?Finish(M,bNew,Name):nullptr;
}
/** Opaque bark. UV0 is authored in physical tiles by the runtime generator; vertex colour R
 *  darkens the lowest half metre (soil contact), G carries the shared wind weight. */
UMaterial* Bark(const TCHAR* Name,UTexture2D* Base,UTexture2D* Normal,UTexture2D* Rough,const FLinearColor& Tint)
{
    bool bNew=false;UMaterial* M=Begin(Name,bNew);
    M->BlendMode=BLEND_Opaque;M->TwoSided=false;M->SetShadingModel(MSM_DefaultLit);
    auto* DetailUV=Node<UMaterialExpressionTextureCoordinate>(M);DetailUV->UTiling=4.0F;DetailUV->VTiling=4.0F;
    auto* Color=Sample(M,Base,TEXT("BaseColorTexture"),SAMPLERTYPE_Color);
    auto* Nrm=Sample(M,Normal,TEXT("NormalTexture"),SAMPLERTYPE_Normal);
    auto* Detail=Sample(M,Normal,TEXT("DetailNormalTexture"),SAMPLERTYPE_Normal,DetailUV);
    auto* Rgh=Sample(M,Rough,TEXT("RoughnessTexture"),SAMPLERTYPE_LinearGrayscale);
    auto* TintParam=Node<UMaterialExpressionVectorParameter>(M);TintParam->ParameterName=TEXT("Color");TintParam->DefaultValue=Tint;
    auto* RoughParam=Node<UMaterialExpressionScalarParameter>(M);RoughParam->ParameterName=TEXT("Roughness");RoughParam->DefaultValue=1.0F;
    auto* Random=Node<UMaterialExpressionPerInstanceRandom>(M);auto* Vertex=Node<UMaterialExpressionVertexColor>(M);
    auto* Depth=Node<UMaterialExpressionPixelDepth>(M);auto* World=Node<UMaterialExpressionWorldPosition>(M);auto* Time=Node<UMaterialExpressionTime>(M);
    auto* Surface=Custom(M,TEXT("return C*Tint.rgb*lerp(.85,1.1,R)*lerp(.42,1.0,V.r);"),CMOT_Float3,
        {{TEXT("C"),Color,0},{TEXT("Tint"),TintParam,0},{TEXT("R"),Random,0},{TEXT("V"),Vertex,0}});
    // Detail normal (same scan at 4x) fades in below ~1.5 m so the 45 cm launch camera keeps relief.
    auto* Blend=Custom(M,TEXT("float k=saturate(1.0-(D-60.0)/120.0)*.55;return normalize(float3(N.xy+Q.xy*k,N.z));"),CMOT_Float3,
        {{TEXT("N"),Nrm,0},{TEXT("Q"),Detail,0},{TEXT("D"),Depth,0}});
    auto* Roughness=Custom(M,TEXT("return saturate(Rg*K*lerp(1.0,1.08,1.0-V.r));"),CMOT_Float1,{{TEXT("Rg"),Rgh,1},{TEXT("K"),RoughParam,0},{TEXT("V"),Vertex,0}});
    auto* Wind=Custom(M,WindCode,CMOT_Float3,{{TEXT("P"),World,0},{TEXT("T"),Time,0},{TEXT("W"),Vertex,2}});
    bool bOk=UMaterialEditingLibrary::ConnectMaterialProperty(Surface,TEXT(""),MP_BaseColor);
    bOk&=UMaterialEditingLibrary::ConnectMaterialProperty(Blend,TEXT(""),MP_Normal);
    bOk&=UMaterialEditingLibrary::ConnectMaterialProperty(Roughness,TEXT(""),MP_Roughness);
    bOk&=UMaterialEditingLibrary::ConnectMaterialProperty(Wind,TEXT(""),MP_WorldPositionOffset);
    return bOk?Finish(M,bNew,Name):nullptr;
}
/** Opaque-masked ground litter ring: one-sided, no WPO, alpha fades the ring into the terrain. */
UMaterial* GroundLitter(const TCHAR* Name,UTexture2D* Base,UTexture2D* Alpha,UTexture2D* Normal,UTexture2D* Rough)
{
    bool bNew=false;UMaterial* M=Begin(Name,bNew);
    M->BlendMode=BLEND_Masked;M->TwoSided=false;M->SetShadingModel(MSM_DefaultLit);M->OpacityMaskClipValue=.5F;
    auto* Color=Sample(M,Base,TEXT("BaseColorTexture"),SAMPLERTYPE_Color);
    auto* Mask=Sample(M,Alpha,TEXT("OpacityTexture"),SAMPLERTYPE_LinearGrayscale);
    auto* Nrm=Sample(M,Normal,TEXT("NormalTexture"),SAMPLERTYPE_Normal);
    auto* Rgh=Sample(M,Rough,TEXT("RoughnessTexture"),SAMPLERTYPE_LinearGrayscale);
    auto* Random=Node<UMaterialExpressionPerInstanceRandom>(M);
    auto* Surface=Custom(M,TEXT("return C*lerp(1.15,1.4,R)*float3(1.06,1.0,.86);"),CMOT_Float3,{{TEXT("C"),Color,0},{TEXT("R"),Random,0}});
    // Soft edge: texture alpha x a broad radial falloff, resolved by a screen-space dither (TAA
    // integrates it), so each ring fades into the ground instead of ending as a hard-edged rug.
    auto* UV=Node<UMaterialExpressionTextureCoordinate>(M);
    auto* Feather=Custom(M,TEXT("float d=length(T-.5);float a=A*(1.0-smoothstep(.22,.5,d));float n=frac(52.9829189*frac(dot(Parameters.SvPosition.xy,float2(.06711056,.00583715))));return a>n*.9+.05?1.0:0.0;"),CMOT_Float1,
        {{TEXT("A"),Mask,1},{TEXT("T"),UV,0}});
    bool bOk=UMaterialEditingLibrary::ConnectMaterialProperty(Surface,TEXT(""),MP_BaseColor);
    bOk&=UMaterialEditingLibrary::ConnectMaterialProperty(Nrm,TEXT("RGB"),MP_Normal);
    bOk&=UMaterialEditingLibrary::ConnectMaterialProperty(Rgh,TEXT("R"),MP_Roughness);
    bOk&=UMaterialEditingLibrary::ConnectMaterialProperty(Feather,TEXT(""),MP_OpacityMask);
    return bOk?Finish(M,bNew,Name):nullptr;
}
bool Mesh(const TCHAR* Name,UMaterialInterface* M)
{
    FString Text;const FString File=FPaths::ProjectDir()/TEXT("SourceAssets/EnvironmentDraft/HabitatDraft/")+(FString(Name)+TEXT(".mesh.json"));
    if(!FFileHelper::LoadFileToString(Text,*File) || Text.Len()>4000000)return false;
    TSharedPtr<FJsonObject> Json;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Json)||!Json.IsValid()||Json->GetStringField(TEXT("schema"))!=TEXT("aerosim.habitat-mesh/v1")||Json->GetStringField(TEXT("asset"))!=Name)return false;
    const auto& Points=Json->GetArrayField(TEXT("positions"));const auto& Normals=Json->GetArrayField(TEXT("normals"));const auto& UV=Json->GetArrayField(TEXT("uv"));const auto& Indices=Json->GetArrayField(TEXT("indices"));
    if(Points.Num()<3||Points.Num()>12000||Points.Num()!=Normals.Num()||Points.Num()!=UV.Num()||Indices.Num()>30000||Indices.Num()%3)return false;
    FMeshDescription Description;FStaticMeshAttributes Attributes(Description);Attributes.Register();Attributes.GetVertexInstanceUVs().SetNumChannels(1);
    TArray<FVertexID> Vertices;float Top=1;
    for(const auto& P:Points){const auto& A=P->AsArray();if(A.Num()!=3)return false;const FVector3f Position(A[0]->AsNumber(),A[1]->AsNumber(),A[2]->AsNumber());if(Position.ContainsNaN()||Position.GetAbsMax()>500)return false;Top=FMath::Max(Top,Position.Z);FVertexID V=Description.CreateVertex();Attributes.GetVertexPositions()[V]=Position;Vertices.Add(V);}
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
            // R: base occlusion; G: wind weight rising with height above the root crown.
            const float H=Attributes.GetVertexPositions()[Vertices[Index]].Z/Top;
            Attributes.GetVertexInstanceUVs().Set(Instance,0,FVector2f(T[0]->AsNumber(),T[1]->AsNumber()));Attributes.GetVertexInstanceColors()[Instance]=FVector4f(.55F+.45F*H,FMath::Square(H)*.35F,0,1);Corners.Add(Instance);
        }
        Description.CreatePolygon(Group,Corners);
    }
    UPackage* P=CreatePackage(*(Destination+TEXT("Meshes/")+Name));P->FullyLoad();UStaticMesh* S=FindObject<UStaticMesh>(P,Name);const bool New=!S;
    if(New)S=NewObject<UStaticMesh>(P,Name,RF_Public|RF_Standalone);
    S->bGenerateMeshDistanceField=false; // no async DF builds (integration disabled project-wide DFs)
    S->SetNumSourceModels(0);S->GetStaticMaterials().Reset();S->GetStaticMaterials().Add(FStaticMaterial(M,TEXT("Foliage")));S->SetAutoComputeLODScreenSize(false);
    const int32 LODCount=FString(Name).Contains(TEXT("Tiny"))?1:3;
    for(int32 LOD=0;LOD<LODCount;++LOD)
    {
        auto& Source=S->AddSourceModel();Source.BuildSettings.bRecomputeNormals=false;Source.BuildSettings.bRecomputeTangents=true;Source.BuildSettings.bUseMikkTSpace=true;Source.BuildSettings.bGenerateLightmapUVs=false;Source.BuildSettings.bGenerateDistanceFieldAsIfTwoSided=false;Source.BuildSettings.DistanceFieldResolutionScale=0.0F;
        Source.ReductionSettings.PercentTriangles=LOD==0?1.0F:LOD==1?.4F:.12F;Source.ScreenSize.Default=LOD==0?1.0F:LOD==1?.05F:.015F;
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
    if(FParse::Param(*Params,TEXT("HeroCliffOnly")))return ImportHeroCliffDraft();
    using namespace HabitatDraft;
    auto& Tools=FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
    TMap<FString,UTexture2D*> Imported;
    for(const FHabitatTextureSource& Source:Textures)
    {
        UAssetImportTask* Task=NewObject<UAssetImportTask>();Task->Filename=FPaths::ProjectDir()/TEXT("SourceAssets/EnvironmentDraft/")/Source.Path;Task->DestinationPath=Destination+TEXT("Textures");Task->DestinationName=Source.Name;Task->Factory=NewObject<UTextureFactory>();Task->bAutomated=true;Task->bReplaceExisting=true;Task->bReplaceExistingSettings=true;
        Tools.Get().ImportAssetTasks({Task});const auto& Objects=Task->GetObjects();UTexture2D* Texture=Objects.Num()==1?Cast<UTexture2D>(Objects[0]):nullptr;
        if(!Texture){UE_LOG(LogTemp,Error,TEXT("HABITAT_TEXTURE_FAILED %s"),Source.Path);return 3;}
        Texture->SRGB=Source.Role==ERole::Color;
        Texture->CompressionSettings=Source.Role==ERole::Normal?TC_Normalmap:Source.Role==ERole::Color?TC_Default:Source.Role==ERole::Masks?TC_Masks:TC_Grayscale;
        Texture->bFlipGreenChannel=false;Texture->MaxTextureSize=2048;
        Texture->bDoScaleMipsForAlphaCoverage=Source.Role==ERole::Alpha;if(Source.Role==ERole::Alpha)Texture->AlphaCoverageThresholds=FVector4(.35F,0,0,0);
        if(Texture->AssetImportData){FString Relative=Task->Filename;FPaths::MakePathRelativeTo(Relative,*FPaths::ProjectDir());Texture->AssetImportData->UpdateFilenameOnly(Relative);}
        Texture->PostEditChange();if(!Save(Texture))return 4;Imported.Add(Source.Name,Texture);
    }
    auto T=[&Imported](const TCHAR* Name){return Imported.FindRef(Name);};
    UMaterial* Grass=Foliage(TEXT("M_CoastDryGrassDraft"),T(TEXT("T_CoastDryGrass_BaseColor")),T(TEXT("T_CoastDryGrass_Alpha")),T(TEXT("T_CoastDryGrass_Normal")),T(TEXT("T_CoastDryGrass_Roughness")),0,.33F,FLinearColor::White);
    UMaterial* Shrub=Foliage(TEXT("M_CoastBroadShrubDraft"),T(TEXT("T_CoastBroadShrub_BaseColor")),T(TEXT("T_CoastBroadShrub_Alpha")),T(TEXT("T_CoastBroadShrub_Normal")),T(TEXT("T_CoastBroadShrub_Roughness")),0,.33F,FLinearColor(.78F,.84F,.72F));
    UMaterial* AnnualForb=Foliage(TEXT("M_CoastAnnualForbDraft"),T(TEXT("T_CoastAnnualForb_BaseColor")),T(TEXT("T_CoastAnnualForb_Alpha")),T(TEXT("T_CoastAnnualForb_Normal")),T(TEXT("T_CoastAnnualForb_Roughness")),0,.35F,FLinearColor(.76F,.88F,.68F));
    UMaterial* CoastalScrub=Foliage(TEXT("M_CoastalScrubDraft"),T(TEXT("T_CoastalScrub_BaseColor")),T(TEXT("T_CoastalScrub_Alpha")),T(TEXT("T_CoastalScrub_Normal")),T(TEXT("T_CoastalScrub_Roughness")),0,.34F,FLinearColor(.72F,.80F,.68F));
    UMaterial* Deadwood=Bark(TEXT("M_CoastDeadwoodDraft"),T(TEXT("T_CoastDeadwood_BaseColor")),T(TEXT("T_CoastDeadwood_Normal")),T(TEXT("T_CoastDeadwood_Roughness")),FLinearColor(.72F,.68F,.62F));
    UMaterial* Fern=Foliage(TEXT("M_FloraFernDraft"),T(TEXT("T_FloraFern_BaseColor")),T(TEXT("T_FloraFern_Alpha")),T(TEXT("T_FloraFern_Normal")),T(TEXT("T_FloraFern_ARM")),1,.36F,FLinearColor(.85F,.92F,.82F));
    UMaterial* Rooibos=Foliage(TEXT("M_FloraRooibosDraft"),T(TEXT("T_FloraRooibos_BaseColor")),T(TEXT("T_FloraRooibos_Alpha")),T(TEXT("T_FloraRooibos_Normal")),T(TEXT("T_FloraRooibos_ARM")),1,.36F,FLinearColor(.70F,.78F,.62F));
    UMaterial* PineNeedles=Foliage(TEXT("M_FloraPineNeedlesDraft"),T(TEXT("T_FloraPineTwig_BaseColor")),T(TEXT("T_FloraPineTwig_Alpha")),T(TEXT("T_FloraPineTwig_Normal")),T(TEXT("T_FloraPineTwig_Roughness")),0,.36F,FLinearColor(.62F,1.08F,.52F));
    UMaterial* FirNeedles=Foliage(TEXT("M_FloraFirNeedlesDraft"),T(TEXT("T_FloraFirTwig_BaseColor")),T(TEXT("T_FloraFirTwig_Alpha")),T(TEXT("T_FloraFirTwig_Normal")),T(TEXT("T_FloraFirTwig_Roughness")),0,.36F,FLinearColor(.42F,.92F,.36F));
    // Live-oak leaves are darker and glossier than the published island_tree_02 scan.
    UMaterial* OakLeaves=Foliage(TEXT("M_FloraOakLeavesDraft"),T(TEXT("T_FloraOakLeaf_BaseColor")),T(TEXT("T_FloraOakLeaf_Alpha")),T(TEXT("T_FloraOakLeaf_Normal")),T(TEXT("T_FloraOakLeaf_Roughness")),0,.38F,FLinearColor(.66F,1.10F,.56F));
    // Douglas-fir proxy bark is tinted warmer (+red), per the ART-FLORA spec.
    UMaterial* PineBark=Bark(TEXT("M_FloraPineBarkDraft"),T(TEXT("T_FloraPineBark_BaseColor")),T(TEXT("T_FloraPineBark_Normal")),T(TEXT("T_FloraPineBark_Roughness")),FLinearColor(.62F,.55F,.50F));
    UMaterial* FirBark=Bark(TEXT("M_FloraFirBarkDraft"),T(TEXT("T_FloraFirBark_BaseColor")),T(TEXT("T_FloraFirBark_Normal")),T(TEXT("T_FloraFirBark_Roughness")),FLinearColor(.45F,.42F,.40F));
    UMaterial* OakBark=Bark(TEXT("M_FloraOakBarkDraft"),T(TEXT("T_FloraOakBark_BaseColor")),T(TEXT("T_FloraOakBark_Normal")),T(TEXT("T_FloraOakBark_Roughness")),FLinearColor(.70F,.68F,.66F));
    UMaterial* Litter=GroundLitter(TEXT("M_FloraTrunkLitterDraft"),T(TEXT("T_FloraTrunkLitter_BaseColor")),T(TEXT("T_FloraTrunkLitter_Alpha")),T(TEXT("T_FloraTrunkLitter_Normal")),T(TEXT("T_FloraTrunkLitter_Roughness")));
    if(!(Grass&&Shrub&&AnnualForb&&CoastalScrub&&Deadwood&&Fern&&Rooibos&&PineNeedles&&FirNeedles&&OakLeaves&&PineBark&&FirBark&&OakBark&&Litter))return 5;
    const TPair<const TCHAR*,UMaterial*> Meshes[]={{TEXT("SM_CoastGrassSmallDraft"),Grass},{TEXT("SM_CoastGrassTallDraft"),Grass},{TEXT("SM_CoastGrassTinyDraft"),Grass},
        {TEXT("SM_CoastShrubADraft"),Shrub},{TEXT("SM_CoastShrubBDraft"),Shrub},
        {TEXT("SM_CoastAnnualForbADraft"),AnnualForb},{TEXT("SM_CoastAnnualForbBDraft"),AnnualForb},{TEXT("SM_CoastAnnualForbCDraft"),AnnualForb},
        {TEXT("SM_CoastDeadwoodADraft"),Deadwood},{TEXT("SM_CoastDeadwoodBDraft"),Deadwood},{TEXT("SM_CoastDeadwoodCDraft"),Deadwood},
        {TEXT("SM_CoastalScrubADraft"),CoastalScrub},{TEXT("SM_CoastalScrubBDraft"),CoastalScrub},{TEXT("SM_CoastalScrubCDraft"),CoastalScrub},{TEXT("SM_CoastalScrubDDraft"),CoastalScrub},
        {TEXT("SM_FloraFernADraft"),Fern},{TEXT("SM_FloraFernBDraft"),Fern},
        {TEXT("SM_FloraRooibosADraft"),Rooibos},{TEXT("SM_FloraRooibosBDraft"),Rooibos}};
    for(const auto& Entry:Meshes)if(!Mesh(Entry.Key,Entry.Value)){UE_LOG(LogTemp,Error,TEXT("HABITAT_MESH_FAILED %s"),Entry.Key);return 6;}
    UE_LOG(LogTemp,Display,TEXT("HABITAT_IMPORT_COMPLETE textures=%d materials=14 meshes=%d state=DRAFT_NOT_USER_CONFIRMED"),UE_ARRAY_COUNT(Textures),UE_ARRAY_COUNT(Meshes));return 0;
}
