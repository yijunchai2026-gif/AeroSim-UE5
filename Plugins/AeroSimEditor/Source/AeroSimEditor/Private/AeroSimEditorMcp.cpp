#include "AeroSimEditorMcp.h"

#include "AeroSimEditorInspection.h"
#include "AeroSimPhase6Transaction.h"
#include "AeroSimPhase7Optimization.h"
#include "AeroSimSha256.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "Json.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "WorldPartition/WorldPartition.h"

namespace AeroSimEditorMcp
{
const TCHAR* RequestSchema = TEXT("aerosim.editor.mcp.request/v1");
const TCHAR* ResponseSchema = TEXT("aerosim.editor.mcp.response/v1");
const TCHAR* CapabilitySchema = TEXT("aerosim.editor.mcp.capabilities/v1");
const TCHAR* Runtime = TEXT("aerosim.runtime.phase7/v1");

FCriticalSection AuditMutex;
TArray<FAeroSimEditorMcpAuditEntry> AuditEntries; // append-only for this Editor process lifetime
FAeroSimPhase6TransactionStore TransactionStore;
TMap<FString, FString> IdempotentRequestDigests;
TMap<FString, FString> IdempotentResponses;

FString Sha256(const FString& Value);

const TArray<FString> ReadOperations = {
    TEXT("capabilities"), TEXT("heartbeat"), TEXT("inspect_project"), TEXT("inspect_world"),
    TEXT("list_actors"), TEXT("inspect_actor"), TEXT("inspect_component"),
    TEXT("inspect_asset_metadata"), TEXT("inspect_data_layers"), TEXT("inspect_world_partition"),
    TEXT("validate_scene"), TEXT("capture_request"), TEXT("inspect_audit"), TEXT("suggest_optimization"),
    TEXT("transaction_inspect"), TEXT("transaction_audit")
};

const TArray<FString> TransactionOperations = {
    TEXT("transaction_dry_run"), TEXT("transaction_snapshot"), TEXT("transaction_apply"),
    TEXT("transaction_validate"), TEXT("transaction_evidence"), TEXT("transaction_commit"),
    TEXT("transaction_rollback"), TEXT("transaction_undo"), TEXT("transaction_redo")
};

const TArray<FString> DeniedSurfaces = {
    TEXT("shell"), TEXT("python"), TEXT("console"), TEXT("process"), TEXT("network"),
    TEXT("arbitrary_file"), TEXT("arbitrary_reflection"), TEXT("arbitrary_uobject"),
    TEXT("production_map"), TEXT("production_asset"), TEXT("arbitrary_asset"), TEXT("promote")
};

const TArray<FString> FixedCameras = {
    TEXT("aircraft-three-quarter"), TEXT("fpv"), TEXT("gimbal-down"),
    TEXT("near-follow"), TEXT("far-follow"), TEXT("scale-support")
};

TSharedRef<FJsonObject> Object()
{
    return MakeShared<FJsonObject>();
}

TArray<TSharedPtr<FJsonValue>> Strings(const TArray<FString>& Values)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    for (const FString& Value : Values)
    {
        Result.Add(MakeShared<FJsonValueString>(Value));
    }
    return Result;
}

TSharedRef<FJsonObject> Vector(const FVector& Value)
{
    TSharedRef<FJsonObject> Result = Object();
    Result->SetNumberField(TEXT("x"), Value.X);
    Result->SetNumberField(TEXT("y"), Value.Y);
    Result->SetNumberField(TEXT("z"), Value.Z);
    return Result;
}

TSharedRef<FJsonObject> Rotation(const FRotator& Value)
{
    TSharedRef<FJsonObject> Result = Object();
    Result->SetNumberField(TEXT("pitch"), Value.Pitch);
    Result->SetNumberField(TEXT("yaw"), Value.Yaw);
    Result->SetNumberField(TEXT("roll"), Value.Roll);
    return Result;
}

TSharedRef<FJsonObject> Transform(const FTransform& Value)
{
    TSharedRef<FJsonObject> Result = Object();
    Result->SetObjectField(TEXT("location_cm"), Vector(Value.GetLocation()));
    Result->SetObjectField(TEXT("rotation_deg"), Rotation(Value.Rotator()));
    Result->SetObjectField(TEXT("scale"), Vector(Value.GetScale3D()));
    return Result;
}

FString Serialize(const TSharedRef<FJsonObject>& Value)
{
    FString Result;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Result);
    FJsonSerializer::Serialize(Value, Writer);
    return Result;
}

TSharedRef<FJsonObject> Envelope(const FString& RequestId, const bool bOk)
{
    TSharedRef<FJsonObject> Root = Object();
    Root->SetStringField(TEXT("schema"), ResponseSchema);
    Root->SetNumberField(TEXT("version"), 1);
    Root->SetStringField(TEXT("request_id"), RequestId);
    Root->SetBoolField(TEXT("ok"), bOk);
    return Root;
}

FString ErrorJson(const FString& RequestId, const TCHAR* Code, const FString& Message)
{
    TSharedRef<FJsonObject> Root = Envelope(RequestId, false);
    TSharedRef<FJsonObject> Error = Object();
    Error->SetStringField(TEXT("code"), Code);
    Error->SetStringField(TEXT("message"), Message);
    Error->SetBoolField(TEXT("retryable"), FCString::Strcmp(Code, TEXT("TIMEOUT")) == 0);
    Root->SetObjectField(TEXT("error"), Error);
    return Serialize(Root);
}

bool ValidRequestId(const FString& Value)
{
    if (Value.IsEmpty() || Value.Len() > 64)
    {
        return false;
    }
    for (const TCHAR Character : Value)
    {
        if (!FChar::IsAlnum(Character) && Character != TEXT('-') && Character != TEXT('_') && Character != TEXT('.'))
        {
            return false;
        }
    }
    return true;
}

