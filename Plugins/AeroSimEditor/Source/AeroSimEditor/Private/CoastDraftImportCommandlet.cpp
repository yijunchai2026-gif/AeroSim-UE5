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
    {TEXT("GravelDraft/roughness.jpg"), TEXT("T_CoastGravel_Roughness"), ETextureRole::Roughness}
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
        BaseExpression = Project(Material, AlignedTexture, Textures[Base], TEXT("BaseColorTexture"), SAMPLERTYPE_Color);
        NormalExpression = Project(Material, AlignedNormal, Textures[Normal], TEXT("NormalTexture"), SAMPLERTYPE_Normal);
        RoughnessExpression = Project(Material, AlignedTexture, Textures[Roughness], TEXT("RoughnessTexture"), SAMPLERTYPE_LinearGrayscale);
        BaseOutput = NormalOutput = RoughnessOutput = TEXT("XYZ Texture");
        if (!BaseExpression || !NormalExpression || !RoughnessExpression) return false;
        // Retain physical 1.8 m scan detail, with separate broad colour/normal
        // structure so a 65 m cliff does not become 36 identical horizontal rows.
        auto* MacroColor = Project(Material, AlignedTexture, Textures[Base], TEXT("RockMacroColor"), SAMPLERTYPE_Color, 2300.0F);
        auto* MacroNormal = Project(Material, AlignedNormal, Textures[Normal], TEXT("RockMacroNormal"), SAMPLERTYPE_Normal, 1100.0F);
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
    if(FString(Name).Contains(TEXT("Ground")))
    {
        auto* Cover=Node<UMaterialExpressionVertexColor>(Material);
        auto* CoverProduct=Node<UMaterialExpressionMultiply>(Material);
        CoverProduct->A.Connect(0,ColorProduct);CoverProduct->B.Connect(0,Cover);SurfaceColor=CoverProduct;
    }
    UMaterialExpression* SurfaceNormal=NormalExpression;UMaterialExpression* SurfaceRoughness=RoughnessProduct;
    FString SurfaceNormalOutput=NormalOutput;
    if(FString(Name).Contains(TEXT("Gravel")))
    {
        auto* Mask=Node<UMaterialExpressionVertexColor>(Material);
        auto* LandUV=Node<UMaterialExpressionTextureCoordinate>(Material);LandUV->UTiling=4000.0F/9000.0F;LandUV->VTiling=4000.0F/9000.0F;
        auto* LandColor=Sample(Material,Textures[0],TEXT("LandBaseColorTexture"),LandUV,SAMPLERTYPE_Color);
        auto* LandNormal=Sample(Material,Textures[1],TEXT("LandNormalTexture"),LandUV,SAMPLERTYPE_Normal);
        auto* LandRough=Sample(Material,Textures[2],TEXT("LandRoughnessTexture"),LandUV,SAMPLERTYPE_LinearGrayscale);
        auto* LandTint=Node<UMaterialExpressionMultiply>(Material);LandTint->A.Connect(0,LandColor);LandTint->B.Connect(0,Mask);
        auto* Blend=Node<UMaterialExpressionLinearInterpolate>(Material);Blend->A.Connect(0,LandTint);Blend->B.Connect(0,ColorProduct);Blend->Alpha.Connect(4,Mask);SurfaceColor=Blend;
        auto* BlendNormal=Node<UMaterialExpressionLinearInterpolate>(Material);BlendNormal->A.Connect(0,LandNormal);BlendNormal->B.Connect(0,NormalExpression);BlendNormal->Alpha.Connect(4,Mask);
        auto* Unit=Node<UMaterialExpressionNormalize>(Material);Unit->VectorInput.Connect(0,BlendNormal);SurfaceNormal=Unit;SurfaceNormalOutput=TEXT("");
        auto* LandRoughProduct=Node<UMaterialExpressionMultiply>(Material);LandRoughProduct->A.Connect(1,LandRough);LandRoughProduct->B.Connect(0,RoughnessFactor);
        auto* BlendRough=Node<UMaterialExpressionLinearInterpolate>(Material);BlendRough->A.Connect(0,LandRoughProduct);BlendRough->B.Connect(0,RoughnessProduct);BlendRough->Alpha.Connect(4,Mask);SurfaceRoughness=BlendRough;
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
    auto* Normal=Custom(EShader::Normal,CMOT_Float3);
    Input(Normal,TEXT("P"),World);Input(Normal,TEXT("T"),Time);Input(Normal,TEXT("A"),Amplitude);Input(Normal,TEXT("C"),Camera);
    Input(Normal,TEXT("N1"),N1);Input(Normal,TEXT("N2"),N2);
    auto* Foam=Custom(EShader::Foam,CMOT_Float1);
    Input(Foam,TEXT("P"),World);Input(Foam,TEXT("T"),Time);Input(Foam,TEXT("A"),Amplitude);Input(Foam,TEXT("C"),Camera);Input(Foam,TEXT("Mask"),Mask);
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
    const bool bRock = BuildMaterial(TEXT("M_CoastRockDraft"), Textures, 3, 4, 5, -1, true, 1.0F, WorldAlignedTexture, WorldAlignedNormal);
    const bool bNeedles = BuildMaterial(TEXT("M_CoastNeedlesDraft"), Textures, 6, 8, 9, 7, false, 1.0F, WorldAlignedTexture, WorldAlignedNormal);
    const bool bBark = BuildMaterial(TEXT("M_CoastBarkDraft"), Textures, 10, 11, 12, -1, false, 1.0F, WorldAlignedTexture, WorldAlignedNormal);
    const bool bWater = BuildWaterMaterial(WaterNormal);
    const bool bGravel=BuildMaterial(TEXT("M_CoastGravelDraft"),Textures,13,14,15,-1,false,4000.0F/253.0F,WorldAlignedTexture,WorldAlignedNormal);
    if (!(bGround && bRock && bNeedles && bBark && bWater && bGravel)) return 7;
    UE_LOG(LogTemp, Display, TEXT("COAST_DRAFT_IMPORT_PASS textures=16 materials=6 state=DRAFT_NOT_USER_CONFIRMED"));
    return 0;
}
