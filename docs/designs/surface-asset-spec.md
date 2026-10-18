# Surface asset spec — Pacific Coast v2 (RD-02 ART-SURFACE)

State: `DRAFT_NOT_USER_CONFIRMED` / `STAGED_NOT_IMPORTED_OR_APPLIED`. This spec lists source textures and how they are meant to be used. Nothing here has been imported into UE, applied to a material, or reviewed visually. It does not show that U3, U5 or U6 are fixed. Only the user decides AAA quality and visual acceptance (`docs/COAST_AAA_PROGRAM.md`).

- Acquisition: `python3 Scripts/acquire_coast_surfaces_v2.py --engine-dir "<UE_5.8>/Engine"`. The script can be re-run safely: if a local file already matches the publisher MD5, it is not downloaded again.
- Index receipt: `SourceAssets/EnvironmentDraft/surface-v2-source-manifest.json`. Each asset also has its own `source.json` beside its maps, holding the URL, authors, license, bytes, MD5, SHA256 and provider scale.
- License: Poly Haven CC0 for 14 assets and ambientCG CC0 for 1 asset. Engine water textures are **referenced only**: they are not copied and they are not CC0.
- Total staged size is 173,940,915 bytes across 57 maps. All maps are 2K JPG. No 4K maps were needed; the macro map is a 15 m aerial scan at 2K.

## 1. Physical scale (read before setting UVs)

Poly Haven's API `dimensions` value is in **millimetres**. The Sandy Gravel 02 intake had a bug: it read 2529.99 as metres, but the true repeat is 2.53 m. Every v2 asset now has an expected millimetre range in the script, and the script aborts if the provider value falls outside that range.

**R2 re-verification (2026-10-18, `--verify-scale`).** I re-read `https://api.polyhaven.com/info/<id>`; its `dimensions` field is in millimetres. The evidence is appended as `scale_reverification` in each `source.json`.

| Asset | `dimensions` (mm) | Width | info response SHA256 |
|---|---|---|---|
| dry_ground_rocks | [4000, 4000] | **4.0 m** | `0b3af3dd8c9a5916d523e32c97a91ea0f6c7704691a13bc2aedaa23844f9ae09` |
| rocky_trail_02 | [2000, 2000] | **2.0 m** | `27ca8b87bd0c36a70363961d73dad2a93cdab26be978df6520cc058dbbc67c2b` |
| worn_rock_natural_01 | [2000, 2000] | 2.0 m | (in receipt) |
| rock_face_03 | [2699.9996, 2699.9996] | 2.7 m | (in receipt) |

All four match the intake receipts. The info hash can differ from the intake hash because the API also returns counters that change; the dimension values are the evidence. In the table, "Tile" is the provider's real-world width. "World repeat" is the recommended UE world-aligned repeat (1 UE unit = 1 cm).

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
- **Macro layer**: `aerial_grass_rock` covers 15 m at 7.3 mm per texel. Use it only as a low-frequency overlay on albedo and roughness. R2 revised the repeat to **173 m** (rotated 23°), with `rocky_terrain_02` at 409 m; see §4.3. The earlier 45–60 m figure fell inside the 20–80 m honeycomb band. It must never be the base texture visible at 45 cm. That was the cause of the U5 blur.
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
| `aerial_grass_rock` (Ground) | Macro tint/roughness variation that breaks tiling at mid and far distance | 15.0 m | 173 m, rotated 23° (R2) | diff, nor_dx, rough | macro | U5, U6 |
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

### 4.1 Height-blend weights: use `disp`, not luminance (R2)

**Recommendation: yes. Drive the layer-transition weights from the `disp` maps.** Albedo luminance is a poor proxy for height:
- On scans it mostly encodes shadow, moisture and mineral colour. Dark wet soil or dark leaves would wrongly read as "low".
- `withered_grass` is bright everywhere, so with luminance it would win every transition.

Steps:
1. Normalise each `disp` per layer. Remap its own p5–p95 to 0–1, because providers do not share a height range. Store the remap as a material parameter.
2. Compute weights:
   - `h_i' = h_i + w_i`, where `w_i` is the painted or procedural slope/aspect weight.
   - `m = max(h_i') − d`, with contrast `d` ≈ 0.08–0.2.
   - `W_i = max(h_i' − m, 0)`, then normalise.
3. Suggested `d` values: 0.1 for grass→soil, 0.15 for soil→rock, 0.2 for sand→pebbles.
4. Sample `disp` only at the base repeat and only within 0–60 m. Beyond that, fall back to plain weight lerp; this saves samples and avoids shimmer.
5. Use the height-blend result only for weights. Do not offset the macro or detail layers with it.

### 4.2 October Bay Area golden-grass colour target (R2)

The R1 frames show orange "desert sand" on exposed grass. Measured on 512² downsamples of the staged maps, in linear RGB:
- `withered_grass` mean is (0.421, 0.300, 0.198), R/G 1.40. It is too bright and slightly warm.
- `dry_ground_rocks` mean is (0.310, 0.181, 0.082), R/G 1.72. It is strongly orange, so it reads as desert when it covers open slopes.

October hills are bleached straw/ochre with olive-grey mottling (green-grey from dry wild oat/brome and scattered coyote brush/oak). They are not orange.

**Albedo targets, linear RGB, before lighting:**

