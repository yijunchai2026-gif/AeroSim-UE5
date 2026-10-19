#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PacificCoastGraybox.h"
#include "ProceduralMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalContinuityTest, "AeroSim.Environment.CoastalCliff.ClosedBedrockAndConnectedMountains", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCoastalContinuityTest::RunTest(const FString& Parameters)
{
    const APacificCoastGraybox* Coast = GetDefault<APacificCoastGraybox>();
    TArray<UProceduralMeshComponent*> Meshes;
    Coast->GetComponents(Meshes);
    TMap<uint64, int32> Edges;
    TArray<FVector> Positions;
    int32 TriangleCount = 0;
    int32 SurfaceParts = 0;
    for (UProceduralMeshComponent* Mesh : Meshes)
    {
        const FString Name = Mesh->GetName();
        if (Name != TEXT("CoastalPlateau") && Name != TEXT("CoastalMountainRock") && Name != TEXT("CoastalMountainScrub") && Name != TEXT("CoastalBedrock") && Name != TEXT("CoastalFracturedRock")) continue;
        FProcMeshSection* Section = Mesh->GetProcMeshSection(0);
        if (!TestNotNull(TEXT("Actual terrain mesh section exists"), Section)) return false;
        ++SurfaceParts;
        if (Positions.IsEmpty())
            for (const FProcMeshVertex& V : Section->ProcVertexBuffer) Positions.Add(V.Position);
        if (!TestEqual(TEXT("Terrain parts use the same sampled vertex domain"), Section->ProcVertexBuffer.Num(), Positions.Num())) return false;
        for (int32 Index = 0; Index < Section->ProcIndexBuffer.Num(); Index += 3)
        {
            const uint32 A = Section->ProcIndexBuffer[Index], B = Section->ProcIndexBuffer[Index + 1], C = Section->ProcIndexBuffer[Index + 2];
            const uint32 Start[] = {A, B, C}, End[] = {B, C, A};
            for (int32 Edge = 0; Edge < 3; ++Edge)
            {
                const uint64 Key = (static_cast<uint64>(FMath::Min(Start[Edge], End[Edge])) << 32) | FMath::Max(Start[Edge], End[Edge]);
                ++Edges.FindOrAdd(Key);
            }
            ++TriangleCount;
        }
    }
    TestEqual(TEXT("All terrain surface classifications are present"), SurfaceParts, 5);
    if (!TestTrue(TEXT("Actual land has a bounded detailed mesh"), TriangleCount > 150000 && TriangleCount < 200000)) return false;
    int32 OpenEdges = 0;
    for (const auto& Edge : Edges) if (Edge.Value != 2) ++OpenEdges;
    TestEqual(TEXT("Every land, cliff, skirt and bottom edge has exactly two incident faces"), OpenEdges, 0);
    TestTrue(TEXT("Bridge spans a real seasonal drainage bed"), FMath::IsNearlyEqual(APacificCoastGraybox::GetCoastalLandHeight(-9000,8500),-180.0F,1.0F));
    for(int32 Side=-1;Side<=1;Side+=2)
        TestTrue(TEXT("Both bridge landings stay at grade"), FMath::Abs(APacificCoastGraybox::GetCoastalLandHeight(-9000+Side*1300*.9510565F,8500+Side*1300*.309017F))<1);
    for(float Y:{-53000.0F,-31000.0F,-19000.0F,19000.0F,23700.0F,54000.0F,89123.0F})
    {
        const double Shore=APacificCoastGraybox::GetCoastalShoreX(Y);double Error=BIG_NUMBER;
        for(auto* Mesh:Meshes)
        {
            if(Mesh->GetFName()!=TEXT("CoastalBedrock")&&Mesh->GetFName()!=TEXT("CoastalFracturedRock"))continue;
            const auto* Section=Mesh->GetProcMeshSection(0);if(!Section)continue;
            for(int32 Index=0;Index<Section->ProcIndexBuffer.Num();Index+=3)
            {
                const FVector A=Section->ProcVertexBuffer[Section->ProcIndexBuffer[Index]].Position;
                const FVector B=Section->ProcVertexBuffer[Section->ProcIndexBuffer[Index+1]].Position;
                const FVector C=Section->ProcVertexBuffer[Section->ProcIndexBuffer[Index+2]].Position;
                const FVector U=B-A,V=C-A;
                const double Det=U.Y*V.Z-U.Z*V.Y;if(FMath::Abs(Det)<.001)continue;
                const double DY=Y-A.Y,DZ=APacificCoastGraybox::SeaLevelCm-A.Z;
                const double S=(DY*V.Z-DZ*V.Y)/Det,T=(U.Y*DZ-U.Z*DY)/Det;
                if(S<0||T<0||S+T>1)continue;
                Error=FMath::Min(Error,FMath::Abs(A.X+S*U.X+T*V.X-Shore));
            }
        }
        TestTrue(TEXT("Foam shoreline matches the actual cliff triangle intersection within3cm"),Error<3);
    }
    float MinX = BIG_NUMBER, MaxAbsY = 0, MountainHeight = 0, MaximumMountainGrade = 0;
    float NorthernRidgeHeight = 0, SouthernRidgeHeight = 0, ForegroundSpurHeight = 0;
    float LowestCoastalRim=BIG_NUMBER, HighestCoastalRim=-BIG_NUMBER;
    FVector MainSummitLocation = FVector::ZeroVector;
    FVector MaximumGradeLocation = FVector::ZeroVector;
    int32 PeripheralVertices = 0, UnsupportedPeripheralVertices = 0, SharedHeightMismatches = 0;
    for (const FVector& P : Positions)
    {
        MinX = FMath::Min(MinX, static_cast<float>(P.X));
        MaxAbsY = FMath::Max(MaxAbsY, FMath::Abs(static_cast<float>(P.Y)));
        if (P.Z > MountainHeight) { MountainHeight = P.Z; MainSummitLocation = P; }
        if (P.Y > 75000 && P.X < -70000) NorthernRidgeHeight = FMath::Max(NorthernRidgeHeight, static_cast<float>(P.Z));
        if (P.Y < -105000 && P.X < -70000) SouthernRidgeHeight = FMath::Max(SouthernRidgeHeight, static_cast<float>(P.Z));
        if (P.X > -100000 && P.X < -60000) ForegroundSpurHeight = FMath::Max(ForegroundSpurHeight, static_cast<float>(P.Z));
        if(FMath::Abs(P.Y)<60000 && FMath::IsNearlyEqual(static_cast<float>(P.X),APacificCoastGraybox::GetCliffEdgeX(P.Y),.1F)
            && FMath::IsNearlyEqual(static_cast<float>(P.Z),APacificCoastGraybox::GetCoastalLandHeight(P.X,P.Y),.1F))
        {
            LowestCoastalRim=FMath::Min(LowestCoastalRim,static_cast<float>(P.Z));
            HighestCoastalRim=FMath::Max(HighestCoastalRim,static_cast<float>(P.Z));
        }
        if (P.X < -40000 && P.Z > 1000)
        {
            const float DX = (APacificCoastGraybox::GetCoastalLandHeight(P.X + 500, P.Y) - APacificCoastGraybox::GetCoastalLandHeight(P.X - 500, P.Y)) / 1000.0F;
            const float DY = (APacificCoastGraybox::GetCoastalLandHeight(P.X, P.Y + 500) - APacificCoastGraybox::GetCoastalLandHeight(P.X, P.Y - 500)) / 1000.0F;
            const float Grade = FMath::Sqrt(DX * DX + DY * DY);
            if (Grade > MaximumMountainGrade) { MaximumMountainGrade = Grade; MaximumGradeLocation = P; }
        }
        if (P.X < -239999 || FMath::Abs(P.Y) > 219999)
        {
            ++PeripheralVertices;
            if (P.Z > APacificCoastGraybox::SeaLevelCm - 3000) ++UnsupportedPeripheralVertices;
        }
        if (P.X < APacificCoastGraybox::GetCliffEdgeX(P.Y) - 1 && P.Z > -11999 && !FMath::IsNearlyEqual(static_cast<float>(P.Z), APacificCoastGraybox::GetCoastalLandHeight(P.X, P.Y), .1F)) ++SharedHeightMismatches;
    }
    TestTrue(TEXT("Connected terrain reaches beyond the former detached mountain and rectangular land bounds"), MinX < -200000 && MaxAbsY > 200000);
    TestTrue(TEXT("Actual shared coastal ridge has a 200–300 m crest"), MountainHeight > 20000 && MountainHeight < 30000);
    TestTrue(TEXT("Actual highest summit belongs to the single main ridge node"), MainSummitLocation.X > -160000 && MainSummitLocation.X < -120000 && MainSummitLocation.Y > -50000 && MainSummitLocation.Y < 10000);
    TestTrue(TEXT("North/south extensions remain subordinate to the main summit"), MountainHeight > NorthernRidgeHeight + 4000 && MountainHeight > SouthernRidgeHeight + 4000);
    TestTrue(TEXT("Foreground diagonal spurs form a lower terrain layer"), ForegroundSpurHeight > 4000 && ForegroundSpurHeight < MountainHeight - 4000);
    TestTrue(TEXT("Sampled mountain shoulders avoid near-vertical noise columns"), MaximumMountainGrade < 1.25F);
    TestTrue(TEXT("Actual coastal rim contains low eroded coves and surviving high headlands rather than an equal-height wall"),
        LowestCoastalRim<-4000 && HighestCoastalRim>-1500 && HighestCoastalRim-LowestCoastalRim>4000);
    TestTrue(TEXT("Peripheral shoreline geometry was sampled"), PeripheralVertices > 1000);
    TestEqual(TEXT("No exposed west/north/south edge terminates above sea"), UnsupportedPeripheralVertices, 0);
    TestEqual(TEXT("Visible foothills and mountains use the shared land height function"), SharedHeightMismatches, 0);
    float MinimumCover = 1.0F, MaximumCover = 0;
    bool bValidCover = true;
    for (float X = -200000; X <= 20000; X += 12000)
        for (float Y = -180000; Y <= 180000; Y += 12000)
        {
            const float Cover = APacificCoastGraybox::GetCoastalVegetationCover(X, Y);
            bValidCover &= FMath::IsFinite(Cover) && Cover >= 0 && Cover <= 1;
            MinimumCover = FMath::Min(MinimumCover, Cover); MaximumCover = FMath::Max(MaximumCover, Cover);
        }
    TestTrue(TEXT("Shared vegetation field is finite, bounded and varies across real ridges and drainage"), bValidCover && MaximumCover - MinimumCover > .3F);
    for (const FVector& Route : FPacificCoastGrayboxContract::GetPlateauClearancePoints())
        TestTrue(TEXT("Vegetation coverage keeps the actual flight route open"), FMath::IsNearlyZero(APacificCoastGraybox::GetCoastalVegetationCover(Route.X, Route.Y)));
    TestTrue(TEXT("Vegetation does not grow beyond the ocean-facing cliff"), FMath::IsNearlyZero(APacificCoastGraybox::GetCoastalVegetationCover(APacificCoastGraybox::GetCliffEdgeX(0) + 1000, 0)));
    AddInfo(FString::Printf(TEXT("Closed terrain metadata: %d triangles; %d vertices; %d classified components; west %.0f cm; north/south %.0f cm; max height %.0f cm; max mountain grade %.3f; open edges %d"), TriangleCount, Positions.Num(), SurfaceParts, MinX, MaxAbsY, MountainHeight, MaximumMountainGrade, OpenEdges));
    AddInfo(FString::Printf(TEXT("Steepest sampled mountain shoulder: %s"), *MaximumGradeLocation.ToString()));
    AddInfo(FString::Printf(TEXT("Umunhum-inspired hierarchy: summit %s; north %.0f; south %.0f; foreground spur %.0f cm"), *MainSummitLocation.ToString(), NorthernRidgeHeight, SouthernRidgeHeight, ForegroundSpurHeight));
    AddInfo(FString::Printf(TEXT("Shared vegetation cover sampled range %.3f–%.3f"), MinimumCover, MaximumCover));
    AddInfo(FString::Printf(TEXT("Actual coastal rim variation %.0f to %.0f cm"),LowestCoastalRim,HighestCoastalRim));
    return true;
}
#endif
