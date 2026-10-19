#include "HeroCliffDraftImport.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Materials/Material.h"
#include "MeshDescription.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/BodySetup.h"
#include "ProductEvidence.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshCompiler.h"
#include "StaticMeshResources.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace HeroCliffDraft
{
constexpr const TCHAR* SourceRoot=TEXT("SourceAssets/EnvironmentDraft/HeroCoveScan/");
constexpr const TCHAR* AssetPath=TEXT("Content/Development/CoastDraft/Meshes/SM_CoastHeroCliffScanDraft.uasset");
constexpr const TCHAR* PackageName=TEXT("/Game/Development/CoastDraft/Meshes/SM_CoastHeroCliffScanDraft");
constexpr const TCHAR* AssetName=TEXT("SM_CoastHeroCliffScanDraft");
constexpr const TCHAR* ImportReceipt=TEXT("Artifacts/ProductShell/AAA/R5/HeroScanImport/receipt.json");
constexpr const TCHAR* ReceiptHash=TEXT("27cb4ab652d29d40e2689d55468599708e3e80a9d6016600b4c15cc34b0ae571");
struct FSource
{
    const TCHAR* Stem;
    int32 Vertices;
    int32 Triangles;
    int32 JsonBytes;
    int32 BinBytes;
    const TCHAR* JsonHash;
    const TCHAR* BinHash;
};
const FSource Sources[]={
    {TEXT("coastal-cliff-40m-lod0"),27145,49995,1755,1468580,TEXT("099cae75b2f82bd5d10ffe86922705e662bb8cf4fc072bf5497f929185424812"),TEXT("56c1f67209c0d53e27a0bb744ee8b764201715256f03184c63de6fcb602a8323")},
    {TEXT("coastal-cliff-40m-lod1"),7435,11997,1746,381884,TEXT("2d8d903ec1e6a6d5cb2a2086b6a12d48acf54ee3dacc91b7e45e824cc609c590"),TEXT("6ee6034e8e2c2bd52a24b72248139d31e0813fa6cfae09dbbbbc29dd6746246b")},
    {TEXT("coastal-cliff-40m-lod2"),2918,3591,1743,136468,TEXT("ab72b18f5a233c5fa6febb4cfdc9e8dba4aae61665036334dfc50360816be3da"),TEXT("73619882ce484cdad80544e6166b999edbe95fa7039936f937d3660f580218fd")}
};
bool SafeProjectPath(const FString& Relative)
{
    if(!FPaths::IsRelative(Relative)||Relative.Contains(TEXT(":"))||Relative.Contains(TEXT("\\")))return false;
    TArray<FString> Parts;Relative.ParseIntoArray(Parts,TEXT("/"),true);FString Path=FPaths::ProjectDir();
    for(const FString& Part:Parts)
    {
        if(Part==TEXT("..")||Part==TEXT("."))return false;
        Path/=Part;if(IFileManager::Get().IsSymlink(*Path))return false;
    }
    return true;
}
bool ReadFixed(const FString& Relative,int64 ExpectedBytes,const TCHAR* ExpectedHash,TArray<uint8>& Bytes)
{
    const FString Filename=FPaths::ProjectDir()/Relative;
    if(!SafeProjectPath(Relative)||IFileManager::Get().FileSize(*Filename)!=ExpectedBytes
        ||!FFileHelper::LoadFileToArray(Bytes,*Filename)||Bytes.Num()!=ExpectedBytes)return false;
    return ProductEvidenceSha256(Bytes)==ExpectedHash;
}
bool Json(const TArray<uint8>& Bytes,TSharedPtr<FJsonObject>& Object)
{
    FString Text;FFileHelper::BufferToString(Text,Bytes.GetData(),Bytes.Num());
    return FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Object)&&Object.IsValid();
}
bool Number(const TSharedPtr<FJsonObject>& Object,const TCHAR* Key,int32 Expected)
{
    double Value=0;return Object.IsValid()&&Object->TryGetNumberField(Key,Value)&&Value==Expected;
}
bool Text(const TSharedPtr<FJsonObject>& Object,const TCHAR* Key,const FString& Expected)
{
    FString Value;return Object.IsValid()&&Object->TryGetStringField(Key,Value)&&Value==Expected;
}
struct FLOD
{
    TArray<FVector3f> Points;
    TArray<FVector3f> Normals;
    TArray<FVector2f> UV;
    TArray<uint32> Indices;
};
uint32 ReadU32(const uint8* Bytes)
{
    return uint32(Bytes[0])|(uint32(Bytes[1])<<8)|(uint32(Bytes[2])<<16)|(uint32(Bytes[3])<<24);
}
float ReadFloat(const uint8* Bytes)
{
    const uint32 Bits=ReadU32(Bytes);float Value;FMemory::Memcpy(&Value,&Bits,4);return Value;
}
bool ReadLOD(const FSource& Source,FLOD& Out)
{
    TArray<uint8> Metadata,Binary;
    if(!ReadFixed(FString(SourceRoot)+Source.Stem+TEXT(".gltf"),Source.JsonBytes,Source.JsonHash,Metadata)
        ||!ReadFixed(FString(SourceRoot)+Source.Stem+TEXT(".bin"),Source.BinBytes,Source.BinHash,Binary))return false;
    TSharedPtr<FJsonObject> Root;if(!Json(Metadata,Root))return false;
    // Hashes freeze every field. These structural checks also document the deliberately narrow
    // parser: one identity node, one triangle primitive, no materials, URI expansion or extensions.
    if(Root->Values.Num()!=8||Root->HasField(TEXT("extensionsUsed"))||Root->HasField(TEXT("materials")))return false;
    const TSharedPtr<FJsonObject>* Asset=nullptr;
    if(!Root->TryGetObjectField(TEXT("asset"),Asset)||!Text(*Asset,TEXT("version"),TEXT("2.0"))||!Number(Root,TEXT("scene"),0))return false;
    const TArray<TSharedPtr<FJsonValue>> *Nodes=nullptr,*Meshes=nullptr,*Buffers=nullptr,*Views=nullptr,*Accessors=nullptr,*Scenes=nullptr;
    if(!Root->TryGetArrayField(TEXT("nodes"),Nodes)||Nodes->Num()!=1||!Root->TryGetArrayField(TEXT("meshes"),Meshes)||Meshes->Num()!=1
        ||!Root->TryGetArrayField(TEXT("buffers"),Buffers)||Buffers->Num()!=1||!Root->TryGetArrayField(TEXT("bufferViews"),Views)||Views->Num()!=4
        ||!Root->TryGetArrayField(TEXT("accessors"),Accessors)||Accessors->Num()!=4||!Root->TryGetArrayField(TEXT("scenes"),Scenes)||Scenes->Num()!=1)return false;
    const auto Node=(*Nodes)[0]->AsObject(),Buffer=(*Buffers)[0]->AsObject();
    if(!Node||Node->Values.Num()!=2||!Number(Node,TEXT("mesh"),0)||!Text(Node,TEXT("name"),Source.Stem)
        ||!Buffer||Buffer->Values.Num()!=2||!Number(Buffer,TEXT("byteLength"),Binary.Num())||!Text(Buffer,TEXT("uri"),FString(Source.Stem)+TEXT(".bin")))return false;
    const TArray<TSharedPtr<FJsonValue>>* PrimitiveArray=nullptr;
    const auto Mesh=(*Meshes)[0]->AsObject();
    if(!Mesh||Mesh->Values.Num()!=1||!Mesh->TryGetArrayField(TEXT("primitives"),PrimitiveArray)||PrimitiveArray->Num()!=1)return false;
    const auto Primitive=(*PrimitiveArray)[0]->AsObject();const TSharedPtr<FJsonObject>* Attr=nullptr;
    if(!Primitive||Primitive->Values.Num()!=3||!Number(Primitive,TEXT("mode"),4)||!Number(Primitive,TEXT("indices"),3)
        ||!Primitive->TryGetObjectField(TEXT("attributes"),Attr)||(*Attr)->Values.Num()!=3
        ||!Number(*Attr,TEXT("POSITION"),0)||!Number(*Attr,TEXT("NORMAL"),1)||!Number(*Attr,TEXT("TEXCOORD_0"),2))return false;
    int32 Offsets[4]{};int64 Offset=0;
    for(int32 I=0;I<4;++I)
    {
        const auto V=(*Views)[I]->AsObject(),A=(*Accessors)[I]->AsObject();
        const int32 Components=I<2?3:I==2?2:1,Count=I==3?Source.Triangles*3:Source.Vertices;
        const int64 Length=int64(Components)*Count*4;
        if(!V||!A||V->Values.Num()!=4||A->Values.Num()!=(I==0?6:4)
            ||!Number(V,TEXT("buffer"),0)||!Number(V,TEXT("byteOffset"),static_cast<int32>(Offset))||!Number(V,TEXT("byteLength"),static_cast<int32>(Length))
            ||!Number(V,TEXT("target"),I==3?34963:34962)||!Number(A,TEXT("bufferView"),I)||!Number(A,TEXT("componentType"),I==3?5125:5126)
            ||!Number(A,TEXT("count"),Count)||!Text(A,TEXT("type"),I<2?TEXT("VEC3"):I==2?TEXT("VEC2"):TEXT("SCALAR"))
            ||A->HasField(TEXT("sparse"))||V->HasField(TEXT("byteStride"))||Offset+Length>Binary.Num())return false;
        Offsets[I]=static_cast<int32>(Offset);Offset+=Length;
    }
    if(Offset!=Binary.Num())return false;
    FBox3f Bounds(ForceInit);
    for(int32 I=0;I<Source.Vertices;++I)
    {
        auto Vec=[&Binary,I,&Offsets](int32 Stream){const uint8* B=Binary.GetData()+Offsets[Stream]+I*12;return FVector3f(ReadFloat(B),ReadFloat(B+4),ReadFloat(B+8));};
        const FVector3f P=Vec(0),N=Vec(1);const uint8* U=Binary.GetData()+Offsets[2]+I*8;
        const FVector2f UV(ReadFloat(U),ReadFloat(U+4));
        if(!FMath::IsFinite(P.X)||!FMath::IsFinite(P.Y)||!FMath::IsFinite(P.Z)||P.GetAbsMax()>50
            ||!FMath::IsFinite(N.X)||!FMath::IsFinite(N.Y)||!FMath::IsFinite(N.Z)||N.SizeSquared()<.99F||N.SizeSquared()>1.01F
            ||!FMath::IsFinite(UV.X)||!FMath::IsFinite(UV.Y)||UV.X<0||UV.X>1||UV.Y<0||UV.Y>1)return false;
        // glTF metres/Y-up -> UE centimetres/Z-up, exposed front +X. UV0 is unchanged.
        const FVector3f Position(P.Z*100,P.X*100,P.Y*100);
        Out.Points.Add(Position);Out.Normals.Add(FVector3f(N.Z,N.X,N.Y));Out.UV.Add(UV);Bounds+=Position;
    }
    const FVector3f Size=Bounds.GetSize();
    if(!FMath::IsNearlyEqual(Size.Y,4000.0F,.1F)||Size.Z<980||Size.Z>1020||Size.X>1100)return false;
    for(int32 I=0;I<Source.Triangles*3;I+=3)
    {
        uint32 Index[3];for(int32 C=0;C<3;++C){Index[C]=ReadU32(Binary.GetData()+Offsets[3]+(I+C)*4);if(Index[C]>=uint32(Source.Vertices))return false;}
        const FVector3f A=Out.Points[Index[0]],B=Out.Points[Index[1]],C=Out.Points[Index[2]];
        const FVector2f U=Out.UV[Index[1]]-Out.UV[Index[0]],V=Out.UV[Index[2]]-Out.UV[Index[0]];
        if(FVector3f::CrossProduct(B-A,C-A).SizeSquared()<=1.e-16F||FMath::Abs(double(U.X)*V.Y-double(U.Y)*V.X)<=2.e-14)return false;
        // Reverse exactly once for the project's clockwise UE front-face convention.
        Out.Indices.Append({Index[0],Index[2],Index[1]});
    }
    return true;
}
// 0: new output permitted; 1: already registered and unchanged (no-op); -1: conflict.
int32 OutputState()
{
    if(!SafeProjectPath(TEXT("docs/environment-draft-assets.json"))||!SafeProjectPath(AssetPath))return -1;
    const FString ManifestFilename=FPaths::ProjectDir()/TEXT("docs/environment-draft-assets.json");
    TArray<uint8> Bytes;const int64 Size=IFileManager::Get().FileSize(*ManifestFilename);
    if(Size<=0||Size>2000000||!FFileHelper::LoadFileToArray(Bytes,*ManifestFilename))return -1;
    TSharedPtr<FJsonObject> M;if(!Json(Bytes,M)||!Text(M,TEXT("schema"),TEXT("aerosim.environment-draft.assets/v1"))||!Text(M,TEXT("state"),TEXT("DRAFT_NOT_USER_CONFIRMED")))return -1;
    const TArray<TSharedPtr<FJsonValue>>* Assets=nullptr;if(!M->TryGetArrayField(TEXT("assets"),Assets))return -1;
    int32 Matches=0;FString Hash;
    for(const auto& Value:*Assets)
    {
        const auto Entry=Value->AsObject();if(!Entry)return -1;
        if(Text(Entry,TEXT("path"),AssetPath)){++Matches;if(!Entry->TryGetStringField(TEXT("sha256"),Hash))return -1;}
    }
    const FString Filename=FPaths::ProjectDir()/AssetPath;
    if(Matches>1||IFileManager::Get().IsSymlink(*Filename))return -1;
    if(!IFileManager::Get().FileExists(*Filename))return Matches==0?0:-1;
    if(Matches!=1||Hash.Len()!=64)return -1; // Never replace an unregistered same-name asset.
    const TSharedPtr<FJsonObject>* Contract=nullptr;
    if(!M->TryGetObjectField(TEXT("hero_cove_scan"),Contract)||!Text(*Contract,TEXT("asset"),AssetPath)
        ||!Text(*Contract,TEXT("source_receipt_sha256"),ReceiptHash)||!Text(*Contract,TEXT("state"),TEXT("DRAFT_NOT_USER_CONFIRMED")))return -1;
    const TArray<TSharedPtr<FJsonValue>>* RegisteredSources=nullptr;
    if(!M->TryGetArrayField(TEXT("sources"),RegisteredSources))return -1;
    for(const FSource& Source:Sources)
        for(int32 Kind=0;Kind<2;++Kind)
        {
            const FString Relative=FString(SourceRoot)+Source.Stem+(Kind==0?TEXT(".gltf"):TEXT(".bin"));
            int32 Registered=0;
            for(const auto& Value:*RegisteredSources)
            {
                const auto Entry=Value->AsObject();if(!Entry)return -1;
                if(Text(Entry,TEXT("path"),Relative))
                {
                    if(!Text(Entry,TEXT("sha256"),Kind==0?Source.JsonHash:Source.BinHash)||!Text(Entry,TEXT("license"),TEXT("CC0-1.0")))return -1;
                    ++Registered;
                }
            }
            if(Registered!=1)return -1;
        }
    const int64 OutputSize=IFileManager::Get().FileSize(*Filename);
    if(OutputSize<=0||OutputSize>64*1024*1024||!FFileHelper::LoadFileToArray(Bytes,*Filename)||ProductEvidenceSha256(Bytes)!=Hash)return -1;
    return 1;
}
bool WriteImportReceipt(const FString& OutputHash)
{
    if(!SafeProjectPath(ImportReceipt)||IFileManager::Get().FileExists(*(FPaths::ProjectDir()/ImportReceipt)))return false;
    TSharedRef<FJsonObject> R=MakeShared<FJsonObject>();
    R->SetStringField(TEXT("schema"),TEXT("aerosim.hero-cove-scan-import/v1"));R->SetStringField(TEXT("asset"),AssetPath);
    R->SetStringField(TEXT("sha256"),OutputHash);R->SetStringField(TEXT("source_receipt_sha256"),ReceiptHash);
    R->SetStringField(TEXT("state"),TEXT("DRAFT_NOT_USER_CONFIRMED"));R->SetStringField(TEXT("import_status"),TEXT("IMPORTED"));R->SetStringField(TEXT("visual_validation"),TEXT("NOT_RUN"));
    TArray<TSharedPtr<FJsonValue>> SourceEntries,Counts;
    for(const FSource& Source:Sources)
    {
        Counts.Add(MakeShared<FJsonValueNumber>(Source.Triangles));
        for(int32 Kind=0;Kind<2;++Kind)
        {
            TSharedRef<FJsonObject> E=MakeShared<FJsonObject>();
            E->SetStringField(TEXT("path"),FString(SourceRoot)+Source.Stem+(Kind==0?TEXT(".gltf"):TEXT(".bin")));
            E->SetStringField(TEXT("sha256"),Kind==0?Source.JsonHash:Source.BinHash);E->SetStringField(TEXT("license"),TEXT("CC0-1.0"));SourceEntries.Add(MakeShared<FJsonValueObject>(E));
        }
    }
    R->SetArrayField(TEXT("sources"),SourceEntries);R->SetArrayField(TEXT("lod_triangles"),Counts);
    FString Output;if(!FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Output)))return false;
    const FString Filename=FPaths::ProjectDir()/ImportReceipt;
    return FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(Filename))
        &&FFileHelper::SaveStringToFile(Output,*Filename,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_NoReplaceExisting);
}
void RollbackNewOutput()
{
    // Called only after the guarded CREATE_NEW save attempt, never for an existing package.
    const FString Filename=FPaths::ProjectDir()/AssetPath;
    IPlatformFile& Files=FPlatformFileManager::Get().GetPlatformFile();
    const bool bRestored=!Files.FileExists(*Filename)||Files.DeleteFile(*Filename);
    UE_LOG(LogTemp,Warning,TEXT("HERO_CLIFF_NEW_OUTPUT_ROLLBACK restored_absence=%s manifest_unchanged=true"),bRestored?TEXT("true"):TEXT("false"));
}
FMeshDescription Description(const FLOD& Data)
{
    FMeshDescription D;FStaticMeshAttributes A(D);A.Register();A.GetVertexInstanceUVs().SetNumChannels(1);
    TArray<FVertexID> Vertices;for(const FVector3f& P:Data.Points){const FVertexID V=D.CreateVertex();A.GetVertexPositions()[V]=P;Vertices.Add(V);}
    const FPolygonGroupID Group=D.CreatePolygonGroup();A.GetPolygonGroupMaterialSlotNames()[Group]=TEXT("Rock");
    for(int32 I=0;I<Data.Indices.Num();I+=3)
    {
        TArray<FVertexInstanceID,TInlineAllocator<3>> Corners;
        for(int32 C=0;C<3;++C)
        {
            const uint32 Index=Data.Indices[I+C];const FVertexInstanceID V=D.CreateVertexInstance(Vertices[Index]);
            A.GetVertexInstanceNormals()[V]=Data.Normals[Index];A.GetVertexInstanceUVs().Set(V,0,Data.UV[Index]);A.GetVertexInstanceColors()[V]=FVector4f(1,1,1,1);Corners.Add(V);
        }
        D.CreatePolygon(Group,Corners);
    }
    return D;
}
}

