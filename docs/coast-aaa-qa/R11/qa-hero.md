# QA-HERO R11 (transcribed by PMO)

**Capture conditions**

- Build 02874d5, fingerprint 63fe4160083b, confirmed on all 100 frames.
- Windowed 1512×949 with letterbox; measured on frames normalized to 1920×1080.
- Captured on the Flight page, so every frame has `world_paused=false`. All frames are distinct.
- All 13 views captured: lit 6-frame plus lighting_only, unlit on 5 views, detail_lighting on 4. Nothing is NOT_RUN.
- Analysis was finished offline after the app exited.
- Grids are in `Artifacts/ProductShell/AAA/R11/qa-hero/crops/cmp-<view>.png` and `crops/mtn-forest.png`.

**Overall:** U1–U6 all FAIL. Most R10b defects improved: rust plaid gone, cove boulders back with foam tracing them, denser canopy. Two items fell short: forest open sky and the shore dune band. The understory drifted slightly darker.

| Metric | R10b | R11 | Target |
|---|---|---|---|
| Canopy green (forest / corridor) | 50.5 / 57.0% | 50.7 / 51.7% | ≥45% ✔ |
| Open sky, forest | 19.6% | **11.7%** | ≈5% ✘ |
| Open sky, corridor | 2.3% | 1.1% | — |
| Floor lit RGB, forest | 47.3,44.5,33.8 | 42.0,40.6,30.1 | ≈45,42,32 (slightly dark) |
| Floor lit RGB, corridor | 46.9,42.8,31.6 | 40.5,37.9,28.9 | ≈45,42,32 (slightly dark) |
| Glint lower-sea RGB | 46,75,91 | 47,75,91 | keep ✔ |
| Shore featureless | 22.3% | 22.1% | — |
| Foam px, shore / cliff (lower frame) | 0.0 / 0.03% | 1.7 / 0.8% | — |
| Rust px, mountain lower frame | 1.2% | 0.0% | — |

| Item | Verdict | Notes |
|---|---|---|
| U1 roots/bark | FAIL (improved) | Dark root swells with some flare and contact on corridor trunks. Flare still under ~1.3× radius on most trunks. |
| U2 density | FAIL (improved) | Forest open sky 19.6 → 11.7%, upper crowns still spindly. Green ≥45%. Corridor stays staggered. North grove ragged but still a block; south grove lobed. |
| U3 sea | FAIL (improved) | Lacy, broken shore fringe with >50% open water, not fully persistent at the far left. About 24 rounded 4–6 m cove boulders, traced well by foam: best cove so far. Glint held. |
| U4 sky | FAIL (unchanged) | Faint directional cirrus and thin haze held. |
| U5 ground | FAIL (unchanged) | No near grass geometry. |
| U6 hills/cliff | FAIL (improved) | Rust plaid gone; mountain shows golden grass with soft mauve scree. Shore upper band still reads as smooth tan-orange dunes, and the rim rust strip remains. North-toe chevron remains. Red marker at bottom centre of the north view is gone. |

## Top defects
1. **terrain:** shore upper band needs fractured rock form; rim rust strip; 22% featureless shore face.
2. **flora:** spindly upper crowns (11.7% vs ≈5%); north grove still a block.
3. **terrain:** north-toe chevron.
4. **seasky:** understory drifting darker; restore about +10%.
5. **terrain/flora (U5):** near-ground grass and debris geometry missing at launch and player heights.

No AAA or acceptance claim.
