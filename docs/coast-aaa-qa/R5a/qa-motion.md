# R5a QA-VISION-MOTION report (transcribed by PMO)
16 sequences × 8, fingerprint a884125b2025…, world_paused=false.
- Sun glint NOT OBSERVED — no water pixel >200 (p99.9 79–111 vs R0 cone 178–183); no review pose faces the sun → inconclusive. PMO adds a `glint` view.
- Foam PASS visibility / IMPROVED — contact-only broken patches; cliff_600 d55aeadb→8ad92f23 375→0 bright px in 2.5 s; shift search 0 px translation → advection NOT confirmed.
- Shore waves PASS / IMPROVED — shore_600 7a7bdd85→a5d8a65b sheet breaks up by f2, ~30 px wall surge, clear waterline, wet band; no rock shimmer.
- Shore defects — stretched vertical-streak rim strip, dark cutout blob on face, faint vertical seam at left edge.
- Cliff tiling IMPROVED (cell pattern gone). Swell travel PASS. Far-field PASS far / mild mid; south navy patch gone (99423da6).
- Canopy shimmer PASS / IMPROVED — forest 822 px std>10 at ~0.3 s (R2 1,947); 11,538 at 0.6 s (R2 4,412) growing with interval, on near lit crowns → likely wind sway; launch-low 292, player 833, mountain 8, overview 1,447 (mostly sea/foam).
- Corridor PASS legibility / IMPROVED (49cc080a→e675427d), nearest gap ≈3 m.
- Perf: p95 water 28–34 ms, land 250–264 ms (carry-over, not steady state); GPU forest 30.9 ms (R2 26.3), corridor 30 (27); LOD0 bound cliff 25.4 M→37.2 M, shore 36.2 M.
