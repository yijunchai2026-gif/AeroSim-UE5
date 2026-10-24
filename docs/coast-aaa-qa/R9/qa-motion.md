# QA-MOTION R9 (transcribed by PMO)

Build 772117482, fingerprint 0275b33d11ea. Windowed 1512×949 with letterbox; bars excluded from the metrics. All frames unpaused and unique; every sequence committed (the `.err` files are empty).

| Item | Verdict | vs R8 v2 | Notes |
|---|---|---|---|
| Foam motion/persistence | PASS | IMPROVED | Surges build and recede (shore 76→229k→158k px over 4.2 s). 0–19% of foam pixels are static (R8: ~65%). No single-frame flashes. |
| Shore white sheet | **FAIL** | REGRESSED (look) | At peak, opaque flat white covers ~30% of the frame, essentially all near water. Reads as a paint flood, not surf. Cliff foam is a broken band at the rock (good). |
| South breakers | PASS (faint) | IMPROVED | Thin animated white lip; only flecks at >1 km. |
| Overview cliff-base | PASS | changed | Smooth surf animation; 0% of changing pixels flip back and forth. |
| Cirrus | PASS | unchanged | Static fibrous hooks. |
| Glint | PASS | unchanged | |
| Far swell | PASS | unchanged | Correlation 0.90–0.99; no stripes or popping. |
| Foliage shimmer | PASS | unchanged | Corridor 207→507 px/MP, smooth (root swells and litter). |
| Frame p95 | PASS (1512×949) | unchanged | 14.5–23.3 ms. |

## Top defects
1. **seasky:** shore surge becomes an opaque flat sheet. Needs opacity driven by foam thickness, broken-up edges, and visible water between patches.
2. **seasky / PMO:** south breakers are judgeable only from a closer review pose.

Not run: TAA ghosting at camera cuts. No AAA or acceptance claim.
