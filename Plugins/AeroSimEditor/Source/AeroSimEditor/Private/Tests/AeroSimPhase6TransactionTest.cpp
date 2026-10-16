#if WITH_DEV_AUTOMATION_TESTS

#include "AeroSimPhase6Transaction.h"
#include "AeroSimSha256.h"
#include "Misc/AutomationTest.h"

namespace
{
FPhase6Action SpawnAction(const FString& Id)
{
    FPhase6Action Action;
    Action.Type = EPhase6ActionType::SpawnApproved;
    Action.ActorId = Id;
    Action.AssetId = TEXT("primitive.cube");
    Action.Transform = FTransform(FRotator::ZeroRotator, FVector(0.0, 0.0, 100.0), FVector::OneVector);
    return Action;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhase6LifecycleTest, "AeroSim.Phase6.Transaction.Lifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPhase6LifecycleTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    TestEqual(TEXT("Portable SHA256 matches the empty-string standard vector"), AeroSimSha256(TEXT("")), FString(TEXT("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")));
    TestEqual(TEXT("Portable SHA256 matches the abc standard vector"), AeroSimSha256(TEXT("abc")), FString(TEXT("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")));
    FAeroSimPhase6TransactionStore Store;
    const FPhase6InspectResult Initial = Store.Inspect();
    TestTrue(TEXT("Initial draft hash is SHA256"), Initial.DraftHash.Len() == 64);

    FPhase6Patch Patch;
    Patch.Schema = TEXT("aerosim.scene-patch/v1");
    Patch.BaseRevision = Initial.Revision;
    Patch.BaseHash = Initial.DraftHash;
    Patch.Actions.Add(SpawnAction(TEXT("draft:cube-1")));

    TestTrue(TEXT("Dry-run succeeds"), Store.DryRun(TEXT("tx-1"), Patch).bOk);
    const FPhase6Result Snapshot = Store.Snapshot(TEXT("tx-1"));
    TestTrue(TEXT("Snapshot creates SHA256 trust anchor"), Snapshot.bOk && Snapshot.Data.Contains(TEXT("snapshot_sha256")));
    TestTrue(TEXT("Apply succeeds"), Store.Apply(TEXT("tx-1")).bOk);
    TestTrue(TEXT("Validate succeeds without skipping G0-G4"), Store.Validate(TEXT("tx-1")).bOk);
    TestTrue(TEXT("Before evidence is fixed-camera and hash-bound"), Store.Evidence(TEXT("tx-1"), TEXT("before"), TEXT("near-follow")).bOk);
    TestTrue(TEXT("After evidence is fixed-camera and hash-bound"), Store.Evidence(TEXT("tx-1"), TEXT("after"), TEXT("near-follow")).bOk);
    const FPhase6Result Commit = Store.Commit(TEXT("tx-1"));
    TestTrue(TEXT("Commit only creates pending-review sandbox"), Commit.bOk && Commit.Data.Contains(TEXT("PENDING_REVIEW_SANDBOX")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhase6RollbackTest, "AeroSim.Phase6.Transaction.RollbackAndConcurrency", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPhase6RollbackTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FAeroSimPhase6TransactionStore Store;
    const FPhase6InspectResult Initial = Store.Inspect();
    FPhase6Patch Patch;
    Patch.Schema = TEXT("aerosim.scene-patch/v1");
    Patch.BaseRevision = Initial.Revision;
    Patch.BaseHash = Initial.DraftHash;
    Patch.Actions.Add(SpawnAction(TEXT("draft:rollback")));

    TestTrue(TEXT("First transaction locks store"), Store.DryRun(TEXT("tx-a"), Patch).bOk);
    TestEqual(TEXT("Concurrent transaction is rejected"), Store.DryRun(TEXT("tx-b"), Patch).Code, FString(TEXT("TRANSACTION_LOCKED")));
    TestTrue(TEXT("Snapshot succeeds"), Store.Snapshot(TEXT("tx-a")).bOk);
    TestTrue(TEXT("Apply succeeds"), Store.Apply(TEXT("tx-a")).bOk);
    TestTrue(TEXT("Rollback succeeds"), Store.Rollback(TEXT("tx-a"), TEXT("automation")).bOk);
    const FPhase6InspectResult Restored = Store.Inspect();
    TestEqual(TEXT("Rollback restores revision"), Restored.Revision, Initial.Revision);
    TestEqual(TEXT("Rollback restores exact draft hash"), Restored.DraftHash, Initial.DraftHash);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhase6PolicyTest, "AeroSim.Phase6.Transaction.FailClosedPolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPhase6PolicyTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FAeroSimPhase6TransactionStore Store;
    const FPhase6InspectResult Initial = Store.Inspect();
    FPhase6Patch Patch;
    Patch.Schema = TEXT("aerosim.scene-patch/v1");
    Patch.BaseRevision = Initial.Revision;
    Patch.BaseHash = Initial.DraftHash;

    FPhase6Action Unsafe = SpawnAction(TEXT("production:actor"));
    Patch.Actions.Add(Unsafe);
    TestEqual(TEXT("Non-draft target is rejected"), Store.DryRun(TEXT("tx-unsafe-1"), Patch).Code, FString(TEXT("TARGET_NOT_ALLOWED")));

    Patch.Actions.Reset();
    Unsafe = SpawnAction(TEXT("draft:bad-asset"));
    Unsafe.AssetId = TEXT("/Game/Arbitrary/Asset");
    Patch.Actions.Add(Unsafe);
    TestEqual(TEXT("Arbitrary asset is rejected"), Store.DryRun(TEXT("tx-unsafe-2"), Patch).Code, FString(TEXT("ASSET_NOT_ALLOWED")));

    Patch.Actions.Reset();
    Patch.Actions.Add(SpawnAction(TEXT("draft:stale")));
    Patch.BaseRevision += 1;
    TestEqual(TEXT("Optimistic revision mismatch is rejected"), Store.DryRun(TEXT("tx-stale"), Patch).Code, FString(TEXT("REVISION_CONFLICT")));
    return true;
}

#endif
