#include "CoastDraftImportCommandlet.h"

#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Engine/Texture2D.h"
#include "EditorFramework/AssetImportData.h"
#include "Factories/TextureFactory.h"
#include "HAL/PlatformFileManager.h"
#include "IAssetTools.h"
#include "MaterialEditingLibrary.h"
#include "PacificCoastGraybox.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCameraPositionWS.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionSingleLayerWaterMaterialOutput.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionPixelDepth.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionPanner.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace CoastDraftImport
{
constexpr const TCHAR* DestinationRoot = TEXT("/Game/Development/CoastDraft/");
enum class ETextureRole : uint8 { Color, Normal, Roughness, Alpha };
struct FSource final { const TCHAR* RelativeFile; const TCHAR* AssetName; ETextureRole Role; };
const FSource Sources[] = {
    {TEXT("Ground/basecolor.jpg"), TEXT("T_CoastGround_BaseColor"), ETextureRole::Color},
    {TEXT("Ground/normal.jpg"), TEXT("T_CoastGround_Normal"), ETextureRole::Normal},
    {TEXT("Ground/roughness.jpg"), TEXT("T_CoastGround_Roughness"), ETextureRole::Roughness},
    {TEXT("Rock/basecolor.jpg"), TEXT("T_CoastRock_BaseColor"), ETextureRole::Color},
    {TEXT("Rock/normal.jpg"), TEXT("T_CoastRock_Normal"), ETextureRole::Normal},
    {TEXT("Rock/roughness.jpg"), TEXT("T_CoastRock_Roughness"), ETextureRole::Roughness},
    {TEXT("PineTree01/needle_diff.png"), TEXT("T_CoastNeedles_BaseColor"), ETextureRole::Color},
    {TEXT("PineTree01/needle_alpha.png"), TEXT("T_CoastNeedles_Alpha"), ETextureRole::Alpha},
    {TEXT("PineTree01/needle_normal.png"), TEXT("T_CoastNeedles_Normal"), ETextureRole::Normal},
    {TEXT("PineTree01/needle_rough.png"), TEXT("T_CoastNeedles_Roughness"), ETextureRole::Roughness},
    {TEXT("PineTree01/pine_tree_01_bark_diff_1k.png"), TEXT("T_CoastBark_BaseColor"), ETextureRole::Color},
    {TEXT("PineTree01/pine_tree_01_bark_nor_dx_1k.png"), TEXT("T_CoastBark_Normal"), ETextureRole::Normal},
    {TEXT("PineTree01/pine_tree_01_bark_rough_1k.png"), TEXT("T_CoastBark_Roughness"), ETextureRole::Roughness},
    {TEXT("GravelDraft/basecolor.jpg"), TEXT("T_CoastGravel_BaseColor"), ETextureRole::Color},
    {TEXT("GravelDraft/normal.jpg"), TEXT("T_CoastGravel_Normal"), ETextureRole::Normal},
    {TEXT("GravelDraft/roughness.jpg"), TEXT("T_CoastGravel_Roughness"), ETextureRole::Roughness},
    // Surface v2 (ART-SURFACE, CC0 Poly Haven, true physical tile sizes in comments).
    {TEXT("Ground/withered_grass/withered_grass_diff_2k.jpg"), TEXT("T_CoastWitheredGrass_BaseColor"), ETextureRole::Color},     // 16, 2.0 m
    {TEXT("Ground/withered_grass/withered_grass_nor_dx_2k.jpg"), TEXT("T_CoastWitheredGrass_Normal"), ETextureRole::Normal},
    {TEXT("Ground/withered_grass/withered_grass_rough_2k.jpg"), TEXT("T_CoastWitheredGrass_Roughness"), ETextureRole::Roughness},
    {TEXT("Ground/sparse_grass/sparse_grass_diff_2k.jpg"), TEXT("T_CoastSparseGrass_BaseColor"), ETextureRole::Color},           // 19, 2.0 m
    {TEXT("Ground/sparse_grass/sparse_grass_nor_dx_2k.jpg"), TEXT("T_CoastSparseGrass_Normal"), ETextureRole::Normal},
    {TEXT("Ground/sparse_grass/sparse_grass_rough_2k.jpg"), TEXT("T_CoastSparseGrass_Roughness"), ETextureRole::Roughness},
    {TEXT("Ground/dry_decay_leaves/dry_decay_leaves_diff_2k.jpg"), TEXT("T_CoastLeafLitter_BaseColor"), ETextureRole::Color},    // 22, 2.0 m
    {TEXT("Ground/dry_decay_leaves/dry_decay_leaves_nor_dx_2k.jpg"), TEXT("T_CoastLeafLitter_Normal"), ETextureRole::Normal},
    {TEXT("Ground/dry_decay_leaves/dry_decay_leaves_rough_2k.jpg"), TEXT("T_CoastLeafLitter_Roughness"), ETextureRole::Roughness},
    {TEXT("Ground/dry_ground_rocks/dry_ground_rocks_diff_2k.jpg"), TEXT("T_CoastStonyGround_BaseColor"), ETextureRole::Color},   // 25, 4.0 m
    {TEXT("Ground/dry_ground_rocks/dry_ground_rocks_nor_dx_2k.jpg"), TEXT("T_CoastStonyGround_Normal"), ETextureRole::Normal},
    {TEXT("Ground/dry_ground_rocks/dry_ground_rocks_rough_2k.jpg"), TEXT("T_CoastStonyGround_Roughness"), ETextureRole::Roughness},
    {TEXT("Ground/dirt/dirt_nor_dx_2k.jpg"), TEXT("T_CoastDetailDirt_Normal"), ETextureRole::Normal},                            // 28, detail
    {TEXT("Ground/dirt/dirt_rough_2k.jpg"), TEXT("T_CoastDetailDirt_Roughness"), ETextureRole::Roughness},
    {TEXT("Ground/aerial_grass_rock/aerial_grass_rock_diff_2k.jpg"), TEXT("T_CoastMacroGrassRock_BaseColor"), ETextureRole::Color}, // 30, macro
    {TEXT("Ground/rocky_trail_02/rocky_trail_02_diff_2k.jpg"), TEXT("T_CoastEarthTrail_BaseColor"), ETextureRole::Color},        // 31, 2.0 m
    {TEXT("Ground/rocky_trail_02/rocky_trail_02_nor_dx_2k.jpg"), TEXT("T_CoastEarthTrail_Normal"), ETextureRole::Normal},
    {TEXT("Ground/rocky_trail_02/rocky_trail_02_rough_2k.jpg"), TEXT("T_CoastEarthTrail_Roughness"), ETextureRole::Roughness},
    {TEXT("Rock/worn_rock_natural_01/worn_rock_natural_01_diff_2k.jpg"), TEXT("T_CoastWornRock_BaseColor"), ETextureRole::Color},   // 34, 2.0 m
    {TEXT("Rock/worn_rock_natural_01/worn_rock_natural_01_nor_dx_2k.jpg"), TEXT("T_CoastWornRock_Normal"), ETextureRole::Normal},
    {TEXT("Rock/worn_rock_natural_01/worn_rock_natural_01_rough_2k.jpg"), TEXT("T_CoastWornRock_Roughness"), ETextureRole::Roughness},
    {TEXT("Rock/rock_face_03/rock_face_03_diff_2k.jpg"), TEXT("T_CoastRockFace_BaseColor"), ETextureRole::Color},               // 37, 2.7 m
    {TEXT("Rock/rock_face_03/rock_face_03_nor_dx_2k.jpg"), TEXT("T_CoastRockFace_Normal"), ETextureRole::Normal},
    {TEXT("Rock/rock_face_03/rock_face_03_rough_2k.jpg"), TEXT("T_CoastRockFace_Roughness"), ETextureRole::Roughness},
    // 40-43: published 8-bit height (disp) maps, used only as height-blend weights (spec §4.1).
    {TEXT("Ground/withered_grass/withered_grass_disp_2k.jpg"), TEXT("T_CoastWitheredGrass_Height"), ETextureRole::Roughness},
    {TEXT("Ground/sparse_grass/sparse_grass_disp_2k.jpg"), TEXT("T_CoastSparseGrass_Height"), ETextureRole::Roughness},
    {TEXT("Ground/dry_decay_leaves/dry_decay_leaves_disp_2k.jpg"), TEXT("T_CoastLeafLitter_Height"), ETextureRole::Roughness},
    {TEXT("Ground/dry_ground_rocks/dry_ground_rocks_disp_2k.jpg"), TEXT("T_CoastStonyGround_Height"), ETextureRole::Roughness},
    // 44-47: ambientCG Foam002 (CC0) for DEV-SEASKY's water foam term; imported here, sampled by them.
    {TEXT("Water/Foam002/Foam002_2K-JPG_Color.jpg"), TEXT("T_CoastFoam_BaseColor"), ETextureRole::Color},
    {TEXT("Water/Foam002/Foam002_2K-JPG_NormalDX.jpg"), TEXT("T_CoastFoam_Normal"), ETextureRole::Normal},
    {TEXT("Water/Foam002/Foam002_2K-JPG_Roughness.jpg"), TEXT("T_CoastFoam_Roughness"), ETextureRole::Roughness},
    {TEXT("Water/Foam002/Foam002_2K-JPG_Opacity.jpg"), TEXT("T_CoastFoam_Opacity"), ETextureRole::Roughness}
};

bool SaveDraft(UObject* Asset)
{
    if (!Asset || !Asset->GetOutermost()->GetName().StartsWith(DestinationRoot)) return false;
    UPackage* Package = Asset->GetOutermost();
    const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
    const FString Allowed = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Development/CoastDraft/"));
    const FString FullFilename = FPaths::ConvertRelativePathToFull(Filename);
    if (!FullFilename.StartsWith(Allowed) || !FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(FullFilename))) return false;
    Asset->MarkPackageDirty();
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;
    const bool bSaved = UPackage::SavePackage(Package, Asset, *FullFilename, SaveArgs);
    if (bSaved) UE_LOG(LogTemp, Display, TEXT("COAST_DRAFT_SAVED %s DRAFT_NOT_USER_CONFIRMED"), *Asset->GetPathName());
    return bSaved;
}

