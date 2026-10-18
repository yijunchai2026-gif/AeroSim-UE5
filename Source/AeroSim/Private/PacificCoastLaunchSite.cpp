#include "PacificCoastGraybox.h"

#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace CoastalLaunch
{
struct FMesh final
{
    TArray<FVector> Vertices;
    TArray<FVector2D> UV;
    TArray<FLinearColor> Colors;
    TArray<int32> Indices;
    int32 Vertex(FVector P, FVector2D Tex = FVector2D::ZeroVector, FLinearColor Color = FLinearColor::White)
    {
        UV.Add(Tex); Colors.Add(Color); return Vertices.Add(P);
    }
};

FLinearColor GroundColor(const FVector& P)
{
    return FMath::Lerp(FLinearColor(1.08F,.86F,.55F), FLinearColor(.25F,.48F,.31F), APacificCoastGraybox::GetCoastalVegetationCover(P.X,P.Y));
}

void Cylinder(FMesh& Mesh, FVector Bottom, FVector Top, float Radius, int32 Sides = 12)
{
    const FVector Axis = (Top-Bottom).GetSafeNormal();
    const FVector Across = FVector::CrossProduct(Axis, FMath::Abs(Axis.Z) > .9F ? FVector::ForwardVector : FVector::UpVector).GetSafeNormal();
    const FVector Other = FVector::CrossProduct(Axis, Across);
    const int32 Base = Mesh.Vertices.Num();
    for (int32 Ring = 0; Ring < 2; ++Ring)
        for (int32 Side = 0; Side < Sides; ++Side)
        {
            const float A = Side * (2.0F*PI/Sides);
            Mesh.Vertex((Ring == 0 ? Bottom : Top) + Radius*(Across*FMath::Cos(A)+Other*FMath::Sin(A)));
        }
    const int32 Low = Mesh.Vertex(Bottom), High = Mesh.Vertex(Top);
    for (int32 Side = 0; Side < Sides; ++Side)
    {
        const int32 Next = (Side+1)%Sides, A=Base+Side, B=Base+Next, C=A+Sides, D=B+Sides;
        Mesh.Indices.Append({A,C,B, B,C,D, Low,A,B, High,D,C});
    }
}

void CaseBody(FMesh& Mesh, FVector Center, float Length, float Width, float Height)
{
    const FVector2D Corners[] = {{-1,-.78F},{-.85F,-1},{.85F,-1},{1,-.78F},{1,.78F},{.85F,1},{-.85F,1},{-1,.78F}};
    const int32 Base = Mesh.Vertices.Num();
    for (int32 Ring=0;Ring<4;++Ring)
    {
        const float Bevel=FMath::Min(2.5F,Height*.3F);
        const float Z[] = {0,Bevel,Height-Bevel,Height};
        const float Scale = Ring==0 || Ring==3 ? .94F : 1.0F;
        for (const FVector2D& Corner : Corners) Mesh.Vertex(Center+FVector(Corner.X*Length*.5F*Scale,Corner.Y*Width*.5F*Scale,Z[Ring]));
    }
    const int32 Bottom=Mesh.Vertex(Center), Top=Mesh.Vertex(Center+FVector(0,0,Height));
    for (int32 Side=0;Side<8;++Side)
    {
        const int32 Next=(Side+1)%8;
        Mesh.Indices.Append({Bottom,Base+Side,Base+Next, Top,Base+24+Next,Base+24+Side});
        for(int32 Ring=0;Ring<3;++Ring)
        {
            const int32 A=Base+Ring*8+Side,B=Base+Ring*8+Next,C=A+8,D=B+8;
            Mesh.Indices.Append({A,C,B,B,C,D});
        }
    }
}

FVector PathPoint(float T)
{
    const FVector A(-400,350,0), B(-4000,2500,0), C(-10500,5000,0), D(-14000,7290,0);
    const float U=1-T;
    return U*U*U*A+3*U*U*T*B+3*U*T*T*C+T*T*T*D;
}
}

