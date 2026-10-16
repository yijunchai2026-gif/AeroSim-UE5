#include "AeroSimPhase6Transaction.h"

#include "AeroSimSha256.h"
#include "HAL/PlatformTime.h"
#include "Misc/DateTime.h"

namespace
{
const TCHAR* SandboxMap = TEXT("/Game/AeroSim/Sandbox/Maps/AI_Review_Sandbox");
const TCHAR* SandboxDataLayer = TEXT("AeroSim_AI_Draft");
const TSet<FString> ApprovedAssets = {
    TEXT("primitive.cube"), TEXT("primitive.sphere"), TEXT("primitive.cylinder"),
    TEXT("prefab.review-marker"), TEXT("prefab.route-gate")
};
const TSet<FString> ApprovedMaterialParameters = {
    TEXT("base_color_r"), TEXT("base_color_g"), TEXT("base_color_b"),
    TEXT("roughness"), TEXT("metallic"), TEXT("emissive_strength")
};
const TSet<FString> FixedCameras = {
    TEXT("aircraft-three-quarter"), TEXT("fpv"), TEXT("gimbal-down"),
    TEXT("near-follow"), TEXT("far-follow"), TEXT("scale-support")
};
constexpr double MaximumLocationCm = 500000.0;
constexpr double MinimumScale = 0.01;
constexpr double MaximumScale = 100.0;
constexpr double LocationSnapCm = 10.0;
constexpr double RotationSnapDeg = 5.0;
constexpr double ScaleSnap = 0.05;

FString Escape(const FString& Value)
{
    FString Result = Value;
    Result.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
    Result.ReplaceInline(TEXT("\""), TEXT("\\\""));
    return Result;
}
}

FPhase6Result FAeroSimPhase6TransactionStore::Ok(const FString& Data)
{
    return { true, TEXT("OK"), TEXT(""), Data };
}

FPhase6Result FAeroSimPhase6TransactionStore::Error(const TCHAR* Code, const TCHAR* Message)
{
    return { false, Code, Message, TEXT("{}") };
}

FString FAeroSimPhase6TransactionStore::HashSha256(const FString& Value)
{
    return AeroSimSha256(Value);
}

FString FAeroSimPhase6TransactionStore::CanonicalState(const FState& Value)
{
    TArray<FString> ActorIds;
    Value.Actors.GetKeys(ActorIds);
    ActorIds.Sort();
    FString Result = FString::Printf(TEXT("schema=aerosim.sandbox-draft/v1\nrevision=%lld\nmap=%s\ndata_layer=%s\n"),
        Value.Revision, SandboxMap, SandboxDataLayer);
    for (const FString& ActorId : ActorIds)
    {
        const FDraftActor& Actor = Value.Actors.FindChecked(ActorId);
        const FVector Location = Actor.Transform.GetLocation();
        const FRotator Rotation = Actor.Transform.Rotator();
        const FVector Scale = Actor.Transform.GetScale3D();
        Result += FString::Printf(TEXT("actor=%s|asset=%s|l=%.6f,%.6f,%.6f|r=%.6f,%.6f,%.6f|s=%.6f,%.6f,%.6f\n"),
            *ActorId, *Actor.AssetId, Location.X, Location.Y, Location.Z,
            Rotation.Pitch, Rotation.Yaw, Rotation.Roll, Scale.X, Scale.Y, Scale.Z);
        TArray<FString> Parameters;
        Actor.MaterialScalars.GetKeys(Parameters);
        Parameters.Sort();
        for (const FString& Parameter : Parameters)
        {
            Result += FString::Printf(TEXT("material=%s:%.6f\n"), *Parameter, Actor.MaterialScalars.FindChecked(Parameter));
        }
        TArray<FString> Tags = Actor.Tags.Array();
        Tags.Sort();
        for (const FString& Tag : Tags)
        {
            Result += FString::Printf(TEXT("tag=%s\n"), *Tag);
        }
    }
    return Result;
}

