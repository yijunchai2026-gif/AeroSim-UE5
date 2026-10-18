#include "PacificCoastGraybox.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
// Batches are material groups, not individual trees: geometry is generated only once.
struct FCoastalTreeMesh
{
    TArray<FVector> Positions;
    TArray<int32> Indices;
    TArray<FVector> Normals;
    TArray<FVector2D> UV;
    TArray<FProcMeshTangent> Tangents;

    void Face(int32 A, int32 B, int32 C)
    {
        Indices.Append({A, B, C});
    }

    void Tube(const FVector& Start, const FVector& End, float BaseRadius, float TipRadius, int32 Sides)
    {
        const FVector Axis = (End - Start).GetSafeNormal();
        FVector U, V;
        Axis.FindBestAxisVectors(U, V);
        const int32 Offset = Positions.Num();
        for (int32 Ring = 0; Ring < 2; ++Ring)
        {
            for (int32 Side = 0; Side < Sides; ++Side)
            {
                const float Angle = Side * 2.0F * PI / Sides;
                const FVector Radial = U * FMath::Cos(Angle) + V * FMath::Sin(Angle);
                Positions.Add((Ring == 0 ? Start : End) + Radial * (Ring == 0 ? BaseRadius : TipRadius));
                Normals.Add((Radial + Axis * ((BaseRadius - TipRadius) / FVector::Distance(Start, End))).GetSafeNormal());
                // Rough bark tiles in physical units instead of stretching one
                // square texture over the entire height of a 30 m trunk.
                UV.Add(FVector2D(Side / static_cast<float>(Sides) * FMath::Max(2.0F * PI * BaseRadius / 180.0F, .25F),
                    Start.Z / 220.0 + Ring * FVector::Distance(Start,End) / 220.0));
                const FVector Tangent = (-U * FMath::Sin(Angle) + V * FMath::Cos(Angle)).GetSafeNormal();
                Tangents.Add(FProcMeshTangent(Tangent,FVector::DotProduct(FVector::CrossProduct(Normals.Last(),Tangent),Axis) < 0));
            }
        }
        // Enforce outward normals even if FindBestAxisVectors changes basis handedness.
        for (int32 Side = 0; Side < Sides; ++Side)
        {
            int32 A = Offset + Side, B = Offset + (Side + 1) % Sides, C = A + Sides, D = B + Sides;
            if (FVector::DotProduct(FVector::CrossProduct(Positions[C] - Positions[A], Positions[B] - Positions[A]), Normals[A]) < 0)
            {
                Face(A, C, B); Face(B, C, D);
            }
            else { Face(A, B, C); Face(B, D, C); }
        }
        const int32 Cap = Positions.Num();
        Positions.Add(End); Normals.Add(Axis); UV.Add(FVector2D(.5F,.5F)); Tangents.Add(FProcMeshTangent(U,false));
        for (int32 Side = 0; Side < Sides; ++Side)
        {
            const int32 A = Offset + Sides + Side, B = Offset + Sides + (Side + 1) % Sides;
            if (FVector::DotProduct(FVector::CrossProduct(Positions[B] - End, Positions[A] - End), Axis) > 0) Face(Cap, A, B);
            else Face(Cap, B, A);
        }
    }

    // Two crossing twig cards use a verified isolated needle atlas island. The
    // masked two-sided material reveals individual photographed needles and sky gaps.
    void Spray(const FVector& Center, const FVector& Direction, const FVector& Size, FRandomStream& Random)
    {
        const FVector Along = (Direction + FVector::UpVector * Random.FRandRange(.05F,.30F)).GetSafeNormal();
        FVector Across, Vertical;
        Along.FindBestAxisVectors(Across,Vertical);
        // A twig is at most 1.3 m long, retaining credible needle scale close up.
        const float Length = FMath::Clamp(static_cast<float>(Size.X) * .82F,65.0F,130.0F);
        const float RollOffset = Random.FRandRange(0,PI);
        for (int32 Plane = 0; Plane < 2; ++Plane)
        {
            const float Roll = RollOffset + Plane * PI * .5F;
            const FVector WidthAxis = Across * FMath::Cos(Roll) + Vertical * FMath::Sin(Roll);
            const FVector Origin = Center + Along * (Plane-.5F) * Length * .12F;
            const FVector HalfLength = Along * Length * .5F, HalfWidth = WidthAxis * Length * .25F;
            const int32 Offset = Positions.Num();
            Positions.Append({Origin-HalfLength-HalfWidth,Origin+HalfLength-HalfWidth,
                Origin+HalfLength+HalfWidth,Origin-HalfLength+HalfWidth});
            UV.Append({FVector2D(0,1),FVector2D(0,0),FVector2D(1,0),FVector2D(1,1)});
            const FVector Normal = FVector::CrossProduct(WidthAxis,Along).GetSafeNormal();
            for (int32 Corner = 0; Corner < 4; ++Corner)
            {
                Normals.Add(Normal);
                Tangents.Add(FProcMeshTangent(WidthAxis,true));
            }
            Face(Offset,Offset+1,Offset+2); Face(Offset,Offset+2,Offset+3);
        }
    }
};
}

