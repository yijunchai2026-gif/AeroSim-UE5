# Surface asset spec — Pacific Coast v2 (RD-02 ART-SURFACE)

State: `DRAFT_NOT_USER_CONFIRMED` / `STAGED_NOT_IMPORTED_OR_APPLIED`. This spec lists source textures and how they are meant to be used. Nothing here has been imported into UE, applied to a material, or reviewed visually. It does not show that U3, U5 or U6 are fixed. Only the user decides AAA quality and visual acceptance (`docs/COAST_AAA_PROGRAM.md`).

- Acquisition: `python3 Scripts/acquire_coast_surfaces_v2.py --engine-dir "<UE_5.8>/Engine"`. The script can be re-run safely: if a local file already matches the publisher MD5, it is not downloaded again.
- Index receipt: `SourceAssets/EnvironmentDraft/surface-v2-source-manifest.json`. Each asset also has its own `source.json` beside its maps, holding the URL, authors, license, bytes, MD5, SHA256 and provider scale.
- License: Poly Haven CC0 for 14 assets and ambientCG CC0 for 1 asset. Engine water textures are **referenced only**: they are not copied and they are not CC0.
- Total staged size is 173,940,915 bytes across 57 maps. All maps are 2K JPG. No 4K maps were needed; the macro map is a 15 m aerial scan at 2K.

## 1. Physical scale (read before setting UVs)

Poly Haven's API `dimensions` value is in **millimetres**. The Sandy Gravel 02 intake had a bug: it read 2529.99 as metres, but the true repeat is 2.53 m. Every v2 asset now has an expected millimetre range in the script, and the script aborts if the provider value falls outside that range. In the table, "Tile" is the provider's real-world width. "World repeat" is the recommended UE world-aligned repeat (1 UE unit = 1 cm).

## 2. Texel-density rule for the 45 cm camera (U5)

The reference case is 1440p (2560 px wide) with about 90° horizontal FOV, which gives about 0.035° per pixel.
- At 0.45 m straight down, one pixel covers about 0.28 mm.
- At 1 m slant distance, one pixel covers about 0.6 mm.

The texture densities below compare against those numbers:
- **Base layers**: a 2K map over 2 m gives 0.98 mm per texel (10.24 px/cm). At the closest views this is magnified about 2–3×. This is the minimum, so do not stretch a base layer beyond its provider tile. The 4 m `dry_ground_rocks` gives 1.95 mm per texel, so use it only where the camera does not hover at 45 cm, or always pair it with the detail layer.
- **Detail layer is required inside about 3 m of the camera**:
  - Use `dirt` normal + roughness at a **0.5 m repeat**. That gives 0.24 mm per texel (41 px/cm), which roughly matches the pixel footprint at 45 cm.
  - Use a second sample at a 0.37 m repeat, rotated 37°, to hide repetition.
  - Fade the detail layer out between 3 m and 15 m by pixel depth.
  - Blend it with RNM/UDN into the base normal at about 0.5–0.7 strength.
  - Changing the physical scale is acceptable for a micro-normal only; never do it for albedo.
- **Macro layer**: `aerial_grass_rock` covers 15 m at 7.3 mm per texel. Use it only as a low-frequency multiply/overlay on albedo (about ±15% value, ±5% hue) and roughness, sampled at a 45–60 m repeat. Combine it with the existing `rocky_terrain_02` (90 m) at a 180 m repeat. It must never be the base texture visible at 45 cm. That was the cause of the U5 blur.
- Set texture group `World`, keep mips on, and do not apply a LOD bias to ground textures. Use a sharpen mip filter if the import allows it. Check visually with `launch-low`.

## 3. Asset table

Role key: macro = low-frequency variation; base = the material layer the landscape paints or blends; detail = tiling close-range normal/roughness/mask.