bool AllowedAssetPath(const FString& Path)
{
    return Path.Len() <= 256 && Path.StartsWith(TEXT("/Game/"), ESearchCase::CaseSensitive)
        && !Path.Contains(TEXT("..")) && !Path.Contains(TEXT("\\")) && !Path.Contains(TEXT(":"));
}

bool MutationLike(const FString& Operation)
{
    static const TArray<FString> Tokens = {
        TEXT("apply"), TEXT("update"), TEXT("create"), TEXT("spawn"), TEXT("delete"), TEXT("remove"),
        TEXT("modify"), TEXT("write"), TEXT("save"), TEXT("import"), TEXT("promote"),
        TEXT("execute"), TEXT("shell"), TEXT("python"), TEXT("console"), TEXT("network"), TEXT("import"), TEXT("promote")
    };
    for (const FString& Token : Tokens)
    {
        if (Operation.Contains(Token, ESearchCase::IgnoreCase))
        {
            return true;
        }
    }
    return false;
}

UWorld* EditorWorld()
{
    return GEditor != nullptr ? GEditor->GetEditorWorldContext().World() : nullptr;
}

AActor* FindActorInWorld(UWorld* World, const FString& Id)
{
    if (World == nullptr || Id.Len() > 512 || Id.Contains(TEXT("..")))
    {
        return nullptr;
    }
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetPathName() == Id)
        {
            return *It;
        }
    }
    return nullptr;
}

void AppendAudit(const FString& RequestId, const FString& Operation, const FString& Code, const FString& RequestJson, const double Started)
{
    FAeroSimEditorMcpAuditEntry Entry;
    Entry.TimestampUtc = FDateTime::UtcNow().ToIso8601();
    Entry.RequestId = RequestId.Left(64);
    Entry.Operation = Operation.Left(64);
    Entry.ResultCode = Code.Left(64);
    Entry.RequestDigest = Sha256(RequestJson);
    Entry.DurationMs = FMath::Max(0, FMath::RoundToInt((FPlatformTime::Seconds() - Started) * 1000.0));
    FScopeLock Lock(&AuditMutex);
    AuditEntries.Add(MoveTemp(Entry));
}

bool IsTimedOut(const double Deadline)
{
    return FPlatformTime::Seconds() > Deadline;
}

TSharedRef<FJsonObject> Capabilities()
{
    TSharedRef<FJsonObject> Data = Object();
    Data->SetStringField(TEXT("schema"), CapabilitySchema);
    Data->SetNumberField(TEXT("version"), 1);
    Data->SetStringField(TEXT("runtime_contract"), Runtime);
    Data->SetBoolField(TEXT("read_only"), false);
    Data->SetBoolField(TEXT("writes_sandbox_only"), true);
    Data->SetArrayField(TEXT("operations"), Strings(ReadOperations));
    Data->SetArrayField(TEXT("transaction_operations"), Strings(TransactionOperations));
    Data->SetArrayField(TEXT("denied_surfaces"), Strings(DeniedSurfaces));
    Data->SetArrayField(TEXT("asset_path_allowlist"), Strings({ TEXT("/Game/") }));
    Data->SetArrayField(TEXT("write_map_allowlist"), Strings({ TEXT("/Game/AeroSim/Sandbox/Maps/AI_Review_Sandbox") }));
    Data->SetArrayField(TEXT("write_data_layer_allowlist"), Strings({ TEXT("AeroSim_AI_Draft") }));
    Data->SetArrayField(TEXT("approved_spawn_ids"), Strings({ TEXT("primitive.cube"), TEXT("primitive.sphere"), TEXT("primitive.cylinder"), TEXT("prefab.review-marker"), TEXT("prefab.route-gate") }));
    Data->SetArrayField(TEXT("finite_actions"), Strings({ TEXT("spawn_approved"), TEXT("set_transform"), TEXT("set_material_scalar"), TEXT("add_tag"), TEXT("remove_tag"), TEXT("delete_draft") }));
    TSharedRef<FJsonObject> Limits = Object();
    Limits->SetNumberField(TEXT("max_request_bytes"), FAeroSimEditorMcpLimits::MaximumRequestBytes);
    Limits->SetNumberField(TEXT("max_response_bytes"), FAeroSimEditorMcpLimits::MaximumResponseBytes);
    Limits->SetNumberField(TEXT("max_page_size"), FAeroSimEditorMcpLimits::MaximumPageSize);
    Limits->SetNumberField(TEXT("max_timeout_ms"), FAeroSimEditorMcpLimits::MaximumTimeoutMs);
    Data->SetObjectField(TEXT("limits"), Limits);
    Data->SetStringField(TEXT("transport"), TEXT("in_process_only"));
    Data->SetStringField(TEXT("audit"), TEXT("append-only memory; no external sensitive writes"));
    Data->SetStringField(TEXT("optimization_status"), TEXT("DRAFT_NOT_USER_CONFIRMED"));
    Data->SetBoolField(TEXT("optimization_advisory_only"), true);
    Data->SetBoolField(TEXT("optimization_requires_phase6_transaction"), true);
    return Data;
}

TSharedRef<FJsonObject> ActorJson(const AActor& Actor)
{
    TSharedRef<FJsonObject> Item = Object();
    Item->SetStringField(TEXT("actor_id"), Actor.GetPathName());
    Item->SetStringField(TEXT("name"), Actor.GetName());
    Item->SetStringField(TEXT("label"), Actor.GetActorLabel());
    Item->SetStringField(TEXT("class"), Actor.GetClass()->GetPathName());
    Item->SetObjectField(TEXT("transform"), Transform(Actor.GetActorTransform()));
    Item->SetBoolField(TEXT("hidden_editor"), Actor.IsHiddenEd());
    Item->SetNumberField(TEXT("component_count"), Actor.GetComponents().Num());
    return Item;
}