bool ResetDraftExpressions(UMaterial* Material)
{
    if (!Material || !Material->GetOutermost()->GetName().StartsWith(DestinationRoot)) return false;
    // Removing graph expressions mutates the underlying collection. Snapshot the
    // owned nodes first instead of invalidating the engine library's ranged view.
    TArray<UMaterialExpression*> Previous;
    for (UMaterialExpression* Expression : Material->GetExpressions()) Previous.Add(Expression);
    for (UMaterialExpression* Expression : Previous)
    {
        if (!Expression || Expression->GetOuter() != Material) return false;
        if (Expression->IsRooted()) Expression->RemoveFromRoot();
        UMaterialEditingLibrary::DeleteMaterialExpression(Material, Expression);
    }
    return Material->GetExpressions().IsEmpty();
}

template <typename T> T* Node(UMaterial* Material)
{
    return CastChecked<T>(UMaterialEditingLibrary::CreateMaterialExpression(Material, T::StaticClass()));
}

UMaterialExpressionTextureSampleParameter2D* Sample(UMaterial* Material, UTexture2D* Texture,
    const TCHAR* Name, UMaterialExpression* UV, EMaterialSamplerType Sampler)
{
    auto* Result = Node<UMaterialExpressionTextureSampleParameter2D>(Material);
    Result->ParameterName = Name; Result->Texture = Texture; Result->SamplerType = Sampler;
    if (UV) Result->Coordinates.Connect(0, UV);
    return Result;
}

UMaterialExpressionMaterialFunctionCall* Project(UMaterial* Material, UMaterialFunctionInterface* Function,
    UTexture2D* Texture, const TCHAR* Name, EMaterialSamplerType Sampler, float PhysicalSize = 180.0F)
{
    auto* Object = Node<UMaterialExpressionTextureObjectParameter>(Material);
    Object->ParameterName = Name; Object->Texture = Texture; Object->SamplerType = Sampler;
    auto* Size = Node<UMaterialExpressionConstant3Vector>(Material);
    Size->Constant = FLinearColor(PhysicalSize, PhysicalSize, PhysicalSize);
    auto* Call = Node<UMaterialExpressionMaterialFunctionCall>(Material);
    if (!Call->SetMaterialFunction(Function)
        || !UMaterialEditingLibrary::ConnectMaterialExpressions(Object, TEXT(""), Call, TEXT("TextureObject"))
        || !UMaterialEditingLibrary::ConnectMaterialExpressions(Size, TEXT(""), Call, TEXT("TextureSize"))) return nullptr;
    return Call;
}

