# R7b QA-VISION-MOTION report (transcribed by PMO)
18 sequences × 8, fingerprint ddc49225daff, world_paused=false.
- Performance FAIL (hitch returns after runtime): early p95 glint 25.9, cliff 34.6, shore 35.2, north_150 36.4 ms; at 10:16:29 (north_600 249E25CC) p95 46.9→243 ms with CPU render 22.7→~75 ms sustained, GPU flat ~23 ms; all later 260–293 ms; cliff re-shot 273 ms (CPU 81) vs 34.6 two minutes earlier → time-dependent CPU render slowdown. GPU avg forest 36.9, corridor 35.5, shore 30.3, cliff 27.7, glint 23.1.
- Canopy motion IMPROVED — forest std>10 px 24,683→9,182 (150 ms), 29,244→14,560 (600 ms); still smooth sway (R5a 822).
- Glint PASS / tighter (x948–1117). Foam advection PASS. Foam002 sharpness PASS / IMPROVED (edge contrast 24–37 vs 4–12). South surf FAIL (0 bright px). Cirrus static PASS.
- Cliff view: clifftop tree line flat rectangular card clusters; sand gully low-poly stepped blocks; shore rock streak strip persists.
PMO hypothesis: r.GenerateMeshDistanceFields=True → async DF build for runtime-built tree meshes completes minutes in → DF shadows/Lumen DF cost on render thread (samples show RenderRayTracedDistanceFieldProjection, Lumen radiance cache).
