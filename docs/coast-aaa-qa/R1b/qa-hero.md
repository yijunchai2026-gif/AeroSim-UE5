# R1b QA-VISION-HERO report (transcribed by PMO)
Build fingerprint 002b1337221f…b908b on all 82 frames. 11 views lit 6-frame + lighting_only (+unlit forest/launch-operator/mountain/cliff/overview). Comparison grids crops/cmp-*.png; index.tsv has per-frame fingerprint. Shore pose moved 344 cm X vs R0 (not exact).
U1 FAIL / IMPROVED — spider legs gone; trunks plain cylinders ~1.1× flare, no buttress/litter/contact; bark now brown longitudinal (was grey).
U2 FAIL / PARTIAL — nearest trunks 3–5 m (R0 8–15 m); uniform plantation (0.5–0.7 m trunks, crowns lifted, no understory); unlit upper canopy ≈38% wood, 18% foliage, 36% sky; overview still two straight-edged rectangles; canopy pixel churn 21.6k px >20/300 ms (sway vs shimmer → QA-MOTION).
U3 FAIL / MIXED — deep navy→green-teal shallows, visible seabed, broken foam touching rock (cliff 8182d2fc, shore 2e29f3e4); still serrated waterline, no talus, straight N/S shores; NEW hard-edged dark-navy patch + horizontal smearing near water (south e51e2339).
U4 FAIL / REGRESSED — saturated clear sky in top band 78–100% → 0–5% (overcast stratocumulus); IMPROVED far layered blue-grey ridges.
U5 FAIL / near IMPROVED, mid/far REGRESSED — near sharp (launch-low c49cbf7f); >50 m flat brown smear; overview land ≈67% dark-brown blobs, 9% tan, 1% green; fish-scale honeycomb 2–8 m cells.
U6 FAIL / shape IMPROVED, surface REGRESSED — rounded masses, V-gully to sea, layered far ridges (lighting_only c43a975a); surface desert-dune + camouflage, no oak/chaparral, white pole trees.
