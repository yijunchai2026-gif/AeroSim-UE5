#if WITH_DEV_AUTOMATION_TESTS
#if WITH_EDITOR
#include "Animation/AnimData/IAnimationDataModel.h"
#endif
#include "Animation/AnimSequence.h"
#include "Misc/AutomationTest.h"
#include "AeroSimGameMode.h"
#include "PacificCoastGraybox.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalWildlifeCompositionTest,
    "AeroSim.Coastal.Wildlife.Composition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoastalWildlifeCompositionTest::RunTest(const FString& Parameters)
{
    const APacificCoastGraybox* Coast = GetDefault<APacificCoastGraybox>();
    if (!TestNotNull(TEXT("Generated coast exists"), Coast)) return false;

    TestEqual(TEXT("Two whales inhabit offshore water"), Coast->GetCoastalWhaleCount(), 2);
    TestEqual(TEXT("Wolves are excluded from the accepted habitat"), Coast->GetCoastalWolfCount(), 0);
    TestEqual(TEXT("One bear inhabits the mountain"), Coast->GetCoastalBearCount(), 1);
    TestEqual(TEXT("Deer herd count"), Coast->GetCoastalDeerCount(), 5);
    TestNotNull(TEXT("Deer coat texture exists"), LoadObject<UTexture2D>(nullptr,
        TEXT("/Game/ThirdParty/Wildlife/DeerSkeletal/deer-animation-skeletal/Textures/T_DeerFur_Albedo.T_DeerFur_Albedo")));
    TestEqual(TEXT("Cattle are excluded from the accepted habitat"), Coast->GetCoastalCattleCount(), 0);
    TestEqual(TEXT("One eagle patrols the wildlife habitat"), Coast->GetCoastalEagleCount(), 1);
    TestEqual(TEXT("Procedural wildlife animation is disabled after asset replacement"), Coast->GetCoastalWildlifeAnimatedBatchCount(), 0);
    TestEqual(TEXT("No procedural wildlife ranges remain active"), Coast->GetCoastalWildlifeAnimatedRangeCount(), 0);
    TestEqual(TEXT("All accepted wildlife instances have deterministic autonomous motion"), Coast->GetCoastalWildlifeAutonomousCount(), 9);

    const TCHAR* AnimationPaths[] = {
        TEXT("/Game/ThirdParty/Wildlife/Whale/Forward_Swim_Anim.Forward_Swim_Anim"),
        TEXT("/Game/ThirdParty/Wildlife/Bear/Bear_AnimatedStand_Idle_01.Bear_AnimatedStand_Idle_01"),
        TEXT("/Game/ThirdParty/Wildlife/Bear/Bear_AnimatedWalkSlow.Bear_AnimatedWalkSlow"),
        TEXT("/Game/ThirdParty/Wildlife/Bear/Bear_AnimatedRun.Bear_AnimatedRun"),
        TEXT("/Game/ThirdParty/Wildlife/DeerSkeletal/deer-animation-skeletal/SkeletalMeshes/deer-animation-skeletalIdleDeer.deer-animation-skeletalIdleDeer"),
        TEXT("/Game/ThirdParty/Wildlife/DeerSkeletal/deer-animation-skeletal/SkeletalMeshes/deer-animation-skeletalWalkdeer.deer-animation-skeletalWalkdeer"),
        TEXT("/Game/ThirdParty/Wildlife/DeerSkeletal/deer-animation-skeletal/SkeletalMeshes/deer-animation-skeletalRundeer.deer-animation-skeletalRundeer"),
        TEXT("/Game/ThirdParty/Wildlife/Eagle/animated-eagle/SkeletalMeshes/animated-eagle_Anim.animated-eagle_Anim")};
    for (const TCHAR* Path : AnimationPaths)
    {
        const UAnimSequence* Animation = LoadObject<UAnimSequence>(nullptr, Path);
        if (!TestNotNull(FString::Printf(TEXT("%s animation exists"), Path), Animation)) continue;
#if WITH_EDITOR
        TestTrue(FString::Printf(TEXT("%s contains bone tracks"), Path), Animation->GetDataModel()->GetNumBoneTracks() > 0);
#endif
        TestTrue(FString::Printf(TEXT("%s has a playable duration"), Path), Animation->GetPlayLength() > 0.1F);
    }

    const TCHAR* ComponentNames[] = {
        TEXT("CoastalWhaleAssetA"), TEXT("CoastalWhaleAssetB"), TEXT("CoastalBearAsset"),
        TEXT("CoastalDeerAsset01"), TEXT("CoastalDeerAsset02"), TEXT("CoastalDeerAsset03"),
        TEXT("CoastalDeerAsset04"), TEXT("CoastalDeerAsset05"), TEXT("CoastalEagleAsset")};
    TArray<USkeletalMeshComponent*> Meshes;
    Coast->GetComponents(Meshes);
    TestEqual(TEXT("Exactly nine accepted wildlife instances use skeletal assets"), Meshes.Num(), 9);
    for (const TCHAR* Name : ComponentNames)
    {
        const USkeletalMeshComponent* Match = nullptr;
        for (const USkeletalMeshComponent* Mesh : Meshes)
            if (Mesh != nullptr && Mesh->GetFName() == Name) { Match = Mesh; break; }
        if (!TestNotNull(FString::Printf(TEXT("%s skeletal wildlife exists"), Name), Match)) continue;
        TestNotNull(FString::Printf(TEXT("%s has a production mesh"), Name), Match->GetSkeletalMeshAsset());
        TestEqual(FString::Printf(TEXT("%s uses single-node animation"), Name), Match->GetAnimationMode(), EAnimationMode::AnimationSingleNode);
        TestEqual(FString::Printf(TEXT("%s never blocks flight"), Name), Match->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
        if (Match->GetName().StartsWith(TEXT("CoastalDeerAsset")))
        {
            const FVector DeerLocation = Match->GetRelativeLocation();
            const double TerrainHeight = APacificCoastGraybox::GetCoastalLandHeight(DeerLocation.X, DeerLocation.Y);
            TestTrue(FString::Printf(TEXT("%s hooves remain above the terrain surface"), Name), DeerLocation.Z >= TerrainHeight);
            const UMaterialInterface* DeerMaterial = Match->GetMaterial(0);
            TestTrue(FString::Printf(TEXT("%s keeps its imported coat material"), Name),
                DeerMaterial != nullptr && DeerMaterial->GetPathName().Contains(TEXT("M_DeerCoat")));
        }
    }

    UWorld* WildlifeWorld = UWorld::CreateWorld(EWorldType::Game, false);
    if (TestNotNull(TEXT("Wildlife grounding test world"), WildlifeWorld))
    {
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        APacificCoastGraybox* LiveCoast = WildlifeWorld->SpawnActor<APacificCoastGraybox>(
            FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
        if (TestNotNull(TEXT("Live wildlife coast"), LiveCoast))
        {
            TInlineComponentArray<USkeletalMeshComponent*> LiveComponents(LiveCoast);
            constexpr float TickSeconds = 1.0F / 30.0F;
            for (int32 Frame = 0; Frame < 90; ++Frame)
            {
                for (USkeletalMeshComponent* Component : LiveComponents)
                {
                    Component->TickAnimation(TickSeconds, false);
                    Component->RefreshBoneTransforms();
                }
                LiveCoast->Tick(TickSeconds);
            }
            static const FName DeerFootBones[] = {
                TEXT("tyriese_miller_c_deer_idle_toetip_back_l_011"),
                TEXT("tyriese_miller_c_deer_idle_toetip_back_r_018"),
                TEXT("tyriese_miller_c_deer_idle_toetip_front_l_042"),
                TEXT("tyriese_miller_c_deer_idle_toetip_front_r_048")};
            for (USkeletalMeshComponent* Component : LiveComponents)
            {
                if (!Component->GetName().StartsWith(TEXT("CoastalDeerAsset")))
                {
                    continue;
                }
                float MinimumClearance = TNumericLimits<float>::Max();
                for (const FName FootBone : DeerFootBones)
                {
                    const FVector FootLocation = Component->GetBoneLocation(FootBone, EBoneSpaces::WorldSpace);
                    const float TerrainHeight = APacificCoastGraybox::GetCoastalLandHeight(FootLocation.X, FootLocation.Y);
                    MinimumClearance = FMath::Min(MinimumClearance, FootLocation.Z - TerrainHeight);
                }
                TestTrue(FString::Printf(TEXT("%s animated hooves do not penetrate terrain"), *Component->GetName()),
                    MinimumClearance >= -1.0F);
                TestTrue(FString::Printf(TEXT("%s animated stance keeps a hoof on terrain"), *Component->GetName()),
                    MinimumClearance <= 4.0F);
            }
        }
        WildlifeWorld->DestroyWorld(false);
    }

    FVector Location;
    FRotator Rotation;
    TestTrue(TEXT("Whale review pose resolves"), AAeroSimGameMode::ResolveReviewView(TEXT("wildlife-whale"), 8, Location, Rotation));
    TestTrue(TEXT("Predator review pose resolves"), AAeroSimGameMode::ResolveReviewView(TEXT("wildlife-predators"), 8, Location, Rotation));
    TestTrue(TEXT("Herd review pose resolves"), AAeroSimGameMode::ResolveReviewView(TEXT("wildlife-herd"), 8, Location, Rotation));
    TestTrue(TEXT("Eagle review pose resolves"), AAeroSimGameMode::ResolveReviewView(TEXT("wildlife-eagle"), 8, Location, Rotation));
    return true;
}
#endif