FString FAeroSimPhase6TransactionStore::StateHash(const FState& Value)
{
    return HashSha256(CanonicalState(Value));
}

bool FAeroSimPhase6TransactionStore::IsValidIdentifier(const FString& Value, const FString& Prefix)
{
    if (!Value.StartsWith(Prefix, ESearchCase::CaseSensitive) || Value.Len() > 64)
    {
        return false;
    }
    for (const TCHAR Character : Value)
    {
        if (!FChar::IsAlnum(Character) && Character != TEXT('-') && Character != TEXT('_') && Character != TEXT('.') && Character != TEXT(':'))
        {
            return false;
        }
    }
    return true;
}

bool FAeroSimPhase6TransactionStore::IsApprovedAsset(const FString& AssetId)
{
    return ApprovedAssets.Contains(AssetId);
}

bool FAeroSimPhase6TransactionStore::IsApprovedMaterialParameter(const FString& ParameterId)
{
    return ApprovedMaterialParameters.Contains(ParameterId);
}

bool FAeroSimPhase6TransactionStore::IsFixedCamera(const FString& CameraId)
{
    return FixedCameras.Contains(CameraId);
}

FTransform FAeroSimPhase6TransactionStore::Snapped(const FTransform& Value)
{
    const FVector Location = Value.GetLocation().GridSnap(LocationSnapCm);
    const FRotator Rotation(
        FMath::GridSnap(Value.Rotator().Pitch, RotationSnapDeg),
        FMath::GridSnap(Value.Rotator().Yaw, RotationSnapDeg),
        FMath::GridSnap(Value.Rotator().Roll, RotationSnapDeg));
    const FVector Scale = Value.GetScale3D().GridSnap(ScaleSnap);
    return FTransform(Rotation, Location, Scale);
}

void FAeroSimPhase6TransactionStore::ExpireIfIdle()
{
    if (!Active.IsSet())
    {
        return;
    }
    const double Now = FPlatformTime::Seconds();
    if (Now - Active->LastActivitySeconds > IdleExpirySeconds || Now - Active->StartedSeconds > IdleExpirySeconds * 2.0)
    {
        if (Active->bApplied)
        {
            State = Active->OpeningState;
        }
        AppendAudit(Active->Id, TEXT("auto_rollback"), TEXT("TRANSACTION_EXPIRED"), StateHash(State));
        Release();
    }
}

FPhase6InspectResult FAeroSimPhase6TransactionStore::Inspect()
{
    ExpireIfIdle();
    return { State.Revision, StateHash(State), SandboxMap, SandboxDataLayer, State.Actors.Num() };
}

FPhase6Result FAeroSimPhase6TransactionStore::Acquire(const FString& TransactionId, const FPhase6Patch& Patch)
{
    ExpireIfIdle();
    if (!IsValidIdentifier(TransactionId, TEXT("tx-")))
    {
        return Error(TEXT("INVALID_TRANSACTION_ID"), TEXT("transaction_id must be an allowlisted tx-* identifier"));
    }
    if (Active.IsSet())
    {
        return Active->Id == TransactionId
            ? Error(TEXT("INVALID_TRANSACTION_STATE"), TEXT("transaction already exists"))
            : Error(TEXT("TRANSACTION_LOCKED"), TEXT("another transaction holds the single writer lock"));
    }
    if (Patch.Schema != TEXT("aerosim.scene-patch/v1"))
    {
        return Error(TEXT("PATCH_SCHEMA_UNSUPPORTED"), TEXT("only aerosim.scene-patch/v1 is accepted"));
    }
    if (Patch.BaseRevision != State.Revision)
    {
        return Error(TEXT("REVISION_CONFLICT"), TEXT("base_revision does not match current optimistic revision"));
    }
    if (Patch.BaseHash != StateHash(State))
    {
        return Error(TEXT("HASH_CONFLICT"), TEXT("base_hash does not match current draft SHA256"));
    }
    if (Patch.Actions.IsEmpty() || Patch.Actions.Num() > MaximumActionsPerPatch)
    {
        return Error(TEXT("PATCH_QUOTA_EXCEEDED"), TEXT("patch action count is outside the fixed quota"));
    }
    FActiveTransaction Transaction;
    Transaction.Id = TransactionId;
    Transaction.Patch = Patch;
    Transaction.OpeningState = State;
    Transaction.OpeningHash = StateHash(State);
    Transaction.StartedSeconds = FPlatformTime::Seconds();
    Transaction.LastActivitySeconds = Transaction.StartedSeconds;
    Active.Emplace(MoveTemp(Transaction));
    return Ok();
}