| Component | R | G | B | R/G | Share of area |
|---|---|---|---|---|---|
| straw (dominant) | 0.24–0.32 | 0.19–0.25 | 0.10–0.14 | 1.20–1.32 | 60–75 % |
| ochre (sun-cured, thin soil) | 0.26–0.34 | 0.18–0.23 | 0.08–0.11 | 1.35–1.48 (hard ceiling 1.5) | 10–20 % |
| olive mottling | 0.11–0.16 | 0.11–0.15 | 0.05–0.08 | 1.00–1.10 | 15–25 %, 3–15 m blotches |

- Overall mean luminance (Rec.709 weights): about 0.18–0.24.
- Any ground pixel with R/G > 1.5 and B/G < 0.45 over more than 1 m² of open slope is the "desert" failure.
- Practical steps:
  - Scale `withered_grass` albedo by about 0.72, and pull R/G toward 1.25 with a small hue shift.
  - Inside the grass mask, desaturate `dry_ground_rocks` by about 30 % and keep it at or below 20 % coverage; reserve it for steep chaparral.
  - Get the olive tone by lerping toward `sparse_grass` (rescaled ×2.2 in brightness) or by a macro tint. Do not add it as a separate texture.
- These are art targets. Check them against `launch-low` and `mountain` frames; they are not acceptance criteria.

### 4.3 Anti-tiling: the 20–80 m honeycomb (R2)

A honeycomb at 20–80 m is the signature of either:
- (a) hex/stochastic tiling whose cell size equals the texture repeat, or
- (b) a single macro texture repeating at about 15–60 m (`aerial_grass_rock` was proposed at 45–60 m, which lands inside that band).

Fixes:
- **No visible cell structure.** If you keep hex tiling, use cell size ≥ 3× the base repeat, a blend contrast ≤ 0.5 driven by `disp` (not a linear crossfade), and random rotation limited to 0–360° for grass/soil only (not for directional rock). Otherwise, prefer two-sample blending:
  - sample A at 1.0× repeat;
  - sample B at 1.37× repeat, rotated 37°, offset by (0.31, 0.71);
  - choose between them with world-space noise at about 23 m.
- **Distance UV switch.** Use the real repeat inside 0–20 m. Blend to a 2.6× repeat between 20 and 60 m, and 6.8× beyond 60 m. The ratios are non-integer, so the repeats do not phase-lock.
- **Macro modulation is not a texture repeat inside 20–80 m.** Build it from three incommensurate world-position noise octaves at 37 m, 113 m and 347 m: albedo value ±12 %, hue ±4 %, roughness ±0.08. Then add:
  - `aerial_grass_rock` at **173 m**, rotated 23°, as a soft-light overlay at 30–40 % strength;
  - existing `rocky_terrain_02` at 180 m → move it to **409 m** to avoid beating against the 173 m layer.
  - This replaces the 45–60 m figure in §2.
- **Scale variance.** Vary each layer's repeat per landscape component, or by 300 m noise, by ±12 %. This is not per-pixel; per-pixel variation causes swimming.
- **Normals fade with distance.** Beyond about 80 m, fade base normal strength to 0.4 and detail normal to 0. Repeating normal highlights are the main contributor to visible hex patterns in low sun at 16:30.

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

## 6. Sea cliff vs talus: worn_rock_natural_01 vs rock_face_03 (R2)

Bay Area and San Mateo/Santa Cruz sea cliffs are mostly pale tan-grey, weakly cemented sandstone and mudstone (Purisima/Santa Cruz Mudstone type). They show horizontal bedding, smooth wave-cut toes and fractured slump blocks. Assigned uses:

| Zone | Texture | Scale / projection | Why |
|---|---|---|---|
| Main cliff face above the spray line (about +3 m to the top) | **worn_rock_natural_01** | Triplanar. Repeat 3.0–4.0 m (1.5–2× the provider 2.0 m is acceptable on faces viewed from 10 m or more). Rotate the planar axes so any banding runs horizontally | Provider tags "cliff, costal, sandstone"; smooth pitted yellow-brown sandstone. Shift colour to the cliff target: linear mean about (0.30, 0.25, 0.17), R/G about 1.2 (measured raw: 0.396/0.275/0.144, R/G 1.44, too yellow) |
| Fractured lower cliff, slump scarps, gully walls, road cuts | **rock_face_03** | Triplanar, 2.7–4.0 m | Cracked, chipped, variegated brown. Reads as fractured blocks; too dark and busy for a whole cliff face (measured mean 0.232/0.145/0.083) |
| Talus/scree apron and fallen blocks at the cliff base | **rock_face_03** on block meshes; ground underneath `dry_ground_rocks` (desaturated) or `low_tide_rocks` below the high-tide line | Real scale (2.7 m / 4.0 m / 2.17 m) | Talus is broken material; keep the smooth sandstone off it so the cliff face and its debris read as different textures |
| Wave-cut toe and splash zone (0 to +3 m) | **seaside_rock** + the §5 wetness | 2.0 m | Dark, wet-looking base |

Neither texture shows true shale lamination. If DEV-TERRAIN wants visible bedding, add a world-Z sine/noise band mask:
- period 0.4–1.5 m, value ±8 %;
- applied to the worn_rock_natural_01 albedo and height.

That is better than searching for another texture. For gully walls, use height blending between the two rock textures with `d` = 0.15. Base it on slope (> 55° → worn_rock, 40–55° → rock_face_03), with the `disp` height deciding the breakup.

## 7. Rejected candidates

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