// Every terrain-following surface shares one land function (DEV-TERRAIN U5/U6).
// Terrain UVs are world cm / 4000, so UTiling = 4000 / physical tile size in cm. Base layers use the
// published physical size (never stretched); habitat weights arrive in vertex colour from
// FPacificCoastHabitat (R exposed grass, G sheltered grass, B woodland, rock = 1 - R - G - B).
struct FLandSurface { UMaterialExpression* Color; UMaterialExpression* Normal; UMaterialExpression* Rough; };
namespace LandCode
{
// Shared HLSL prefix (R2). Habitat weights are resolved PER PIXEL:
//  - vertex colour carries the broad aspect/drainage field from FPacificCoastHabitat;
//  - the interpolated geometric normal adds steep-rock exposure per pixel;
//  - mip-free procedural world-space value noise (1.7/6.5/23/80 m) plus near-field texture height
//    perturbs every weight, then a narrow top-blend turns the smooth vertex gradients into
//    irregular, noise-broken transitions a few metres wide (no soft camouflage blobs).
// Each layer is re-centred on a Bay Area October palette (golden straw/ochre with olive mottling,
// olive-green sheltered grass, dark litter, grey-brown stony ground). Beyond ~40 m the fine layers
// fade to their palette mean so their 2-4 m tiles cannot form a visible repeat; macro texture and
// noise mottling keep far ground textured.
const TCHAR* Weights = TEXT(
    "struct LN { float h(float2 p){ float3 q=frac(float3(p.xyx)*.1031); q+=dot(q,q.yzx+33.33); return frac((q.x+q.y)*q.z);}"
    " float n(float2 p){ float2 i=floor(p),f=frac(p); f=f*f*(3-2*f); return lerp(lerp(h(i),h(i+float2(1,0)),f.x),lerp(h(i+float2(0,1)),h(i+1),f.x),f.y);} };"
    "LN N; float3 L=float3(.30,.59,.11); float2 p=WP.xy;"
    // transition jitter at 1.7 m / 6.5 m; macro mottling at 37 / 113 / 347 m (spec §4.3)
    "float nA=N.n(p/170.0+3.1), nB=N.n(p/650.0-11.7), nC=N.n(p/3700.0+29.3), nD=N.n(p/11300.0-47.9), nE=N.n(p/34700.0+71.3);"
    "float fineMix=1-smoothstep(2000.0,6000.0,D);"
    "float nearFade=1-smoothstep(300.0,1500.0,D);"
    "float normalFade=1-smoothstep(5000.0,8000.0,D);"
    "float swap=smoothstep(.35,.65,nB*.7+nA*.3);"
    // Far x6.8 samples keep only ~half their contrast around the published mean, so no 2-14 m cell
    // pattern (QA 'honeycomb') can read at 20-200 m; procedural mottling supplies the variation.
    "float3 EF2=lerp(float3(.419,.297,.196),EF,.5), SF2=lerp(float3(.080,.048,.008),SF,.5), WF2=lerp(float3(.173,.076,.035),WF,.5), RF2=lerp(float3(.306,.178,.080),RF,.45);"
    "float3 EC=lerp(lerp(ED,E2D,swap),EF2,1-fineMix), SC=lerp(lerp(SD,S2D,1-swap),SF2,1-fineMix);"
    "float3 WC=lerp(WD,WF2,1-fineMix), RC=lerp(RD,RF2,1-fineMix);"
    "float3 w3=saturate(VC.rgb); float4 w=float4(w3,saturate(1-w3.r-w3.g-w3.b));"
    "float steep=smoothstep(.87,.70,VN.z+(nB-.5)*.10);"
    "w.w=max(w.w,steep); w.z=max(w.z,LIT*.85); w/=max(dot(w,1),1e-3);"
    // R7: steepness from the true facet (screen derivatives of world position), not only the
    // interpolated vertex normal, so every near-vertical cap facet takes world projection.
    "float3 facet=normalize(cross(ddx(WP),ddy(WP))); float cliffFace=smoothstep(.55,.32,min(VN.z,abs(facet.z)));"
    "float4 jitter=float4(nB*.55+nC*.45, nC*.6+nA*.4, nB*.5+nD*.5, nA*.6+nB*.4)-.5;"
    "float4 h=float4(EH.r,SH.r,WH.r,RH.r)-.5;"
    "float4 hw=w+jitter*.62*saturate(w*3+.1)+h*.35*fineMix*saturate(w*4);"
    "float top=max(max(hw.x,hw.y),max(hw.z,hw.w))-.16;"
    "float4 b=max(hw-top,0); b/=max(dot(b,1),1e-4);"
    // R11 (ownership probe: the rust 'plank' band is land at 47-50 deg drawn by the planar stony
    // layer): facet-true rock projection wherever the slope exceeds ~35 deg (soft 30-45 deg) or the
    // stony/rock layer wins the habitat blend.
    "float rockMask=max(cliffFace,max(smoothstep(.866,.707,abs(facet.z)),smoothstep(.45,.65,b.w)));");
}
FLandSurface BuildLandSurface(UMaterial* Material, const TArray<UTexture2D*>& Textures, bool bLitterMask, UMaterialFunctionInterface* AlignedTexture)
{
    auto UV=[Material](float PhysicalCm){auto* C=Node<UMaterialExpressionTextureCoordinate>(Material);C->UTiling=C->VTiling=4000.0F/PhysicalCm;return C;};
    // Rotated, non-commensurate tiling for anti-repetition and the 0.37 m detail layer.
    auto Rotated=[Material](float PhysicalCm,float Radians)
    {
        auto* Base=Node<UMaterialExpressionTextureCoordinate>(Material);
        auto* R=Node<UMaterialExpressionCustom>(Material);R->OutputType=CMOT_Float2;
        R->Code=FString::Printf(TEXT("float c=%f,s=%f; return float2(c*UV.x-s*UV.y,s*UV.x+c*UV.y)*%f+float2(.37,.61);"),FMath::Cos(Radians),FMath::Sin(Radians),4000.0F/PhysicalCm);
        FCustomInput In;In.InputName=TEXT("UV");In.Input.Connect(0,Base);R->Inputs.Add(In);return R;
    };
    auto Shared=[](UMaterialExpressionTextureSampleParameter2D* S){S->SamplerSource=SSM_Wrap_WorldGroupSettings;return S;};
    auto Col=[&](int32 Index,const TCHAR* Name,UMaterialExpression* C){return Shared(Sample(Material,Textures[Index],Name,C,SAMPLERTYPE_Color));};
    auto Nor=[&](int32 Index,const TCHAR* Name,UMaterialExpression* C){return Shared(Sample(Material,Textures[Index],Name,C,SAMPLERTYPE_Normal));};
    auto Rgh=[&](int32 Index,const TCHAR* Name,UMaterialExpression* C){return Shared(Sample(Material,Textures[Index],Name,C,SAMPLERTYPE_LinearGrayscale));};
    UMaterialExpression* Grass=UV(200.0F);UMaterialExpression* Stone=UV(400.0F);
    // Spec §4.3: near dual sample at 1.37x scale + 37 deg; far repeat x6.8 (also rotated) beyond ~40 m.
    const float Turn=FMath::DegreesToRadians(37.0F);
    UMaterialExpression* Grass2=Rotated(274.0F,Turn);
    UMaterialExpression* GrassFar=Rotated(1360.0F,Turn);UMaterialExpression* StoneFar=Rotated(2720.0F,Turn);
    UMaterialExpression* Detail=UV(50.0F);UMaterialExpression* Detail2=Rotated(37.0F,Turn);
    struct FIn{const TCHAR* Name;UMaterialExpression* Node;};
    const TArray<FIn> Inputs={
        {TEXT("ED"),Col(16,TEXT("ExposedGrassColor"),Grass)},{TEXT("EN"),Nor(17,TEXT("ExposedGrassNormal"),Grass)},{TEXT("ER"),Rgh(18,TEXT("ExposedGrassRoughness"),Grass)},
        {TEXT("E2D"),Col(16,TEXT("ExposedGrassColorB"),Grass2)},{TEXT("E2N"),Nor(17,TEXT("ExposedGrassNormalB"),Grass2)},{TEXT("EF"),Col(16,TEXT("ExposedGrassColorFar"),GrassFar)},{TEXT("EH"),Rgh(40,TEXT("ExposedGrassHeight"),Grass)},
        {TEXT("SD"),Col(19,TEXT("ShelteredGrassColor"),Grass)},{TEXT("SN"),Nor(20,TEXT("ShelteredGrassNormal"),Grass)},{TEXT("SR"),Rgh(21,TEXT("ShelteredGrassRoughness"),Grass)},
        {TEXT("S2D"),Col(19,TEXT("ShelteredGrassColorB"),Grass2)},{TEXT("S2N"),Nor(20,TEXT("ShelteredGrassNormalB"),Grass2)},{TEXT("SF"),Col(19,TEXT("ShelteredGrassColorFar"),GrassFar)},{TEXT("SH"),Rgh(41,TEXT("ShelteredGrassHeight"),Grass)},
        {TEXT("WD"),Col(22,TEXT("WoodlandLitterColor"),Grass)},{TEXT("WN"),Nor(23,TEXT("WoodlandLitterNormal"),Grass)},{TEXT("WR"),Rgh(24,TEXT("WoodlandLitterRoughness"),Grass)},
        {TEXT("WF"),Col(22,TEXT("WoodlandLitterColorFar"),GrassFar)},{TEXT("WH"),Rgh(42,TEXT("WoodlandLitterHeight"),Grass)},
        {TEXT("RD"),Col(25,TEXT("StonyGroundColor"),Stone)},{TEXT("RN"),Nor(26,TEXT("StonyGroundNormal"),Stone)},{TEXT("RR"),Rgh(27,TEXT("StonyGroundRoughness"),Stone)},
        {TEXT("RF"),Col(25,TEXT("StonyGroundColorFar"),StoneFar)},{TEXT("RH"),Rgh(43,TEXT("StonyGroundHeight"),Stone)},
        {TEXT("DN"),Nor(28,TEXT("DetailNormal"),Detail)},{TEXT("DR"),Rgh(29,TEXT("DetailRoughness"),Detail)},
        {TEXT("DN2"),Nor(28,TEXT("DetailNormalB"),Detail2)},{TEXT("DR2"),Rgh(29,TEXT("DetailRoughnessB"),Detail2)},
        // Macro modulation only (albedo/roughness), spec §4.3 R2: aerial scan at 173 m rotated 23 deg and
        // the legacy rocky_terrain_02 scan at 409 m. No macro normal (the stretched 90 m normal
        // produced the 5-10 m "dune ripple" reported by QA).
        {TEXT("M1"),Col(30,TEXT("MacroVariation"),Rotated(17300.0F,FMath::DegreesToRadians(23.0F)))},
        {TEXT("M2"),Col(0,TEXT("LandBaseColorTexture"),UV(40900.0F))}};
    auto* Cover=Node<UMaterialExpressionVertexColor>(Material);
    auto* Depth=Node<UMaterialExpressionPixelDepth>(Material);
    // R5a: near-vertical cap faces (cove lip, gully walls) would smear the XY-planar ground UVs into
    // vertical streaks; they take world-aligned triplanar sandstone (same as the cliff) instead.
    auto Pass=[Material](UMaterialExpression* Call){auto* M=Node<UMaterialExpressionMultiply>(Material);UMaterialEditingLibrary::ConnectMaterialExpressions(Call,TEXT("XYZ Texture"),M,TEXT("A"));M->ConstB=1.0F;return M;};
    // R10 self-check (shore: vertical streaks on the cove headscarp): WorldAlignedTexture weights its
    // projections by the interpolated vertex normal, which on the coarse cap disagrees with the steep
    // facet. Steep land faces use facet-true triplanar weights from screen derivatives instead.
    auto FacetTriplanar=[Material](UTexture2D* Texture,const TCHAR* Name,float PhysicalCm)->UMaterialExpression*
    {
        auto* Object=Node<UMaterialExpressionTextureObjectParameter>(Material);
        Object->ParameterName=Name;Object->Texture=Texture;Object->SamplerType=SAMPLERTYPE_Color;
        auto* World=Node<UMaterialExpressionWorldPosition>(Material);
        auto* Tri=Node<UMaterialExpressionCustom>(Material);Tri->OutputType=CMOT_Float3;
        Tri->Code=FString::Printf(TEXT("float3 f=normalize(cross(ddx(P),ddy(P))); float3 w=pow(abs(f),4.0); w/=max(dot(w,1),1e-4); float s=1.0/%f;"
            "return Texture2DSample(Tex,TexSampler,P.yz*s).rgb*w.x+Texture2DSample(Tex,TexSampler,P.xz*s).rgb*w.y+Texture2DSample(Tex,TexSampler,P.xy*s).rgb*w.z;"),PhysicalCm);
        {FCustomInput I;I.InputName=TEXT("P");I.Input.Connect(0,World);Tri->Inputs.Add(I);}
        {FCustomInput I;I.InputName=TEXT("Tex");I.Input.Connect(0,Object);Tri->Inputs.Add(I);}
        return Tri;
    };
    UMaterialExpression* SteepColor=FacetTriplanar(Textures[34],TEXT("SteepFaceColor"),350.0F);
    UMaterialExpression* SteepRough=Pass(Project(Material,AlignedTexture,Textures[36],TEXT("SteepFaceRoughness"),SAMPLERTYPE_LinearGrayscale,350.0F));
    // R9 self-check: the cove headscarp (steep land faces) still read pale gold next to the cliff. Steep
    // faces now take the cliff's own macro tone (cliff_side, world-aligned at 23 m) with the sandstone
    // only as luminance detail, matching the rock material's tinted colour.
    UMaterialExpression* SteepMacro=FacetTriplanar(Textures[3],TEXT("SteepFaceMacro"),2300.0F);
    // Trunk-base litter (DEV-FLORA GetForestLitterMask) arrives as 1 - vertex alpha on terrain;
    // trail surfaces use their alpha as the worn-core mask, so they read no litter.
    UMaterialExpression* LitterAlpha=nullptr;
    if(bLitterMask){auto* Inv=Node<UMaterialExpressionCustom>(Material);Inv->OutputType=CMOT_Float1;Inv->Code=TEXT("return saturate(1-A);");
        FCustomInput I;I.InputName=TEXT("A");I.Input.Connect(4,Cover);Inv->Inputs.Add(I);LitterAlpha=Inv;}
    else{auto* Zero=Node<UMaterialExpressionScalarParameter>(Material);Zero->ParameterName=TEXT("NoLitter");Zero->DefaultValue=0.0F;LitterAlpha=Zero;}
    auto* World=Node<UMaterialExpressionWorldPosition>(Material);
    auto* GeometryNormal=Node<UMaterialExpressionVertexNormalWS>(Material);
    auto Make=[&](ECustomMaterialOutputType Type,const FString& Body)
    {
        auto* C=Node<UMaterialExpressionCustom>(Material);C->OutputType=Type;C->Code=FString(LandCode::Weights)+Body;
        for(const FIn& In:Inputs){FCustomInput Item;Item.InputName=In.Name;Item.Input.Connect(0,In.Node);C->Inputs.Add(Item);}
        FCustomInput V;V.InputName=TEXT("VC");V.Input.Connect(0,Cover);C->Inputs.Add(V);
        FCustomInput DIn;DIn.InputName=TEXT("D");DIn.Input.Connect(0,Depth);C->Inputs.Add(DIn);
        FCustomInput WIn;WIn.InputName=TEXT("WP");WIn.Input.Connect(0,World);C->Inputs.Add(WIn);
        FCustomInput NIn;NIn.InputName=TEXT("VN");NIn.Input.Connect(0,GeometryNormal);C->Inputs.Add(NIn);
        FCustomInput TIn;TIn.InputName=TEXT("TRI");TIn.Input.Connect(0,SteepColor);C->Inputs.Add(TIn);
        FCustomInput TRIn;TRIn.InputName=TEXT("TRR");TRIn.Input.Connect(0,SteepRough);C->Inputs.Add(TRIn);
        FCustomInput TMIn;TMIn.InputName=TEXT("TRM");TMIn.Input.Connect(0,SteepMacro);C->Inputs.Add(TMIn);
        FCustomInput LIn;LIn.InputName=TEXT("LIT");LIn.Input.Connect(0,LitterAlpha);C->Inputs.Add(LIn);
        return C;
    };
    // Palette re-centring: texture / published mean * target (linear). Means measured from the
    // 2K sources: withered .419/.297/.196, sparse .080/.048/.008, litter .173/.076/.035, stony .306/.178/.080.
    // October Bay Area grass palette (spec R2, linear): straw .28/.22/.12 (most), ochre .30/.205/.095
    // (~15%), olive mottling .135/.13/.065 (~20%); R/G <= 1.5, mean luminance ~.2. Stony ground is
    // desaturated (the raw dry_ground_rocks mean R/G 1.72 read as desert sand).
    auto* Color=Make(CMOT_Float3,TEXT(
        "float mot=nC*.5+nD*.3+nA*.2;"
        "float3 eT=float3(.28,.22,.12); eT=lerp(eT,float3(.30,.205,.095),smoothstep(.30,.20,mot)); eT=lerp(eT,float3(.135,.13,.065),smoothstep(.66,.76,mot));"
        "float3 sT=lerp(float3(.16,.155,.075),float3(.10,.125,.05),nC);"
        "float3 wT=lerp(lerp(float3(.10,.078,.048),float3(.066,.08,.04),nD),float3(.20,.14,.08)*(.85+.3*nA),LIT);"
        // R3: re-centre by luminance with only clamped partial chroma, so a near-zero source channel
        // (sparse_grass blue mean .008) cannot be amplified into slate-blue; stony ground is a darker
        // weathered grey-brown (QA: grey-white "salt" patches).
        "float3 rT=float3(.165,.135,.098)*(.85+.3*nA);"
        "float3 e=eT*lerp(dot(EC,L)/.31,clamp(EC/float3(.419,.297,.196),.5,1.8),.5);"
        "float3 s=sT*lerp(dot(SC,L)/.0533,clamp(SC/float3(.080,.048,.008),.6,1.6),.25);"
        "float3 wl=wT*lerp(dot(WC,L)/.0976,clamp(WC/float3(.173,.076,.035),.6,1.6),.35);"
        "float3 r=rT*clamp(dot(RC,L)/.2073,.55,1.25);"
        "float3 c=e*b.x+s*b.y+wl*b.z+r*b.w;"
        "float m1=clamp(dot(M1,L)/.13,.65,1.4), m2=clamp(dot(M2,L)/.077,.75,1.3);"
        "c*=lerp(1,m1*m2,.25+.3*(1-fineMix));"
        "c*=1+.24*(float3(nC,nD,nE)-.5);"
        // R8: steep cap faces match the cliff rock tone (was a pale-gold sandstone that read untextured).
        "float3 rockTone=lerp(TRI,TRM,.72)*float3(.58,.64,.68)*float3(1.12,1.06,.92)*clamp(dot(TRI,L)/.33,.7,1.3);"
        "c=lerp(c,rockTone,rockMask);"
        "return min(c,float3(.6,.5,.4));"));
    auto* Normal=Make(CMOT_Float3,TEXT(
        // R3: grass normals mix a 1.37x/37 deg second sample by noise (no periodic bands in
        // lighting_only); detail normal strength is noise-modulated and fades by ~8 m.
        "float3 en=lerp(EN,E2N,swap), sn=lerp(SN,S2N,1-swap);"
        "float3 n=en*b.x+sn*b.y+WN*b.z+RN*b.w; n.xy*=lerp(.15,1,normalFade);"
        "float detailFade=1-smoothstep(200.0,800.0,D);"
        "float2 d=(DN.xy*lerp(.25,.6,nA)+DN2.xy*lerp(.6,.25,nA))*detailFade*.75;"
        "n.xy*=1-.6*rockMask; d*=1-.6*rockMask;"
        "return normalize(float3(n.xy+d,max(n.z,.2)));"));
    auto* Rough=Make(CMOT_Float1,TEXT(
        "float r=lerp(.86,ER.r,fineMix)*b.x+lerp(.84,SR.r,fineMix)*b.y+lerp(.9,WR.r,fineMix)*b.z+lerp(.8,RR.r,fineMix)*b.w;"
        "r*=lerp(1,(DR.r+DR2.r)*.67,nearFade*.5);"
        "r*=lerp(.92,1.08,nC);"
        "r=lerp(r,TRR.r,rockMask);"
        "return saturate(r);"));
    return {Color,Normal,Rough};
}