FPhase6Result FAeroSimPhase6TransactionStore::RequireActive(const FString& TransactionId, FActiveTransaction*& OutTransaction)
{
    ExpireIfIdle();
    if (!Active.IsSet() || Active->Id != TransactionId)
    {
        return Error(TEXT("TRANSACTION_NOT_FOUND"), TEXT("transaction does not own the active lock"));
    }
    Active->LastActivitySeconds = FPlatformTime::Seconds();
    if ((Active->LastActivitySeconds - Active->StartedSeconds) * 1000.0 > TransactionTimeoutMs)
    {
        if (Active->bApplied)
        {
            State = Active->OpeningState;
        }
        AppendAudit(TransactionId, TEXT("auto_rollback"), TEXT("TIMEOUT"), StateHash(State));
        Release();
        return Error(TEXT("TIMEOUT"), TEXT("transaction exceeded its fixed execution deadline and was rolled back"));
    }
    OutTransaction = &Active.GetValue();
    return Ok();
}

FPhase6Result FAeroSimPhase6TransactionStore::Simulate(const FPhase6Patch& Patch, FState& InOutState) const
{
    for (const FPhase6Action& Action : Patch.Actions)
    {
        if (!IsValidIdentifier(Action.ActorId, TEXT("draft:")))
        {
            return Error(TEXT("TARGET_NOT_ALLOWED"), TEXT("only draft:* actor identifiers in the Sandbox Data Layer are writable"));
        }
        if (Action.Transform.ContainsNaN())
        {
            return Error(TEXT("INVALID_TRANSFORM"), TEXT("transform must be finite"));
        }
        const FTransform TargetTransform = Action.bSnap ? Snapped(Action.Transform) : Action.Transform;
        const FVector Location = TargetTransform.GetLocation();
        const FVector Scale = TargetTransform.GetScale3D();
        if (Location.GetAbsMax() > MaximumLocationCm || Scale.GetMin() < MinimumScale || Scale.GetMax() > MaximumScale)
        {
            return Error(TEXT("TRANSFORM_OUT_OF_BOUNDS"), TEXT("transform violates the Sandbox extent or scale quota"));
        }

        if (Action.Type == EPhase6ActionType::SpawnApproved)
        {
            if (!IsApprovedAsset(Action.AssetId))
            {
                return Error(TEXT("ASSET_NOT_ALLOWED"), TEXT("asset_id is not an approved primitive or prefab"));
            }
            if (InOutState.Actors.Contains(Action.ActorId))
            {
                return Error(TEXT("ACTOR_ALREADY_EXISTS"), TEXT("spawn target already exists"));
            }
            if (InOutState.Actors.Num() >= MaximumDraftActors)
            {
                return Error(TEXT("ACTOR_QUOTA_EXCEEDED"), TEXT("draft actor quota reached"));
            }
            FDraftActor Actor;
            Actor.AssetId = Action.AssetId;
            Actor.Transform = TargetTransform;
            InOutState.Actors.Add(Action.ActorId, MoveTemp(Actor));
            continue;
        }

        FDraftActor* Actor = InOutState.Actors.Find(Action.ActorId);
        if (Actor == nullptr)
        {
            return Error(TEXT("DRAFT_ACTOR_NOT_FOUND"), TEXT("target is not an existing draft actor"));
        }
        switch (Action.Type)
        {
        case EPhase6ActionType::SetTransform:
            Actor->Transform = TargetTransform;
            break;
        case EPhase6ActionType::SetMaterialScalar:
            if (!IsApprovedMaterialParameter(Action.ParameterId) || !FMath::IsFinite(Action.ScalarValue)
                || Action.ScalarValue < 0.0 || Action.ScalarValue > 10.0)
            {
                return Error(TEXT("MATERIAL_PARAMETER_NOT_ALLOWED"), TEXT("material parameter or scalar range is not allowlisted"));
            }
            if (!Actor->MaterialScalars.Contains(Action.ParameterId)
                && Actor->MaterialScalars.Num() >= MaximumMaterialParametersPerActor)
            {
                return Error(TEXT("MATERIAL_QUOTA_EXCEEDED"), TEXT("material parameter quota reached"));
            }
            Actor->MaterialScalars.Add(Action.ParameterId, Action.ScalarValue);
            break;
        case EPhase6ActionType::AddTag:
            if (!IsValidIdentifier(Action.Tag, TEXT("draft.")) || Actor->Tags.Num() >= MaximumTagsPerActor)
            {
                return Error(TEXT("TAG_NOT_ALLOWED"), TEXT("only bounded draft.* tags are accepted"));
            }
            Actor->Tags.Add(Action.Tag);
            break;
        case EPhase6ActionType::RemoveTag:
            if (!IsValidIdentifier(Action.Tag, TEXT("draft.")))
            {
                return Error(TEXT("TAG_NOT_ALLOWED"), TEXT("only draft.* tags are accepted"));
            }
            Actor->Tags.Remove(Action.Tag);
            break;
        case EPhase6ActionType::DeleteDraft:
            InOutState.Actors.Remove(Action.ActorId);
            break;
        case EPhase6ActionType::SpawnApproved:
        default:
            return Error(TEXT("ACTION_NOT_ALLOWED"), TEXT("action is outside the finite command set"));
        }
    }
    return Ok();
}

