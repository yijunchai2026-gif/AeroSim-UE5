# QA-HERO R9 (transcribed by PMO)

Build 772117482, fingerprint 0275b33d11ea. Windowed 1512×949 with letterbox, measured on frames normalized to 1920×1080. 100 frames, all taken on the Result page with `world_paused=true`; motion is deferred to QA-MOTION. Grids: `crops/cmp-<view>.png`.

Overall: **U1–U6 all FAIL**. This is the most improved round so far, and both numeric targets were met.

| Metric | R8 | R9 | Target |
|---|---|---|---|
| Canopy green (forest / corridor) | 38.0 / 37.7% | **53.7 / 47.4%** | ≥45% ✔ |
| Floor lit RGB, forest | 23.7,26.6,22.7 | **44.1,43.4,34.8** | ≈45,42,32 ✔ |
| Floor lit RGB, corridor | 22.6,25.0,21.3 | 40.5,40.3,32.2 | ≈45,42,32 (slightly dark) |
| Shore featureless | 22.2% | 22.1% | — |
| Glint lower-sea RGB | 46,75,89 | 71,92,105 | — |

| Item | Verdict | Notes |
|---|---|---|
| U1 roots/bark | FAIL (improved) | Root swells; trunks sit firmly; litter patchy and off the path. No flare yet at ~1.3× trunk radius. |
| U2 density | FAIL (mixed) | Green ≥45%; south grove lobed; path readable again. **New:** corridor trunks stand in strict orchard rows. North grove is still a parallelogram. |
| U3 sea | FAIL (mixed) | Boulders and surf good; glint swell good. **New:** opaque white foam sheet covers ~40% of near water (shore, cliff). Giant flat-faced ~15 m boulder wedge. Glint sea slightly milky and beige from the warm sky light. Sunlit field looks fine. |
| U4 sky | FAIL (improved) | Marine layer is now a thin haze. Cirrus hooks fan out radially like a starburst instead of sharing one wind direction. |
| U5 ground | FAIL (unchanged) | |
| U6 hills/cliff | FAIL (mixed) | Ledge band now rust, not gold, but reads as flat untextured planks in cliff and shore. North-toe chevron stair-steps remain. Orange waterline strip. |

## Top defects
1. **seasky:** opaque foam sheet; needs streaks with >50% open water.
2. **terrain:** giant flat-faced boulder wedge; untextured rust planks in the ledge band.
3. **flora:** orchard rows in the corridor (needs 1–2 m positional jitter); north grove block.
4. **terrain:** north-toe stair-steps; orange waterline strip; 22% featureless shore face.
5. **seasky:** radial starburst cirrus; milky warm cast on the sea.

No AAA or acceptance claim.