bool BuildMaterial(const TCHAR* Name, const TArray<UTexture2D*>& Textures, int32 Base, int32 Normal, int32 Roughness,
    int32 Alpha, bool bWorldProjection, float UVScale, UMaterialFunctionInterface* AlignedTexture, UMaterialFunctionInterface* AlignedNormal)
{
    const FString PackageName = FString(DestinationRoot) + TEXT("Materials/") + Name;
    UPackage* Package = CreatePackage(*PackageName);
    Package->FullyLoad();
    UMaterial* Material = FindObject<UMaterial>(Package, Name);
    const bool bNew = Material == nullptr;
    if (bNew) Material = NewObject<UMaterial>(Package, Name, RF_Public | RF_Standalone);
    if (!Material) return false;
    Material->PreEditChange(nullptr);
    if (!ResetDraftExpressions(Material)) return false;
    Material->BlendMode = Alpha >= 0 ? BLEND_Masked : BLEND_Opaque;
    Material->TwoSided = Alpha >= 0;
    Material->OpacityMaskClipValue = .33F;
    Material->bTangentSpaceNormal = !bWorldProjection;
    Material->SetShadingModel(Alpha >= 0 ? MSM_TwoSidedFoliage : MSM_DefaultLit);
    auto* Tint = Node<UMaterialExpressionVectorParameter>(Material);
    Tint->ParameterName = TEXT("Color"); Tint->DefaultValue = FLinearColor::White;
    auto* RoughnessFactor = Node<UMaterialExpressionScalarParameter>(Material);
    RoughnessFactor->ParameterName = TEXT("Roughness"); RoughnessFactor->DefaultValue = 1.0F;
    auto* UV = Node<UMaterialExpressionTextureCoordinate>(Material);
    UV->UTiling = UVScale; UV->VTiling = UVScale;
    UMaterialExpression* BaseExpression;
    UMaterialExpression* NormalExpression;
    UMaterialExpression* RoughnessExpression;
    FString BaseOutput, NormalOutput, RoughnessOutput;
    if (bWorldProjection)
    {
        // Near: worn_rock_natural_01 triplanar at its published 2.0 m; broad structure from cliff_side below.
        // R3: used at 3.5 m (spec 2-4 m) so the 65 m face is not read as a fine repeating knit.
        BaseExpression = Project(Material, AlignedTexture, Textures[Base], TEXT("BaseColorTexture"), SAMPLERTYPE_Color, 350.0F);
        NormalExpression = Project(Material, AlignedNormal, Textures[Normal], TEXT("NormalTexture"), SAMPLERTYPE_Normal, 350.0F);
        RoughnessExpression = Project(Material, AlignedTexture, Textures[Roughness], TEXT("RoughnessTexture"), SAMPLERTYPE_LinearGrayscale, 350.0F);
        BaseOutput = NormalOutput = RoughnessOutput = TEXT("XYZ Texture");
        if (!BaseExpression || !NormalExpression || !RoughnessExpression) return false;
        // Retain physical 1.8 m scan detail, with separate broad colour/normal
        // structure so a 65 m cliff does not become 36 identical horizontal rows.
        auto* MacroColor = Project(Material, AlignedTexture, Textures[3], TEXT("RockMacroColor"), SAMPLERTYPE_Color, 2300.0F);
        auto* MacroNormal = Project(Material, AlignedNormal, Textures[4], TEXT("RockMacroNormal"), SAMPLERTYPE_Normal, 1100.0F);
        if (!MacroColor || !MacroNormal) return false;
        auto* ColorBlend = Node<UMaterialExpressionLinearInterpolate>(Material);
        ColorBlend->ConstAlpha = .72F;
        auto* NormalBlend = Node<UMaterialExpressionLinearInterpolate>(Material);
        NormalBlend->ConstAlpha = .65F;
        if (!UMaterialEditingLibrary::ConnectMaterialExpressions(BaseExpression, BaseOutput, ColorBlend, TEXT("A"))
            || !UMaterialEditingLibrary::ConnectMaterialExpressions(MacroColor, TEXT("XYZ Texture"), ColorBlend, TEXT("B"))
            || !UMaterialEditingLibrary::ConnectMaterialExpressions(NormalExpression, NormalOutput, NormalBlend, TEXT("A"))
            || !UMaterialEditingLibrary::ConnectMaterialExpressions(MacroNormal, TEXT("XYZ Texture"), NormalBlend, TEXT("B"))) return false;
        auto* UnitNormal = Node<UMaterialExpressionNormalize>(Material);
        UnitNormal->VectorInput.Connect(0, NormalBlend);
        BaseExpression = ColorBlend; NormalExpression = UnitNormal;
        BaseOutput = NormalOutput = TEXT("");
        // Spec R2: worn sandstone (pulled toward pale tan) forms the upper sea-cliff face; fractured
        // rock_face_03 (published 2.7 m, used at 4 m) takes over on the lower cliff, slumps and talus
        // with a noise-broken boundary ~35-45 m above the sea.
        // R3 (QA fish-scale): rock_face_03 at 7 m and 18.3 m (x2.6), mixed by low-frequency world
        // noise, so no single few-metre cell repeats; colour carries irregular horizontal strata.
        auto* FaceColorNear = Project(Material, AlignedTexture, Textures[37], TEXT("LowerCliffColor"), SAMPLERTYPE_Color, 700.0F);
        auto* FaceNormalNear = Project(Material, AlignedNormal, Textures[38], TEXT("LowerCliffNormal"), SAMPLERTYPE_Normal, 700.0F);
        auto* FaceRough = Project(Material, AlignedTexture, Textures[39], TEXT("LowerCliffRoughness"), SAMPLERTYPE_LinearGrayscale, 700.0F);
        auto* FaceColorBroad = Project(Material, AlignedTexture, Textures[37], TEXT("LowerCliffColorBroad"), SAMPLERTYPE_Color, 1830.0F);
        auto* FaceNormalBroad = Project(Material, AlignedNormal, Textures[38], TEXT("LowerCliffNormalBroad"), SAMPLERTYPE_Normal, 1830.0F);
        if (!FaceColorNear || !FaceNormalNear || !FaceRough || !FaceColorBroad || !FaceNormalBroad) return false;
        auto* CliffWorld = Node<UMaterialExpressionWorldPosition>(Material);
        auto* FaceMix = Node<UMaterialExpressionCustom>(Material); FaceMix->OutputType = CMOT_Float1;
        FaceMix->Code = TEXT("return smoothstep(-.35,.35,sin(P.x*.00071+P.z*.0011+sin(P.y*.00093)*2.3)*.6+sin(P.y*.0016-P.z*.0007)*.4);");
        { FCustomInput I; I.InputName = TEXT("P"); I.Input.Connect(0, CliffWorld); FaceMix->Inputs.Add(I); }
        auto* FaceColorMix = Node<UMaterialExpressionLinearInterpolate>(Material);
        bool bMix = UMaterialEditingLibrary::ConnectMaterialExpressions(FaceColorNear, TEXT("XYZ Texture"), FaceColorMix, TEXT("A"));
        bMix &= UMaterialEditingLibrary::ConnectMaterialExpressions(FaceColorBroad, TEXT("XYZ Texture"), FaceColorMix, TEXT("B"));
        FaceColorMix->Alpha.Connect(0, FaceMix);
        auto* FaceNormalMix = Node<UMaterialExpressionLinearInterpolate>(Material);
        bMix &= UMaterialEditingLibrary::ConnectMaterialExpressions(FaceNormalNear, TEXT("XYZ Texture"), FaceNormalMix, TEXT("A"));
        bMix &= UMaterialEditingLibrary::ConnectMaterialExpressions(FaceNormalBroad, TEXT("XYZ Texture"), FaceNormalMix, TEXT("B"));
        FaceNormalMix->Alpha.Connect(0, FaceMix);
        auto* Strata = Node<UMaterialExpressionCustom>(Material); Strata->OutputType = CMOT_Float3;
        Strata->Code = TEXT("float w=P.z+260*sin(P.y*.0009+sin(P.x*.0013)*1.7)+90*sin(P.y*.0041); float b=sin(w/230.0)*.6+sin(w/83.0+1.3)*.4; return C*(1+.16*b);");
        { FCustomInput I; I.InputName = TEXT("P"); I.Input.Connect(0, CliffWorld); Strata->Inputs.Add(I); }
        { FCustomInput I; I.InputName = TEXT("C"); I.Input.Connect(0, FaceColorMix); Strata->Inputs.Add(I); }
        if (!bMix) return false;
        UMaterialExpression* FaceColor = Strata; UMaterialExpression* FaceNormal = FaceNormalMix;
        auto* Lower = Node<UMaterialExpressionCustom>(Material); Lower->OutputType = CMOT_Float1;
        Lower->Code = TEXT("float n=sin(P.x*.0021+sin(P.y*.0013)*2.1)*.5+sin(P.y*.0047+P.x*.0011)*.5; return 1-smoothstep(-3200.0,-2400.0,P.z+n*500.0);");
        { FCustomInput I; I.InputName = TEXT("P"); I.Input.Connect(0, CliffWorld); Lower->Inputs.Add(I); }
        auto* Tan = Node<UMaterialExpressionMultiply>(Material); Tan->A.Connect(0, BaseExpression);
        auto* TanTint = Node<UMaterialExpressionConstant3Vector>(Material); TanTint->Constant = FLinearColor(1.12F, 1.06F, .92F); Tan->B.Connect(0, TanTint);
        auto* CliffColor = Node<UMaterialExpressionLinearInterpolate>(Material); CliffColor->A.Connect(0, Tan);
        CliffColor->B.Connect(0, FaceColor); bool bFace = true;
        CliffColor->Alpha.Connect(0, Lower);
        auto* CliffNormal = Node<UMaterialExpressionLinearInterpolate>(Material); CliffNormal->A.Connect(0, NormalExpression);
        CliffNormal->B.Connect(0, FaceNormal);
        CliffNormal->Alpha.Connect(0, Lower);
        auto* CliffUnit = Node<UMaterialExpressionNormalize>(Material); CliffUnit->VectorInput.Connect(0, CliffNormal);
        auto* CliffRough = Node<UMaterialExpressionLinearInterpolate>(Material);
        bFace &= UMaterialEditingLibrary::ConnectMaterialExpressions(RoughnessExpression, RoughnessOutput, CliffRough, TEXT("A"));
        bFace &= UMaterialEditingLibrary::ConnectMaterialExpressions(FaceRough, TEXT("XYZ Texture"), CliffRough, TEXT("B"));
        CliffRough->Alpha.Connect(0, Lower);
        if (!bFace) return false;
        // R5a P0 (QA lighting_only: scan face shaded as a flat panel): WorldAlignedNormal returns an
        // axis-aligned world normal that REPLACES the geometric normal in this world-space material,
        // erasing the scan's real relief. Re-base the triplanar detail on the interpolated vertex
        // normal so geometry shading survives and the texture only perturbs it.
        auto* GeometryNormal = Node<UMaterialExpressionVertexNormalWS>(Material);
        auto* Rebased = Node<UMaterialExpressionCustom>(Material); Rebased->OutputType = CMOT_Float3;
        // The flat-texture response of the triplanar blend is the weighted signed axis set; subtracting
        // that smooth blend (not a hard dominant axis) leaves only the texture perturbation.
        Rebased->Code = TEXT("float3 w=pow(abs(VN),4.0); w/=max(dot(w,1),1e-4); float3 flat=normalize(sign(VN)*w+1e-5);"
            "return normalize(VN+(normalize(TN)-flat)*.85);");
        { FCustomInput I; I.InputName = TEXT("VN"); I.Input.Connect(0, GeometryNormal); Rebased->Inputs.Add(I); }
        { FCustomInput I; I.InputName = TEXT("TN"); I.Input.Connect(0, CliffUnit); Rebased->Inputs.Add(I); }
        // R8 (QA: streak strip near the waterline): WorldAlignedTexture weights its projections by the
        // interpolated vertex normal; where a crease makes that normal disagree with the true facet
        // (e.g. talus -> vertical face), the top projection smears vertically. There the colour is
        // re-projected with facet-true triplanar weights from screen derivatives of world position.
        auto* FacetTexture = Node<UMaterialExpressionTextureObjectParameter>(Material);
        FacetTexture->ParameterName = TEXT("FacetRockColor"); FacetTexture->Texture = Textures[Base]; FacetTexture->SamplerType = SAMPLERTYPE_Color;
        auto* FacetColor = Node<UMaterialExpressionCustom>(Material); FacetColor->OutputType = CMOT_Float3;
        // R10: any strong vertex/facet disagreement (also horizontal slivers under sideways vertex
        // normals, the orange waterline strip) takes the facet-true projection.
        FacetColor->Code = TEXT("float3 f=normalize(cross(ddx(P),ddy(P))); float mis=saturate(max((abs(VN.z)-abs(f.z))*2.5,(abs(f.z)-abs(VN.z))*2.5));"
            "float3 w=pow(abs(f),4.0); w/=max(dot(w,1),1e-4); float s=1.0/350.0;"
            "float3 t=Texture2DSample(Tex,TexSampler,P.yz*s).rgb*w.x+Texture2DSample(Tex,TexSampler,P.xz*s).rgb*w.y+Texture2DSample(Tex,TexSampler,P.xy*s).rgb*w.z;"
            // R9: keep the face's own tone (macro cliff_side colour); the re-projected sample only
            // contributes luminance detail, so these pixels never turn pale, untextured gold.
            "float detail=clamp(dot(t,float3(.3,.59,.11))/.33,.6,1.4); return lerp(C,M*float3(1.12,1.06,.92)*detail,mis);");
        { FCustomInput I; I.InputName = TEXT("P"); I.Input.Connect(0, CliffWorld); FacetColor->Inputs.Add(I); }
        { FCustomInput I; I.InputName = TEXT("VN"); I.Input.Connect(0, GeometryNormal); FacetColor->Inputs.Add(I); }
        { FCustomInput I; I.InputName = TEXT("C"); I.Input.Connect(0, CliffColor); FacetColor->Inputs.Add(I); }
        { FCustomInput I; I.InputName = TEXT("Tex"); I.Input.Connect(0, FacetTexture); FacetColor->Inputs.Add(I); }
        {
            auto* MacroPass = Node<UMaterialExpressionMultiply>(Material); MacroPass->ConstB = 1.0F;
            UMaterialEditingLibrary::ConnectMaterialExpressions(MacroColor, TEXT("XYZ Texture"), MacroPass, TEXT("A"));
            FCustomInput I; I.InputName = TEXT("M"); I.Input.Connect(0, MacroPass); FacetColor->Inputs.Add(I);
        }
        BaseExpression = FacetColor; NormalExpression = Rebased; RoughnessExpression = CliffRough; RoughnessOutput = TEXT("");
    }
    else
    {
        BaseExpression = Sample(Material, Textures[Base], TEXT("BaseColorTexture"), UV, SAMPLERTYPE_Color);
        NormalExpression = Sample(Material, Textures[Normal], TEXT("NormalTexture"), UV, SAMPLERTYPE_Normal);
        RoughnessExpression = Sample(Material, Textures[Roughness], TEXT("RoughnessTexture"), UV, SAMPLERTYPE_LinearGrayscale);
        BaseOutput = NormalOutput = TEXT("RGB"); RoughnessOutput = TEXT("R");
    }
    auto* ColorProduct = Node<UMaterialExpressionMultiply>(Material);
    auto* RoughnessProduct = Node<UMaterialExpressionMultiply>(Material);
    bool bConnected = UMaterialEditingLibrary::ConnectMaterialExpressions(BaseExpression, BaseOutput, ColorProduct, TEXT("A"));
    ColorProduct->B.Connect(0, Tint);
    bConnected &= UMaterialEditingLibrary::ConnectMaterialExpressions(RoughnessExpression, RoughnessOutput, RoughnessProduct, TEXT("A"));
    RoughnessProduct->B.Connect(0, RoughnessFactor);
    UMaterialExpression* SurfaceColor=ColorProduct;
    UMaterialExpression* SurfaceNormal=NormalExpression;UMaterialExpression* SurfaceRoughness=RoughnessProduct;
    FString SurfaceNormalOutput=NormalOutput;
    if(FString(Name).Contains(TEXT("Ground")))
    {
        const FLandSurface Land=BuildLandSurface(Material,Textures,true,AlignedTexture);
        auto* TintLand=Node<UMaterialExpressionMultiply>(Material);TintLand->A.Connect(0,Land.Color);TintLand->B.Connect(0,Tint);
        auto* RoughLand=Node<UMaterialExpressionMultiply>(Material);RoughLand->A.Connect(0,Land.Rough);RoughLand->B.Connect(0,RoughnessFactor);
        SurfaceColor=TintLand;SurfaceNormal=Land.Normal;SurfaceRoughness=RoughLand;SurfaceNormalOutput=TEXT("");
    }
    if(FString(Name).Contains(TEXT("Gravel")))
    {
        // Launch apron keeps the fine sandy gravel; trails (PathEarth=1, set per component at runtime)
        // become a compacted earth path. The worn core meets the habitat ground through a
        // height-blended, irregular shoulder driven by vertex alpha rather than a flat strip edge.
        auto* Mask=Node<UMaterialExpressionVertexColor>(Material);
        const FLandSurface Land=BuildLandSurface(Material,Textures,false,AlignedTexture);
        auto* TrailUV=Node<UMaterialExpressionTextureCoordinate>(Material);TrailUV->UTiling=TrailUV->VTiling=4000.0F/200.0F;
        auto* TrailColor=Sample(Material,Textures[31],TEXT("TrailColor"),TrailUV,SAMPLERTYPE_Color);TrailColor->SamplerSource=SSM_Wrap_WorldGroupSettings;
        auto* TrailNormal=Sample(Material,Textures[32],TEXT("TrailNormal"),TrailUV,SAMPLERTYPE_Normal);TrailNormal->SamplerSource=SSM_Wrap_WorldGroupSettings;
        auto* TrailRough=Sample(Material,Textures[33],TEXT("TrailRoughness"),TrailUV,SAMPLERTYPE_LinearGrayscale);TrailRough->SamplerSource=SSM_Wrap_WorldGroupSettings;
        auto* PathEarth=Node<UMaterialExpressionScalarParameter>(Material);PathEarth->ParameterName=TEXT("PathEarth");PathEarth->DefaultValue=0.0F;
        auto* Core=Node<UMaterialExpressionCustom>(Material);Core->OutputType=CMOT_Float4;
        // Core alpha is height-biased by the core texture inside the transition (irregular edge).
        Core->Code=TEXT("float3 L=float3(.3,.59,.11); float3 c=lerp(G,T*float3(.92,.86,.80),P); float hgt=dot(c,L)*1.6;"
            "float a=saturate((A*1.35-.18)+(hgt-.5)*.55*saturate(A*(1-A)*4)); return float4(c,a);");
        auto In=[](UMaterialExpressionCustom* C,const TCHAR* N,UMaterialExpression* E,int32 Out){FCustomInput I;I.InputName=N;I.Input.Connect(Out,E);C->Inputs.Add(I);};
        In(Core,TEXT("G"),ColorProduct,0);In(Core,TEXT("T"),TrailColor,0);In(Core,TEXT("P"),PathEarth,0);In(Core,TEXT("A"),Mask,4);
        auto* CoreColor=Node<UMaterialExpressionCustom>(Material);CoreColor->OutputType=CMOT_Float3;CoreColor->Code=TEXT("return V.rgb;");In(CoreColor,TEXT("V"),Core,0);
        auto* CoreAlpha=Node<UMaterialExpressionCustom>(Material);CoreAlpha->OutputType=CMOT_Float1;CoreAlpha->Code=TEXT("return V.a;");In(CoreAlpha,TEXT("V"),Core,0);
        // Trampled shoulder: the land beside the core is slightly darker and smoother (worn grass).
        auto* Worn=Node<UMaterialExpressionCustom>(Material);Worn->OutputType=CMOT_Float3;
        Worn->Code=TEXT("float s=saturate(1-abs(A-.3)*3.0)*(.35+.65*P); return C*lerp(1,.78,s);");
        In(Worn,TEXT("C"),Land.Color,0);In(Worn,TEXT("A"),Mask,4);In(Worn,TEXT("P"),PathEarth,0);
        auto* Blend=Node<UMaterialExpressionLinearInterpolate>(Material);Blend->A.Connect(0,Worn);Blend->B.Connect(0,CoreColor);Blend->Alpha.Connect(0,CoreAlpha);SurfaceColor=Blend;
        auto* CoreNormal=Node<UMaterialExpressionLinearInterpolate>(Material);CoreNormal->A.Connect(0,NormalExpression);CoreNormal->B.Connect(0,TrailNormal);CoreNormal->Alpha.Connect(0,PathEarth);
        auto* BlendNormal=Node<UMaterialExpressionLinearInterpolate>(Material);BlendNormal->A.Connect(0,Land.Normal);BlendNormal->B.Connect(0,CoreNormal);BlendNormal->Alpha.Connect(0,CoreAlpha);
        auto* Unit=Node<UMaterialExpressionNormalize>(Material);Unit->VectorInput.Connect(0,BlendNormal);SurfaceNormal=Unit;SurfaceNormalOutput=TEXT("");
        auto* CoreRough=Node<UMaterialExpressionLinearInterpolate>(Material);CoreRough->A.Connect(0,RoughnessProduct);CoreRough->B.Connect(1,TrailRough);CoreRough->Alpha.Connect(0,PathEarth);
        auto* LandRoughProduct=Node<UMaterialExpressionMultiply>(Material);LandRoughProduct->A.Connect(0,Land.Rough);LandRoughProduct->B.Connect(0,RoughnessFactor);
        auto* BlendRough=Node<UMaterialExpressionLinearInterpolate>(Material);BlendRough->A.Connect(0,LandRoughProduct);BlendRough->B.Connect(0,CoreRough);BlendRough->Alpha.Connect(0,CoreAlpha);SurfaceRoughness=BlendRough;
    }
    if(bWorldProjection)
    {
        auto* World=Node<UMaterialExpressionWorldPosition>(Material);
        // DEV-SEASKY RockWetness (shared sea-state contract): fully wet below still water and a
        // ragged 0.5-2 m splash band that rises with the same arriving swell crests as the foam.
        auto* WetTime=Node<UMaterialExpressionTime>(Material);
        auto* WetCamera=Node<UMaterialExpressionCameraPositionWS>(Material);
        auto* WetAmplitude=Node<UMaterialExpressionScalarParameter>(Material);WetAmplitude->ParameterName=TEXT("WaveAmplitudeScale");WetAmplitude->DefaultValue=1.0F;
        auto* Wet=Node<UMaterialExpressionCustom>(Material);Wet->OutputType=CMOT_Float1;
        Wet->Code=APacificCoastGraybox::GetCoastalWaterShaderCode(APacificCoastGraybox::ECoastalWaterShader::RockWetness);Wet->Inputs.Reset();
        auto WetInput=[Wet](const TCHAR* InputName,UMaterialExpression* Value){FCustomInput Item;Item.InputName=InputName;Item.Input.Connect(0,Value);Wet->Inputs.Add(Item);};
        WetInput(TEXT("P"),World);WetInput(TEXT("T"),WetTime);WetInput(TEXT("A"),WetAmplitude);WetInput(TEXT("C"),WetCamera);
        auto* WetColor=Node<UMaterialExpressionMultiply>(Material);WetColor->A.Connect(0,SurfaceColor);WetColor->ConstB=.44F;
        auto* ColorBlend=Node<UMaterialExpressionLinearInterpolate>(Material);ColorBlend->A.Connect(0,SurfaceColor);ColorBlend->B.Connect(0,WetColor);ColorBlend->Alpha.Connect(0,Wet);SurfaceColor=ColorBlend;
        auto* WetRough=Node<UMaterialExpressionLinearInterpolate>(Material);WetRough->A.Connect(0,SurfaceRoughness);WetRough->ConstB=.24F;WetRough->Alpha.Connect(0,Wet);SurfaceRoughness=WetRough;
    }
    bConnected &= UMaterialEditingLibrary::ConnectMaterialProperty(SurfaceColor, TEXT(""), MP_BaseColor);
    bConnected &= UMaterialEditingLibrary::ConnectMaterialProperty(SurfaceRoughness, TEXT(""), MP_Roughness);
    bConnected &= UMaterialEditingLibrary::ConnectMaterialProperty(SurfaceNormal, SurfaceNormalOutput, MP_Normal);
    if (Alpha >= 0)
    {
        auto* Mask = Sample(Material, Textures[Alpha], TEXT("OpacityTexture"), UV, SAMPLERTYPE_LinearGrayscale);
        // Recover thin needle coverage at distance without filling empty atlas
        // background: the maximum bias .16 always stays below the .33 clip.
        // Mips and alpha-coverage scaling remain enabled; nearby cards are unchanged.
        auto* Depth = Node<UMaterialExpressionPixelDepth>(Material);
        auto* StartFade = Node<UMaterialExpressionSubtract>(Material);
        StartFade->A.Connect(0,Depth); StartFade->ConstB = 18000.0F;
        auto* Span = Node<UMaterialExpressionDivide>(Material);
        Span->A.Connect(0,StartFade); Span->ConstB = 45000.0F;
        auto* Fade = Node<UMaterialExpressionSaturate>(Material); Fade->Input.Connect(0,Span);
        auto* Coverage = Node<UMaterialExpressionScalarParameter>(Material);
        Coverage->ParameterName = TEXT("DistantNeedleCoverage"); Coverage->DefaultValue = .16F;
        auto* Bias = Node<UMaterialExpressionMultiply>(Material);
        Bias->A.Connect(0,Fade); Bias->B.Connect(0,Coverage);
        auto* CoveredMask = Node<UMaterialExpressionAdd>(Material);
        bConnected &= UMaterialEditingLibrary::ConnectMaterialExpressions(Mask,TEXT("R"),CoveredMask,TEXT("A"));
        CoveredMask->B.Connect(0,Bias);
        bConnected &= UMaterialEditingLibrary::ConnectMaterialProperty(CoveredMask, TEXT(""), MP_OpacityMask);
        bConnected &= UMaterialEditingLibrary::ConnectMaterialProperty(ColorProduct, TEXT(""), MP_SubsurfaceColor);
    }
    if (!bConnected) return false;
    UMaterialEditingLibrary::LayoutMaterialExpressions(Material);
    Material->PostEditChange();
    const TArray<FString> Errors = UMaterialEditingLibrary::RecompileMaterial(Material);
    for (const FString& Error : Errors) UE_LOG(LogTemp, Error, TEXT("COAST_DRAFT_MATERIAL_ERROR %s %s"), Name, *Error);
    if (!Errors.IsEmpty()) return false;
    if (bNew) FAssetRegistryModule::AssetCreated(Material);
    return SaveDraft(Material);
}

