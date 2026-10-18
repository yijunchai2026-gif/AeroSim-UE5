#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PacificCoastFlora.h"
#include "PacificCoastGraybox.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureStreamingTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalFloraStreamingTest, "AeroSim.Coastal.Flora.RuntimeMaterialStreamingMetadata",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCoastalFloraStreamingTest::RunTest(const FString& Parameters)
{
    int32 RuntimeSlots = 0;
    const auto& Archetypes = CoastFlora::GetArchetypes();
    if (!TestEqual(TEXT("All thirteen runtime archetypes participate in the regression"), Archetypes.Num(), 13)) return false;
    for (const CoastFlora::FArchetype& Archetype : Archetypes)
    {
        if (!TestNotNull(TEXT("Runtime mesh exists"), Archetype.Mesh)) return false;
        const auto& Materials = Archetype.Mesh->GetStaticMaterials();
        if (!TestEqual(TEXT("Both generated material slots are audited"), Materials.Num(), 2)) return false;
        for (int32 Slot = 0; Slot < Materials.Num(); ++Slot)
        {
            const FMeshUVChannelInfo& Info = Materials[Slot].UVChannelData;
            const FString Label = FString::Printf(TEXT("%s/%s"), *Archetype.Name.ToString(), *Materials[Slot].MaterialSlotName.ToString());
            TestTrue(Label + TEXT(" has streaming metadata before its first query"), Info.bInitialized);
            TestTrue(Label + TEXT(" has actual finite positive cm/UV0 density"), FMath::IsFinite(Info.LocalUVDensities[0]) && Info.LocalUVDensities[0] > UE_SMALL_NUMBER);
            TestFalse(Label + TEXT(" does not use an arbitrary manual override"), Info.bOverrideDensities);
            for (int32 Channel = 1; Channel < MAX_TEXCOORDS; ++Channel)
                TestEqual(Label + TEXT(" does not invent absent UV channels"), Info.LocalUVDensities[Channel], 0.0F);
            // Exercise the exact mesh accessor that raised the cooked R2 ensure.
            if (Info.bInitialized) TestTrue(Label + TEXT(" is returned by the real streaming accessor"), Archetype.Mesh->GetUVChannelData(Slot) == &Info);
            AddInfo(FString::Printf(TEXT("FLORA_UV_SLOT mesh=%s slot=%d initialized=%d uv0_cm_per_uv=%.6f"),
                *Archetype.Name.ToString(), Slot, Info.bInitialized, Info.LocalUVDensities[0]));
            ++RuntimeSlots;
        }
    }
    TestEqual(TEXT("Twenty-six runtime slots are covered"), RuntimeSlots, 26);

    const APacificCoastGraybox* Coast = GetDefault<APacificCoastGraybox>();
    if (!TestNotNull(TEXT("Actual scene CDO exists"), Coast)) return false;
    TArray<UInstancedStaticMeshComponent*> Batches; Coast->GetComponents(Batches);
    int32 VisibleBatches = 0, QueriedSlots = 0;
    for (const UInstancedStaticMeshComponent* Batch : Batches)
    {
        if (!Batch->IsVisible() || Batch->bHiddenInGame || Batch->GetInstanceCount() == 0) continue;
        const UStaticMesh* Mesh = Batch->GetStaticMesh();
        if (!TestNotNull(Batch->GetName() + TEXT(" uses an actual mesh"), Mesh)) continue;
        ++VisibleBatches;
        for (int32 Slot = 0; Slot < Mesh->GetStaticMaterials().Num(); ++Slot)
        {
            if (!Batch->GetMaterial(Slot)) continue;
            const FString Label = Batch->GetName() + TEXT("/") + FString::FromInt(Slot);
            const FMeshUVChannelInfo& Info = Mesh->GetStaticMaterials()[Slot].UVChannelData;
            if (!TestTrue(Label + TEXT(" initializes imported or runtime slot metadata"), Info.bInitialized)) continue;
            FPrimitiveMaterialInfo Streaming;
            TestTrue(Label + TEXT(" real ISM streaming query succeeds"), Batch->GetMaterialStreamingData(Slot, Streaming));
            TestTrue(Label + TEXT(" query returns initialized UV data"), Streaming.UVChannelData && Streaming.UVChannelData->bInitialized);
            if (Streaming.UVChannelData)
                TestTrue(Label + TEXT(" query returns usable finite UV0 density"), FMath::IsFinite(Streaming.UVChannelData->LocalUVDensities[0]) && Streaming.UVChannelData->LocalUVDensities[0] > UE_SMALL_NUMBER);
            ++QueriedSlots;
        }
    }
    TestTrue(TEXT("Regression exercises visible tree and habitat ISM batches"), VisibleBatches > 13 && QueriedSlots >= 26);
    AddInfo(FString::Printf(TEXT("FLORA_UV_STREAMING_AUDIT visible_batches=%d queried_slots=%d"), VisibleBatches, QueriedSlots));
    return true;
}
#endif