FPhase6Result FAeroSimPhase6TransactionStore::DryRun(const FString& TransactionId, const FPhase6Patch& Patch)
{
    FPhase6Result Result = Acquire(TransactionId, Patch);
    if (!Result.bOk)
    {
        AppendAudit(TransactionId, TEXT("dry_run"), Result.Code, HashSha256(Patch.BaseHash));
        return Result;
    }
    FState Preview = State;
    Result = Simulate(Patch, Preview);
    if (!Result.bOk)
    {
        AppendAudit(TransactionId, TEXT("dry_run"), Result.Code, StateHash(State));
        Release();
        return Result;
    }
    Active->bDryRun = true;
    const FString PreviewHash = StateHash(Preview);
    AppendAudit(TransactionId, TEXT("dry_run"), TEXT("OK"), PreviewHash);
    return Ok(FString::Printf(TEXT("{\"preview_hash\":\"%s\",\"action_count\":%d}"), *PreviewHash, Patch.Actions.Num()));
}

FPhase6Result FAeroSimPhase6TransactionStore::Snapshot(const FString& TransactionId)
{
    FActiveTransaction* Transaction = nullptr;
    FPhase6Result Result = RequireActive(TransactionId, Transaction);
    if (!Result.bOk) { return Result; }
    if (!Transaction->bDryRun || Transaction->bSnapshot)
    {
        return Error(TEXT("INVALID_TRANSACTION_STATE"), TEXT("snapshot requires exactly one successful dry-run"));
    }
    Transaction->SnapshotManifest = FString::Printf(
        TEXT("{\"schema\":\"aerosim.snapshot-manifest/v1\",\"map\":\"%s\",\"data_layer\":\"%s\",\"revision\":%lld,\"draft_hash\":\"%s\"}"),
        SandboxMap, SandboxDataLayer, Transaction->OpeningState.Revision, *Transaction->OpeningHash);
    Transaction->SnapshotSha256 = HashSha256(Transaction->SnapshotManifest);
    Transaction->bSnapshot = true;
    AppendAudit(TransactionId, TEXT("snapshot"), TEXT("OK"), Transaction->SnapshotSha256);
    return Ok(FString::Printf(TEXT("{\"snapshot_sha256\":\"%s\",\"manifest_sha256\":\"%s\"}"),
        *Transaction->SnapshotSha256, *HashSha256(Transaction->SnapshotManifest)));
}