FString Sha256(const FString& Value)
{
    return AeroSimSha256(Value);
}

FVector JsonVector(const TSharedPtr<FJsonObject>& Value, const FVector& Default)
{
    if (!Value.IsValid()) { return Default; }
    double X = Default.X;
    double Y = Default.Y;
    double Z = Default.Z;
    Value->TryGetNumberField(TEXT("x"), X);
    Value->TryGetNumberField(TEXT("y"), Y);
    Value->TryGetNumberField(TEXT("z"), Z);
    return FVector(X, Y, Z);
}

FPhase6Result ParsePatch(const TSharedPtr<FJsonObject>& Params, FPhase6Patch& OutPatch)
{
    const TSharedPtr<FJsonObject>* PatchPointer = nullptr;
    if (!Params.IsValid() || !Params->TryGetObjectField(TEXT("patch"), PatchPointer) || PatchPointer == nullptr || !PatchPointer->IsValid())
    {
        return { false, TEXT("INVALID_PATCH"), TEXT("params.patch must be an object"), TEXT("{}") };
    }
    const TSharedPtr<FJsonObject> Patch = *PatchPointer;
    Patch->TryGetStringField(TEXT("schema"), OutPatch.Schema);
    double Revision = 0.0;
    Patch->TryGetNumberField(TEXT("base_revision"), Revision);
    OutPatch.BaseRevision = FMath::RoundToInt64(Revision);
    Patch->TryGetStringField(TEXT("base_hash"), OutPatch.BaseHash);
    const TArray<TSharedPtr<FJsonValue>>* Actions = nullptr;
    if (!Patch->TryGetArrayField(TEXT("actions"), Actions) || Actions == nullptr)
    {
        return { false, TEXT("INVALID_PATCH"), TEXT("patch.actions must be an array"), TEXT("{}") };
    }
    for (const TSharedPtr<FJsonValue>& ActionValue : *Actions)
    {
        const TSharedPtr<FJsonObject> ActionObject = ActionValue.IsValid() ? ActionValue->AsObject() : nullptr;
        if (!ActionObject.IsValid())
        {
            return { false, TEXT("INVALID_PATCH"), TEXT("each action must be an object"), TEXT("{}") };
        }
        FString Type;
        FPhase6Action Action;
        ActionObject->TryGetStringField(TEXT("type"), Type);
        ActionObject->TryGetStringField(TEXT("actor_id"), Action.ActorId);
        ActionObject->TryGetStringField(TEXT("asset_id"), Action.AssetId);
        ActionObject->TryGetStringField(TEXT("parameter_id"), Action.ParameterId);
        ActionObject->TryGetStringField(TEXT("tag"), Action.Tag);
        ActionObject->TryGetNumberField(TEXT("value"), Action.ScalarValue);
        ActionObject->TryGetBoolField(TEXT("snap"), Action.bSnap);
        if (Type == TEXT("spawn_approved")) { Action.Type = EPhase6ActionType::SpawnApproved; }
        else if (Type == TEXT("set_transform")) { Action.Type = EPhase6ActionType::SetTransform; }
        else if (Type == TEXT("set_material_scalar")) { Action.Type = EPhase6ActionType::SetMaterialScalar; }
        else if (Type == TEXT("add_tag")) { Action.Type = EPhase6ActionType::AddTag; }
        else if (Type == TEXT("remove_tag")) { Action.Type = EPhase6ActionType::RemoveTag; }
        else if (Type == TEXT("delete_draft")) { Action.Type = EPhase6ActionType::DeleteDraft; }
        else { return { false, TEXT("ACTION_NOT_ALLOWED"), TEXT("action type is outside the finite command set"), TEXT("{}") }; }

        const TSharedPtr<FJsonObject>* TransformPointer = nullptr;
        if (ActionObject->TryGetObjectField(TEXT("transform"), TransformPointer) && TransformPointer != nullptr && TransformPointer->IsValid())
        {
            const TSharedPtr<FJsonObject>* Location = nullptr;
            const TSharedPtr<FJsonObject>* RotationValue = nullptr;
            const TSharedPtr<FJsonObject>* Scale = nullptr;
            (*TransformPointer)->TryGetObjectField(TEXT("location_cm"), Location);
            (*TransformPointer)->TryGetObjectField(TEXT("rotation_deg"), RotationValue);
            (*TransformPointer)->TryGetObjectField(TEXT("scale"), Scale);
            const FVector LocationVector = JsonVector(Location != nullptr ? *Location : nullptr, FVector::ZeroVector);
            const FVector RotationVector = JsonVector(RotationValue != nullptr ? *RotationValue : nullptr, FVector::ZeroVector);
            const FVector ScaleVector = JsonVector(Scale != nullptr ? *Scale : nullptr, FVector::OneVector);
            Action.Transform = FTransform(FRotator(RotationVector.X, RotationVector.Y, RotationVector.Z), LocationVector, ScaleVector);
        }
        OutPatch.Actions.Add(MoveTemp(Action));
    }
    return { true, TEXT("OK"), TEXT(""), TEXT("{}") };
}

TSharedRef<FJsonObject> ParseResultData(const FPhase6Result& Result)
{
    TSharedPtr<FJsonObject> Parsed;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Result.Data);
    return FJsonSerializer::Deserialize(Reader, Parsed) && Parsed.IsValid() ? Parsed.ToSharedRef() : Object();
}
}

