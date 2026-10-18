# Coastal naturalism revision — ownership before implementation

2026-10-17. User rejects geometric trees, repeated rounded mountains, flat sea and an unsupported side of the land. This explicitly authorizes one UI reviewer and two programmers. Repair G0/G1 terrain continuity first, then G2 silhouette/composition, then G3 surface treatment. No AAA completion or user acceptance is inferred.

Root owns APacificCoastGraybox header, Landscape orchestration, ocean, integration and build evidence. Terrain programmer owns new PacificCoastTerrain.cpp and terrain topology Automation. Vegetation programmer owns new PacificCoastTrees.cpp. UI owns visual critique/camera proposal documentation only. No concurrent edits to shared files.

Interfaces: BuildCoastalTerrain(UMaterialInterface*), BuildCoastalTrees(UMaterialInterface*), BuildCoastalOcean(UMaterialInterface*) are private actor members called once from the constructor. Builders may attach components to Root and record CoastalColorParts/CoastalPartColors/GeneratedPrimitiveCount. Terrain exclusively defines GetCliffEdgeX/GetCoastalLandHeight. Tree builder sets bounded CoastalTreeCount=240, samples terrain and clears flight/boardwalk routes. Root alone edits shared header and orchestration.

Retain explicit Entry map, no imported binary assets or production claims. Hardcoded engine materials are constructor references. Clockwise Unreal front faces with outward normals, deterministic geometry and bounded startup allocation. Sea extends beyond all land boundaries; every visible peripheral terrain edge descends below sea instead of ending at plateau height. Mountain foothills share the land height function. Existing level training area remains supported. No atmosphere or post-processing changes may conceal gaps.

Validation: static and Python checks, real engine build/Automation/Cook/Package, packaged Metal identical overview before/after plus route/side views. Preserve previous CoastalPackage and desktop target until replacement verified. Geometry/material limits and absent formal assets/performance/human review stay explicit.

## Phase 7 development asset route

The user's request for AAA-quality trees/terrain/sea exposed the limit of solid-color procedural meshes in real close views. The current stage is Phase7; historical pure-C++ Phase2/Phase4 no-binary checks may not silently prevent a necessary current-stage draft asset import. Permit only explicitly manifested, SHA256-pinned CC0 textures and locally generated materials in Content/Development/CoastDraft. This is a development review scene, PROTOTYPE/UNVERIFIED, not formal production import/promotion or named-aircraft delivery. All other unmanifested binary assets remain forbidden by static checks. Runtime uses hardcoded draft asset IDs. The Editor-only import commandlet is an offline build tool with fixed source/destination paths, never exposed through MCP/Command API. No formal map or asset is overwritten.

Vegetation programmer owns verified Poly Haven needle/bark texture acquisition and draft card foliage; terrain programmer owns offline Editor import/material authoring; root owns ground/rock acquisition, module config, runtime assignment, content policy and build; UI continues to reject unsuitable real images. Formal visual acceptance remains pending_user; no successful pipeline may be relabeled AAA quality.

## Canopy allocation revision

Real packaged forest captures rejected the 2-secondary-shoot / 12–16 twig branch as visibly bare. Replace with 6 alternating secondary shoots distributed along each primary branch, 5–8 photographed needle sprays per shoot and 8 primary sprays. Physical twig scale, masked sky gaps, alpha coverage mips, deterministic seed and route clearance remain. Explicit High allocation ceiling becomes 4.2m tree vertices / 2.8m tree triangles (2.2m needle triangles); whole-slice ceiling 4m triangles. These are startup geometry allocation checks, not performance acceptance or a measured memory budget. Low still suppresses foliage shadows; proper foliage instancing/LOD and target hardware profiling remain outstanding. The isolated slope-material island is removed by using the same macro ground surface on all mountain patches; actual cliff/talus keep the rock material.

## Distant foliage and cliff repetition follow-up

Packaged canopy revision d5d757b8 improves near-tree coverage but far trees remain sparse and the cliff's 1.8m repeat reads as horizontal fabric. Add bounded distance coverage compensation to the masked needle shader (zero before 180m, ramps across 450m, max .16 below the .33 mask clip so empty atlas background remains empty; normal alpha mips remain). Add separate 23m colour / 11m normal macro projections blended with physical-scale detail on rock. These macro layers are authored variation, not a claim that the original 1.8m scan was captured at those scales. Reimport all fixed draft materials, freeze v3 hashes and repeat the build/capture lane. No visual success is inferred before real render inspection.

Current High and Low both allocate the same generated tree geometry; Low only disables needle shadows and simplifies clouds/water. The allocation ceiling is not a claim of a geometry LOD system. Proper instance/LOD/memory and Shipping performance work remains G4-open.
