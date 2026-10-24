# QA-MOTION R8 v2 (transcribed by PMO)

Build 3c68c299b, fingerprint 418a2a7645ed. Windowed 1512×949, letterboxed; the bars are excluded from the metrics. Lit sequences: 6 frames at 300 and 600 ms (plus one 8-frame shore sequence after the relaunch). No frame was paused, and every frame is unique. The neutral-hover oscillation is excluded; it belongs to DEV-FLIGHT. The earlier black-frame set in `../` is invalid.

| Item | Verdict | vs R7b | Notes |
|---|---|---|---|
| Foam advection | **FAIL** | REGRESSED | Cliff has only ~600 bright px (R7b: 5,760), ~65% of them static. Shore foam appears in 0/6 frames; after the relaunch a single patch flashes for 0.6 s. |
| South-shore surf | PARTIAL/FAIL | IMPROVED band | A turbid band animates, but there are no breaking white water and no breakers. |
| Far-ocean swell | PASS | unchanged | Crests travel steadily (correlation ~0.9); no stripes, no popping. |
| Cirrus | PASS | IMPROVED | Static (Δ ≤1.4 grey levels); no swirl. |
| Sun glint | PASS | unchanged | Narrow sun path that evolves smoothly. |
| Foliage shimmer / far crowns | PASS | IMPROVED | Flickering px per MP: forest 3,035 → 594, corridor 868 → 207. |
| LOD pops | PASS (static cameras) | unchanged | Not exercised in flight. |
| Frame p95 | PASS (resolution caveat) | IMPROVED | 14.6–23.4 ms; no mid-run hitch. |

## Top defects (all DEV-SEASKY)
1. Foam no longer advects or persists.
2. No white-water breakers on the south shore.
3. A new overview flicker hotspot at the cliff base is caused by foam, so fixing #1 should clear it.

Not run: TAA ghosting at camera cuts. No AAA or acceptance claim.
