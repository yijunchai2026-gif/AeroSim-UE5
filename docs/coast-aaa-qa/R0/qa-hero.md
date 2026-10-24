# R0 QA-VISION-HERO report (transcribed by PMO from the tester's hand-back)

Build fingerprint 88096586170cdf83af723365de81230cd8420b8c170a9edfc93d8a1f9f08bc1e (from aerosim_inspect). 11 views × lit 6-frame + lighting_only; extra unlit/detail_lighting and 8-frame sequences; 34 calls all COMMITTED, none world-paused; poses match v8. R0 is visually unchanged from v8 frame-023 except clouds. Evidence: captures/index.tsv (view, mode, frame, evidence_png, image_sha256), crops/.

Verdicts: U1–U6 all FAIL (U2 collision/seed/instancing/LOD NOT_RUN from images; U3 motion per QA-MOTION). AAA not achieved; user pending.

Top defects
1. U5 DEV-TERRAIN ground smear: launch-low lit f0 (093326-B433EA4F, 28555119…) nearest ~15 m ≈45% of frame, 0.5–2 m blobs; lighting_only: perfectly flat, no micro geometry.
2. U1 DEV-FLORA spider legs: forest lighting_only (093403-49336E5B, 9adc9048…) 3–4 straight cone wedges ~1–1.5 m from axis (≈3–5× radius vs ≤1.3×), one tip sinks below ground, no contact shadow.
3. U2 DEV-FLORA sparse: overview lit f0 (1d5040a8…) 2–3 parallelogram clumps ~150–200 trees; forest trunks 8–15 m apart, no understory, no corridor.
4. U6 DEV-TERRAIN mountain = one pyramid: mountain lighting_only (eb7e39ce…) two planar flanks + knife spur, no gullies/layered ridges, uniform 5–10 m dune ripple.
5. U6 single olive material on every aspect; no golden grass, oak or chaparral; ~60 thin conifers in one trough.
6. U3 stair-stepped waterline: cliff lit f0 (f65d92eb…) right-angle steps 5–20 m, no talus/coves, vertical wall into water.
7. U3 foam as dashed line 1–3 m off rock (cliff unlit a05f999d…), barely visible in lit, no wet band.
8. U3 tropical glassy turquoise, ruler-straight ~1 km north/south shores (ba132693…, 68949251…).
9. U1 bark grey concrete: forest unlit/detail_lighting (50464710…, 156cb650…) flat pale albedo, no vertical furrows.
10. U4 sky: neutral grey-white haze, low-altitude sea dissolves into sky (bridge, launch-operator), heavy changing cumulus, no cirrus/marine layer.

Pilot notes: canopy and windsock static; no tight avoidance gaps; blurry ground gives no height cue.
Tool notes: fingerprint missing from capture summaries (fixed in integration b0a217b); `shore` pose faces a fully shadowed wall at Z=-6220 (lighting_only nearly black) — re-pose with side light (PMO-owned ResolveReviewView); player capture has no HUD.
