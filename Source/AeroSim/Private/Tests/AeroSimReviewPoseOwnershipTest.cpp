#if WITH_DEV_AUTOMATION_TESTS
// DEV-TERRAIN investigation tool (R11): which component/material draws each part of a review pose.
// Spawns the coast in a game world, ray-casts (complex, visibility) through a pixel grid of the
// cliff and shore review cameras (same pose table and 60 deg vertical FOV as the bridge capture,
// 1440x900 viewport with a 16:9 active area) and prints an ownership map plus per-owner counts.
// For land hits it also predicts the ground material's rock-projection branch ('#').
// Diagnostic only: it asserts nothing about appearance.
#include "Misc/AutomationTest.h"
#include "AeroSimGameMode.h"
#include "PacificCoastGraybox.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReviewPoseOwnershipTest, "AeroSim.Diagnostics.Terrain.ReviewPoseOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FReviewPoseOwnershipTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Test world"), World)) return false;
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    APacificCoastGraybox* Coast = World->SpawnActor<APacificCoastGraybox>(FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
    if (!TestNotNull(TEXT("Coast"), Coast)) { World->DestroyWorld(false); return false; }
    constexpr int32 Width = 1440, Height = 900, ActiveTop = 45, ActiveHeight = 810;
    const float TanHalf = FMath::Tan(FMath::DegreesToRadians(30.0F));
    for (const TCHAR* View : {TEXT("cliff"), TEXT("shore"), TEXT("north")})
    {
        FVector Location; FRotator Rotation;
        if (!AAeroSimGameMode::ResolveReviewView(View, 8, Location, Rotation)) continue;
        const FVector Forward = Rotation.Vector(), Right = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y), Up = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Z);
        TMap<FString, TCHAR> Letters; TMap<FString, int32> Counts; TMap<FString, FVector> NormalSum, PointSum;
        int32 GroundCells = 0, RockProjected = 0, SteepGround = 0, SteepRockProjected = 0;
        const FString Alphabet = TEXT("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
        AddInfo(FString::Printf(TEXT("OWNERSHIP %s camera %s rot %s; map cell = 30 px, '.' sky, '~' sea"), View, *Location.ToString(), *Rotation.ToString()));
        for (int32 PY = ActiveTop + 15; PY < ActiveTop + ActiveHeight; PY += 30)
        {
            FString Line = FString::Printf(TEXT("MAP %s y%03d "), View, PY);
            for (int32 PX = 15; PX < Width; PX += 30)
            {
                const float NX = (PX - Width * .5F) / (ActiveHeight * .5F) * TanHalf;
                const float NY = -(PY - (ActiveTop + ActiveHeight * .5F)) / (ActiveHeight * .5F) * TanHalf;
                const FVector Direction = (Forward + Right * NX + Up * NY).GetSafeNormal();
                FHitResult Hit;
                FCollisionQueryParams Query(TEXT("Ownership"), true);
                const bool bHit = World->LineTraceSingleByChannel(Hit, Location, Location + Direction * 3000000.0, ECC_Visibility, Query);
                const double SeaT = Direction.Z < 0 ? (APacificCoastGraybox::SeaLevelCm - Location.Z) / Direction.Z : BIG_NUMBER;
                if (!bHit && SeaT >= BIG_NUMBER) { Line += TEXT("."); continue; }
                if (!bHit || SeaT < Hit.Distance) { Line += TEXT("~"); continue; }
                const UPrimitiveComponent* Component = Hit.GetComponent();
                const UMaterialInterface* Material = Component ? Component->GetMaterial(0) : nullptr;
                const FString Owner = FString::Printf(TEXT("%s [%s]"), Component ? *Component->GetName() : TEXT("?"),
                    Material ? *Material->GetMaterial()->GetName() : TEXT("none"));
                if (!Letters.Contains(Owner)) Letters.Add(Owner, Alphabet[FMath::Min(Letters.Num(), Alphabet.Len() - 1)]);
                // R11: predicted ground-material branch for land hits. The complex trace normal is the true
                // facet normal, matching the material's ddx/ddy facet; the habitat rock weight stands in for
                // the stony layer winning the blend. '#' = land drawn with the facet-true rock projection.
                const bool bGround = Material && Material->GetMaterial()->GetName() == TEXT("M_CoastGroundDraft");
                if (bGround)
                {
                    const float FacetZ = FMath::Abs(static_cast<float>(Hit.ImpactNormal.Z));
                    const float Slope = 1.0F - FMath::SmoothStep(.707F, .866F, FacetZ);
                    const float Steep = 1.0F - FMath::SmoothStep(.32F, .55F, FacetZ);
                    const float Stony = FMath::SmoothStep(.45F, .65F, FPacificCoastHabitat::Sample(Hit.ImpactPoint.X, Hit.ImpactPoint.Y).Rock);
                    const float RockMask = FMath::Max(Steep, FMath::Max(Slope, Stony));
                    ++GroundCells; if (RockMask >= .5F) ++RockProjected;
                    if (FacetZ < .8F) { ++SteepGround; if (RockMask >= .5F) ++SteepRockProjected; }
                    if (RockMask >= .5F) { Line += TEXT("#"); ++Counts.FindOrAdd(Owner); NormalSum.FindOrAdd(Owner) += Hit.ImpactNormal; PointSum.FindOrAdd(Owner) += Hit.ImpactPoint; continue; }
                }
                Line += FString::Chr(Letters[Owner]);
                ++Counts.FindOrAdd(Owner);
                NormalSum.FindOrAdd(Owner) += Hit.ImpactNormal; PointSum.FindOrAdd(Owner) += Hit.ImpactPoint;
            }
            AddInfo(Line);
        }
        AddInfo(FString::Printf(TEXT("BRANCH %s ground cells %d, facet-true rock projection %d; ground facets steeper than 37 deg %d, of which rock-projected %d"),
            View, GroundCells, RockProjected, SteepGround, SteepRockProjected));
        for (const auto& Entry : Letters)
            AddInfo(FString::Printf(TEXT("LEGEND %s %c = %s: %d cells, mean normal %s, mean point %s"), View, Entry.Value, *Entry.Key, Counts[Entry.Key],
                *(NormalSum[Entry.Key] / Counts[Entry.Key]).GetSafeNormal().ToString(), *(PointSum[Entry.Key] / Counts[Entry.Key]).ToString()));
    }
    World->DestroyWorld(false);
    return true;
}
#endif
