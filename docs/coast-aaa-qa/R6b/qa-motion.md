# R6b QA-VISION-MOTION report (transcribed by PMO)
18 sequences × 8, fingerprint ab2518739f54, world_paused=false (paused glint attempt set aside in paused_attempt/).
- Sun glint PASS / IMPROVED — glint_150 9cbb8876→bff54777: single bounded path x684–1619 y500–1002, >200 px 11,719–22,333/frame, smooth evolution, no sparkle (R5a: none).
- Foam advection PASS measured / IMPROVED — cliff_600 −5 px per 0.6 s ×3 intervals while fading; shore_150 ≥10 px per 0.3 s (search cap); R5a 0 px. Mostly longshore on screen.
- Shore foam look — soft/smeared sheet; stretched streak strip on rock near waterline persists.
- N/S surf — north PARTIAL (broken flecks moving), south FAIL (no white water).
- Canopy: shimmer PASS, motion metric REGRESSED — forest std>10 px 822→24,683 (150 ms), 11,538→29,244 (600 ms); near-crown change grows with interval (2.8→4.4→7.5 levels) = smooth sway on large near lit cards; corridor 3,652, overview 4,243, launch-low 439, player 803, mountain 23.
- Corridor PASS / IMPROVED (also legible from forest pose).
- Perf with Lumen FAIL / REGRESSED — GPU forest 36.6/33.2 ms (R5a 30.9), corridor 37.9/41.1 (30.1), shore 37 (24), cliff 31.6 (22.5); frame p95 ~270–289 ms on ALL views incl. water (R5a water 28–34); glint_600 p95 climbs 74→270 ms → recurring hitches.
