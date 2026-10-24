# R2 QA-VISION-MOTION report (transcribed by PMO)
16 lit sequences × 8, fingerprint 819c0d34f31d…, world_paused=false, Clear sky.
- Canopy shimmer PASS minor / IMPROVED — std>10 px: forest 160,662→1,947; launch-low 7,677→200; player 2,290→850; overview 657→7; mountain 209→16; canopy crop change 5.9–7.8→0.6–1.3 levels; blue speckles gone (forest 87ec214a→4aa4d482).
- Shore PARTIAL / IMPROVED — arc gone, turbid band, first visible surge 8–13 px up the wall (shore_600 74f84a5b→c1a84ec6); still mirror-like over rock.
- N/S surf PARTIAL / IMPROVED — north broken flecks; south turbid band, little white water.
- South navy patch FAIL / REGRESSED — ruler-straight diagonal edge across open sea (south_600 4a9164ff).
- Swell travel PASS / UNCHANGED; banding mild FAIL / IMPROVED (moiré gone, long parallel crests remain).
- Foam FAIL / REGRESSED — shrank to tiny contact flecks; no measurable shoreward drift.
- Sun glitter — zero pixels >200 on cliff/south/north water (R0 ~90 px cone): specular may be lost.
- Corridor FAIL legibility (corridor 13021eaf): marginally deeper sightline only.
- Density PASS / IMPROVED (continuous crowns in overview).
- Perf: GPU 18–31 ms; p95 rolling window ~27 ms water / 260–280 ms land (carries camera-cut spikes; not steady state); cliff LOD0 bound 32.3 M→25.4 M.
- Other: new cellular tile on cliff rock; forest floor flat slate blue-grey; plateau grassland restored.
