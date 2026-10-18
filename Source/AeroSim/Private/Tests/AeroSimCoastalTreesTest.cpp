#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PacificCoastGraybox.h"
#include "ProceduralMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalTreeGeometryTest,
    "AeroSim.Environment.CoastalTrees.ActualSupportClearanceAndGeometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoastalTreeGeometryTest::RunTest(const FString& Parameters)
{
    const APacificCoastGraybox* Coast = GetDefault<APacificCoastGraybox>();
    if (!TestNotNull(TEXT("Generated coast exists"), Coast)) return false;
    TArray<UProceduralMeshComponent*> Meshes;
    Coast->GetComponents(Meshes);
    TArray<FVector> Origins;
    int32 BatchCount = 0, TotalVertices = 0, TotalTriangles = 0;
    int32 InvalidVertices = 0, InvalidIndices = 0, DegenerateTriangles = 0, ReversedFaces = 0;
    int32 BarkTriangles = 0, NeedleTriangles = 0;
    float MinimumBaseHeight = MAX_flt, MaximumBaseHeight = -MAX_flt;
    for (UProceduralMeshComponent* Mesh : Meshes)
    {
        const bool bBark = Mesh->GetFName() == TEXT("CoastalBranchSkeleton");
        if (!bBark && !Mesh->GetName().StartsWith(TEXT("CoastalNeedles"))) continue;
        ++BatchCount;
        if (bBark)
        {
            for (const FName& Tag : Mesh->ComponentTags)
            {
                const FString Text = Tag.ToString();
                if (!Text.StartsWith(TEXT("CoastalTreeBase|"))) continue;
                TArray<FString> Parts;
                Text.ParseIntoArray(Parts, TEXT("|"), false);
                if (!TestEqual(TEXT("Tree metadata has three coordinates"), Parts.Num(), 4)) return false;
                const FVector Origin(FCString::Atod(*Parts[1]), FCString::Atod(*Parts[2]), FCString::Atod(*Parts[3]));
                Origins.Add(Origin);
                TestTrue(TEXT("Tree base is finite"), !Origin.ContainsNaN());
                TestTrue(TEXT("Tree is supported by the common terrain height field"),
                    FMath::Abs(Origin.Z - APacificCoastGraybox::GetCoastalLandHeight(Origin.X,Origin.Y)) < .01);
                MinimumBaseHeight = FMath::Min(MinimumBaseHeight, static_cast<float>(Origin.Z));
                MaximumBaseHeight = FMath::Max(MaximumBaseHeight, static_cast<float>(Origin.Z));
                TestTrue(TEXT("Tree stands inland with crown clearance from the cliff"),
                    APacificCoastGraybox::GetCliffEdgeX(Origin.Y) - Origin.X > 2000);
                // Distance from the actual route polyline, including the return leg.
                const TArray<FVector>& Route = FPacificCoastGrayboxContract::GetRoutePoints();
                for (int32 Segment = 1; Segment < Route.Num(); ++Segment)
                {
                    const FVector2D A(Route[Segment - 1].X, Route[Segment - 1].Y);
                    const FVector2D B(Route[Segment].X, Route[Segment].Y);
                    const FVector2D P(Origin.X,Origin.Y), AB = B-A;
                    const double T = FMath::Clamp(FVector2D::DotProduct(P-A,AB) / AB.SizeSquared(),0.0,1.0);
                    TestTrue(TEXT("Tree base clears the flight route by at least 25 m"), FVector2D::Distance(P,A+AB*T) >= 2500);
                }
                const double WalkYaw = FMath::DegreesToRadians(18.0);
                const FVector2D Relative(Origin.X+9000,Origin.Y-8500);
                const double Along = Relative.X*FMath::Cos(WalkYaw)+Relative.Y*FMath::Sin(WalkYaw);
                const double Across = -Relative.X*FMath::Sin(WalkYaw)+Relative.Y*FMath::Cos(WalkYaw);
                const double DX = FMath::Max(FMath::Abs(Along)-1300.0,0.0);
                const double DY = FMath::Max(FMath::Abs(Across)-110.0,0.0);
                TestTrue(TEXT("Trunk and crown do not intrude into the actual boardwalk footprint"), FMath::Sqrt(DX*DX+DY*DY) > 950);
            }
        }
        TestEqual(TEXT("Visible trees have no expensive triangle collision"), Mesh->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
        FProcMeshSection* Section = Mesh->GetProcMeshSection(0);
        if (!TestNotNull(TEXT("Tree material batch contains actual mesh geometry"), Section)) return false;
        TestFalse(TEXT("Visible mesh sections do not cook collision"), Section->bEnableCollision);
        TotalVertices += Section->ProcVertexBuffer.Num();
        TotalTriangles += Section->ProcIndexBuffer.Num()/3;
        if (bBark) BarkTriangles += Section->ProcIndexBuffer.Num()/3;
        else NeedleTriangles += Section->ProcIndexBuffer.Num()/3;
        TestEqual(TEXT("Mesh index count describes complete triangles"), Section->ProcIndexBuffer.Num()%3,0);
        for (const FProcMeshVertex& Vertex : Section->ProcVertexBuffer)
        {
            if (Vertex.Position.ContainsNaN() || Vertex.Normal.ContainsNaN() || FMath::Abs(Vertex.Normal.SizeSquared()-1.0) > .01)
                ++InvalidVertices;
        }
        for (int32 Index = 0; Index+2 < Section->ProcIndexBuffer.Num(); Index += 3)
        {
            const uint32 IA = Section->ProcIndexBuffer[Index], IB = Section->ProcIndexBuffer[Index+1], IC = Section->ProcIndexBuffer[Index+2];
            if (IA >= static_cast<uint32>(Section->ProcVertexBuffer.Num()) || IB >= static_cast<uint32>(Section->ProcVertexBuffer.Num()) || IC >= static_cast<uint32>(Section->ProcVertexBuffer.Num()))
            { ++InvalidIndices; continue; }
            const FProcMeshVertex& A = Section->ProcVertexBuffer[IA];
            const FProcMeshVertex& B = Section->ProcVertexBuffer[IB];
            const FProcMeshVertex& C = Section->ProcVertexBuffer[IC];
            const FVector GeometricNormal = FVector::CrossProduct(C.Position-A.Position,B.Position-A.Position);
            if (GeometricNormal.SizeSquared() < .0001) ++DegenerateTriangles;
            else if (FVector::DotProduct(GeometricNormal,A.Normal+B.Normal+C.Normal) <= 0) ++ReversedFaces;
        }
    }
    TestEqual(TEXT("Five bounded material batches"),BatchCount,5);
    TestEqual(TEXT("Every generated tree exposes a real support origin"),Origins.Num(),240);
    for (int32 A = 0; A < Origins.Num(); ++A)
        for (int32 B = A+1; B < Origins.Num(); ++B)
            TestTrue(TEXT("Distinct trees retain 9.5 m center spacing"), FVector::DistSquaredXY(Origins[A],Origins[B]) >= FMath::Square(949.99));
    TestTrue(TEXT("Trees include supported elevated foothill specimens"),MaximumBaseHeight-MinimumBaseHeight > 100);
    TArray<UHierarchicalInstancedStaticMeshComponent*> Proxies;
    Coast->GetComponents(Proxies);
    bool bFoundProxies = false;
    for (const auto* Proxy : Proxies)
        if (Proxy->GetFName() == TEXT("CoastalTrunkCollisionProxies"))
        {
            bFoundProxies = true;
            TestEqual(TEXT("Three simple trunk collision segments per actual tree"), Proxy->GetInstanceCount(), 720);
            TestFalse(TEXT("Collision proxies never render"), Proxy->IsVisible());
            TestEqual(TEXT("Simple trunk proxies block collision"), Proxy->GetCollisionEnabled(), ECollisionEnabled::QueryAndPhysics);
        }
    TestTrue(TEXT("Simple trunk collision proxy batch exists"), bFoundProxies);
    TestTrue(TEXT("Tree geometry stays within the vertex allocation budget"),TotalVertices > 100000 && TotalVertices <= 4800000);
    TestTrue(TEXT("Tree geometry stays within the triangle allocation budget"),TotalTriangles > 100000 && TotalTriangles <= 3200000);
    TestTrue(TEXT("Real branch hierarchy exists"),BarkTriangles > 50000);
    TestTrue(TEXT("Dense needle cards use four triangles per two-plane spray"),NeedleTriangles > 200000 && NeedleTriangles <= 2200000 && NeedleTriangles % 4 == 0);
    TestEqual(TEXT("All positions and lighting normals are finite and normalized"),InvalidVertices,0);
    TestEqual(TEXT("All triangles reference existing vertices"),InvalidIndices,0);
    TestEqual(TEXT("All triangles have nonzero area"),DegenerateTriangles,0);
    TestEqual(TEXT("Unreal clockwise faces agree with outward shading normals"),ReversedFaces,0);
    AddInfo(FString::Printf(TEXT("Actual coastal tree geometry: %d vertices / %d triangles; bark %d / needles %d"),TotalVertices,TotalTriangles,BarkTriangles,NeedleTriangles));
    return true;
}
#endif
