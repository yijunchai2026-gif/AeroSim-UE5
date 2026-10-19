// DEV-FLORA (RD-08) private flora contract shared by trees, habitat and Automation.
// Everything is deterministic: fixed seeds, no time or platform dependence.
#pragma once

#include "CoreMinimal.h"

class UStaticMesh;

namespace CoastFlora
{
enum class ESpecies : uint8 { DouglasFir, MontereyPine, CoastLiveOak, Sapling };
enum class EAge : uint8 { Mature, Pole, Sapling };

struct FTrunkSegment
{
    FVector3f Start, End;
    float Radius = 0;
};

/** One instanced tree archetype: a runtime-built static mesh (bark slot 0, foliage slot 1, four LODs). */
struct FArchetype
{
    FName Name;
    ESpecies Species = ESpecies::DouglasFir;
    EAge Age = EAge::Mature;
    float Height = 0;
    /** Trunk radius at 1.3 m (breast height) measured on the generated LOD0 vertices. */
    float BreastRadius = 0;
    /** Largest generated trunk radius between ground and the top of the root collar (LOD0 vertices). */
    float CollarRadius = 0;
    float CollarHeight = 0;
    /** Depth of trunk geometry below the instance origin (always buried). */
    float BuriedDepth = 0;
    float CrownBase = 0;
    float CrownRadius = 0;
    /** Lowest live-branch/leaf geometry height inside a radius of 3 m from the stem axis. */
    float LowestFoliageNearStem = 0;
    int32 StemCount = 1;
    TArray<FTrunkSegment> Collision;
    TArray<int32> LodTriangles;
    UStaticMesh* Mesh = nullptr;
    bool IsOverstory() const { return Age != EAge::Sapling; }
};

struct FPlacedTree
{
    FVector Base = FVector::ZeroVector;
    float Yaw = 0, Scale = 1, LeanPitch = 0, LeanYaw = 0;
    int32 Archetype = 0;
    bool bNearForest = false;
    bool bCorridorFlank = false;
};

struct FCorridorSample
{
    FVector2D Point = FVector2D::ZeroVector;
    FVector2D Tangent = FVector2D(1, 0);
    float HalfWidth = 0;
    float Distance = 0;
};

struct FLayout
{
    TArray<FPlacedTree> Trees;
    TArray<FCorridorSample> Corridor;
    /** Intended surface gaps (cm) of the deliberate corridor constrictions. */
    TArray<float> GateGaps;
    int32 NearTrees = 0, NearOverstoryTrees = 0, NearOverstoryStems = 0, NearSaplings = 0, DistantTrees = 0;
};

/** Unit trunk-base litter disc (radius 100 cm, foliage slot 1), built once per process. */
UStaticMesh* GetTrunkLitterMesh();
/** Builds the archetype meshes once per process (transient, rooted). */
const TArray<FArchetype>& GetArchetypes();
const FLayout& GetLayout();
FTransform InstanceTransform(const FPlacedTree& Tree);

/** 0..1 near-forest stand density (feathered edges and glades); 0 outside the forest zones. */
float NearStandDensity(float X, float Y);
/** Distance (cm) from the training corridor centreline; outputs the local clear half width. */
float CorridorDistance(const FVector2D& Point, float* OutHalfWidth = nullptr, FVector2D* OutTangent = nullptr);
/** True if the point lies within Clearance of the flight route, launch path, boardwalk or trails. */
bool IsProtected(const FVector2D& Point, float Clearance);
/** True within Clearance of the launch access path, forest trails or boardwalk (not the flight route). */
bool IsWalkway(const FVector2D& Point, float Clearance);
/** Boardwalk footprint distance used by the historical tree clearance test. */
float BoardwalkDistance(const FVector2D& Point);
}
