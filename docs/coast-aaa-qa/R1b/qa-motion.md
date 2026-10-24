# R1b QA-VISION-MOTION report (transcribed by PMO)
14 lit sequences × 8 frames, all fingerprint 002b1337221f…, world_paused=false.
- Swells travel: PASS / IMPROVED — north_600 ee76c7d2→9bf1e190 (4.29 s) crests −4…−5 px per 0.61 s (~7 px/s away from camera); south −3…−4; R0 0 px. Cliff drift weak/mixed.
- Shoreline displacement: FAIL / REGRESSED — shore pose moved (X 22833→22489); water glassy/transparent, merges with reflected rock, no readable waterline; semicircular arc artifact sweeps up between frames (shore_600 90ff1d59).
- Foam: PARTIAL / IMPROVED — cliff_600 d333ac26 contact-only, broken, persists/fades ~1.8 s; no clear advection; no surf on N/S shores.
- Depth colour: PASS / IMPROVED.
- Parallel banding: FAIL mild / UNCHANGED — south mid-distance regular diagonal crests + moiré patch lower-left.
- Glitter: NOT_RUN (overcast sky, no glint).
- Canopy shimmer: FAIL / REGRESSED — mean frame-to-frame canopy change 6–9 grey levels (R0 <0.4); std>10 pixels forest 160,662 (R0 140), launch-low 7,677 (185), player 2,290 (37); static thin branches show edge flicker → shimmer, not only wind; blue speckles in foliage.
- Density: PASS likely over target / IMPROVED — trunks ~3–5 m (forest bed9aebf); pole plantation, thin canopy.
- Corridor: NOT VISIBLE from forest/launch-low/player poses.
- Other: grass gone at mid/far (sand + brown patches, visible tile pattern); forest/overview hitch frame p95 254–287 ms (GPU 21–28 ms, Development + synchronous readback, diagnostic only); cliff frustum LOD0 triangles 3.48 M → 32.3 M.
Tool: interval_ms=150 → ~270–440 ms real; bridge waits ~3.9 s before frame 0 (TAA cut ghosting NOT_RUN).
