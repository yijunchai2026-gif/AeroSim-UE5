# QA-MOTION R10b (transcribed by PMO)

Build 7ed9a3b, fingerprint 940de1db14ae. Windowed 1512×949; letterbox excluded. All frames unpaused and unique; no keys pressed.

| Item | Verdict | vs R9 | Notes |
|---|---|---|---|
| Foam builds/recedes | PASS | IMPROVED | One ~1.8 s shore surge peaking at ~2.5% of the frame (R9: ~30% opaque). No foam pixel stays on in every frame. Cliff contact band persists. |
| Foam visible between surges | FAIL (minor) | regressed in stills | Quiet phase has almost no foam (shore_300x6: 0 px); cliff has a thin static fringe. |
| Marbled filaments: shimmer/aliasing | PASS | new | Soft edges, no sparkle. Churns in place rather than drifting shoreward. |
| Cirrus | PASS | unchanged | |
| Glint | PASS | unchanged | |
| Far swell | PASS | unchanged | |
| Foliage shimmer | PASS | IMPROVED | Corridor 507→188 px/MP, overview 5,097→2,465. |
| Frame p95 | PASS (1512×949) | unchanged | 15.4–24.3 ms. |

## Top defects (DEV-SEASKY)
1. Foam duty cycle too low: needs a persistent broken fringe and lingering residue between surges.
2. Foam churns in place instead of drifting shoreward.

Not run: TAA ghosting at camera cuts. No AAA or acceptance claim.
