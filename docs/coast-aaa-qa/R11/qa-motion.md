# QA-MOTION R11 (offline, transcribed by PMO)

**Source.** QA-HERO's live lit sequences (`Artifacts/ProductShell/AAA/R11/qa-hero/captures/<view>-lit.json`).
- Build 02874d5, fingerprint 63fe4160083b.
- Windowed 1512×949; letterbox excluded from measurements.
- 13 sequences × 6 frames, unpaused and unique.
- Actual interval ≈0.31 s, span ≈1.55 s.
- No 600 ms sequences exist, so the long-window items are NOT_RUN.
- Baseline: the R10b 300 ms × 6 sequences.

| Item | Verdict | vs R10b | Notes |
|---|---|---|---|
| Persistent foam fringe/residue | PASS | IMPROVED | Shore shows a lacy fringe in every frame (8.7k→78.6k px; R10b: 0). Cliff holds a steady 4.4–4.5k px (R10b: 210). |
| Foam build/recede cycle | PARTIAL | — | Build is visible within 1.55 s; recede is outside the window. |
| Foam drift | PASS (moves), direction differs | IMPROVED | Shore foam moves ≈60 px/s **along** the shore, with no visible shoreward component. Cliff fringe is static. |
| Single-frame flashes | PASS | — | Moving filament edges only, no popping patches. |
| Filament shimmer/aliasing | PASS | unchanged | Soft edges, no sparkle. |
| Full surge cycle 600 ms × 8 | NOT_RUN | — | Needs live capture. |
| Cirrus | PASS | unchanged | Static. |
| Glint | PASS | unchanged | |
| Far swell | PASS | unchanged | Correlation 0.97–0.98; no stripes or popping. |
| Foliage shimmer | PASS (shimmer); sway up | REGRESSED (amount) | Changing px per MP: forest 279→**1,178**, overview 2,465→3,374. Smooth (0–3% flip-flop), but crown sway is about 4× R10b. |
| Frame p95 | PASS (1512×949) | unchanged | 15.3–23.9 ms; GPU up to 27.8 ms (corridor). |

## Top defects
1. **seasky:** foam drifts along the shore instead of toward it; cliff fringe doesn't drift. Check the flow direction against the 1.2 m/s shoreward design.
2. **flora:** forest crown sway ~4× R10b. Check the wind amplitude.
3. **PMO:** capture live 600 ms × 8 sequences at shore/cliff for the full surge/recede cycle.

Not run: TAA ghosting at camera cuts. No AAA or acceptance claim.