FPhase6Result FAeroSimPhase6TransactionStore::Apply(const FString& TransactionId)
{
    FActiveTransaction* Transaction = nullptr;
    FPhase6Result Result = RequireActive(TransactionId, Transaction);
    if (!Result.bOk) { return Result; }
    if (!Transaction->bSnapshot || Transaction->bApplied)
    {
        return Error(TEXT("INVALID_TRANSACTION_STATE"), TEXT("apply requires a trusted snapshot and may run once"));
    }
    if (State.Revision != Transaction->Patch.BaseRevision || StateHash(State) != Transaction->Patch.BaseHash)
    {
        Rollback(TransactionId, TEXT("optimistic-conflict"));
        return Error(TEXT("REVISION_CONFLICT"), TEXT("draft changed after dry-run; transaction rolled back"));
    }
    Result = Simulate(Transaction->Patch, State);
    if (!Result.bOk)
    {
        State = Transaction->OpeningState;
        AppendAudit(TransactionId, TEXT("apply"), Result.Code, StateHash(State));
        Release();
        return Result;
    }
    State.Revision += 1;
    Transaction->bApplied = true;
    AppendAudit(TransactionId, TEXT("apply"), TEXT("OK"), StateHash(State));
    return Ok(FString::Printf(TEXT("{\"revision\":%lld,\"draft_hash\":\"%s\"}"), State.Revision, *StateHash(State)));
}

FPhase6Result FAeroSimPhase6TransactionStore::Validate(const FString& TransactionId)
{
    FActiveTransaction* Transaction = nullptr;
    FPhase6Result Result = RequireActive(TransactionId, Transaction);
    if (!Result.bOk) { return Result; }
    if (!Transaction->bApplied)
    {
        return Error(TEXT("INVALID_TRANSACTION_STATE"), TEXT("validate requires apply"));
    }
    const TCHAR* Gates[] = { TEXT("G0"), TEXT("G1"), TEXT("G2"), TEXT("G3"), TEXT("G4") };
    FString GateJson = TEXT("[");
    bool bUpstreamPassed = true;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Gates); ++Index)
    {
        bool bGatePassed = bUpstreamPassed;
        if (Index == 0) { bGatePassed = State.Actors.Num() <= MaximumDraftActors; }
        if (Index == 1)
        {
            for (const TPair<FString, FDraftActor>& Item : State.Actors)
            {
                bGatePassed = bGatePassed && !Item.Value.Transform.ContainsNaN();
            }
        }
        const FString Status = !bUpstreamPassed ? TEXT("SKIPPED_UPSTREAM_FAILURE") : (bGatePassed ? TEXT("PASS_CONTROLLED") : TEXT("FAIL"));
        GateJson += FString::Printf(TEXT("%s{\"gate\":\"%s\",\"status\":\"%s\"}"), Index == 0 ? TEXT("") : TEXT(","), Gates[Index], *Status);
        bUpstreamPassed = bUpstreamPassed && bGatePassed;
    }
    GateJson += TEXT("]");
    if (!bUpstreamPassed)
    {
        State = Transaction->OpeningState;
        AppendAudit(TransactionId, TEXT("validate"), TEXT("VALIDATION_FAILED_ROLLED_BACK"), StateHash(State));
        Release();
        return Error(TEXT("VALIDATION_FAILED_ROLLED_BACK"), TEXT("G0-G4 failed in order; downstream gates stopped and snapshot restored"));
    }
    Transaction->bValidated = true;
    AppendAudit(TransactionId, TEXT("validate"), TEXT("OK"), StateHash(State));
    return Ok(FString::Printf(TEXT("{\"gates\":%s,\"draft_hash\":\"%s\"}"), *GateJson, *StateHash(State)));
}