// DEV-SEASKY: sparse deterministic cirrus for the sky dome. Unlit translucent; the streak
// field is procedural HLSL from the runtime sea/sky contract (no texture, no wind/time).
bool BuildCirrusMaterial()
{
    using EShader=APacificCoastGraybox::ECoastalWaterShader;
    const TCHAR* Name=TEXT("M_CoastCirrusDraft");
    const FString PackageName=FString(DestinationRoot)+TEXT("Materials/")+Name;
    UPackage* Package=CreatePackage(*PackageName);Package->FullyLoad();
    UMaterial* Material=FindObject<UMaterial>(Package,Name);const bool bNew=Material==nullptr;
    if(bNew)Material=NewObject<UMaterial>(Package,Name,RF_Public|RF_Standalone);
    if(!Material)return false;
    Material->PreEditChange(nullptr);if(!ResetDraftExpressions(Material))return false;
    Material->BlendMode=BLEND_Translucent;Material->TwoSided=true;Material->SetShadingModel(MSM_Unlit);
    Material->bUseTranslucencyVertexFog=false; // a 50 km dome must not be washed out by height fog
    auto* World=Node<UMaterialExpressionWorldPosition>(Material);World->WorldPositionShaderOffset=WPT_ExcludeAllShaderOffsets;
    auto* Camera=Node<UMaterialExpressionCameraPositionWS>(Material);
    auto* Sun=Node<UMaterialExpressionVectorParameter>(Material);Sun->ParameterName=TEXT("SunDirection");Sun->DefaultValue=FLinearColor(.8F,-.5F,.36F);
    auto* Field=Node<UMaterialExpressionCustom>(Material);Field->OutputType=CMOT_Float1;
    Field->Code=APacificCoastGraybox::GetCoastalWaterShaderCode(EShader::Cirrus);Field->Inputs.Reset();
    auto Input=[](UMaterialExpressionCustom* Expression,const TCHAR* InputName,UMaterialExpression* Value)
    {FCustomInput Item;Item.InputName=InputName;Item.Input.Connect(0,Value);Expression->Inputs.Add(Item);};
    Input(Field,TEXT("P"),World);Input(Field,TEXT("C"),Camera);Input(Field,TEXT("S"),Sun);
    auto* Strength=Node<UMaterialExpressionScalarParameter>(Material);Strength->ParameterName=TEXT("CirrusOpacity");Strength->DefaultValue=.3F;
    auto* Opacity=Node<UMaterialExpressionMultiply>(Material);Opacity->A.Connect(0,Field);Opacity->B.Connect(0,Strength);
    auto* Color=Node<UMaterialExpressionVectorParameter>(Material);Color->ParameterName=TEXT("CirrusColor");Color->DefaultValue=FLinearColor(1.8F,1.75F,1.65F);
    bool Connected=UMaterialEditingLibrary::ConnectMaterialProperty(Color,TEXT(""),MP_EmissiveColor);
    Connected&=UMaterialEditingLibrary::ConnectMaterialProperty(Opacity,TEXT(""),MP_Opacity);
    if(!Connected)return false;
    UMaterialEditingLibrary::LayoutMaterialExpressions(Material);Material->PostEditChange();
    const TArray<FString> Errors=UMaterialEditingLibrary::RecompileMaterial(Material);
    for(const FString& Error:Errors)UE_LOG(LogTemp,Error,TEXT("COAST_DRAFT_CIRRUS_ERROR %s"),*Error);
    if(!Errors.IsEmpty())return false;
    if(bNew)FAssetRegistryModule::AssetCreated(Material);return SaveDraft(Material);
}

