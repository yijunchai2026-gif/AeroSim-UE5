#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PacificCoastGraybox.h"
#include "FlightPawn.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalLaunchSiteTest,"AeroSim.Environment.LaunchSite.GroundedStartAndCleanScenery",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoastalLaunchSiteTest::RunTest(const FString& Parameters)
{
    const auto* Coast=GetDefault<APacificCoastGraybox>();
    TArray<UTextRenderComponent*> Labels;Coast->GetComponents(Labels);
    for(const auto* Label:Labels)
        TestTrue(TEXT("Normal demo has no airborne diagnostic text"),Label->bHiddenInGame&&!Label->IsVisible()&&!Label->CastShadow);
    TArray<UStaticMeshComponent*> Parts;Coast->GetComponents(Parts);
    for(const auto* Part:Parts)
        if(Part->ComponentHasTag(TEXT("CoastalCourseDiagnostic")))
            TestTrue(TEXT("Hidden course markers never leave invisible collisions"),Part->bHiddenInGame&&!Part->IsVisible()&&Part->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
    TestTrue(TEXT("Former giant platform is removed"),Parts.FindByPredicate([](const auto* P){return P->GetFName()==TEXT("TakeoffPad");})==nullptr);
    TestTrue(TEXT("Former high curb is removed"),Parts.FindByPredicate([](const auto* P){return P->GetFName()==TEXT("PadNorthCurb");})==nullptr);
    TArray<UProceduralMeshComponent*> Surfaces;Coast->GetComponents(Surfaces);int32 GravelSurfaces=0;
    for(auto* Mesh:Surfaces)
    {
        if(Mesh->GetFName()!=TEXT("CoastalLaunchApron")&&Mesh->GetFName()!=TEXT("CoastalLaunchAccessTrail")&&Mesh->GetFName()!=TEXT("CoastalForestTrail"))continue;
        ++GravelSurfaces;const auto* Material=Mesh->GetMaterial(0);UTexture* Gravel=nullptr;UTexture* Land=nullptr;
        TestTrue(TEXT("Actual ground surface uses imported fine gravel"),Material&&Material->GetTextureParameterValue(FMaterialParameterInfo(TEXT("BaseColorTexture")),Gravel)&&Gravel&&Gravel->GetFName()==TEXT("T_CoastGravel_BaseColor"));
        TestTrue(TEXT("Actual shoulder retains original ground texture for blending"),Material&&Material->GetTextureParameterValue(FMaterialParameterInfo(TEXT("LandBaseColorTexture")),Land)&&Land&&Land->GetFName()==TEXT("T_CoastGround_BaseColor"));
        const auto* Section=Mesh->GetProcMeshSection(0);bool Edge=false,Core=false;
        if(Section)for(const auto& Vertex:Section->ProcVertexBuffer){Edge|=Vertex.Color.A==0;Core|=Vertex.Color.A>240;}
        TestTrue(TEXT("Ground geometry contains both natural shoulder and worn center"),Edge&&Core);
    }
    TestEqual(TEXT("Only launch apron and two trail surfaces receive fine material"),GravelSurfaces,3);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    if(!TestNotNull(TEXT("Test world exists"),World))return false;
    FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Pawn=World->SpawnActor<AFlightPawn>(FVector(55,79,208),FRotator::ZeroRotator,Spawn);
    if(!TestNotNull(TEXT("Actual Pawn exists"),Pawn)){World->DestroyWorld(false);return false;}
    const FVector Surface(55,79,APacificCoastGraybox::GetCoastalLandHeight(55,79)+1.8F);
    TestTrue(TEXT("Actual start can be grounded on the mat"),Pawn->InitializeGroundedSpawn(Surface));
    Pawn->GetComponents(Parts);double Lowest=BIG_NUMBER,LowestVisual=BIG_NUMBER;
    for(const auto* Part:Parts)
    {
        if(!Part->GetStaticMesh())continue;
        const double Bottom=Part->CalcBounds(Part->GetComponentTransform()).GetBox().Min.Z;
        if(Part->GetName().StartsWith(TEXT("Support_")))Lowest=FMath::Min(Lowest,Bottom);
        if(!Part->GetName().Contains(TEXT("LOD")))LowestVisual=FMath::Min(LowestVisual,Bottom);
    }
    TestTrue(TEXT("Real support feet clear the mat by3mm"),FMath::Abs(Lowest-Surface.Z-.3)<.05);
    TestTrue(TEXT("Gimbal/body do not extend below the support plane"),LowestVisual>=Surface.Z-.1);
    TestTrue(TEXT("Grounded actor is not floating at the old2m origin"),Pawn->GetActorLocation().Z>5&&Pawn->GetActorLocation().Z<20);
    TestEqual(TEXT("Start retains grounded flight state"),Pawn->GetFlightModel().Phase,EFlightPhase::Grounded);
    const FTransform Start=Pawn->GetActorTransform();
    Pawn->ConfigureReviewFlightState(400,0);Pawn->ResetFlight();
    TestTrue(TEXT("Restart returns to the same grounded mat pose"),Pawn->GetActorTransform().Equals(Start,.01));
    World->DestroyWorld(false);
    return true;
}
#endif