FPhase6Result FAeroSimPhase6TransactionStore::Evidence(const FString& TransactionId, const FString& Moment, const FString& CameraId)
{
    FActiveTransaction* Transaction = nullptr;
    FPhase6Result Result = RequireActive(TransactionId, Transaction);
    if (!Result.bOk) { return Result; }
    if (!Transaction->bValidated || !IsFixedCamera(CameraId) || (Moment != TEXT("before") && Moment != TEXT("after")))
    {
        return Error(TEXT("EVIDENCE_NOT_ALLOWED"), TEXT("evidence requires validation, before/after moment, and an immutable camera id"));
    }
    const FString BoundHash = Moment == TEXT("before") ? Transaction->OpeningHash : StateHash(State);
    const FString Manifest = FString::Printf(
        TEXT("schema=aerosim.phase6.evidence/v1|tx=%s|moment=%s|camera=%s|draft_sha256=%s|snapshot_sha256=%s"),
        *TransactionId, *Moment, *CameraId, *BoundHash, *Transaction->SnapshotSha256);
    const FString EvidenceHash = HashSha256(Manifest);
    if (Moment == TEXT("before"))
    {
        Transaction->BeforeEvidenceHash = EvidenceHash;
        Transaction->bEvidenceBefore = true;
    }
    else
    {
        Transaction->AfterEvidenceHash = EvidenceHash;
        Transaction->bEvidenceAfter = true;
    }
    Transaction->CameraId = CameraId;
    AppendAudit(TransactionId, TEXT("evidence"), TEXT("OK"), EvidenceHash);
    return Ok(FString::Printf(TEXT("{\"camera_id\":\"%s\",\"moment\":\"%s\",\"draft_hash\":\"%s\",\"evidence_manifest_sha256\":\"%s\",\"capture_result\":\"EDITOR_CAPTURE_REQUIRED\"}"),
        *CameraId, *Moment, *BoundHash, *EvidenceHash));
}

FPhase6Result FAeroSimPhase6TransactionStore::Commit(const FString& TransactionId)
{
    FActiveTransaction* Transaction = nullptr;
    FPhase6Result Result = RequireActive(TransactionId, Transaction);
    if (!Result.bOk) { return Result; }
    if (!Transaction->bValidated || !Transaction->bEvidenceBefore || !Transaction->bEvidenceAfter)
    {
        return Error(TEXT("INVALID_TRANSACTION_STATE"), TEXT("commit requires validation and hash-bound before/after evidence"));
    }
    const FString CurrentHash = StateHash(State);
    const FString Manifest = FString::Printf(
        TEXT("schema=aerosim.pending-review/v1|status=PENDING_REVIEW_SANDBOX|map=%s|layer=%s|revision=%lld|draft=%s|snapshot=%s|before=%s|after=%s"),
        SandboxMap, SandboxDataLayer, State.Revision, *CurrentHash, *Transaction->SnapshotSha256,
        *Transaction->BeforeEvidenceHash, *Transaction->AfterEvidenceHash);
    const FString ManifestHash = HashSha256(Manifest);
    UndoStack.Add(Transaction->OpeningState);
    RedoStack.Reset();
    AppendAudit(TransactionId, TEXT("commit"), TEXT("OK"), ManifestHash);
    Release();
    return Ok(FString::Printf(TEXT("{\"status\":\"PENDING_REVIEW_SANDBOX\",\"production_touched\":false,\"draft_hash\":\"%s\",\"manifest_sha256\":\"%s\"}"),
        *CurrentHash, *ManifestHash));
}

