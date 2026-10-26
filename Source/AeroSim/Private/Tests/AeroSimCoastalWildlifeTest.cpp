#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AeroSimGameMode.h"
#include "PacificCoastGraybox.h"
#include "ProceduralMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalWildlifeCompositionTest,
    "AeroSim.Coastal.Wildlife.Composition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoastalWildlifeCompositionTest::RunTest(const FString& Parameters)
{
    const APacificCoastGraybox* Coast = GetDefault<APacificCoastGraybox>();
    if (!TestNotNull(TEXT("Generated coast exists"), Coast)) return false;

    TestEqual(TEXT("Two whales inhabit offshore water"), Coast->GetCoastalWhaleCount(), 2);
    TestEqual(TEXT("Wolf pack count"), Coast->GetCoastalWolfCount(), 3);
    TestEqual(TEXT("One bear inhabits the mountain"), Coast->GetCoastalBearCount(), 1);
    TestEqual(TEXT("Deer herd count"), Coast->GetCoastalDeerCount(), 5);
    TestEqual(TEXT("Cattle herd count"), Coast->GetCoastalCattleCount(), 4);
    TestEqual(TEXT("Every wildlife material batch is independently animated"), Coast->GetCoastalWildlifeAnimatedBatchCount(), 10);
    TestEqual(TEXT("Animation ranges preserve per-animal phase across material layers"), Coast->GetCoastalWildlifeAnimatedRangeCount(), 47);

    const TCHAR* ComponentNames[] = {
        TEXT("CoastalWhales"), TEXT("CoastalWhaleDarkDetails"), TEXT("CoastalWhaleLightDetails"),
        TEXT("CoastalWolves"), TEXT("CoastalBear"),
        TEXT("CoastalDeer"), TEXT("CoastalDeerAntlers"), TEXT("CoastalCattle"),
        TEXT("CoastalWildlifeDarkDetails"), TEXT("CoastalWildlifeLightDetails")};
    int32 TotalTriangles = 0;
    for (const TCHAR* Name : ComponentNames)
    {
        TArray<UProceduralMeshComponent*> Meshes;
        Coast->GetComponents(Meshes);
        const UProceduralMeshComponent* Match = nullptr;
        for (const UProceduralMeshComponent* Mesh : Meshes)
            if (Mesh != nullptr && Mesh->GetFName() == Name) { Match = Mesh; break; }
        if (!TestNotNull(FString::Printf(TEXT("%s wildlife batch exists"), Name), Match)) continue;
        const FProcMeshSection* Section = const_cast<UProceduralMeshComponent*>(Match)->GetProcMeshSection(0);
        if (TestNotNull(FString::Printf(TEXT("%s has geometry"), Name), Section))
        {
            TestTrue(FString::Printf(TEXT("%s has vertices"), Name), !Section->ProcVertexBuffer.IsEmpty());
            TotalTriangles += Section->ProcIndexBuffer.Num() / 3;
        }
        TestEqual(FString::Printf(TEXT("%s never blocks flight"), Name), Match->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
    }
    AddInfo(FString::Printf(TEXT("Wildlife triangle budget: %d / 30000"), TotalTriangles));
    TestTrue(TEXT("Wildlife stays within the lightweight geometry budget"), TotalTriangles > 0 && TotalTriangles < 30000);

    FVector Location;
    FRotator Rotation;
    TestTrue(TEXT("Whale review pose resolves"), AAeroSimGameMode::ResolveReviewView(TEXT("wildlife-whale"), 8, Location, Rotation));
    TestTrue(TEXT("Predator review pose resolves"), AAeroSimGameMode::ResolveReviewView(TEXT("wildlife-predators"), 8, Location, Rotation));
    TestTrue(TEXT("Herd review pose resolves"), AAeroSimGameMode::ResolveReviewView(TEXT("wildlife-herd"), 8, Location, Rotation));
    return true;
}
#endif
