# QA-HERO R10b (transcribed by PMO)

Build 7ed9a3b, fingerprint 940de1db14ae. Windowed 1512×949 with letterbox, measured normalized to 1920×1080. 100 frames, all paused stills on the Result page. Grids are in `crops/cmp-<view>.png`; mountain/north/glint detail in `crops/quad.png`.

Overall: **U1–U6 all FAIL**. Most R9 regressions are fixed. New top defect: rust plaid rectangles on the mountain grass.

| Metric | R9 | R10b | Target |
|---|---|---|---|
| Canopy green (forest / corridor) | 53.7 / 47.4% | 50.5 / 57.0% | ≥45% ✔ |
| Open sky (forest / corridor) | 4.5 / 0.2% | **19.6** / 2.3% | — |
| Floor lit RGB, forest | 44.1,43.4,34.8 | 47.3,44.5,33.8 | ≈45,42,32 ✔ |
| Floor lit RGB, corridor | 40.5,40.3,32.2 | 46.9,42.8,31.6 | ≈45,42,32 ✔ |
| Glint lower-sea RGB | 71,92,105 | 46,75,91 (R8 level) | — |
| Shore featureless | 22.1% | 22.3% | — |

| Item | Verdict | Notes |
|---|---|---|
| U1 roots/bark | FAIL (**regressed**) | R9 root swells gone; trunk bases are plain cylinders again. |
| U2 density | FAIL (mixed) | Orchard rows fixed. **Forest upper crowns spindly** (open sky 19.6%). North grove still a parallelogram. |
| U3 sea | FAIL (mixed) | Milky cast fixed, boulder wedge fixed, foam sheet fixed. **Overshot:** foam nearly absent. Cove boulders mostly removed instead of rebuilt at 4–6 m. |
| U4 sky | FAIL (improved) | Cirrus shares one direction, starburst gone, faint but natural. |
| U5 ground | FAIL (unchanged) | |
| U6 hills/cliff | FAIL (mixed) | Cove rust planks gone, continuous strata. **New:** rust plaid rectangles 50–150 m across on the mountain/overview grass (facet projection painting the scree patches). Shore upper band reads as smooth orange dunes with a rim strip. North-toe chevron remains. Red object at bottom centre of the north view: check whether it's a debug marker. |

## Top defects
1. **terrain:** plaid rectangles on mountain grass. Restrict the projection to true cliff faces.
2. **seasky:** cove foam now absent. Need lacy foam with >50% open water at the rock contact.
3. **flora:** spindly crowns, lost root swells, north grove block.
4. **terrain:** smooth orange dune band and rim strip; north-toe chevron; 22% featureless shore.
5. **terrain/seasky:** cove boulders removed rather than rebuilt at 4–6 m.

No AAA or acceptance claim.