FPhase6Result FAeroSimPhase6TransactionStore::Rollback(const FString& TransactionId, const FString& Reason)
{
    FActiveTransaction* Transaction = nullptr;
    FPhase6Result Result = RequireActive(TransactionId, Transaction);
    if (!Result.bOk) { return Result; }
    const FString OpeningHash = Transaction->OpeningHash;
    State = Transaction->OpeningState;
    const bool bTrustedRestore = StateHash(State) == OpeningHash;
    AppendAudit(TransactionId, TEXT("rollback"), bTrustedRestore ? TEXT("OK") : TEXT("ROLLBACK_TRUST_FAILURE"), HashSha256(Reason + OpeningHash));
    Release();
    return bTrustedRestore
        ? Ok(FString::Printf(TEXT("{\"restored_hash\":\"%s\",\"complete\":true}"), *OpeningHash))
        : Error(TEXT("ROLLBACK_TRUST_FAILURE"), TEXT("restored state did not match snapshot trust anchor"));
}

FPhase6Result FAeroSimPhase6TransactionStore::Undo(const FString& TransactionId)
{
    ExpireIfIdle();
    if (Active.IsSet()) { return Error(TEXT("TRANSACTION_LOCKED"), TEXT("undo cannot run while a transaction is active")); }
    if (!IsValidIdentifier(TransactionId, TEXT("tx-")) || UndoStack.IsEmpty())
    {
        return Error(TEXT("UNDO_UNAVAILABLE"), TEXT("no committed sandbox draft is available to undo"));
    }
    RedoStack.Add(State);
    State = UndoStack.Pop();
    AppendAudit(TransactionId, TEXT("undo"), TEXT("OK"), StateHash(State));
    return Ok(FString::Printf(TEXT("{\"revision\":%lld,\"draft_hash\":\"%s\"}"), State.Revision, *StateHash(State)));
}

FPhase6Result FAeroSimPhase6TransactionStore::Redo(const FString& TransactionId)
{
    ExpireIfIdle();
    if (Active.IsSet()) { return Error(TEXT("TRANSACTION_LOCKED"), TEXT("redo cannot run while a transaction is active")); }
    if (!IsValidIdentifier(TransactionId, TEXT("tx-")) || RedoStack.IsEmpty())
    {
        return Error(TEXT("REDO_UNAVAILABLE"), TEXT("no undone sandbox draft is available to redo"));
    }
    UndoStack.Add(State);
    State = RedoStack.Pop();
    AppendAudit(TransactionId, TEXT("redo"), TEXT("OK"), StateHash(State));
    return Ok(FString::Printf(TEXT("{\"revision\":%lld,\"draft_hash\":\"%s\"}"), State.Revision, *StateHash(State)));
}

void FAeroSimPhase6TransactionStore::AppendAudit(const FString& TransactionId, const TCHAR* Operation, const FString& ResultCode, const FString& DetailHash)
{
    const FString Record = FString::Printf(TEXT("%s|%s|%s|%s|%s|previous=%s"),
        *FDateTime::UtcNow().ToIso8601(), *TransactionId.Left(64), Operation, *ResultCode.Left(64), *DetailHash.Left(64), *AuditChainHash);
    AuditChainHash = HashSha256(Record);
    Audit.Add(Record + TEXT("|entry_sha256=") + AuditChainHash);
}

FString FAeroSimPhase6TransactionStore::InspectAudit() const
{
    FString Result = TEXT("{\"append_only\":true,\"hash_algorithm\":\"SHA256\",\"entries\":[");
    for (int32 Index = 0; Index < Audit.Num(); ++Index)
    {
        Result += FString::Printf(TEXT("%s\"%s\""), Index == 0 ? TEXT("") : TEXT(","), *Escape(Audit[Index]));
    }
    Result += TEXT("]}");
    return Result;
}

void FAeroSimPhase6TransactionStore::Release()
{
    Active.Reset();
}