| Asset (folder) | Purpose | Tile (provider) | World repeat | Maps | Role | Serves |
|---|---|---|---|---|---|---|
| `withered_grass` (Ground) | Dry golden California grassland: sun-facing (S/W) hill slopes and launch surroundings | 2.00 m | 2.0 m | diff, nor_dx, rough, disp | base | U5, U6 |
| `sparse_grass` (Ground) | Green-brown grass/soil for shaded N/E slopes, swales and gully bottoms. Complements WIP `GrassGroundDraft` (`grass_ground`, 2.51 m), which was not re-acquired | 2.00 m | 2.0 m | diff, nor_dx, rough, disp | base | U5, U6 |
| `rocky_trail_02` (Ground) | Bare compacted dirt/trail soil for trails, launch access and fire roads | 2.00 m | 2.0 m | diff, nor_dx, rough, disp | base | U5 |
| `dry_ground_rocks` (Ground) | Chaparral soil under scrub on steep dry slopes | 4.00 m | 4.0 m | diff, nor_dx, rough, disp | base | U5, U6 |
| `forest_leaves_04` (Ground) | Pine-needle forest floor in the conifer stand and training corridor | 1.50 m | 1.5 m | diff, nor_dx, rough, disp | base | U5 |
| `dry_decay_leaves` (Ground) | Oak leaf litter in shaded oak woodland, canyon bottoms and N-facing slopes | 2.00 m | 2.0 m | diff, nor_dx, rough, disp | base | U5, U6 |
| `dirt` (Ground) | Tileable close-range detail normal/roughness, applied to every ground layer | 2.00 m | **0.5 m** (+0.37 m rotated) | nor_dx, rough | detail | U5 |
| `aerial_grass_rock` (Ground) | Macro tint/roughness variation that breaks tiling at mid and far distance | 15.0 m | 45–60 m | diff, nor_dx, rough | macro | U5, U6 |
| `worn_rock_natural_01` (Rock) | Weathered tan sandstone: steep hill outcrops and upper coastal cliff faces | 2.00 m | 2.0–4.0 m (triplanar) | diff, nor_dx, rough, disp | base | U6, U3 |
| `rock_face_03` (Rock) | Fractured tan rock face for gully walls, road cuts and shale-like outcrops | 2.70 m | 2.7–5.4 m (triplanar) | diff, nor_dx, rough, disp | base | U6 |
| `seaside_rock` (Rock) | Dark weathered rock for the splash zone and cliff toe | 2.00 m | 2.0 m (triplanar) | diff, nor_dx, rough, disp | base | U3 |
| `low_tide_rocks` (Rock) | Intertidal rocky sand/gravel band below the cliff | 2.17 m | 2.17 m | diff, nor_dx, rough, disp | base | U3 |
| `damp_beach_sand` (Ground) | Cove floor and swash-zone sand | 2.00 m | 2.0 m | diff, nor_dx, rough, disp | base | U3 |
| `clean_pebbles` (Ground) | Shingle back-beach and storm berm in coves | 2.00 m | 2.0 m | diff, nor_dx, rough, disp | base | U3 |
| `Foam002` (Water, ambientCG) | Broken sea foam: mask, normal, roughness and opacity | not published | 3–4 m (proposal; check visually) | Color, NormalDX, Roughness, Opacity | detail | U3 |

The `disp` maps are 8-bit height maps. They are for height-lerp layer blending, so that grass gives way to soil around pebble crowns and similar shapes. They are not meant for tessellation or displacement.

## 4. Slope/aspect blending recipe (U6)

These are suggested starting thresholds for DEV-TERRAIN to tune:
- Slope below 25° and facing S/W: `withered_grass`.
- Slope below 25° and facing N/E, or in a gully (concavity/flow mask): `sparse_grass`. Under the oak mask, use `dry_decay_leaves`.
- Slope 25–40°: `dry_ground_rocks` (chaparral). Rock starts to show through via the height-lerp of `disp`.
- Slope above 40° or on cliff faces: triplanar `worn_rock_natural_01`. Use `rock_face_03` in gullies and cuts.
- Inside the forest mask: `forest_leaves_04`. Trails: `rocky_trail_02` (the existing `sandy_gravel_02` stays on the launch apron).
- Base layers are 2–4 m, which leaves a narrow window before tiling becomes visible. Apply the macro layer everywhere, and add stochastic or rotated tiling for the grass layers.