void APacificCoastGraybox::BuildCoastalTrees(UMaterialInterface* Material)
{
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    UHierarchicalInstancedStaticMeshComponent* TrunkProxies = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("CoastalTrunkCollisionProxies"));
    TrunkProxies->SetupAttachment(Root);
    TrunkProxies->SetStaticMesh(CylinderFinder.Object);
    TrunkProxies->SetCollisionProfileName(TEXT("BlockAll"));
    TrunkProxies->SetHiddenInGame(true);
    TrunkProxies->SetVisibility(false);
    TrunkProxies->SetCastShadow(false);
    // Three primitive cylinders approximate each bent trunk. Rendered branches and
    // needles never enter triangle collision cooking or the physics broad phase.
    TrunkProxies->ComponentTags.Add(TEXT("CoastalTrunkProxiesPerTree|3"));
    FCoastalTreeMesh Batches[5];
    FRandomStream Random(21630);
    TArray<FVector2D> TreeBases;
    TArray<FVector> TreeOrigins;
    // Candidate rejection is bounded. Compact irregular groves connect neighboring
    // crowns while retaining openings and sparse trees at the forest edge.
    for (int32 Attempt = 0; Attempt < 6000 && CoastalTreeCount < 240; ++Attempt)
    {
        const float X = Random.FRandRange(-28000.0F, 10300.0F);
        const float Y = (Attempt % 2 == 0 ? -1.0F : 1.0F) * Random.FRandRange(7000.0F, 25000.0F);
        const float GroveDensity = .17F + .83F * FMath::Square(.5F + .5F * FMath::Sin(X / 6500.0F + FMath::Sin(Y / 4400.0F)));
        if (Random.FRand() > GroveDensity) continue;
        if (Y > 0 && X > -17200 && X < -1700 && FMath::Abs(Y - (8500.0F + .325F * (X + 9000.0F))) < 2100.0F) continue;
        bool bTooClose = false;
        for (const FVector2D& Other : TreeBases)
        {
            if (FVector2D::DistSquared(Other, FVector2D(X,Y)) < FMath::Square(950.0F)) { bTooClose = true; break; }
        }
        if (bTooClose) continue;
        TreeBases.Add(FVector2D(X,Y));
        const FVector Base(X, Y, GetCoastalLandHeight(X,Y));
        TreeOrigins.Add(Base);
        const bool bWindPine = CoastalTreeCount % 5 == 0;
        const float Height = Random.FRandRange(bWindPine ? 1300.0F : 1800.0F, bWindPine ? 2300.0F : 3400.0F);
        const float Yaw = Random.FRandRange(0, 2.0F * PI);
        const FVector Bend = FVector(FMath::Cos(Yaw), FMath::Sin(Yaw), 0) * Height * (bWindPine ? .13F : .025F);
        const float Radius = Height * Random.FRandRange(.013F, .020F);
        auto Spine = [Base, Bend, Height](float T) { return Base + FVector(0,0,Height * T) + Bend * T * T; };
        for (int32 Segment = 0; Segment < 3; ++Segment)
        {
            const float T0 = Segment / 3.0F, T1 = (Segment + 1) / 3.0F;
            const FVector Start = Spine(T0), End = Spine(T1), Axis = End-Start;
            const float ProxyRadius = Radius * FMath::Pow(1.0F-T0,.75F) + 2;
            TrunkProxies->AddInstance(FTransform(FQuat::FindBetweenNormals(FVector::UpVector,Axis.GetSafeNormal()),
                (Start+End)*.5, FVector(ProxyRadius*.02F,ProxyRadius*.02F,Axis.Size()*.01F)));
        }
        for (int32 Segment = 0; Segment < 7; ++Segment)
        {
            const float T0 = Segment / 7.0F, T1 = (Segment + 1) / 7.0F;
            Batches[0].Tube(Spine(T0) - (Segment == 0 ? FVector(0,0,18) : FVector::ZeroVector), Spine(T1),
                Radius * FMath::Pow(1.0F - T0, .75F) + 2, Radius * FMath::Pow(1.0F - T1, .75F) + 2, 7);
        }
        for (int32 RootIndex = 0; RootIndex < 4; ++RootIndex)
        {
            const float Angle = Yaw + RootIndex * PI * .5F + Random.FRandRange(-.3F,.3F);
            const FVector Foot = Base + FVector(FMath::Cos(Angle), FMath::Sin(Angle),0) * Radius * 4.5F;
            const FVector Anchored(Foot.X, Foot.Y, GetCoastalLandHeight(Foot.X,Foot.Y) - Radius * .3F);
            Batches[0].Tube(Spine(.025F), Anchored, Radius * .65F, Radius * .12F, 5);
        }
        const int32 Tiers = Random.RandRange(bWindPine ? 4 : 7, bWindPine ? 6 : 9);
        const float CrownAsymmetry = Random.FRandRange(.08F,.23F);
        const float CrownYaw = Random.FRandRange(0,2.0F*PI);
        for (int32 Tier = 0; Tier < Tiers; ++Tier)
        {
            const float TierT = FMath::Lerp(bWindPine ? .43F : .22F,.91F,Tier/static_cast<float>(Tiers-1));
            const int32 Branches = Random.RandRange(bWindPine ? 3 : 4,bWindPine ? 4 : 5);
            for (int32 Branch = 0; Branch < Branches; ++Branch)
            {
                if (Random.FRand() < .06F) continue;
                const float T = TierT + Random.FRandRange(-.024F,.024F);
                const float Angle = Yaw + Branch * 2.0F * PI / Branches + Tier * 2.39996F + Random.FRandRange(-.44F,.44F);
                const FVector Direction(FMath::Cos(Angle), FMath::Sin(Angle),0);
                const float Envelope = bWindPine ? FMath::Sin((T - .3F) * PI) : FMath::Pow(1.0F - T, .59F);
                const float Length = Height * .29F * Envelope * Random.FRandRange(.83F,1.15F)
                    * (1.0F + CrownAsymmetry * FMath::Cos(Angle-CrownYaw));
                const FVector Start = Spine(T);
                const FVector Elbow = Start + Direction * Length * .55F - FVector(0,0,Length * .13F);
                const FVector Tip = Start + Direction * Length + FVector(0,0,Length * .04F);
                Batches[0].Tube(Start, Elbow, Radius * .27F * (1.0F-T), Radius * .10F, 5);
                Batches[0].Tube(Elbow, Tip, Radius * .10F, 1.8F, 5);
                auto BranchPoint = [Start,Elbow,Tip](float Along)
                {
                    return Along < .55F ? FMath::Lerp(Start,Elbow,Along/.55F)
                        : FMath::Lerp(Elbow,Tip,(Along-.55F)/.45F);
                };
                const FVector Across(-Direction.Y,Direction.X,0);
                constexpr int32 ShootCount = 6;
                FVector SecondaryStarts[ShootCount], SecondaryEnds[ShootCount], SecondaryDirections[ShootCount];
                for (int32 Shoot = 0; Shoot < ShootCount; ++Shoot)
                {
                    const float Side = Shoot % 2 == 0 ? -1.0F : 1.0F;
                    SecondaryStarts[Shoot] = BranchPoint(.24F+(Shoot/2)*.24F);
                    SecondaryDirections[Shoot] = (Direction*.42F+Across*Side*.86F
                        +FVector::UpVector*Random.FRandRange(.03F,.15F)).GetSafeNormal();
                    const float ShootLength = FMath::Clamp(Length*Random.FRandRange(.35F,.54F)*(1.0F-(Shoot/2)*.12F),75.0F,260.0F);
                    SecondaryEnds[Shoot] = SecondaryStarts[Shoot]+SecondaryDirections[Shoot]*ShootLength;
                    Batches[0].Tube(SecondaryStarts[Shoot],SecondaryEnds[Shoot],FMath::Clamp(Radius*.055F,1.8F,3.8F),.7F,4);
                }
                // Dense needle-bearing secondary shoots form the bough, rather
                // than a few isolated tufts at the ends of long bare branches.
                const int32 TwigsPerShoot = Length > 300.0F ? 8 : 5;
                const int32 SecondaryTwigs = ShootCount * TwigsPerShoot;
                const int32 TwigCount = SecondaryTwigs + 8;
                for (int32 Twig = 0; Twig < TwigCount; ++Twig)
                {
                    FVector Attachment, TwigDirection;
                    if (Twig < SecondaryTwigs)
                    {
                        const int32 Shoot = Twig % ShootCount;
                        const float Fraction = FMath::Clamp((Twig/ShootCount+.5F)/static_cast<float>(TwigsPerShoot)
                            +Random.FRandRange(-.04F,.04F),.08F,1.0F);
                        Attachment = FMath::Lerp(SecondaryStarts[Shoot],SecondaryEnds[Shoot],Fraction);
                        TwigDirection = (SecondaryDirections[Shoot]*.72F+Direction*.35F
                            +Across*Random.FRandRange(-.72F,.72F)
                            +FVector::UpVector*Random.FRandRange(-.24F,.35F)).GetSafeNormal();
                    }
                    else
                    {
                        const float Fraction = (Twig-SecondaryTwigs+.5F)/8.0F;
                        const float Along = .18F+.80F*FMath::Sqrt(Fraction)+Random.FRandRange(-.035F,.035F);
                        Attachment = BranchPoint(Along);
                        TwigDirection = (Direction+Across*Random.FRandRange(-.85F,.85F)).GetSafeNormal();
                    }
                    const float TwigLength = Random.FRandRange(80.0F,130.0F);
                    const int32 Shade = 1 + ((CoastalTreeCount + Tier + Branch + Twig / 2) % 4);
                    Batches[Shade].Spray(Attachment+TwigDirection*TwigLength*.40F,TwigDirection,
                        FVector(TwigLength/ .82F,0,0),Random);
                }
            }
        }
        // The leader retains needles but never closes the canopy into a cone.
        for (int32 Leader = 0; Leader < 6; ++Leader)
        {
            const FVector Direction(FMath::Cos(Yaw+Leader*2.1F), FMath::Sin(Yaw+Leader*2.1F),0);
            Batches[2].Spray(Spine(.85F + Leader * .027F)+Direction*35.0F, Direction,
                FVector(Height*.038F,Height*.033F,Height*.055F), Random);
        }
        ++CoastalTreeCount;
    }
    // Fewer, fuller distant pine groups follow sheltered drainage shoulders.
    // Real twig cards form overlapping boughs; no whole-tree billboard or solid crown.
    FRandomStream DistantRandom(918437);
    int32 DistantCount=0;
    TArray<FVector2D> DistantBases;
    FVector2D GroveCenter=FVector2D::ZeroVector;
    int32 GroveRemaining=0;
    for(int32 Attempt=0;Attempt<18000 && DistantCount<260;++Attempt)
    {
        if(GroveRemaining<=0)
        {
            GroveCenter=FVector2D(DistantRandom.FRandRange(-168000,-43000),DistantRandom.FRandRange(-105000,105000));
            GroveRemaining=DistantRandom.RandRange(10,17);
        }
        const float GroveAngle=DistantRandom.FRandRange(0,2*PI);
        const float GroveRadius=FMath::Sqrt(DistantRandom.FRand())*2200.0F;
        const float X=GroveCenter.X+FMath::Cos(GroveAngle)*GroveRadius,Y=GroveCenter.Y+FMath::Sin(GroveAngle)*GroveRadius;
        const float Cover=GetCoastalVegetationCover(X,Y);
        if(Cover<.62F || DistantRandom.FRand()>Cover*Cover*Cover){--GroveRemaining;continue;}
        bool Close=false;
        for(const FVector2D& Other:DistantBases)if(FVector2D::DistSquared(Other,FVector2D(X,Y))<FMath::Square(700.0F)){Close=true;break;}
        if(Close){--GroveRemaining;continue;}
        const FVector Base(X,Y,GetCoastalLandHeight(X,Y));
        if(Base.Z<200){--GroveRemaining;continue;}
        DistantBases.Add(FVector2D(X,Y));
        const float Height=DistantRandom.FRandRange(1100,1900),Yaw=DistantRandom.FRandRange(0,2*PI);
        Batches[0].Tube(Base-FVector(0,0,20),Base+FVector(Height*.025F,0,Height),Height*.016F,2,5);
        const float CrownYaw=DistantRandom.FRandRange(0,2*PI);
        for(int32 Tier=0;Tier<6;++Tier)
        {
            const int32 Branches=DistantRandom.RandRange(4,5);
            for(int32 Branch=0;Branch<Branches;++Branch)
            {
                const float T=.18F+Tier*.142F+DistantRandom.FRandRange(-.04F,.04F);
                const float Angle=Yaw+Branch*2*PI/Branches+Tier*2.39996F+DistantRandom.FRandRange(-.43F,.43F);
                const FVector Direction(FMath::Cos(Angle),FMath::Sin(Angle),DistantRandom.FRandRange(-.08F,.13F));
                const FVector Across(-Direction.Y,Direction.X,0);
                const FVector Start=Base+FVector(Height*.025F*T,0,Height*T);
                const float Length=Height*.40F*FMath::Pow(1-T,.57F)
                    *(1.0F+.18F*FMath::Cos(Angle-CrownYaw))*DistantRandom.FRandRange(.88F,1.08F);
                Batches[0].Tube(Start,Start+Direction*Length,4,1,3);
                FVector Shoots[2],ShootAxes[2],ShootEnds[2];
                for(int32 Shoot=0;Shoot<2;++Shoot)
                {
                    Shoots[Shoot]=Start+Direction*Length*(.33F+Shoot*.28F);
                    ShootAxes[Shoot]=(Direction*.40F+Across*(Shoot==0?-.88F:.88F)
                        +FVector::UpVector*DistantRandom.FRandRange(.03F,.15F)).GetSafeNormal();
                    ShootEnds[Shoot]=Shoots[Shoot]+ShootAxes[Shoot]*FMath::Clamp(Length*.40F,90.0F,240.0F);
                    Batches[0].Tube(Shoots[Shoot],ShootEnds[Shoot],2.5F,.7F,3);
                }
                for(int32 Twig=0;Twig<12;++Twig)
                {
                    FVector Attachment,TwigDirection;
                    if(Twig<8)
                    {
                        const int32 Shoot=Twig%2;
                        Attachment=FMath::Lerp(Shoots[Shoot],ShootEnds[Shoot],(Twig/2+.6F)/4.0F);
                        TwigDirection=(ShootAxes[Shoot]*.75F+Direction*.35F+Across*DistantRandom.FRandRange(-.25F,.25F)).GetSafeNormal();
                    }
                    else
                    {
                        const float Along=.28F+.68F*FMath::Sqrt((Twig-7.5F)/4.0F);
                        Attachment=Start+Direction*Length*Along;
                        TwigDirection=(Direction+Across*DistantRandom.FRandRange(-.8F,.8F)).GetSafeNormal();
                    }
                    Batches[1+(DistantCount+Tier+Branch+Twig/3)%4].Spray(Attachment+TwigDirection*45.0F,
                        TwigDirection,FVector(DistantRandom.FRandRange(135,158),0,0),DistantRandom);
                }
            }
        }
        for(int32 Leader=0;Leader<3;++Leader)
        {
            const float Angle=Yaw+Leader*2.1F;
            const FVector Direction(FMath::Cos(Angle),FMath::Sin(Angle),.25F);
            Batches[2].Spray(Base+FVector(Height*.025F,0,Height*(.89F+Leader*.038F))+Direction*35,
                Direction,FVector(150,0,0),DistantRandom);
        }
        ++DistantCount;
        --GroveRemaining;
    }
    // Reallocate part of the far-tree budget to supported, low conifer regrowth
    // at the forest edge. These are draft needle plants, not an oak/shrub asset claim.
    TArray<FVector> UnderstoryOrigins;
    TArray<FVector2D> ProtectedTrail;
    const FVector2D WalkCenter(-9000,8500);
    const FVector2D WalkAlong(FMath::Cos(FMath::DegreesToRadians(18.0F)),FMath::Sin(FMath::DegreesToRadians(18.0F)));
    const FVector2D WalkAcross(-WalkAlong.Y,WalkAlong.X);
    for(int32 Side=-1;Side<=1;Side+=2)
        for(int32 Row=0;Row<=120;++Row)
        {
            const float Distance=1520+Row*55;
            const float Bend=FMath::Square(FMath::Max(0.0F,Distance-2200)/6000.0F)*1600;
            ProtectedTrail.Add(WalkCenter+WalkAlong*(Side*Distance)+WalkAcross*Bend);
        }
    const FVector2D LaunchPath[]={FVector2D(-400,350),FVector2D(-4000,2500),FVector2D(-10500,5000),FVector2D(-14000,6900)};
    auto SegmentDistance=[](const FVector2D& P,const FVector2D& A,const FVector2D& B)
    {
        const FVector2D Delta=B-A;
        const double T=FMath::Clamp(FVector2D::DotProduct(P-A,Delta)/Delta.SizeSquared(),0.0,1.0);
        return FVector2D::Distance(P,A+Delta*T);
    };
    FRandomStream UnderstoryRandom(47623);
    for(int32 Attempt=0;Attempt<8000 && UnderstoryOrigins.Num()<480 && !TreeOrigins.IsEmpty();++Attempt)
    {
        const FVector Parent=TreeOrigins[UnderstoryRandom.RandRange(0,TreeOrigins.Num()-1)];
        const float Angle=UnderstoryRandom.FRandRange(0,2*PI),Distance=UnderstoryRandom.FRandRange(260,900);
        const FVector2D XY(Parent.X+FMath::Cos(Angle)*Distance,Parent.Y+FMath::Sin(Angle)*Distance);
        bool Clear=XY.Size()>1150 && XY.X<GetCliffEdgeX(XY.Y)-500;
        for(const FVector& Tree:TreeOrigins)
            if(FVector2D::DistSquared(XY,FVector2D(Tree.X,Tree.Y))<FMath::Square(180.0F)){Clear=false;break;}
        const auto& Route=FPacificCoastGrayboxContract::GetRoutePoints();
        for(int32 Index=1;Clear && Index<Route.Num();++Index)
            if(SegmentDistance(XY,FVector2D(Route[Index-1].X,Route[Index-1].Y),FVector2D(Route[Index].X,Route[Index].Y))<2750)Clear=false;
        if(Clear && SegmentDistance(XY,WalkCenter-WalkAlong*1520,WalkCenter+WalkAlong*1520)<430)Clear=false;
        for(int32 Index=1;Clear && Index<ProtectedTrail.Num();++Index)
        {
            if(Index==121)continue; // The two opposite trail approaches are separate polylines.
            if(SegmentDistance(XY,ProtectedTrail[Index-1],ProtectedTrail[Index])<430)Clear=false;
        }
        for(int32 Index=1;Clear && Index<4;++Index)
            if(SegmentDistance(XY,LaunchPath[Index-1],LaunchPath[Index])<650)Clear=false;
        for(const FVector& Other:UnderstoryOrigins)
            if(FVector2D::DistSquared(XY,FVector2D(Other.X,Other.Y))<FMath::Square(180.0F)){Clear=false;break;}
        if(!Clear)continue;
        int32 ExistingVertices=0,ExistingTriangles=0;
        for(const FCoastalTreeMesh& Batch:Batches){ExistingVertices+=Batch.Positions.Num();ExistingTriangles+=Batch.Indices.Num()/3;}
        // Every low plant is exactly seven 3-sided tubes and 36 paired sprays.
        if(ExistingVertices+337>4800000 || ExistingTriangles+207>3200000)break;
        const FVector Base(XY.X,XY.Y,GetCoastalLandHeight(XY.X,XY.Y));
        const float Height=UnderstoryRandom.FRandRange(70,180);
        Batches[0].Tube(Base-FVector(0,0,12),Base+FVector(0,0,Height*.64F),2.0F,.8F,3);
        for(int32 Stem=0;Stem<6;++Stem)
        {
            const float Yaw=Angle+Stem*2.39996F+UnderstoryRandom.FRandRange(-.4F,.4F);
            const FVector Axis=FVector(FMath::Cos(Yaw),FMath::Sin(Yaw),UnderstoryRandom.FRandRange(.4F,1.1F)).GetSafeNormal();
            const FVector Start=Base+FVector(0,0,Height*UnderstoryRandom.FRandRange(.12F,.34F));
            const FVector End=Start+Axis*Height*UnderstoryRandom.FRandRange(.65F,.95F);
            Batches[0].Tube(Start,End,1.4F,.5F,3);
            for(int32 Twig=0;Twig<6;++Twig)
            {
                const FVector Across(-Axis.Y,Axis.X,0);
                const FVector Direction=(Axis+Across*UnderstoryRandom.FRandRange(-.8F,.8F)).GetSafeNormal();
                const FVector Point=FMath::Lerp(Start,End,(Twig+.6F)/6.0F);
                Batches[1+(UnderstoryOrigins.Num()+Stem+Twig/2)%4].Spray(Point+Direction*23,Direction,FVector(95,0,0),UnderstoryRandom);
            }
        }
        UnderstoryOrigins.Add(Base);
    }
    const FLinearColor Colors[] = { FLinearColor(.13F,.089F,.049F), FLinearColor(.033F,.084F,.039F),
        FLinearColor(.060F,.14F,.062F), FLinearColor(.091F,.18F,.075F), FLinearColor(.042F,.115F,.065F) };
    const TCHAR* Names[] = { TEXT("CoastalBranchSkeleton"), TEXT("CoastalNeedlesShadow"), TEXT("CoastalNeedlesDeep"),
        TEXT("CoastalNeedlesTips"), TEXT("CoastalNeedlesCool") };
    for (int32 Batch = 0; Batch < 5; ++Batch)
    {
        UProceduralMeshComponent* Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(Names[Batch]);
        Mesh->SetupAttachment(Root);
        Mesh->bUseAsyncCooking = true;
        Mesh->SetCollisionProfileName(TEXT("NoCollision"));
        Mesh->CreateMeshSection_LinearColor(0, Batches[Batch].Positions, Batches[Batch].Indices, Batches[Batch].Normals,
            Batches[Batch].UV, TArray<FLinearColor>(), Batches[Batch].Tangents, false);
        Mesh->SetMaterial(0,Material);
        Mesh->SetCastShadow(true);
        if (Batch == 0)
        {
            // Stable inspectable origins let Automation test actual generated support
            // and clearances instead of duplicating the generator's equations.
            Mesh->ComponentTags.Add(FName(*FString::Printf(TEXT("CoastalDistantTreeCount|%d"),DistantCount)));
            Mesh->ComponentTags.Add(FName(*FString::Printf(TEXT("CoastalUnderstoryCount|%d"),UnderstoryOrigins.Num())));
            for (const FVector& Origin : TreeOrigins)
                Mesh->ComponentTags.Add(FName(*FString::Printf(TEXT("CoastalTreeBase|%.3f|%.3f|%.3f"), Origin.X, Origin.Y, Origin.Z)));
            for(const FVector& Origin:UnderstoryOrigins)
                Mesh->ComponentTags.Add(FName(*FString::Printf(TEXT("CoastalUnderstoryBase|%.3f|%.3f|%.3f"),Origin.X,Origin.Y,Origin.Z)));
        }
        else Mesh->ComponentTags.Add(FName(*FString::Printf(TEXT("CoastalNeedleCardCount|%d"),Batches[Batch].Positions.Num()/4)));
        CoastalColorParts.Add(Mesh); CoastalPartColors.Add(Colors[Batch]);
        ++GeneratedPrimitiveCount;
    }
    TrunkProxies->ComponentTags.Add(FName(*FString::Printf(TEXT("CoastalTrunkProxyCount|%d"),TrunkProxies->GetInstanceCount())));
}