int32 ImportHeroCliffDraft()
{
    using namespace HeroCliffDraft;
    // This mode never executes the old texture/material/flora import loops.
    if(IFileManager::Get().IsSymlink(*(FPaths::ProjectDir()/SourceRoot)))return 20;
    TArray<uint8> Receipt;
    if(!ReadFixed(FString(SourceRoot)+TEXT("source-receipt.json"),13472,ReceiptHash,Receipt))return 21;
    FLOD LODs[3];for(int32 LOD=0;LOD<3;++LOD)if(!ReadLOD(Sources[LOD],LODs[LOD])){UE_LOG(LogTemp,Error,TEXT("HERO_CLIFF_SOURCE_REJECTED lod=%d"),LOD);return 22;}
    const int32 State=OutputState();
    if(State<0){UE_LOG(LogTemp,Error,TEXT("HERO_CLIFF_OUTPUT_CONFLICT refusing existing unregistered/modified/missing output"));return 23;}
    if(State==1){UE_LOG(LogTemp,Display,TEXT("HERO_CLIFF_ALREADY_REGISTERED no-op; no package writes"));return 0;}
    if(!SafeProjectPath(ImportReceipt)||IFileManager::Get().FileExists(*(FPaths::ProjectDir()/ImportReceipt)))return 32;
    UPackage* Package=CreatePackage(PackageName);Package->FullyLoad();
    if(FindObject<UObject>(Package,AssetName)){UE_LOG(LogTemp,Error,TEXT("HERO_CLIFF_IN_MEMORY_CONFLICT"));return 24;}
    UStaticMesh* Mesh=NewObject<UStaticMesh>(Package,AssetName,RF_Public|RF_Standalone);
    Mesh->GetStaticMaterials().Add(FStaticMaterial(UMaterial::GetDefaultMaterial(MD_Surface),TEXT("Rock")));
    Mesh->SetAutoComputeLODScreenSize(false);Mesh->GetNaniteSettings().bEnabled=false;Mesh->bHasNavigationData=false;
    for(int32 LOD=0;LOD<3;++LOD)
    {
        auto& Source=Mesh->AddSourceModel();Source.BuildSettings.bRecomputeNormals=false;Source.BuildSettings.bRecomputeTangents=true;
        Source.BuildSettings.bUseMikkTSpace=true;Source.BuildSettings.bUseFullPrecisionUVs=true;Source.BuildSettings.bGenerateLightmapUVs=false;
        Source.BuildSettings.bRemoveDegenerates=false;Source.ReductionSettings.PercentTriangles=1;Source.ReductionSettings.MaxDeviation=0;Source.ReductionSettings.BaseLODModel=LOD;
        Source.ScreenSize.Default=LOD==0?1.0F:LOD==1?.20F:.06F;
        Mesh->CreateMeshDescription(LOD,Description(LODs[LOD]));Mesh->CommitMeshDescription(LOD);
    }
    Mesh->CreateBodySetup();Mesh->GetBodySetup()->DefaultInstance.SetCollisionProfileName(TEXT("NoCollision"));
    Mesh->GetBodySetup()->CollisionTraceFlag=CTF_UseSimpleAsComplex;
    // Isolated visual review only. Original closed terrain remains collision authority; this
    // package must not be promoted into playable flight until the G1 surface fit is verified.
    if(!Mesh->AssetImportData)Mesh->AssetImportData=NewObject<UAssetImportData>(Mesh);
    const FString ImportFilename=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/SourceRoot/TEXT("coastal-cliff-40m-lod0.gltf"));
    // Let UE store a package-relative path. A raw project-relative argument would be
    // resolved against the commandlet working directory instead of the project.
    Mesh->AssetImportData->UpdateFilenameOnly(ImportFilename);
    if(!FPaths::IsSamePath(Mesh->AssetImportData->GetFirstFilename(),ImportFilename))return 34;
    Mesh->Build(false);TArray<UStaticMesh*> Compiling{Mesh};FStaticMeshCompilingManager::Get().FinishCompilation(Compiling);
    if(Mesh->GetNumLODs()!=3||!Mesh->GetRenderData())return 25;
    for(int32 LOD=0;LOD<3;++LOD)
    {
        const uint32 Triangles=Mesh->GetRenderData()->LODResources[LOD].GetNumTriangles();
        if(Triangles!=uint32(Sources[LOD].Triangles))return 26;
        UE_LOG(LogTemp,Display,TEXT("HERO_CLIFF_LOD lod=%d triangles=%u source_vertices=%d"),LOD,Triangles,Sources[LOD].Vertices);
    }
    Mesh->UpdateUVChannelData(true);
    const FMeshUVChannelInfo* UV=Mesh->GetUVChannelData(0);
    if(!UV||!UV->bInitialized||!FMath::IsFinite(UV->LocalUVDensities[0])||UV->LocalUVDensities[0]<=0)return 27;
    const FVector Size=Mesh->GetBoundingBox().GetSize();if(!FMath::IsNearlyEqual(Size.Y,4000.0,.1)||Size.Z<980||Size.Z>1020)return 28;
    const FString Filename=FPaths::ProjectDir()/AssetPath;
    // Recheck immediately before the only disk mutation. No reimport or overwrite path exists.
    if(OutputState()!=0||IFileManager::Get().IsSymlink(*FPaths::GetPath(Filename)))return 29;
    if(!FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(Filename)))return 30;
    Mesh->MarkPackageDirty();FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
    if(!UPackage::SavePackage(Package,Mesh,*Filename,Args)){RollbackNewOutput();return 31;}
    TArray<uint8> Saved;
    if(!FFileHelper::LoadFileToArray(Saved,*Filename)||Saved.Num()>64*1024*1024||!WriteImportReceipt(ProductEvidenceSha256(Saved)))
    {
        RollbackNewOutput();UE_LOG(LogTemp,Error,TEXT("HERO_CLIFF_RECEIPT_FAILED output registration refused; inspect the fixed ignored receipt before retry"));return 33;
    }
    FAssetRegistryModule::AssetCreated(Mesh);
    UE_LOG(LogTemp,Display,TEXT("HERO_CLIFF_IMPORT_COMPLETE assets=1 lods=3 state=DRAFT_NOT_USER_CONFIRMED visual=NOT_RUN collision=ISOLATED_NO_COLLISION; freeze manifest with --hero-cliff-only before rerun"));
    return 0;
}