## 5. Coast and sea (U3)

**Wetness (no provider wet variants exist).** Use one material parameter `Wetness`, driven by height above mean sea level, the shore-distance field and weather:
- Albedo: multiply in linear by about 0.55–0.7.
- Roughness: lerp toward 0.08–0.2.
- Normal: flatten slightly (about 0.7×) where pooled water fills the hollows; the `disp` low values work as a puddle mask.

Bands:
- Constantly wet: 0 to +0.6 m (tide plus swash).
- Damp gradient: up to about +1.5 m.
- Spray speckle on exposed headlands: up to about +3 m.

Layer order from the water up:
1. `damp_beach_sand` / `low_tide_rocks` (wet)
2. `seaside_rock` (wet → damp)
3. `worn_rock_natural_01` cliff (dry above the spray line)

**Foam.**
- Use `Foam002` Opacity as the coverage mask and multiply it by:
  - a contact mask: SceneDepth − PixelDepth < about 0.5–1.5 m, or a shore distance field;
  - a low-frequency panning noise, so foam is broken and never forms a continuous line.
- Use Color as the albedo tint (near white). Use Roughness, and blend NormalDX at reduced strength.
- Proposed repeat is 3–4 m. The provider gives no physical size, so this is not measured.
- The ambientCG tags include "soap", so treat this as a generic foam texture, not a surf capture.
- Engine alternative: `/Water/Textures/Foam/T_WaterFlow_01_Foam_Tiled` (+`_N`).

**Ocean normals — engine-bundled, referenced not copied.** These come with UE 5.8.3 CL 58210709 under the UE EULA and are not CC0. SHA256 values are in the index manifest. Repeats are proposals, not provider-measured:

| Scale | Package path | Proposed repeat | Pan |
|---|---|---|---|
| large swell | `/Water/Textures/Normals/T_Water_TilingNormal_With_Height_02_Softened` | 40–60 m | 0.3–0.6 m/s along the wind |
| mid chop | `/Water/Textures/Normals/T_Water_TilingNormal_Waves_02` | 8–12 m | 0.8–1.2 m/s, rotated 20–35° from the large layer |
| fine ripple | `/Engine/Functions/Engine_MaterialFunctions02/ExampleContent/Textures/water_n` (already used in v8) | 1.5–3 m | about 1.5 m/s, a third direction |

To avoid the "parallel stripes" complaint, use non-integer repeat ratios between layers, three different pan directions, and a fade of the fine layer by distance (gone by about 150 m). Distance normal strength should decay so that far glints stay calm.

## 6. Rejected candidates

| Candidate | Reason |
|---|---|
| `grass_ground` (PH) | Already staged in WIP `GrassGroundDraft`; duplicate |
| `dry_ground_01` (PH) | Cracked-mud polygon pattern reads as a playa, not CA grassland |
| `rock_face` (PH, Namaqualand) | Red-brown hue does not match Bay Area tan sandstone |
| `sandstone_cracks`, `mossy_sandstone`, `seaworn_sandstone_brick` (PH) | Orange or man-made masonry |
| `marble_cliff_*` (PH) | Marble geology; 4–20 m aerial scale |
| `coast_sand_01/02/05` (PH) | 15–25 m aerial scans; too coarse for 45 cm cove views |
| `forest_ground_04`, `leaves_forest_ground` (PH) | Overlap with the chosen pine/oak litter; kept within the budget |
| ambientCG `Foam001`, `Foam003` | Same set as Foam002; one variant is enough |
| CC0 ocean-surface normal maps | None found on Poly Haven or ambientCG (searched water/ocean/wave); engine references used instead |
| Any non-CC0 source | Not considered (policy) |
