# R0 QA-VISION-MOTION report (transcribed by PMO from the tester's hand-back)

Build: baseline package, fingerprint 88096586… (manifest summary lacked the fingerprint field — PMO fix pending). 14 lit sequences x 8 frames, all world_paused=false, unique sha per frame. Raw tool JSONs in this directory; derived/ is derived, not evidence.

1. FAIL (DEV-SEASKY) Waves do not propagate: zero alignment drift on cliff/south/north; ripple normals change in place; no swell. Evidence cliff_600 f0 a8731bb1 (0.000 s) → f7 96f913fa (4.335 s). Shore: ±2–4 px bobbing, coarse faceted waterline.
2. FAIL (DEV-SEASKY/DEV-TERRAIN) Foam pops in/out (overview f0 30cec66f); none at shadowed cliff base or shore; no wet-rock band; continuous pale coast edge north/south; uniform teal shore→open sea.
3. FAIL (DEV-FLORA) Density: open savanna, trunks ~8–15 m apart, ~50–120 stems/ha (forest f0 7f6de22f); 13 frustum components ≈3.45 M triangles.
4. FAIL (DEV-FLORA/DEV-TERRAIN) No training corridor (no 2.5–4 m gap anywhere).
5. FAIL mild (DEV-SEASKY) Parallel mid-distance streaks (north f0 1d49aeb0).
6. Observation (DEV-FLORA) No wind motion over 4.3 s.
7. PASS minor: foliage shimmer isolated single-pixel twinkles.
8. PASS (this sun angle only): bounded sun glitter.
9. PASS: no texture popping, static camera 2.3–4.3 s.
10. NOT_RUN: TAA ghosting at camera cut (bridge settles ≥30 frames before frame 0).

Tool notes: interval_ms=150 yields ~320–440 ms real spacing (synchronous PNG readback floor); evidence PNGs live in the app container; no lighting_only sequences in this lane. Not AAA or user acceptance.