// Fixed engine path resolved by the commandlet constructor; no runtime path loading.
UTexture2D* MidWaterNormal=nullptr;

bool BuildWaterMaterial(UTexture2D* NormalTexture)
{
    // DEV-SEASKY: all wave/foam/colour HLSL comes from the runtime sea-state contract
    // (APacificCoastGraybox::GetCoastalWaterShaderCode) so CPU tests and the shader agree.
    if(!NormalTexture)return false;
    using EShader=APacificCoastGraybox::ECoastalWaterShader;
    // Mid-scale engine Water-plugin normal (UE content, referenced in-engine, not copied).
    UTexture2D* MidNormal=MidWaterNormal?MidWaterNormal:NormalTexture;
    const TCHAR* Name=TEXT("M_CoastWaterDraft");
    const FString PackageName=FString(DestinationRoot)+TEXT("Materials/")+Name;
    UPackage* Package=CreatePackage(*PackageName);Package->FullyLoad();
    UMaterial* Material=FindObject<UMaterial>(Package,Name);const bool bNew=Material==nullptr;
    if(bNew)Material=NewObject<UMaterial>(Package,Name,RF_Public|RF_Standalone);
    if(!Material)return false;
    Material->PreEditChange(nullptr);if(!ResetDraftExpressions(Material))return false;
    Material->BlendMode=BLEND_Opaque;Material->TwoSided=false;Material->bTangentSpaceNormal=false;
    Material->SetShadingModel(MSM_SingleLayerWater);
    auto* World=Node<UMaterialExpressionWorldPosition>(Material);World->WorldPositionShaderOffset=WPT_ExcludeAllShaderOffsets;
    auto* Camera=Node<UMaterialExpressionCameraPositionWS>(Material);
    auto* Time=Node<UMaterialExpressionTime>(Material);
    auto* Mask=Node<UMaterialExpressionVertexColor>(Material);
    auto* Amplitude=Node<UMaterialExpressionScalarParameter>(Material);Amplitude->ParameterName=TEXT("WaveAmplitudeScale");Amplitude->DefaultValue=1;
    auto Input=[](UMaterialExpressionCustom* Expression,const TCHAR* InputName,UMaterialExpression* Value)
    {FCustomInput Item;Item.InputName=InputName;Item.Input.Connect(0,Value);Expression->Inputs.Add(Item);};
    auto Custom=[&](EShader Kind,ECustomMaterialOutputType Type)
    {
        auto* Expression=Node<UMaterialExpressionCustom>(Material);Expression->OutputType=Type;
        Expression->Code=APacificCoastGraybox::GetCoastalWaterShaderCode(Kind);Expression->Inputs.Reset();
        return Expression;
    };
    auto* Offset=Custom(EShader::Offset,CMOT_Float3);
    Input(Offset,TEXT("P"),World);Input(Offset,TEXT("T"),Time);Input(Offset,TEXT("A"),Amplitude);Input(Offset,TEXT("C"),Camera);
    // Two detail scales (~40 m and ~6 m repeat), panning with the NW wind sea at different rates.
    auto* FirstUV=Node<UMaterialExpressionTextureCoordinate>(Material);FirstUV->UTiling=.25F;FirstUV->VTiling=.25F;
    auto* FirstPan=Node<UMaterialExpressionPanner>(Material);FirstPan->Coordinate.Connect(0,FirstUV);FirstPan->SpeedX=-.010F;FirstPan->SpeedY=-.006F;FirstPan->bFractionalPart=true;
    auto* SecondUV=Node<UMaterialExpressionTextureCoordinate>(Material);SecondUV->UTiling=1.6F;SecondUV->VTiling=1.6F;
    auto* SecondPan=Node<UMaterialExpressionPanner>(Material);SecondPan->Coordinate.Connect(0,SecondUV);SecondPan->SpeedX=-.021F;SecondPan->SpeedY=-.017F;SecondPan->bFractionalPart=true;
    auto* N1=Sample(Material,MidNormal,TEXT("WaterNormalA"),FirstPan,SAMPLERTYPE_Normal);
    auto* N2=Sample(Material,NormalTexture,TEXT("WaterNormalB"),SecondPan,SAMPLERTYPE_Normal);
    auto* ThirdUV=Node<UMaterialExpressionTextureCoordinate>(Material);ThirdUV->UTiling=.018F;ThirdUV->VTiling=.018F;
    auto* ThirdPan=Node<UMaterialExpressionPanner>(Material);ThirdPan->Coordinate.Connect(0,ThirdUV);ThirdPan->SpeedX=-.0012F;ThirdPan->SpeedY=-.0007F;ThirdPan->bFractionalPart=true;
    auto* N3=Sample(Material,MidNormal,TEXT("WaterNormalFar"),ThirdPan,SAMPLERTYPE_Normal);
    auto* Normal=Custom(EShader::Normal,CMOT_Float3);
    Input(Normal,TEXT("P"),World);Input(Normal,TEXT("T"),Time);Input(Normal,TEXT("A"),Amplitude);Input(Normal,TEXT("C"),Camera);
    Input(Normal,TEXT("N1"),N1);Input(Normal,TEXT("N2"),N2);Input(Normal,TEXT("N3"),N3);
    auto* Foam=Custom(EShader::Foam,CMOT_Float1);
    Input(Foam,TEXT("P"),World);Input(Foam,TEXT("T"),Time);Input(Foam,TEXT("A"),Amplitude);Input(Foam,TEXT("C"),Camera);Input(Foam,TEXT("Mask"),Mask);
    {FCustomInput Alpha;Alpha.InputName=TEXT("MaskA");Alpha.Input.Connect(4,Mask);Foam->Inputs.Add(Alpha);}
    // Foam002 opacity imported above by this commandlet (already in memory; no path loading).
    UTexture2D* FoamOpacity=FindObject<UTexture2D>(nullptr,TEXT("/Game/Development/CoastDraft/Textures/T_CoastFoam_Opacity.T_CoastFoam_Opacity"));
    auto* FoamTex=Node<UMaterialExpressionTextureObjectParameter>(Material);FoamTex->ParameterName=TEXT("FoamTexture");
    FoamTex->Texture=FoamOpacity?FoamOpacity:NormalTexture;FoamTex->SamplerType=FoamOpacity?SAMPLERTYPE_LinearGrayscale:SAMPLERTYPE_Normal;
    Input(Foam,TEXT("FoamTex"),FoamTex);
    auto* Deep=Node<UMaterialExpressionConstant3Vector>(Material);Deep->Constant=FLinearColor(.012F,.024F,.034F);
    auto* White=Node<UMaterialExpressionConstant3Vector>(Material);White->Constant=FLinearColor(.66F,.70F,.70F);
    auto* Color=Node<UMaterialExpressionLinearInterpolate>(Material);Color->A.Connect(0,Deep);Color->B.Connect(0,White);Color->Alpha.Connect(0,Foam);
    auto* Rough=Node<UMaterialExpressionScalarParameter>(Material);Rough->ParameterName=TEXT("Roughness");Rough->DefaultValue=.07F;
    auto* SurfaceRough=Custom(EShader::Roughness,CMOT_Float1);
    Input(SurfaceRough,TEXT("R"),Rough);Input(SurfaceRough,TEXT("F"),Foam);Input(SurfaceRough,TEXT("P"),World);Input(SurfaceRough,TEXT("C"),Camera);
    auto* Opacity=Node<UMaterialExpressionLinearInterpolate>(Material);Opacity->ConstA=.03F;Opacity->ConstB=.9F;Opacity->Alpha.Connect(0,Foam);
    auto* Specular=Node<UMaterialExpressionScalarParameter>(Material);Specular->ParameterName=TEXT("WaterSpecular");Specular->DefaultValue=.255F;
    auto* Volume=Node<UMaterialExpressionSingleLayerWaterMaterialOutput>(Material);
    auto* Scatter=Custom(EShader::Scattering,CMOT_Float3);Input(Scatter,TEXT("Mask"),Mask);
    auto* Absorb=Custom(EShader::Absorption,CMOT_Float3);Input(Absorb,TEXT("Mask"),Mask);
    Volume->ScatteringCoefficients.Connect(0,Scatter);Volume->AbsorptionCoefficients.Connect(0,Absorb);
    bool Connected=UMaterialEditingLibrary::ConnectMaterialProperty(Color,TEXT(""),MP_BaseColor);
    Connected&=UMaterialEditingLibrary::ConnectMaterialProperty(Normal,TEXT(""),MP_Normal);
    Connected&=UMaterialEditingLibrary::ConnectMaterialProperty(SurfaceRough,TEXT(""),MP_Roughness);
    Connected&=UMaterialEditingLibrary::ConnectMaterialProperty(Specular,TEXT(""),MP_Specular);
    Connected&=UMaterialEditingLibrary::ConnectMaterialProperty(Opacity,TEXT(""),MP_Opacity);
    Connected&=UMaterialEditingLibrary::ConnectMaterialProperty(Offset,TEXT(""),MP_WorldPositionOffset);
    if(!Connected)return false;
    UMaterialEditingLibrary::LayoutMaterialExpressions(Material);Material->PostEditChange();
    const TArray<FString> Errors=UMaterialEditingLibrary::RecompileMaterial(Material);
    for(const FString& Error:Errors)UE_LOG(LogTemp,Error,TEXT("COAST_DRAFT_WATER_ERROR %s"),*Error);
    if(!Errors.IsEmpty())return false;
    if(bNew)FAssetRegistryModule::AssetCreated(Material);return SaveDraft(Material);
}

}

