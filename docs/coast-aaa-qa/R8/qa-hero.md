# QA-HERO R8 (transcribed by PMO)

Build 3c68c299b, fingerprint 418a2a7645ed. Windowed 1512×949 (16:9 letterbox rows 50–899; built-in display only — fullscreen read back black). Frames: `captures-v2/` + `index.tsv`, normalized to 1920×1080 in `norm/`, R7b-vs-R8 grids in `crops/cmp-<view>.png`. `captures/` = unusable black frames.

Overall: **all U-items FAIL**. Terrain, flora and cirrus improved vs R7b. The forest floor darkened, which is a regression: the corridor path is now barely readable.

| Metric | R7b | R8 | Target |
|---|---|---|---|
| Canopy strict-green, top 400 rows (forest / corridor) | 19.9% / 30.1% | 38.0% / 37.7% | ≥45% |
| Forest-floor lit RGB (forest / corridor) | 34,34,28 / 34,33,26 | 24,27,23 / 23,25,21 | ≈45,42,32 |
| Shore lighting_only featureless area | 24.3% | 22.2% | — |

| Item | Verdict | Notes |
|---|---|---|
| U1 roots/bark | FAIL (unchanged) | Trunks meet a uniformly dark floor; no visible contact or flare. Bark is fine. |
| U2 density | FAIL (mixed) | Paper flags gone; far crowns are rounded and broken. Green share 38%. **Regression:** corridor path unreadable. Groves still straight-edged, plantation-like. |
| U3 sea | FAIL (improved) | Glint swells are the best so far. South/north surf present. Flat underwater bands remain at the north headland toe. |
| U4 sky | FAIL (improved) | Swirls gone, cirrus directional. Some streaks smeared. Understory about 45% too dark. Marine layer is a flat white band. |
| U5 ground | FAIL (unchanged) | |
| U6 hills/cliff | FAIL (improved) | Slump trays gone. Upper ledge band shows flat-shaded gold facets (cliff, overview, shore top). North-toe contour stair-steps remain. |

## Top defects
1. **seasky (+flora litter):** understory too dark (24,27,23); corridor path unreadable.
2. **terrain:** untextured, flat-shaded gold facets in the upper ledge band.
3. **flora:** canopy green 38% (target ≥45%); rectangular grove edges; no visible trunk-base contact.
4. **terrain:** north headland toe contour stair-steps; shore upper face 22% featureless.
5. **seasky:** smeared cirrus streaks; flat marine-layer band.

No AAA or acceptance claim.