FString UAeroSimEditorMcp::ExecuteRequest(const FString& RequestJson)
{
    using namespace AeroSimEditorMcp;
    const double Started = FPlatformTime::Seconds();
    FString RequestId = TEXT("unavailable");
    FString Operation = TEXT("invalid");
    FString ResultCode = TEXT("INTERNAL_ERROR");
    FString Response;

    auto Finish = [&](const FString& Value, const FString& Code)
    {
        ResultCode = Code;
        FString Bounded = Value;
        if (Bounded.Len() * sizeof(TCHAR) > FAeroSimEditorMcpLimits::MaximumResponseBytes)
        {
            ResultCode = TEXT("RESPONSE_TOO_LARGE");
            Bounded = ErrorJson(RequestId, TEXT("RESPONSE_TOO_LARGE"), TEXT("Response exceeded the fixed output limit; request a smaller page."));
        }
        AppendAudit(RequestId, Operation, ResultCode, RequestJson, Started);
        return Bounded;
    };

    if (!IsInGameThread())
    {
        return Finish(ErrorJson(RequestId, TEXT("EDITOR_THREAD_REQUIRED"), TEXT("Requests must run synchronously on the Editor game thread.")), TEXT("EDITOR_THREAD_REQUIRED"));
    }
    if (RequestJson.Len() * sizeof(TCHAR) > FAeroSimEditorMcpLimits::MaximumRequestBytes)
    {
        return Finish(ErrorJson(RequestId, TEXT("REQUEST_TOO_LARGE"), TEXT("Request exceeds the fixed input limit.")), TEXT("REQUEST_TOO_LARGE"));
    }

    TSharedPtr<FJsonObject> Request;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RequestJson);
    if (!FJsonSerializer::Deserialize(Reader, Request) || !Request.IsValid())
    {
        return Finish(ErrorJson(RequestId, TEXT("INVALID_JSON"), TEXT("Request must be one JSON object.")), TEXT("INVALID_JSON"));
    }

    Request->TryGetStringField(TEXT("request_id"), RequestId);
    Request->TryGetStringField(TEXT("operation"), Operation);
    FString Schema;
    double Version = 0.0;
    Request->TryGetStringField(TEXT("schema"), Schema);
    Request->TryGetNumberField(TEXT("version"), Version);
    if (Schema != RequestSchema || Version != 1.0)
    {
        return Finish(ErrorJson(RequestId, TEXT("SCHEMA_UNSUPPORTED"), TEXT("Only aerosim.editor.mcp.request/v1 is accepted.")), TEXT("SCHEMA_UNSUPPORTED"));
    }
    if (!ValidRequestId(RequestId))
    {
        return Finish(ErrorJson(TEXT("unavailable"), TEXT("INVALID_REQUEST_ID"), TEXT("request_id must be 1-64 ASCII letters, digits, dot, dash, or underscore.")), TEXT("INVALID_REQUEST_ID"));
    }
    if (Operation.IsEmpty() || Operation.Len() > 64)
    {
        return Finish(ErrorJson(RequestId, TEXT("INVALID_OPERATION"), TEXT("operation is required and limited to 64 characters.")), TEXT("INVALID_OPERATION"));
    }

    const FString RequestDigest = Sha256(RequestJson);
    if (TransactionOperations.Contains(Operation))
    {
        if (const FString* ExistingDigest = IdempotentRequestDigests.Find(RequestId))
        {
            if (*ExistingDigest != RequestDigest)
            {
                return Finish(ErrorJson(RequestId, TEXT("REQUEST_ID_REUSE"), TEXT("request_id was already used with a different SHA256 digest.")), TEXT("REQUEST_ID_REUSE"));
            }
            if (const FString* ExistingResponse = IdempotentResponses.Find(RequestId))
            {
                return *ExistingResponse;
            }
        }
    }

    const TSharedPtr<FJsonObject>* ParamsPointer = nullptr;
    TSharedPtr<FJsonObject> Params = Object();
    if (Request->TryGetObjectField(TEXT("params"), ParamsPointer) && ParamsPointer != nullptr && ParamsPointer->IsValid())
    {
        Params = *ParamsPointer;
    }
    double TimeoutValue = FAeroSimEditorMcpLimits::DefaultTimeoutMs;
    Request->TryGetNumberField(TEXT("timeout_ms"), TimeoutValue);
    const int32 TimeoutMs = FMath::Clamp(FMath::RoundToInt(TimeoutValue), 1, FAeroSimEditorMcpLimits::MaximumTimeoutMs);
    const double Deadline = Started + (static_cast<double>(TimeoutMs) / 1000.0);

    if (MutationLike(Operation) && !TransactionOperations.Contains(Operation))
    {
        return Finish(ErrorJson(RequestId, TEXT("READ_ONLY_DENIED"), TEXT("Mutation is only available through the finite Phase 6 transaction operations.")), TEXT("READ_ONLY_DENIED"));
    }
    if (!ReadOperations.Contains(Operation) && !TransactionOperations.Contains(Operation))
    {
        return Finish(ErrorJson(RequestId, TEXT("OPERATION_NOT_SUPPORTED"), TEXT("Operation is not in capability discovery.")), TEXT("OPERATION_NOT_SUPPORTED"));
    }

    TSharedRef<FJsonObject> Data = Object();
    if (Operation == TEXT("capabilities"))
    {
        Data = Capabilities();
    }
    else if (Operation == TEXT("heartbeat"))
    {
        const FAeroSimHeartbeat Heartbeat = UAeroSimEditorInspection::Heartbeat();
        Data->SetStringField(TEXT("service"), Heartbeat.Service);
        Data->SetStringField(TEXT("protocol_version"), Heartbeat.ProtocolVersion);
        Data->SetBoolField(TEXT("healthy"), Heartbeat.bHealthy);
        Data->SetStringField(TEXT("status"), Heartbeat.Status);
        Data->SetStringField(TEXT("runtime_contract"), Runtime);
    }
    else if (Operation == TEXT("inspect_project"))
    {
        const FAeroSimProjectInspection Inspection = UAeroSimEditorInspection::InspectProject();
        Data->SetStringField(TEXT("project_name"), Inspection.ProjectName);
        Data->SetStringField(TEXT("engine_version"), Inspection.EngineVersion);
        Data->SetBoolField(TEXT("is_editor"), Inspection.bIsEditor);
        Data->SetBoolField(TEXT("read_only"), true);
        Data->SetStringField(TEXT("runtime_contract"), Runtime);
    }
    else if (Operation == TEXT("inspect_audit"))
    {
        double OffsetValue = 0.0;
        double LimitValue = FAeroSimEditorMcpLimits::DefaultPageSize;
        Params->TryGetNumberField(TEXT("offset"), OffsetValue);
        Params->TryGetNumberField(TEXT("limit"), LimitValue);
        const TArray<FAeroSimEditorMcpAuditEntry> Entries = InspectAudit(FMath::Max(0, FMath::RoundToInt(OffsetValue)), FMath::RoundToInt(LimitValue));
        TArray<TSharedPtr<FJsonValue>> Items;
        for (const FAeroSimEditorMcpAuditEntry& Entry : Entries)
        {
            TSharedRef<FJsonObject> Item = Object();
            Item->SetStringField(TEXT("timestamp_utc"), Entry.TimestampUtc);
            Item->SetStringField(TEXT("request_id"), Entry.RequestId);
            Item->SetStringField(TEXT("operation"), Entry.Operation);
            Item->SetStringField(TEXT("result_code"), Entry.ResultCode);
            Item->SetStringField(TEXT("request_digest_sha256"), Entry.RequestDigest);
            Item->SetNumberField(TEXT("duration_ms"), Entry.DurationMs);
            Items.Add(MakeShared<FJsonValueObject>(Item));
        }
        Data->SetArrayField(TEXT("items"), Items);
        Data->SetBoolField(TEXT("external_write"), false);
    }
    else if (Operation == TEXT("transaction_inspect"))
    {
        const FPhase6InspectResult Inspection = TransactionStore.Inspect();
        Data->SetNumberField(TEXT("revision"), static_cast<double>(Inspection.Revision));
        Data->SetStringField(TEXT("draft_hash"), Inspection.DraftHash);
        Data->SetStringField(TEXT("map_id"), Inspection.MapId);
        Data->SetStringField(TEXT("data_layer_id"), Inspection.DataLayerId);
        Data->SetNumberField(TEXT("actor_count"), Inspection.ActorCount);
        Data->SetStringField(TEXT("status"), TEXT("DRAFT_NOT_USER_CONFIRMED"));
    }
    else if (Operation == TEXT("transaction_audit"))
    {
        TSharedPtr<FJsonObject> AuditData;
        const TSharedRef<TJsonReader<>> AuditReader = TJsonReaderFactory<>::Create(TransactionStore.InspectAudit());
        if (FJsonSerializer::Deserialize(AuditReader, AuditData) && AuditData.IsValid())
        {
            Data = AuditData.ToSharedRef();
        }
    }
    else if (TransactionOperations.Contains(Operation))
    {
        FString TransactionId;
        Params->TryGetStringField(TEXT("transaction_id"), TransactionId);
        FPhase6Result TransactionResult;
        if (Operation == TEXT("transaction_dry_run"))
        {
            FPhase6Patch Patch;
            TransactionResult = ParsePatch(Params, Patch);
            if (TransactionResult.bOk)
            {
                TransactionResult = TransactionStore.DryRun(TransactionId, Patch);
            }
        }
        else if (Operation == TEXT("transaction_snapshot")) { TransactionResult = TransactionStore.Snapshot(TransactionId); }
        else if (Operation == TEXT("transaction_apply")) { TransactionResult = TransactionStore.Apply(TransactionId); }
        else if (Operation == TEXT("transaction_validate")) { TransactionResult = TransactionStore.Validate(TransactionId); }
        else if (Operation == TEXT("transaction_commit")) { TransactionResult = TransactionStore.Commit(TransactionId); }
        else if (Operation == TEXT("transaction_undo")) { TransactionResult = TransactionStore.Undo(TransactionId); }
        else if (Operation == TEXT("transaction_redo")) { TransactionResult = TransactionStore.Redo(TransactionId); }
        else if (Operation == TEXT("transaction_rollback"))
        {
            FString Reason = TEXT("caller-requested");
            Params->TryGetStringField(TEXT("reason"), Reason);
            TransactionResult = TransactionStore.Rollback(TransactionId, Reason.Left(128));
        }
        else if (Operation == TEXT("transaction_evidence"))
        {
            FString Moment;
            FString CameraId;
            Params->TryGetStringField(TEXT("moment"), Moment);
            Params->TryGetStringField(TEXT("camera_id"), CameraId);
            TransactionResult = TransactionStore.Evidence(TransactionId, Moment, CameraId);
        }
        if (!TransactionResult.bOk)
        {
            return Finish(ErrorJson(RequestId, *TransactionResult.Code, TransactionResult.Message), TransactionResult.Code);
        }
        Data = ParseResultData(TransactionResult);
    }
    else if (Operation == TEXT("suggest_optimization"))
    {
        FPhase7OptimizationRequest SuggestionRequest;
        SuggestionRequest.RequestId = RequestId;
        Params->TryGetStringField(TEXT("map_id"), SuggestionRequest.MapId);
        Params->TryGetStringField(TEXT("data_layer_id"), SuggestionRequest.DataLayerId);
        Params->TryGetStringField(TEXT("quality_profile_id"), SuggestionRequest.QualityProfileId);
        double MaximumSuggestions = 8.0;
        Params->TryGetNumberField(TEXT("maximum_suggestions"), MaximumSuggestions);
        SuggestionRequest.MaximumSuggestions = FMath::RoundToInt(MaximumSuggestions);
        FString Kind;
        Params->TryGetStringField(TEXT("kind"), Kind);
        if (Kind == TEXT("layout")) { SuggestionRequest.Kind = EPhase7OptimizationKind::Layout; }
        else if (Kind == TEXT("lighting")) { SuggestionRequest.Kind = EPhase7OptimizationKind::Lighting; }
        else if (Kind == TEXT("lod")) { SuggestionRequest.Kind = EPhase7OptimizationKind::Lod; }
        else if (Kind == TEXT("performance")) { SuggestionRequest.Kind = EPhase7OptimizationKind::Performance; }
        else { return Finish(ErrorJson(RequestId, TEXT("KIND_NOT_ALLOWED"), TEXT("kind must be layout, lighting, lod, or performance.")), TEXT("KIND_NOT_ALLOWED")); }
        FString SuggestionError;
        const TArray<FPhase7OptimizationSuggestion> Suggestions = FPhase7OptimizationAdvisor::Suggest(SuggestionRequest, SuggestionError);
        if (!SuggestionError.IsEmpty())
        {
            return Finish(ErrorJson(RequestId, *SuggestionError, TEXT("Optimization request failed closed.")), SuggestionError);
        }
        TArray<TSharedPtr<FJsonValue>> Items;
        for (const FPhase7OptimizationSuggestion& Suggestion : Suggestions)
        {
            TSharedRef<FJsonObject> Item = Object();
            Item->SetStringField(TEXT("suggestion_id"), Suggestion.SuggestionId);
            Item->SetStringField(TEXT("summary"), Suggestion.Summary);
            Item->SetStringField(TEXT("rationale"), Suggestion.Rationale);
            Item->SetStringField(TEXT("status"), Suggestion.Status);
            Item->SetStringField(TEXT("required_gate"), Suggestion.RequiredGate);
            Item->SetBoolField(TEXT("can_convert_to_scene_patch"), Suggestion.bCanConvertToScenePatch);
            Item->SetBoolField(TEXT("applied"), Suggestion.bApplied);
            Items.Add(MakeShared<FJsonValueObject>(Item));
        }
        Data->SetArrayField(TEXT("items"), Items);
        Data->SetBoolField(TEXT("requires_human_review"), true);
        Data->SetBoolField(TEXT("requires_phase6_transaction_for_write"), true);
        Data->SetStringField(TEXT("status"), TEXT("DRAFT_NOT_USER_CONFIRMED"));
    }
    else
    {
        UWorld* World = EditorWorld();
        if (World == nullptr)
        {
            return Finish(ErrorJson(RequestId, TEXT("EDITOR_WORLD_UNAVAILABLE"), TEXT("No active Editor world is available.")), TEXT("EDITOR_WORLD_UNAVAILABLE"));
        }

        if (Operation == TEXT("inspect_world"))
        {
            Data->SetStringField(TEXT("world_id"), World->GetPathName());
            Data->SetStringField(TEXT("map_name"), World->GetMapName());
            Data->SetStringField(TEXT("world_type"), LexToString(World->WorldType));
            Data->SetBoolField(TEXT("is_partitioned"), World->IsPartitionedWorld());
            int32 ActorCount = 0;
            for (TActorIterator<AActor> It(World); It; ++It) { ++ActorCount; }
            Data->SetNumberField(TEXT("loaded_actor_count"), ActorCount);
        }
        else if (Operation == TEXT("list_actors"))
        {
            double CursorValue = 0.0;
            double LimitValue = FAeroSimEditorMcpLimits::DefaultPageSize;
            Params->TryGetNumberField(TEXT("cursor"), CursorValue);
            Params->TryGetNumberField(TEXT("limit"), LimitValue);
            const int32 Cursor = FMath::Max(0, FMath::RoundToInt(CursorValue));
            const int32 Limit = FMath::Clamp(FMath::RoundToInt(LimitValue), 1, FAeroSimEditorMcpLimits::MaximumPageSize);
            TArray<TSharedPtr<FJsonValue>> Items;
            int32 Index = 0;
            bool bHasMore = false;
            for (TActorIterator<AActor> It(World); It; ++It, ++Index)
            {
                if (IsTimedOut(Deadline))
                {
                    return Finish(ErrorJson(RequestId, TEXT("TIMEOUT"), TEXT("Actor enumeration exceeded timeout.")), TEXT("TIMEOUT"));
                }
                if (Index < Cursor) { continue; }
                if (Items.Num() >= Limit) { bHasMore = true; break; }
                Items.Add(MakeShared<FJsonValueObject>(ActorJson(**It)));
            }
            Data->SetArrayField(TEXT("items"), Items);
            Data->SetNumberField(TEXT("cursor"), Cursor);
            Data->SetNumberField(TEXT("limit"), Limit);
            Data->SetBoolField(TEXT("has_more"), bHasMore);
            if (bHasMore) { Data->SetNumberField(TEXT("next_cursor"), Cursor + Items.Num()); }
        }
        else if (Operation == TEXT("inspect_actor") || Operation == TEXT("inspect_component"))
        {
            FString ActorId;
            Params->TryGetStringField(TEXT("actor_id"), ActorId);
            AActor* Actor = FindActorInWorld(World, ActorId);
            if (Actor == nullptr)
            {
                return Finish(ErrorJson(RequestId, TEXT("ACTOR_NOT_FOUND"), TEXT("actor_id must exactly match an actor in the active Editor world.")), TEXT("ACTOR_NOT_FOUND"));
            }
            if (Operation == TEXT("inspect_actor"))
            {
                Data = ActorJson(*Actor);
                TArray<TSharedPtr<FJsonValue>> Components;
                for (const UActorComponent* Component : Actor->GetComponents())
                {
                    TSharedRef<FJsonObject> Item = Object();
                    Item->SetStringField(TEXT("component_id"), Component->GetPathName());
                    Item->SetStringField(TEXT("name"), Component->GetName());
                    Item->SetStringField(TEXT("class"), Component->GetClass()->GetPathName());
                    Components.Add(MakeShared<FJsonValueObject>(Item));
                }
                Data->SetArrayField(TEXT("components"), Components);
            }
            else
            {
                FString ComponentId;
                Params->TryGetStringField(TEXT("component_id"), ComponentId);
                const UActorComponent* Match = nullptr;
                for (const UActorComponent* Component : Actor->GetComponents())
                {
                    if (Component->GetPathName() == ComponentId) { Match = Component; break; }
                }
                if (Match == nullptr)
                {
                    return Finish(ErrorJson(RequestId, TEXT("COMPONENT_NOT_FOUND"), TEXT("component_id must belong to actor_id.")), TEXT("COMPONENT_NOT_FOUND"));
                }
                Data->SetStringField(TEXT("component_id"), Match->GetPathName());
                Data->SetStringField(TEXT("name"), Match->GetName());
                Data->SetStringField(TEXT("class"), Match->GetClass()->GetPathName());
                Data->SetBoolField(TEXT("active"), Match->IsActive());
                if (const USceneComponent* Scene = Cast<USceneComponent>(Match))
                {
                    Data->SetObjectField(TEXT("world_transform"), Transform(Scene->GetComponentTransform()));
                    Data->SetBoolField(TEXT("visible"), Scene->IsVisible());
                    Data->SetStringField(TEXT("mobility"), StaticEnum<EComponentMobility::Type>()->GetNameStringByValue(Scene->Mobility));
                }
            }
        }
        else if (Operation == TEXT("inspect_asset_metadata"))
        {
            FString AssetPath;
            Params->TryGetStringField(TEXT("asset_path"), AssetPath);
            if (!AllowedAssetPath(AssetPath))
            {
                return Finish(ErrorJson(RequestId, TEXT("PATH_NOT_ALLOWED"), TEXT("Only normalized /Game/ object paths are allowed.")), TEXT("PATH_NOT_ALLOWED"));
            }
            const FAssetRegistryModule& Module = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
            const FAssetData Asset = Module.Get().GetAssetByObjectPath(FSoftObjectPath(AssetPath));
            if (!Asset.IsValid())
            {
                return Finish(ErrorJson(RequestId, TEXT("ASSET_NOT_FOUND"), TEXT("No indexed asset matches asset_path; assets are never loaded by this operation.")), TEXT("ASSET_NOT_FOUND"));
            }
            Data->SetStringField(TEXT("object_path"), Asset.GetObjectPathString());
            Data->SetStringField(TEXT("package_name"), Asset.PackageName.ToString());
            Data->SetStringField(TEXT("asset_name"), Asset.AssetName.ToString());
            Data->SetStringField(TEXT("class_path"), Asset.AssetClassPath.ToString());
            Data->SetNumberField(TEXT("package_flags"), Asset.PackageFlags);
            Data->SetBoolField(TEXT("on_disk"), Asset.IsUAsset());
            Data->SetBoolField(TEXT("object_loaded"), Asset.IsAssetLoaded());
        }
        else if (Operation == TEXT("inspect_data_layers"))
        {
            TSet<FString> Seen;
            TArray<TSharedPtr<FJsonValue>> Layers;
            for (TActorIterator<AActor> It(World); It; ++It)
            {
                for (const UDataLayerInstance* Layer : It->GetDataLayerInstances())
                {
                    if (Layer == nullptr) { continue; }
                    const FString LayerId = Layer->GetFName().ToString();
                    if (Seen.Contains(LayerId)) { continue; }
                    Seen.Add(LayerId);
                    TSharedRef<FJsonObject> Item = Object();
                    Item->SetStringField(TEXT("id"), LayerId);
                    Item->SetStringField(TEXT("short_name"), Layer->GetDataLayerShortName());
                    Item->SetBoolField(TEXT("effective_loaded_in_editor"), Layer->IsEffectiveLoadedInEditor());
                    Layers.Add(MakeShared<FJsonValueObject>(Item));
                }
            }
            Data->SetArrayField(TEXT("items"), Layers);
            Data->SetNumberField(TEXT("count"), Layers.Num());
        }
        else if (Operation == TEXT("inspect_world_partition"))
        {
            const UWorldPartition* Partition = World->GetWorldPartition();
            Data->SetBoolField(TEXT("enabled"), Partition != nullptr);
            if (Partition != nullptr)
            {
                Data->SetStringField(TEXT("world_partition_id"), Partition->GetPathName());
                Data->SetBoolField(TEXT("streaming_enabled"), Partition->IsStreamingEnabled());
                Data->SetBoolField(TEXT("is_main_world_partition"), Partition->IsMainWorldPartition());
            }
        }
        else if (Operation == TEXT("validate_scene"))
        {
            int32 ActorCount = 0;
            int32 InvalidTransformCount = 0;
            for (TActorIterator<AActor> It(World); It; ++It)
            {
                ++ActorCount;
                if (It->GetActorTransform().ContainsNaN()) { ++InvalidTransformCount; }
            }
            TArray<TSharedPtr<FJsonValue>> Gates;
            auto AddGate = [&Gates](const TCHAR* Gate, const TCHAR* Status, const TCHAR* Evidence)
            {
                TSharedRef<FJsonObject> Item = Object();
                Item->SetStringField(TEXT("gate"), Gate);
                Item->SetStringField(TEXT("status"), Status);
                Item->SetStringField(TEXT("evidence"), Evidence);
                Gates.Add(MakeShared<FJsonValueObject>(Item));
            };
            AddGate(TEXT("G0"), ActorCount > 0 ? TEXT("PASS_AUTOMATED") : TEXT("FAIL"), TEXT("active world and loaded actor existence only"));
            AddGate(TEXT("G1"), InvalidTransformCount == 0 ? TEXT("PASS_AUTOMATED") : TEXT("FAIL"), TEXT("finite transforms only; scale/support require visual review"));
            AddGate(TEXT("G2"), TEXT("PENDING_MANUAL_REVIEW"), TEXT("normal gameplay viewpoint evidence required"));
            AddGate(TEXT("G3"), TEXT("PENDING_MANUAL_REVIEW"), TEXT("material/light/weather/audio evidence required"));
            AddGate(TEXT("G4"), TEXT("PENDING_ENGINEERING_EVIDENCE"), TEXT("build, package, performance, LOD and repeatability evidence required"));
            Data->SetArrayField(TEXT("gates"), Gates);
            Data->SetStringField(TEXT("overall"), ActorCount > 0 && InvalidTransformCount == 0 ? TEXT("PENDING_MANUAL_REVIEW") : TEXT("FAIL"));
            Data->SetNumberField(TEXT("loaded_actor_count"), ActorCount);
            Data->SetNumberField(TEXT("invalid_transform_count"), InvalidTransformCount);
        }
        else if (Operation == TEXT("capture_request"))
        {
            FString CameraId;
            Params->TryGetStringField(TEXT("camera_id"), CameraId);
            if (!FixedCameras.Contains(CameraId))
            {
                return Finish(ErrorJson(RequestId, TEXT("CAMERA_NOT_ALLOWED"), TEXT("camera_id must be one of the immutable evidence presets.")), TEXT("CAMERA_NOT_ALLOWED"));
            }
            double Width = 1920.0;
            double Height = 1080.0;
            Params->TryGetNumberField(TEXT("width"), Width);
            Params->TryGetNumberField(TEXT("height"), Height);
            const int32 WidthPixels = FMath::Clamp(FMath::RoundToInt(Width), 320, 4096);
            const int32 HeightPixels = FMath::Clamp(FMath::RoundToInt(Height), 180, 4096);
            Data->SetStringField(TEXT("status"), TEXT("EDITOR_CAPTURE_REQUIRED"));
            Data->SetStringField(TEXT("camera_id"), CameraId);
            Data->SetStringField(TEXT("world_id"), World->GetPathName());
            Data->SetNumberField(TEXT("width"), WidthPixels);
            Data->SetNumberField(TEXT("height"), HeightPixels);
            Data->SetStringField(TEXT("format"), TEXT("png"));
            Data->SetStringField(TEXT("artifact_sha256"), TEXT("PENDING_CAPTURE"));
            Data->SetStringField(TEXT("capture_result"), TEXT("NOT_RUN"));
            Data->SetBoolField(TEXT("writes_file"), false);
            Data->SetStringField(TEXT("instruction"), TEXT("A human-controlled Editor evidence runner must resolve the fixed camera, capture, hash, and attach the artifact."));
        }
    }

    if (IsTimedOut(Deadline))
    {
        return Finish(ErrorJson(RequestId, TEXT("TIMEOUT"), TEXT("Operation exceeded timeout before response serialization.")), TEXT("TIMEOUT"));
    }
    TSharedRef<FJsonObject> Root = Envelope(RequestId, true);
    Root->SetObjectField(TEXT("data"), Data);
    const FString CompletedResponse = Finish(AeroSimEditorMcp::Serialize(Root), TEXT("OK"));
    if (TransactionOperations.Contains(Operation))
    {
        if (IdempotentResponses.Num() >= 512)
        {
            IdempotentRequestDigests.Reset();
            IdempotentResponses.Reset();
        }
        IdempotentRequestDigests.Add(RequestId, RequestDigest);
        IdempotentResponses.Add(RequestId, CompletedResponse);
    }
    return CompletedResponse;
}

TArray<FAeroSimEditorMcpAuditEntry> UAeroSimEditorMcp::InspectAudit(const int32 Offset, const int32 Limit)
{
    using namespace AeroSimEditorMcp;
    FScopeLock Lock(&AuditMutex);
    TArray<FAeroSimEditorMcpAuditEntry> Result;
    const int32 SafeOffset = FMath::Clamp(Offset, 0, AuditEntries.Num());
    const int32 SafeLimit = FMath::Clamp(Limit, 1, FAeroSimEditorMcpLimits::MaximumPageSize);
    for (int32 Index = SafeOffset; Index < AuditEntries.Num() && Result.Num() < SafeLimit; ++Index)
    {
        Result.Add(AuditEntries[Index]);
    }
    return Result;
}

FString UAeroSimEditorMcp::ProtocolSchema()
{
    return AeroSimEditorMcp::RequestSchema;
}

FString UAeroSimEditorMcp::RuntimeContract()
{
    return AeroSimEditorMcp::Runtime;
}
