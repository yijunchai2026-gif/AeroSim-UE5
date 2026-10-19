#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PacificCoastGraybox.h"
#include "ProceduralMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalHeroCoveTest,"AeroSim.Environment.CoastalCliff.HeroCoveReliefAndWaterline",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoastalHeroCoveTest::RunTest(const FString& Parameters)
{
    const APacificCoastGraybox* Coast=GetDefault<APacificCoastGraybox>();
    TArray<UProceduralMeshComponent*> Meshes;Coast->GetComponents(Meshes);
    TArray<FProcMeshSection*> Sections;
    TArray<float> RimRows;
    for(UProceduralMeshComponent* Mesh:Meshes)
    {
        const FString Name=Mesh->GetName();
        if(Name!=TEXT("CoastalPlateau")&&Name!=TEXT("CoastalMountainRock")&&Name!=TEXT("CoastalMountainScrub")
            &&Name!=TEXT("CoastalBedrock")&&Name!=TEXT("CoastalFracturedRock"))continue;
        FProcMeshSection* Section=Mesh->GetProcMeshSection(0);if(!Section)continue;
        Sections.Add(Section);
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
    const float X=APacificCoastGraybox::GetCliffEdgeX(-19000)-1400;
    const double Floor=RenderedHeight(X,-19000),BankA=RenderedHeight(X,-20000),BankB=RenderedHeight(X,-18000);
    TestTrue(TEXT("Actual rendered and collision mesh contains a drainage cut between supported uneven banks"),Floor>-10000&&Floor<(BankA+BankB)*.5-120);
    AddInfo(FString::Printf(TEXT("Hero cove actual mesh: %d rim rows, largest spacing %.1f cm; bed %.1f, banks %.1f/%.1f cm"),RimRows.Num(),LargestSpacing,Floor,BankA,BankB));
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
