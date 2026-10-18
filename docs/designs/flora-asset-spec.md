# Flora source asset spec (ART-FLORA, RD-02)

State: `DRAFT_NOT_USER_CONFIRMED` / `STAGED_NOT_IMPORTED_OR_APPLIED`. 2026-10-18.
Serves user complaints **U1** (roots and bark look fake) and **U2** (forest density, understory) from `docs/COAST_AAA_PROGRAM.md`.
This doc only describes source material and how it should be used. Nothing here has been imported, rendered or visually accepted. Whether anything looks AAA is decided only by the user.

- Acquisition: `python3 Scripts/acquire_coast_flora.py` (idempotent; it re-verifies the provider MD5 and size, and only downloads again when they don't match).
- Receipts: `SourceAssets/EnvironmentDraft/FloraDraft/source.json` records the URL, authors, CC0 license, bytes, publisher MD5, SHA256, pixel size, colour space, normal convention and glTF triangle counts for each file.
- Totals: 67 files, 89,658,860 bytes. Every texture is ≤2048 px on its longest side. Every file comes from a Poly Haven asset under [CC0](https://polyhaven.com/license).
- Species honesty: all of these are scans of non-Californian species, used as visual proxies. None of them establishes that something is a Monterey pine, a Douglas-fir or a coast live oak.

## 1. Texel-density target

The camera range is 45 cm (drone on the pad, operator view) to 5 m (forest corridor). At 1920 px and about 90° horizontal FOV, one screen pixel covers about **0.47 mm at 0.45 m**, **1 mm at 1 m** and **5.2 mm at 5 m**.

- Hero trunks and litter within 1 m: aim for about 1 mm per texel, and add a detail-normal overlay (the same bark normal at ×4 tiling, blended in by distance below 1.5 m) so the surface still has detail at 45 cm.
- Background at 5 m or more: 2–5 mm per texel is enough.
- Don't scale UVs past the "max" column below. Doing so recreates the blurry, smooth-bark look.

## 2. Bark (U1). Maps: base colour (sRGB), normal (DirectX, UE native), roughness (linear), displacement (linear)

| Asset | Intended tree | Real-world tile | 2K density | UV rule | Notes |
|---|---|---|---|---|---|
| `pine_bark` (Dimitrios Savva) | Monterey pine, primary | 2.0 × 2.0 m | 0.98 mm/texel | V: 1 tile per 2.0 m of height. U: round(circumference / 2.0 m), at least 1, so the seam closes (a 0.7 m trunk gives 1; a 1.2 m trunk gives 2) | Red-brown plates between deep dark furrows; this is the U1 read. Displacement: start at about 2 cm amplitude with Nanite tessellation or POM, only within 5 m |
| `bark_willow_02` (Charlotte Baglioni) | Douglas-fir proxy | 2.15 × 2.15 m | 1.05 mm/texel | as above, with 2.15 m | Deep vertical corky furrows. Grey-brown, so tint it warmer (+red) in the material for Douglas-fir |
| `jolcham_oak_bark_01` (Charlotte Baglioni) | Coast live oak, primary | 1.0 m (U) × 2.0 m (V), 1024×2048 px | 0.98 mm/texel | U: round(circumference / 1.0 m). V: 1 per 2.0 m | Dark grey furrowed oak bark with some moss. Only the 1k file was taken, because the provider's "2k" is 2048×4096 |
| `tree_bark_03` (Rob Tuytel) | Coast live oak, alternate (blocky, light grey) | 1.0 × 1.0 m | 0.49 mm/texel | U: round(circumference / 1.0 m). V: 1 per 1.0 m | Light grey ridges with lichen. Good for 45 cm close-ups; at 5 m, break up the repetition with a macro tint |

Root flare (U1): bark V must keep running continuously down into the flare. Use the same material with world-aligned blending into the ground over the bottom 20–40 cm, plus a litter decal ring (§4). Don't model separate root "legs".

## 3. Foliage cards (U1/U2). Maps: base colour, alpha, normal (DirectX), roughness; two-sided masked, opacity-mask clip 0.33–0.40

| Atlas | Content | Suggested card size in world | Use |
|---|---|---|---|
| `pine_tree_01` twig 2K | Two pine needle sprigs (top left and middle right) plus needle-fascicle strips (bottom right). The large white region of the alpha is UV-baked cone and branch geometry, **not a card**: crop only the needle islands | Fascicle sprig 20–30 cm (Monterey pine needles are 10–15 cm). Clump cards 60–120 cm built from 6–12 sprigs | Monterey pine branch tips. A 2K upgrade of `PineTree01` 1K |
| `fir_tree_01` twig 2K | 6 fir sprays (flat, two-ranked) plus a stem strip | 30–50 cm per spray. Branch cards 1–2 m | Douglas-fir proxy and the conifer silhouette mid-layer |
| `island_tree_02` leaves 2K | 8 isolated oval leaves on black | Scale each leaf to 4–7 cm (coast live oak); about 10× that per cluster card (40–60 cm) | Broadleaf oak canopy clusters. Darken and add gloss (roughness × 0.8) for live oak |
| `searsia_lucida` 2K | About 8 leathery leaves, forked twigs, and a bark strip (right side; opaque, not for cards) | Leaves 3–6 cm. Twig cards 30–50 cm | Chaparral and live-oak twig cards; winter-twig silhouettes |

Alpha notes:
- The alpha maps are JPG, so import them as linear grayscale with no sRGB.
- Use a single channel as the material Opacity Mask.
- Enable "Dither Opacity Mask" only if TAA ghosting is acceptable.
- The diffuse is colour-dilated outside the leaf shapes, so mip bleed is safe. Still, never sample the dilated area as a card.

Card and LOD recommendations:
- LOD0 (<15 m): sprigs on real branch meshes, 1 card per 20–40 cm of branch.
- LOD1 (15–60 m): clump cards of 40–120 cm.
- LOD2 (60–200 m): about 6–10 crossed crown billboard planes.
- Beyond 200 m: an impostor.
- For each tree species, aim for ≤15k triangles at LOD0 and ≤3k at LOD1, so that 300–400 trees per hectare (U2) can stay instanced.

## 4. Forest-floor litter (U1/U2). Maps: base colour, normal (DirectX), roughness, displacement

| Asset | Content | Real tile | 2K density | Use |
|---|---|---|---|---|
| `forest_leaves_04` (Rob Tuytel) | Pine needles, small cones, twigs on soil | 1.5 × 1.5 m | 0.73 mm/texel | Ground layer under pines. Also a 1.5–3 m radius decal ring at the trunk base to hide the trunk-to-ground contact (U1) |
| `dry_decay_leaves` (Amal Kumar) | Dense dry brown broadleaf litter | 2.0 × 2.0 m | 0.98 mm/texel | Under the oak canopy and in shaded gullies (U6 hand-off to DEV-TERRAIN) |

Blend with world-aligned UVs. Mask by canopy distance: within 0.3–3 m of a trunk, use mostly the litter.

## 5. Understory models (U2). glTF 1k bundles plus separate 1k Alpha and DirectX normal

The glTF bundle's own normal map is OpenGL. Use the `*_nor_dx_1k.jpg` sibling in UE, and assign the separate alpha in the material. Bounds below are measured from the glTF accessors, converted to UE cm (X, Y, Z-up).

| Asset | Meshes (tris; bounds in cm) | Real-world target | Recommendation |
|---|---|---|---|
| `fern_02` | 4 clumps: 2384 (99×89×43), 2248 (87×77×35), 784 (55×61×29), 816 (57×59×21) | Western sword fern, 60–150 cm | Scale ×1.3–2.0. Shaded slopes and pine-forest floor. Same mesh is LOD0; LOD1 is about 30% (cards) |
| `shrub_04` | 1 mesh: 27,327 tris (58×15×22) | Low groundcover | Small: use only as edge filler. Scale ×1.5. Decimate to ≤8k for mass instancing |
| `wild_rooibos_bush` | 5 bushes: 13151 (70×84×54), 6287, 4204, 2684, 1193 | Coyote brush / chaparral, 1–2.5 m | Scale ×2–4 for chaparral massing on sunny slopes. The fine yellow-green foliage reads as dry-season shrub |
| `dry_branches_medium_01` | 3 branches: 5616 (78 cm long), 5254 (104 cm), 5933 (130 cm) | Fallen limbs | Scatter 20–60 per hectare in the forest, with no collision under 20 cm diameter |

Saplings (U2): no CC0 sapling under 400k triangles was found. Build pine saplings (1–3 m) procedurally from the `pine_tree_01` and `fir_tree_01` sprigs on a 3–5 whorl stem, ≤4k triangles.

## 6. Rejected candidates

- Full tree meshes: `pine_tree_01` (17.4M triangles), `fir_tree_01` (7.85M), `tree_small_02` (4.65M), `island_tree_02` (1.76M), `searsia_lucida` (842k), `searsia_burchellii` (1.43M). They are too heavy for 300–400 trees per hectare; only their textures were taken. `jacaranda_tree` was also rejected because it is a non-native purple-flowering ornamental.
- `pine_tree_01` trunk_a/b/c: unique UV-baked scan atlases, not tileable, and only usable on the 17M mesh.
- `pine_sapling_small` (398k triangles; 17.8 MB glTF .bin, which is outside LFS rules) and `fir_sapling` (433k triangles, 24 MB): too heavy. `pine_sapling_small`'s twig atlas is UV-baked geometry and not card-friendly.
- `knotted_pine_bark`: smooth with knots, which would recreate U1.
- `metasequoia_bark`, `chinese_cedar_bark`: fibrous redwood or cedar bark, the wrong target species. Deferred in case coast redwood is added.
- `leaves_forest_ground`: saturated orange autumn deciduous litter.
- `pine_roots`, `root_cluster_01/02`: exposed root scans would reintroduce leg-like roots (U1).
- ambientCG: CC0 and acceptable, but not needed. It would also widen the script's host whitelist.
- Quixel, Sketchfab, asset stores, and anything requiring attribution or with an unclear license: rejected by policy.
