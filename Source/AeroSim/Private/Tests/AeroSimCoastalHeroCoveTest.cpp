#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PacificCoastGraybox.h"
#include "ProceduralMeshComponent.h"

namespace CoastalTerrain
{
float BaselineLandHeight(float X,float Y);
float BaselineCliffEdgeX(float Y);
void GetR5CliffReference(float Y,TArray<FVector>& Result);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalHeroCoveTest,"AeroSim.Environment.CoastalCliff.HeroCoveReliefAndWaterline",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoastalHeroCoveTest::RunTest(const FString& Parameters)
{
    const APacificCoastGraybox* Coast=GetDefault<APacificCoastGraybox>();
    TArray<UProceduralMeshComponent*> Meshes;Coast->GetComponents(Meshes);
    TArray<FProcMeshSection*> Sections,CliffSections;
    TArray<float> RimRows;
    for(UProceduralMeshComponent* Mesh:Meshes)
    {
        const FString Name=Mesh->GetName();
        if(Name!=TEXT("CoastalPlateau")&&Name!=TEXT("CoastalMountainRock")&&Name!=TEXT("CoastalMountainScrub")
            &&Name!=TEXT("CoastalBedrock")&&Name!=TEXT("CoastalFracturedRock"))continue;
        FProcMeshSection* Section=Mesh->GetProcMeshSection(0);if(!Section)continue;
        Sections.Add(Section);
        if(Name==TEXT("CoastalBedrock")||Name==TEXT("CoastalFracturedRock"))CliffSections.Add(Section);
        if(Name!=TEXT("CoastalPlateau"))continue;
        for(const FProcMeshVertex& V:Section->ProcVertexBuffer)
            if(FMath::Abs(V.Position.Y+19000)<2400
                &&FMath::IsNearlyEqual(static_cast<float>(V.Position.X),APacificCoastGraybox::GetCliffEdgeX(V.Position.Y),.1F)
                &&FMath::IsNearlyEqual(static_cast<float>(V.Position.Z),APacificCoastGraybox::GetCoastalLandHeight(V.Position.X,V.Position.Y),.1F))
                RimRows.Add(V.Position.Y);
    }
    if(!TestEqual(TEXT("Actual shared closed-terrain sections are available"),Sections.Num(),5))return false;
    RimRows.Sort();float LargestSpacing=0;
    for(int32 Index=1;Index<RimRows.Num();++Index)LargestSpacing=FMath::Max(LargestSpacing,RimRows[Index]-RimRows[Index-1]);
    TestTrue(TEXT("Only the hero cove has sufficient alongshore mesh resolution for metre-scale geology"),RimRows.Num()>25&&LargestSpacing<150);
    auto RenderedHeight=[&Sections](double X,double Y)
    {
        double Height=-BIG_NUMBER;
        for(const FProcMeshSection* Section:Sections)
            for(int32 Index=0;Index<Section->ProcIndexBuffer.Num();Index+=3)
            {
                const FVector A=Section->ProcVertexBuffer[Section->ProcIndexBuffer[Index]].Position;
                const FVector U=Section->ProcVertexBuffer[Section->ProcIndexBuffer[Index+1]].Position-A;
                const FVector V=Section->ProcVertexBuffer[Section->ProcIndexBuffer[Index+2]].Position-A;
                const double Det=U.X*V.Y-U.Y*V.X;if(FMath::Abs(Det)<.001)continue;
                const double DX=X-A.X,DY=Y-A.Y,S=(DX*V.Y-DY*V.X)/Det,T=(U.X*DY-U.Y*DX)/Det;
                if(S>=0&&T>=0&&S+T<=1)Height=FMath::Max(Height,A.Z+S*U.Z+T*V.Z);
            }
        return Height;
    };
    // R6: probe the deliberately offset, curved main drainage, retaining the original depth gate.
    const float X=APacificCoastGraybox::GetCliffEdgeX(-19400)-1800;
    const double Floor=RenderedHeight(X,-19400),BankA=RenderedHeight(X,-20250),BankB=RenderedHeight(X,-18550);
    TestTrue(TEXT("Actual rendered and collision mesh contains a drainage cut between supported uneven banks"),Floor>-10000&&Floor<(BankA+BankB)*.5-120);
    AddInfo(FString::Printf(TEXT("Hero cove actual mesh: %d rim rows, largest spacing %.1f cm; bed %.1f, banks %.1f/%.1f cm"),RimRows.Num(),LargestSpacing,Floor,BankA,BankB));
    // Independent R5 station reference: changing the cap must not move the sea contact mesh.
    FProcMeshSection* Domain=Sections[0];int32 FrozenSamples=0,ChangedWaterSamples=0;
    for(const float Y:RimRows)
    {
        TArray<FVector> Reference;CoastalTerrain::GetR5CliffReference(Y,Reference);
        for(const FVector& Expected:Reference)
        {
            if(Expected.Z>APacificCoastGraybox::SeaLevelCm+450.0F)continue;
            ++FrozenSamples;bool Found=false;
            for(const FProcMeshVertex& V:Domain->ProcVertexBuffer)
                if(V.Position.Equals(Expected,.001)){Found=true;break;}
            if(!Found)++ChangedWaterSamples;
        }
    }
    TestTrue(TEXT("Original water-band geometry is sampled"),FrozenSamples>400);
    TestEqual(TEXT("All R5 sea-crossing and adjacent lower vertices survive unchanged"),ChangedWaterSamples,0);

    int32 RefinedRows=0;float LargestOuterSpacing=0;
    for(const float Y:RimRows)
    {
        if(FMath::Abs(Y+19000)>1800)continue;
        const float Edge=APacificCoastGraybox::GetCliffEdgeX(Y);TArray<float> Xs;
        for(const FProcMeshVertex& V:Domain->ProcVertexBuffer)
            if(FMath::IsNearlyEqual(float(V.Position.Y),Y,.01F)&&V.Position.X>Edge-3000&&V.Position.X<Edge-100
                &&FMath::IsNearlyEqual(float(V.Position.Z),APacificCoastGraybox::GetCoastalLandHeight(V.Position.X,Y),.1F))Xs.Add(V.Position.X);
        Xs.Sort();if(Xs.Num()>15)++RefinedRows;
        for(int32 I=1;I<Xs.Num();++I)LargestOuterSpacing=FMath::Max(LargestOuterSpacing,Xs[I]-Xs[I-1]);
    }
    TestTrue(TEXT("Actual outer land is refined only in the hero reach"),RefinedRows>20);
    TestTrue(TEXT("Actual cap/drainage cells resolve the geometry below 1.5 m"),LargestOuterSpacing>0&&LargestOuterSpacing<=145);
    AddInfo(FString::Printf(TEXT("R6 upper cove: %d frozen sea-adjacent vertices, %d refined land rows, outer X spacing %.1f cm"),FrozenSamples,RefinedRows,LargestOuterSpacing));

    // Check exact real triangles at irregular Y/Z, not a bilinear duplicate of the implementation.
    float MaximumFaceError=0;int32 FaceSamples=0;
    for(const auto* Section:CliffSections)
        for(int32 I=0;I<Section->ProcIndexBuffer.Num();I+=3)
        {
            const FVector A=Section->ProcVertexBuffer[Section->ProcIndexBuffer[I]].Position;
            const FVector B=Section->ProcVertexBuffer[Section->ProcIndexBuffer[I+1]].Position;
            const FVector C=Section->ProcVertexBuffer[Section->ProcIndexBuffer[I+2]].Position;
            const FVector P=A*.23+B*.31+C*.46;
            if(FMath::Abs(P.Y+19000)>2450||P.Z<APacificCoastGraybox::SeaLevelCm-300||P.Z>-5300)continue;
            // Only the closed cliff, not top land/skirts: YZ projected area must be nonzero.
            const FVector U=B-A,V=C-A;if(FMath::Abs(U.Y*V.Z-U.Z*V.Y)<.01)continue;
            if(P.X<APacificCoastGraybox::GetCliffEdgeX(P.Y)-1)continue;
            MaximumFaceError=FMath::Max(MaximumFaceError,FMath::Abs(float(P.X)-FPacificCoastHabitat::GetCliffFaceX(P.Y,P.Z)));++FaceSamples;
        }
    TestTrue(TEXT("Actual non-axis-aligned cliff triangles are queried"),FaceSamples>100);
    TestTrue(TEXT("Cliff query returns the actual rendered triangle, not a bilinear patch"),MaximumFaceError<.1F);

    // Height changes are zero at the protected flight core and outside this single 50 m cove.
    for(const float Y:{-24000.0F,-21500.0F,-19400.0F,-16500.0F,-14000.0F})
        for(const float SX:{0.0F,10500.0F,12000.0F,50000.0F})
            TestTrue(TEXT("Bounded hero edit preserves the core and outside height authority"),
                FMath::IsNearlyEqual(APacificCoastGraybox::GetCoastalLandHeight(SX,Y),CoastalTerrain::BaselineLandHeight(SX,Y),.001F));

    for(const float Y:{-19000.0F,-18913.0F,-18100.0F})
    {
        const double Shore=APacificCoastGraybox::GetCoastalShoreX(Y);double Error=BIG_NUMBER;
        for(const FProcMeshSection* Section:Sections)
            for(int32 Index=0;Index<Section->ProcIndexBuffer.Num();Index+=3)
            {
                const FVector A=Section->ProcVertexBuffer[Section->ProcIndexBuffer[Index]].Position;
                const FVector U=Section->ProcVertexBuffer[Section->ProcIndexBuffer[Index+1]].Position-A;
                const FVector V=Section->ProcVertexBuffer[Section->ProcIndexBuffer[Index+2]].Position-A;
                const double Det=U.Y*V.Z-U.Z*V.Y;if(FMath::Abs(Det)<.001)continue;
                const double DY=Y-A.Y,DZ=APacificCoastGraybox::SeaLevelCm-A.Z;
                const double S=(DY*V.Z-DZ*V.Y)/Det,T=(U.Y*DZ-U.Z*DY)/Det;
                if(S>=0&&T>=0&&S+T<=1)Error=FMath::Min(Error,FMath::Abs(A.X+S*U.X+T*V.X-Shore));
            }
        TestTrue(TEXT("Refined cove waterline still matches the actual triangle intersection within 3 cm"),Error<3);
    }
    return true;
}
#endif
