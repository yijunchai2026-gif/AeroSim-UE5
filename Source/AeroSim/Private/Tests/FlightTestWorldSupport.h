#pragma once

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "FlightPawn.h"

namespace AeroSimTestSupport
{
// Input lifecycle tests need actual support to make their Grounded expectation meaningful.
// A floor in the fixture must not become a default runtime support plane.
inline void AddFloorAndGroundPawn(UWorld* World,AFlightPawn* Pawn)
{
    if(!World || !Pawn) return;
    AActor* Floor=World->SpawnActor<AActor>();
    UBoxComponent* Box=NewObject<UBoxComponent>(Floor);
    Floor->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(500,500,10));
    Box->SetCollisionObjectType(ECC_WorldStatic);
    Box->SetCollisionResponseToAllChannels(ECR_Block);
    Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Box->RegisterComponent();
    Floor->SetActorLocation(FVector(0,0,-10));
    Pawn->InitializeGroundedSpawn(FVector::ZeroVector);
}
}