void APacificCoastGraybox::BuildCoastalLaunchSite(UStaticMesh* CubeMesh)
{
    using namespace CoastalLaunch;
    (void)CubeMesh;
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Solid(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    const UMeshComponent* Land=Cast<UMeshComponent>(GetDefaultSubobjectByName(TEXT("CoastalPlateau")));
    UMaterialInterface* Ground=CoastalGravelMaterial ? CoastalGravelMaterial.Get() : (Land && Land->GetMaterial(0) ? Land->GetMaterial(0) : Solid.Object.Get());
    auto Emit=[this](const TCHAR* Name,FMesh& Data,UMaterialInterface* Material,FLinearColor Color,bool Collision)
    {
        TArray<FVector> Normals;Normals.Init(FVector::ZeroVector,Data.Vertices.Num());
        for(int32 Index=0;Index<Data.Indices.Num();Index+=3)
        {
            const int32 A=Data.Indices[Index],B=Data.Indices[Index+1],C=Data.Indices[Index+2];
            const FVector N=FVector::CrossProduct(Data.Vertices[C]-Data.Vertices[A],Data.Vertices[B]-Data.Vertices[A]);
            Normals[A]+=N;Normals[B]+=N;Normals[C]+=N;
        }
        for(FVector& Normal:Normals)Normal.Normalize();
        UProceduralMeshComponent* Mesh=CreateDefaultSubobject<UProceduralMeshComponent>(Name);
        Mesh->SetupAttachment(Root);Mesh->bUseAsyncCooking=true;
        Mesh->SetCollisionProfileName(Collision?TEXT("BlockAll"):TEXT("NoCollision"));
        Mesh->CreateMeshSection_LinearColor(0,Data.Vertices,Data.Indices,Normals,Data.UV,Data.Colors,TArray<FProcMeshTangent>(),Collision);
        Mesh->SetMaterial(0,Material);Mesh->SetCastShadow(Collision || FString(Name).Contains(TEXT("Windsock")));Mesh->SetCastContactShadow(Collision);
        CoastalColorParts.Add(Mesh);CoastalPartColors.Add(Color);++GeneratedPrimitiveCount;
    };

    // A field team's 8 x 10 m compacted working patch blends back into the existing scanned ground.
    FMesh Apron;
    constexpr int32 Sectors=64,Rings=10;
    Apron.Vertex(FVector(0,0,GetCoastalLandHeight(0,0)+1.1F),FVector2D::ZeroVector,FLinearColor(1.08F,.86F,.55F,1));
    for(int32 Ring=1;Ring<=Rings;++Ring)
        for(int32 Sector=0;Sector<Sectors;++Sector)
        {
            const float Angle=Sector*(2.0F*PI/Sectors),R=Ring/static_cast<float>(Rings);
            const float EdgeVariation=1+.055F*FMath::Sin(Angle*3+.7F)+.035F*FMath::Sin(Angle*7);
            FVector P(FMath::Cos(Angle)*400*R*EdgeVariation,FMath::Sin(Angle)*500*R*EdgeVariation,0);
            P.Z=GetCoastalLandHeight(P.X,P.Y)+FMath::Lerp(1.1F,-.20F,FMath::SmoothStep(.90F,1.0F,R));
            const float Compacted=1-FMath::SmoothStep(.45F,.88F,R);
            FLinearColor C=GroundColor(P);C.A=Compacted;
            Apron.Vertex(P,FVector2D(P.X/4000,P.Y/4000),C);
        }
    for(int32 Sector=0;Sector<Sectors;++Sector)Apron.Indices.Append({0,1+(Sector+1)%Sectors,1+Sector});
    for(int32 Ring=0;Ring<Rings-1;++Ring)
        for(int32 Sector=0;Sector<Sectors;++Sector)
        {
            const int32 Next=(Sector+1)%Sectors,A=1+Ring*Sectors+Sector,B=1+Ring*Sectors+Next,C=A+Sectors,D=B+Sectors;
            Apron.Indices.Append({A,B,C,B,D,C});
        }
    Emit(TEXT("CoastalLaunchApron"),Apron,Ground,FLinearColor::White,false);

    // The two-meter flexible launch mat covers the actual spawn XY; there is no raised concrete platform.
    FMesh Mat,MatRing;
    const FVector MatCenter(55,79,0);
    const int32 Center=Mat.Vertex(MatCenter+FVector(0,0,1.8F));
    for(int32 Side=0;Side<Sectors;++Side)
    {
        const float Angle=Side*(2.0F*PI/Sectors);
        Mat.Vertex(MatCenter+FVector(FMath::Cos(Angle)*100,FMath::Sin(Angle)*100,1.8F));
        MatRing.Vertex(MatCenter+FVector(FMath::Cos(Angle)*87,FMath::Sin(Angle)*87,1.84F));
        MatRing.Vertex(MatCenter+FVector(FMath::Cos(Angle)*92,FMath::Sin(Angle)*92,1.84F));
    }
    for(int32 Side=0;Side<Sectors;++Side)
    {
        const int32 Next=(Side+1)%Sectors;
        Mat.Indices.Append({Center,Next+1,Side+1});
        MatRing.Indices.Append({Side*2,Next*2,Side*2+1, Side*2+1,Next*2,Next*2+1});
    }
    Emit(TEXT("CoastalLaunchMat"),Mat,Solid.Object,FLinearColor(.045F,.055F,.052F),false);
    Emit(TEXT("CoastalLaunchMatGuide"),MatRing,Solid.Object,FLinearColor(.22F,.13F,.04F),false);

    // Continuous terrain-following access to the existing woodland trail; the final meters overlap and blend.
    FMesh Trail;
    constexpr int32 TrailRows=181,TrailColumns=7;
    const float Lateral[TrailColumns]={-1,-.8F,-.45F,0,.45F,.8F,1};
    for(int32 Row=0;Row<TrailRows;++Row)
    {
        const float T=Row/static_cast<float>(TrailRows-1);
        const FVector CenterPoint=PathPoint(T),Direction=(PathPoint(FMath::Min(1.0F,T+.001F))-PathPoint(FMath::Max(0.0F,T-.001F))).GetSafeNormal();
        const FVector Across(-Direction.Y,Direction.X,0);
        const float Fade=(1-FMath::SmoothStep(.965F,1.0F,T))*FMath::SmoothStep(0.0F,.025F,T);
        const float HalfWidth=75+8*FMath::Sin(T*31)+4*FMath::Sin(T*83);
        for(int32 Column=0;Column<TrailColumns;++Column)
        {
            const float Edge=FMath::Abs(Lateral[Column]);
            FVector P=CenterPoint+Across*(Lateral[Column]*HalfWidth);
            P.Z=GetCoastalLandHeight(P.X,P.Y)+FMath::Lerp(.85F,-.25F,Edge*Edge);
            FLinearColor C=GroundColor(P);C.A=(1-FMath::SmoothStep(.35F,1.0F,Edge))*Fade;
            Trail.Vertex(P,FVector2D(P.X/4000,P.Y/4000),C);
        }
    }
    for(int32 Row=0;Row<TrailRows-1;++Row)
        for(int32 Column=0;Column<TrailColumns-1;++Column)
        {
            const int32 A=Row*TrailColumns+Column,B=A+1,C=A+TrailColumns,D=C+1;
            Trail.Indices.Append({A,B,C,B,D,C});
        }
    Emit(TEXT("CoastalLaunchAccessTrail"),Trail,Ground,FLinearColor::White,false);

    // Two restrained carry cases: beveled shells, lid seam and supported handles at the rear working edge.
    FMesh Cases,CaseLids,Hardware;
    const FVector CasePositions[]={{-370,320,0},{-440,285,0}};
    for(int32 Index=0;Index<2;++Index)
    {
        FVector P=CasePositions[Index];P.Z=GetCoastalLandHeight(P.X,P.Y)-.4F;
        const float L=Index==0?60:46,W=Index==0?40:32,H=Index==0?30:23;
        CaseBody(Cases,P,L,W,H-5);
        CaseBody(CaseLids,P+FVector(0,0,H-5),L+.3F,W+.3F,5);
        Cylinder(Hardware,P+FVector(-10,0,H),P+FVector(-10,0,H+5),1.2F,8);
        Cylinder(Hardware,P+FVector(10,0,H),P+FVector(10,0,H+5),1.2F,8);
        Cylinder(Hardware,P+FVector(-10,0,H+5),P+FVector(10,0,H+5),1.5F,8);
    }
    Emit(TEXT("CoastalInspectionCases"),Cases,Solid.Object,FLinearColor(.055F,.065F,.065F),true);
    Emit(TEXT("CoastalInspectionCaseLids"),CaseLids,Solid.Object,FLinearColor(.075F,.085F,.082F),true);
    Emit(TEXT("CoastalInspectionCaseHandles"),Hardware,Solid.Object,FLinearColor(.13F,.14F,.14F),false);

    // A small supported fabric windsock makes the coastal working site's purpose legible.
    const FVector MastFoot(-475,430,GetCoastalLandHeight(-475,430));
    const FVector Mouth=MastFoot+FVector(35,0,300),Wind(0,1,0),Across(1,0,0);
    FMesh Mast,Anchor;
    Cylinder(Mast,MastFoot+FVector(0,0,-15),MastFoot+FVector(0,0,305),2.8F);
    Cylinder(Mast,MastFoot+FVector(0,0,300),MastFoot+FVector(35,0,300),1.5F,8);
    Cylinder(Mast,Mouth,Mouth+FVector(0,0,19),.75F,6);
    for(int32 Side=0;Side<20;++Side)
    {
        const float A=Side*(2*PI/20),B=(Side+1)*(2*PI/20);
        Cylinder(Mast,Mouth+FVector(19*FMath::Cos(A),0,19*FMath::Sin(A)),Mouth+FVector(19*FMath::Cos(B),0,19*FMath::Sin(B)),.5F,6);
    }
    Cylinder(Anchor,MastFoot+FVector(0,0,-7),MastFoot+FVector(0,0,3),18,16);
    Emit(TEXT("CoastalWindMast"),Mast,Solid.Object,FLinearColor(.19F,.21F,.20F),true);
    Emit(TEXT("CoastalWindMastFoot"),Anchor,Solid.Object,FLinearColor(.23F,.23F,.20F),true);
    FMesh Orange,Canvas;
    constexpr int32 SockRings=9,SockSides=20;
    for(int32 Shell=0;Shell<2;++Shell)
        for(int32 Ring=0;Ring<SockRings;++Ring)
            for(int32 Side=0;Side<SockSides;++Side)
            {
                const float T=Ring/static_cast<float>(SockRings-1),Angle=Side*(2*PI/SockSides);
                const float Radius=FMath::Lerp(19.0F,7.5F,T)-(Shell==1?.35F:0);
                const FVector P=Mouth+Wind*(T*115)+FVector(0,0,-18*T*T)+Radius*(Across*FMath::Cos(Angle)+FVector::UpVector*FMath::Sin(Angle));
                Orange.Vertex(P);Canvas.Vertex(P);
            }
    for(int32 Shell=0;Shell<2;++Shell)
        for(int32 Ring=0;Ring<SockRings-1;++Ring)
            for(int32 Side=0;Side<SockSides;++Side)
            {
                const int32 Base=Shell*SockRings*SockSides,A=Base+Ring*SockSides+Side,B=Base+Ring*SockSides+(Side+1)%SockSides,C=A+SockSides,D=B+SockSides;
                TArray<int32>& Faces=(Ring/2)%2==0?Orange.Indices:Canvas.Indices;
                if(Shell==0)Faces.Append({A,B,C,B,D,C});else Faces.Append({A,C,B,B,C,D});
            }
    Emit(TEXT("CoastalWindsockOrange"),Orange,Solid.Object,FLinearColor(.64F,.20F,.055F),false);
    Emit(TEXT("CoastalWindsockCanvas"),Canvas,Solid.Object,FLinearColor(.68F,.66F,.56F),false);
}