UCoastDraftImportCommandlet::UCoastDraftImportCommandlet()
{
    IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
    static ConstructorHelpers::FObjectFinder<UMaterialFunctionInterface> TextureFunction(TEXT("/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture.WorldAlignedTexture"));
    static ConstructorHelpers::FObjectFinder<UMaterialFunctionInterface> NormalFunction(TEXT("/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedNormal.WorldAlignedNormal"));
    static ConstructorHelpers::FObjectFinder<UTexture2D> WaterNormalTexture(TEXT("/Engine/Functions/Engine_MaterialFunctions02/ExampleContent/Textures/water_n.water_n"));
    WorldAlignedTexture = TextureFunction.Object;
    WorldAlignedNormal = NormalFunction.Object;
    WaterNormal = WaterNormalTexture.Object;
    static ConstructorHelpers::FObjectFinderOptional<UTexture2D> MidWaterNormalTexture(TEXT("/Water/Textures/Normals/T_Water_TilingNormal_Waves_02.T_Water_TilingNormal_Waves_02"));
    CoastDraftImport::MidWaterNormal = MidWaterNormalTexture.Get();
}

int32 UCoastDraftImportCommandlet::Main(const FString& Params)
{
    using namespace CoastDraftImport;
    // No caller-supplied filenames, package paths, reflection or code are accepted.
    (void)Params;
    if (!WorldAlignedTexture || !WorldAlignedNormal || !WaterNormal) return 2;
    FString SourceRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("SourceAssets/EnvironmentDraft/"));
    FPaths::NormalizeDirectoryName(SourceRoot);
    if (!FPaths::CollapseRelativeDirectories(SourceRoot)) return 3;
    SourceRoot += TEXT("/");
    TArray<FString> SourceFiles;
    for (const FSource& Source : Sources)
    {
        FString Filename = FPaths::ConvertRelativePathToFull(SourceRoot / Source.RelativeFile);
        FPaths::NormalizeFilename(Filename);
        if (!FPaths::CollapseRelativeDirectories(Filename) || !Filename.StartsWith(SourceRoot)
            || !FPlatformFileManager::Get().GetPlatformFile().FileExists(*Filename))
        {
            UE_LOG(LogTemp, Error, TEXT("COAST_DRAFT_SOURCE_MISSING %s"), Source.RelativeFile);
            return 4;
        }
        SourceFiles.Add(Filename);
    }
    FAssetToolsModule& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
    TArray<UTexture2D*> Textures;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Sources); ++Index)
    {
        UAssetImportTask* Task = NewObject<UAssetImportTask>();
        Task->Filename = SourceFiles[Index]; Task->DestinationPath = FString(DestinationRoot) + TEXT("Textures");
        Task->DestinationName = Sources[Index].AssetName;
        Task->Factory = NewObject<UTextureFactory>();
        Task->bAutomated = true; Task->bAsync = false; Task->bReplaceExisting = true; Task->bReplaceExistingSettings = true; Task->bSave = false;
        AssetTools.Get().ImportAssetTasks({Task});
        const TArray<UObject*>& Objects = Task->GetObjects();
        UTexture2D* Texture = Objects.Num() == 1 ? Cast<UTexture2D>(Objects[0]) : nullptr;
        if (!Texture || Texture->GetOutermost()->GetName() != Task->DestinationPath + TEXT("/") + Sources[Index].AssetName) return 5;
        Texture->SRGB = Sources[Index].Role == ETextureRole::Color;
        Texture->CompressionSettings = Sources[Index].Role == ETextureRole::Normal ? TC_Normalmap :
            (Sources[Index].Role == ETextureRole::Color ? TC_Default : TC_Grayscale);
        Texture->bFlipGreenChannel = false; // Whitelist requires DirectX normal maps.
        Texture->bDoScaleMipsForAlphaCoverage = Sources[Index].Role == ETextureRole::Alpha;
        if (Texture->bDoScaleMipsForAlphaCoverage) Texture->AlphaCoverageThresholds = FVector4(.33F,0,0,0);
        if (Texture->AssetImportData != nullptr)
        {
            FString RelativeSource = SourceFiles[Index];
            FPaths::MakePathRelativeTo(RelativeSource, *FPaths::ProjectDir());
            Texture->AssetImportData->UpdateFilenameOnly(RelativeSource);
        }
        Texture->PostEditChange();
        if (!SaveDraft(Texture)) return 6;
        Textures.Add(Texture);
    }
    // Terrain UVs are position/4000 cm; the actual source scan spans 9000 cm, not 200 cm.
    const bool bGround = BuildMaterial(TEXT("M_CoastGroundDraft"), Textures, 0, 1, 2, -1, false, 4000.0F / 9000.0F, WorldAlignedTexture, WorldAlignedNormal);
    const bool bRock = BuildMaterial(TEXT("M_CoastRockDraft"), Textures, 34, 35, 36, -1, true, 1.0F, WorldAlignedTexture, WorldAlignedNormal);
    const bool bNeedles = BuildMaterial(TEXT("M_CoastNeedlesDraft"), Textures, 6, 8, 9, 7, false, 1.0F, WorldAlignedTexture, WorldAlignedNormal);
    const bool bBark = BuildMaterial(TEXT("M_CoastBarkDraft"), Textures, 10, 11, 12, -1, false, 1.0F, WorldAlignedTexture, WorldAlignedNormal);
    const bool bWater = BuildWaterMaterial(WaterNormal);
    const bool bCirrus = BuildCirrusMaterial(); // DEV-SEASKY
    const bool bGravel=BuildMaterial(TEXT("M_CoastGravelDraft"),Textures,13,14,15,-1,false,4000.0F/253.0F,WorldAlignedTexture,WorldAlignedNormal);
    if (!(bGround && bRock && bNeedles && bBark && bWater && bGravel && bCirrus)) return 7;
    UE_LOG(LogTemp, Display, TEXT("COAST_DRAFT_IMPORT_PASS textures=48 materials=7 state=DRAFT_NOT_USER_CONFIRMED"));
    return 0;
}
