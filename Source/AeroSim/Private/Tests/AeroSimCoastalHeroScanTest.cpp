#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PacificCoastGraybox.h"
#include "FlightPawn.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "PhysicsEngine/BodySetup.h"
#include "ProceduralMeshComponent.h"
#include "StaticMeshResources.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"

// DEV-TERRAIN R5: single rigid hero-cove scan fitted to the closed cliff, with matching collision.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalHeroScanTest, "AeroSim.Environment.CoastalTerrain.HeroCoveScanFitAndCollision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCoastalHeroScanTest::RunTest(const FString& Parameters)
{
    const APacificCoastGraybox* Coast = GetDefault<APacificCoastGraybox>();
    TArray<UStaticMeshComponent*> Statics; Coast->GetComponents(Statics);
    TArray<UStaticMeshComponent*> Scans;
    for (UStaticMeshComponent* Mesh : Statics)
        if (Mesh->GetStaticMesh() && Mesh->GetStaticMesh()->GetFName() == TEXT("SM_CoastHeroCliffScanDraft")) Scans.Add(Mesh);
    if (!TestEqual(TEXT("Exactly one hero scan instance (no arraying)"), Scans.Num(), 1)) return false;
    const UStaticMeshComponent* Scan = Scans[0];
    const UStaticMesh* Mesh = Scan->GetStaticMesh();
    TestEqual(TEXT("Hero scan component name"), Scan->GetFName(), FName(TEXT("CoastalHeroCliffScan")));
    TestTrue(TEXT("Rigid placement: unit scale, no stretching"), Scan->GetRelativeScale3D().Equals(FVector::OneVector, 1e-4));
    TestTrue(TEXT("Rigid placement: no rotation"), Scan->GetRelativeRotation().IsNearlyZero(1e-3));
    if (!TestNotNull(TEXT("Scan render data"), Mesh->GetRenderData())) return false;
    const int32 Expected[] = {49995, 11997, 3591};
    TestEqual(TEXT("Three original LODs"), Mesh->GetRenderData()->LODResources.Num(), 3);
    for (int32 LOD = 0; LOD < FMath::Min(3, Mesh->GetRenderData()->LODResources.Num()); ++LOD)
        TestEqual(FString::Printf(TEXT("LOD%d keeps its source triangle count"), LOD), static_cast<int32>(Mesh->GetRenderData()->LODResources[LOD].GetNumTriangles()), Expected[LOD]);
    const FTransform World = Scan->GetRelativeTransform();
    const FBox Bounds = Mesh->GetBoundingBox().TransformBy(World);
    TestTrue(TEXT("Scan is 40 m wide and ~10 m high"), FMath::IsNearlyEqual(Bounds.GetSize().Y, 4000.0, 1.0) && Bounds.GetSize().Z > 980 && Bounds.GetSize().Z < 1020);
    TestTrue(TEXT("Lowest scan point is above still water"), Bounds.Min.Z > APacificCoastGraybox::SeaLevelCm);
    TestTrue(TEXT("Scan stays inside the single Y=-190 m hero reach"), Bounds.Min.Y >= -21001 && Bounds.Max.Y <= -16999);
    TestEqual(TEXT("Scan collision is enabled for the playable build"), Scan->GetCollisionEnabled(), ECollisionEnabled::QueryAndPhysics);
    TestTrue(TEXT("Scan collision uses its rendered LOD0 triangles"), Mesh->GetBodySetup() && Mesh->GetBodySetup()->CollisionTraceFlag == CTF_UseComplexAsSimple);

    // R5a P0: the scan's real relief must reach shading. (1) Imported normals vary (not flattened);
    // (2) the world-space rock material re-bases its triplanar normal on the geometric normal.
    {
        const FStaticMeshLODResources& Lod = Mesh->GetRenderData()->LODResources[0];
        const int32 NormalCount = Lod.VertexBuffers.StaticMeshVertexBuffer.GetNumVertices();
        FVector Mean = FVector::ZeroVector;
        for (int32 Index = 0; Index < NormalCount; ++Index) Mean += FVector(Lod.VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(Index));
        Mean /= FMath::Max(1, NormalCount);
        double Spread = 0;
        for (int32 Index = 0; Index < NormalCount; ++Index) Spread += (FVector(Lod.VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(Index)) - Mean).Size();
        Spread /= FMath::Max(1, NormalCount);
        TestTrue(TEXT("Scan LOD0 normals carry real relief (mean deviation > 0.3)"), Spread > .3);
        AddInfo(FString::Printf(TEXT("Hero scan normals: mean %s, mean deviation %.3f"), *Mean.ToString(), Spread));
#if WITH_EDITORONLY_DATA
        const UMaterialInterface* RockMaterial = Scan->GetMaterial(0);
        bool bRebased = false;
        if (RockMaterial && RockMaterial->GetMaterial())
            for (const UMaterialExpression* Expression : RockMaterial->GetMaterial()->GetExpressions())
                bRebased |= Expression && Expression->IsA<UMaterialExpressionVertexNormalWS>();
        TestTrue(TEXT("World-space rock material keeps the geometric normal (no flat triplanar replacement)"), bRebased);
#endif
    }

    // Fit against the actual closed cliff face: perimeter (crop edges) buried, interior visible, no coplanar fight.
    const FStaticMeshLODResources& LOD0 = Mesh->GetRenderData()->LODResources[0];
    const int32 Count = LOD0.VertexBuffers.PositionVertexBuffer.GetNumVertices();
    TArray<FVector> Points;
    for (int32 Index = 0; Index < Count; ++Index) Points.Add(World.TransformPosition(FVector(LOD0.VertexBuffers.PositionVertexBuffer.VertexPosition(Index))));
    TArray<uint32> Indices; LOD0.IndexBuffer.GetCopy(Indices);
    TMap<FIntVector, int32> Weld; TArray<int32> Id; Id.SetNum(Count);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const FIntVector Key(FMath::RoundToInt(Points[Index].X * 10), FMath::RoundToInt(Points[Index].Y * 10), FMath::RoundToInt(Points[Index].Z * 10));
        const int32* Found = Weld.Find(Key); Id[Index] = Found ? *Found : Weld.Add(Key, Index);
    }
    TMap<uint64, int32> Edges;
    for (int32 Index = 0; Index + 2 < Indices.Num(); Index += 3)
        for (int32 Corner = 0; Corner < 3; ++Corner)
        {
            const uint32 A = Id[Indices[Index + Corner]], B = Id[Indices[Index + (Corner + 1) % 3]];
            ++Edges.FindOrAdd((static_cast<uint64>(FMath::Min(A, B)) << 32) | FMath::Max(A, B));
        }
    TSet<int32> Perimeter;
    for (const auto& Edge : Edges) if (Edge.Value == 1) { Perimeter.Add(static_cast<int32>(Edge.Key >> 32)); Perimeter.Add(static_cast<int32>(Edge.Key & 0xffffffff)); }
    int32 ExposedPerimeter = 0, Visible = 0, Coplanar = 0;
    float WorstPerimeter = -BIG_NUMBER;
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const FVector& P = Points[Index];
        const float Delta = P.X - FPacificCoastHabitat::GetCliffFaceX(P.Y, P.Z);
        if (Delta > 0) ++Visible;
        if (FMath::Abs(Delta) < 2.0F) ++Coplanar;
    }
    for (int32 Index : Perimeter)
    {
        const FVector& P = Points[Index];
        const float Delta = P.X - FPacificCoastHabitat::GetCliffFaceX(P.Y, P.Z);
        const bool bUnderCap = P.X < APacificCoastGraybox::GetCliffEdgeX(P.Y) && P.Z < APacificCoastGraybox::GetCoastalLandHeight(P.X, P.Y) - 10;
        if (Delta > -5.0F && !bUnderCap)
        {
            if (ExposedPerimeter < 12) AddInfo(FString::Printf(TEXT("Exposed crop vertex local (%.0f, %.0f, %.0f) delta %.0f"), P.X - World.GetLocation().X, P.Y - World.GetLocation().Y, P.Z - World.GetLocation().Z, Delta));
            ++ExposedPerimeter;
        }
        WorstPerimeter = FMath::Max(WorstPerimeter, bUnderCap ? -BIG_NUMBER : Delta);
    }
    TestTrue(TEXT("Scan perimeter was found"), Perimeter.Num() > 1000);
    TestEqual(TEXT("Every crop edge is buried in the closed cliff or under the land cap"), ExposedPerimeter, 0);
    TestTrue(TEXT("At least 40% of the scan rock is the visible surface"), Visible * 10 >= Count * 4);
    TestTrue(TEXT("Less than 3% of scan vertices lie within 2 cm of the terrain face (no Z-fight)"), Coplanar * 100 < Count * 3);
    AddInfo(FString::Printf(TEXT("Hero scan fit: %d vertices, %.1f%% visible, %d perimeter (worst %.0f cm), %d near-coplanar; bounds %s"),
        Count, 100.0F * Visible / Count, Perimeter.Num(), WorstPerimeter, Coplanar, *Bounds.ToString()));

    // R6: measure the actual open scan crown, not the old padded support lookup. Its soil edge
    // must follow real rock with a small burial margin rather than projecting metres into air.
    float MaximumCrownReach=0,MinimumCrownReach=BIG_NUMBER,MinimumSoil=BIG_NUMBER,MaximumSoil=0;int32 CrownSamples=0;
    for(float Y=Bounds.Min.Y+200;Y<=Bounds.Max.Y-200;Y+=200)
    {
        TArray<FVector> Crossings;
        for(const auto& Edge:Edges)
        {
            if(Edge.Value!=1)continue;
            const FVector A=Points[int32(Edge.Key>>32)],B=Points[int32(Edge.Key&0xffffffff)];
            if(Y<FMath::Min(A.Y,B.Y)||Y>FMath::Max(A.Y,B.Y)||FMath::Abs(B.Y-A.Y)<.001)continue;
            Crossings.Add(FMath::Lerp(A,B,(Y-A.Y)/(B.Y-A.Y)));
        }
        if(Crossings.IsEmpty())continue;
        float Top=-BIG_NUMBER;for(const FVector& P:Crossings)Top=FMath::Max(Top,float(P.Z));
        float CrownX=-BIG_NUMBER;for(const FVector& P:Crossings)if(P.Z>Top-35)CrownX=FMath::Max(CrownX,float(P.X));
        const float RimX=APacificCoastGraybox::GetCliffEdgeX(Y),RimZ=APacificCoastGraybox::GetCoastalLandHeight(RimX,Y);
        const float Reach=RimX-CrownX,Cover=RimZ-Top;++CrownSamples;
        MinimumCrownReach=FMath::Min(MinimumCrownReach,Reach);MaximumCrownReach=FMath::Max(MaximumCrownReach,Reach);
        MinimumSoil=FMath::Min(MinimumSoil,Cover);MaximumSoil=FMath::Max(MaximumSoil,Cover);
    }
    TestTrue(TEXT("Actual scan crown is measured throughout the hero face"),CrownSamples>=18);
    TestTrue(TEXT("Soil edge extends only 20-40 cm beyond the real crown, rather than up to 5.7 m"),MinimumCrownReach>=20&&MaximumCrownReach<=40);
    TestTrue(TEXT("Crown is covered by a thin supported 1.1-1.85 m soil layer"),MinimumSoil>=110&&MaximumSoil<=185);
    AddInfo(FString::Printf(TEXT("R6 actual crown: %d cuts, reach %.1f..%.1f cm, soil %.1f..%.1f cm"),CrownSamples,MinimumCrownReach,MaximumCrownReach,MinimumSoil,MaximumSoil));

    // The rebuilt reach is still the same closed, supported cliff: its rim is above the scan top.
    for (float Y = -20000; Y <= -18000; Y += 250)
        TestTrue(TEXT("Land rim stands above the scan top inside the reach"),
            APacificCoastGraybox::GetCoastalLandHeight(APacificCoastGraybox::GetCliffEdgeX(Y), Y) > Bounds.Max.Z + 50);

    // Real collision: a swept sphere meets exactly the drawn surface (scan where it is proud, terrain elsewhere).
    UWorld* GameWorld = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Test world"), GameWorld)) return false;
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    APacificCoastGraybox* Live = GameWorld->SpawnActor<APacificCoastGraybox>(FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
    if (TestNotNull(TEXT("Coast spawned in test world"), Live))
    {
        int32 Probes = 0, ScanHits = 0, Mismatch = 0, Missed = 0, BehindFace = 0;
        float WorstGap = 0;
        // Thin probe sphere: a fat sphere legitimately stops early on oblique lips beside the ray.
        constexpr float Radius = 2.0F;
        FCollisionQueryParams Complex(TEXT("HeroComplex"), true), Simple(TEXT("HeroSweep"), false);
        for (float Y = Bounds.Min.Y + 300; Y <= Bounds.Max.Y - 300; Y += 350)
            for (float Z = Bounds.Min.Z + 100; Z <= Bounds.Max.Z - 100; Z += 120)
            {
                ++Probes;
                const FVector Start(40000, Y, Z), End(10000, Y, Z);
                FHitResult Line, Sweep;
                const bool bLine = GameWorld->LineTraceSingleByChannel(Line, Start, End, ECC_Visibility, Complex);
                const bool bSweep = GameWorld->SweepSingleByChannel(Sweep, Start, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(Radius), Simple);
                if (!bLine || !bSweep) { ++Missed; continue; }
                if (Line.GetComponent() && Line.GetComponent()->GetFName() == TEXT("CoastalHeroCliffScan")) ++ScanHits;
                // The sphere centre stops at most one radius (plus small slack for oblique facets) before the drawn surface.
                const float Gap = static_cast<float>(Sweep.Location.X - Line.ImpactPoint.X);
                WorstGap = FMath::Max(WorstGap, FMath::Abs(Gap));
                // On a facet inclined to the ray the sphere legitimately stops Radius/|n.x| early.
                if (Gap < -1.0F || Gap > Radius / FMath::Max(.2F, FMath::Abs(static_cast<float>(Line.ImpactNormal.X))) + 1.0F) ++Mismatch;
                if (Line.ImpactPoint.X < FPacificCoastHabitat::GetCliffFaceX(Y, Z) - 30.0F) ++BehindFace;
            }
        TestEqual(TEXT("Every probe hits the coast with both trace and sweep"), Missed, 0);
        TestEqual(TEXT("Pawn-channel sweep stops at the visible surface (within one sphere radius)"), Mismatch, 0);
        TestEqual(TEXT("Nothing is hit behind the closed terrain face"), BehindFace, 0);
        TestTrue(TEXT("The scan itself is what most probes meet"), ScanHits * 10 >= Probes * 4);
        AddInfo(FString::Printf(TEXT("Hero sweep: %d probes, %d on scan, worst sweep/trace gap %.1f cm"), Probes, ScanHits, WorstGap));
        // Exercise the real Pawn collision root, not only a tiny diagnostic sphere. No input,
        // controller or flight-model tick is used: this checks the actor's actual swept movement.
        AFlightPawn* Pawn=GameWorld->SpawnActor<AFlightPawn>(FVector(40000,Bounds.GetCenter().Y,Bounds.Max.Z),FRotator::ZeroRotator,Spawn);
        if(TestNotNull(TEXT("Actual flight Pawn is available for cap/rock sweeps"),Pawn))
        {
            const UBoxComponent* Box=Cast<UBoxComponent>(Pawn->GetRootComponent());
            TestNotNull(TEXT("Collision root is the actual aircraft box"),Box);
            int32 PawnProbes=0,PawnMisses=0,PawnPenetration=0,PawnSurfaceMismatch=0;float WorstContactError=0;
            for(float Y=Bounds.Min.Y+350;Y<=Bounds.Max.Y-350;Y+=550)
                for(float Z:{float(Bounds.Min.Z+250),float(Bounds.Max.Z-50),float(Bounds.Max.Z+80)})
                {
                    ++PawnProbes;const FVector Start(40000,Y,Z),End(10000,Y,Z);
                    Pawn->SetActorLocation(Start,false,nullptr,ETeleportType::TeleportPhysics);
                    FHitResult Hit;Pawn->AddActorWorldOffset(End-Start,true,&Hit,ETeleportType::None);
                    if(!Hit.bBlockingHit){++PawnMisses;continue;}
                    if(Hit.bStartPenetrating)++PawnPenetration;
                    FHitResult VisibleContact;FCollisionQueryParams Query(TEXT("PawnContactVisible"),true);Query.AddIgnoredActor(Pawn);
                    if(!GameWorld->LineTraceSingleByChannel(VisibleContact,FVector(40000,Hit.ImpactPoint.Y,Hit.ImpactPoint.Z),FVector(10000,Hit.ImpactPoint.Y,Hit.ImpactPoint.Z),ECC_Visibility,Query))
                    {++PawnSurfaceMismatch;continue;}
                    const float Error=FMath::Abs(float(VisibleContact.ImpactPoint.X-Hit.ImpactPoint.X));WorstContactError=FMath::Max(WorstContactError,Error);
                    if(Error>3.0F)++PawnSurfaceMismatch;
                }
            TestEqual(TEXT("Actual Pawn swept motion always stops on the coast"),PawnMisses,0);
            TestEqual(TEXT("Actual Pawn does not begin inside the coast"),PawnPenetration,0);
            TestEqual(TEXT("Actual Pawn contacts the visible rock/cap, not an invisible backing wall"),PawnSurfaceMismatch,0);
            AddInfo(FString::Printf(TEXT("R6 actual Pawn: %d swept moves, contact/visible error %.2f cm"),PawnProbes,WorstContactError));
            Pawn->Destroy();
        }

    }
    GameWorld->DestroyWorld(false);
    return true;
}
#endif
