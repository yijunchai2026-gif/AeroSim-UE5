#pragma once

#include "CoreMinimal.h"

/** Finite Phase 6 command set. No arbitrary property names, reflection, paths, or executable payloads. */
enum class EPhase6ActionType : uint8
{
    SpawnApproved,
    SetTransform,
    SetMaterialScalar,
    AddTag,
    RemoveTag,
    DeleteDraft
};

struct AEROSIMEDITOR_API FPhase6Action final
{
    EPhase6ActionType Type = EPhase6ActionType::SetTransform;
    FString ActorId;
    FString AssetId;
    FTransform Transform = FTransform::Identity;
    FString ParameterId;
    double ScalarValue = 0.0;
    FString Tag;
    bool bSnap = true;
};

struct AEROSIMEDITOR_API FPhase6Patch final
{
    FString Schema;
    int64 BaseRevision = 0;
    FString BaseHash;
    TArray<FPhase6Action> Actions;
};

struct AEROSIMEDITOR_API FPhase6Result final
{
    bool bOk = false;
    FString Code;
    FString Message;
    FString Data;
};

struct AEROSIMEDITOR_API FPhase6InspectResult final
{
    int64 Revision = 0;
    FString DraftHash;
    FString MapId;
    FString DataLayerId;
    int32 ActorCount = 0;
};

/**
 * Editor-only deterministic ScenePatch transaction store.
 *
 * The store models only allowlisted Sandbox draft actors. It intentionally has no filesystem,
 * network, console, shell, Python, reflection, object loading, arbitrary asset, or production
 * promotion surface. The same methods are used by Editor UI integrations and MCP dispatch.
 */
class AEROSIMEDITOR_API FAeroSimPhase6TransactionStore final
{
public:
    static constexpr int32 MaximumActionsPerPatch = 64;
    static constexpr int32 MaximumDraftActors = 256;
    static constexpr int32 MaximumTagsPerActor = 16;
    static constexpr int32 MaximumMaterialParametersPerActor = 8;
    static constexpr int32 TransactionTimeoutMs = 2000;
    static constexpr double IdleExpirySeconds = 30.0;

    FPhase6InspectResult Inspect();
    FPhase6Result DryRun(const FString& TransactionId, const FPhase6Patch& Patch);
    FPhase6Result Snapshot(const FString& TransactionId);
    FPhase6Result Apply(const FString& TransactionId);
    FPhase6Result Validate(const FString& TransactionId);
    FPhase6Result Evidence(const FString& TransactionId, const FString& Moment, const FString& CameraId);
    FPhase6Result Commit(const FString& TransactionId);
    FPhase6Result Rollback(const FString& TransactionId, const FString& Reason);
    FPhase6Result Undo(const FString& TransactionId);
    FPhase6Result Redo(const FString& TransactionId);
    FString InspectAudit() const;

private:
    struct FDraftActor final
    {
        FString AssetId;
        FTransform Transform = FTransform::Identity;
        TMap<FString, double> MaterialScalars;
        TSet<FString> Tags;
    };

    struct FState final
    {
        int64 Revision = 0;
        TMap<FString, FDraftActor> Actors;
    };

    struct FActiveTransaction final
    {
        FString Id;
        FPhase6Patch Patch;
        FState OpeningState;
        FString OpeningHash;
        FString SnapshotManifest;
        FString SnapshotSha256;
        FString BeforeEvidenceHash;
        FString AfterEvidenceHash;
        FString CameraId;
        double StartedSeconds = 0.0;
        double LastActivitySeconds = 0.0;
        bool bDryRun = false;
        bool bSnapshot = false;
        bool bApplied = false;
        bool bValidated = false;
        bool bEvidenceBefore = false;
        bool bEvidenceAfter = false;
    };

    FState State;
    TOptional<FActiveTransaction> Active;
    TArray<FState> UndoStack;
    TArray<FState> RedoStack;
    TArray<FString> Audit;
    FString AuditChainHash = FString::ChrN(64, TEXT('0'));

    static FPhase6Result Ok(const FString& Data = TEXT("{}"));
    static FPhase6Result Error(const TCHAR* Code, const TCHAR* Message);
    static FString HashSha256(const FString& Value);
    static FString CanonicalState(const FState& Value);
    static FString StateHash(const FState& Value);
    static bool IsValidIdentifier(const FString& Value, const FString& Prefix);
    static bool IsApprovedAsset(const FString& AssetId);
    static bool IsApprovedMaterialParameter(const FString& ParameterId);
    static bool IsFixedCamera(const FString& CameraId);
    static FTransform Snapped(const FTransform& Value);
    FPhase6Result Acquire(const FString& TransactionId, const FPhase6Patch& Patch);
    FPhase6Result RequireActive(const FString& TransactionId, FActiveTransaction*& OutTransaction);
    FPhase6Result Simulate(const FPhase6Patch& Patch, FState& InOutState) const;
    void ExpireIfIdle();
    void AppendAudit(const FString& TransactionId, const TCHAR* Operation, const FString& ResultCode, const FString& DetailHash);
    void Release();
};
