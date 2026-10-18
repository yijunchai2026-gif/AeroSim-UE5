# Product execution status

2026-10-16, America/Los_Angeles. The latest user execution brief supersedes the earlier no-commit/no-push instruction for complete, verified batches. No force push, automatic merge, or production content promotion is authorized.

## Current batch: Product Shell A

Owner: PMO-01 integrates; UI-01/RD-07 native interface; RD-01 settings; RD-02 input safety; RD-08 evidence; RD-10 build; QA-01/02/03 verification. These are virtual responsibility lanes, not claimed people.

Goal: users can click through Home, Hangar, mission briefing, Preflight, Flight, Settings, Visual Lab, Result and Replay. Menus must not move the aircraft. Settings must preview without saving, recover after timeout, persist only successful Apply, and recover on save failure. Paused flight resumes only after explicit user action and physical input neutralization.

Dependencies: main 108a521, installed UE 5.8.3, existing Phase 7 runtime and Phase 5/6 Editor sandbox boundaries. Shared interface and ownership are frozen in adr/PRODUCT_SHELL_BATCH_A.md.

Acceptance: full static/Python, UE Automation, Mac Development compile, Cook/Package, actual Metal captures with source/build/platform/camera/environment and SHA256. Human first-use timing, visual G0-G4 sign-off, production asset and hardware qualification are separate gates.

Budget: no duplicated scene rendering; Lab uses one fixed camera and sequential A/B. Diagnostic engine counters are not Shipping performance certification. No added per-frame actor rebuilding or automatic evidence capture.

Actual results: ENGINEERING_ACCEPTANCE_OPEN. Python PASS 52/52 and all static validators PASS; matching pixel-repair source has Mac compile, UE Automation 54/54 and Cook/Package PASS. Nine Metal page captures have image/manifest integrity PASS; full diagnostic evidence FAIL because draw calls remain NOT_MEASURED. Native mouse navigation was initially unresponsive; one button event pair and entry into Settings were subsequently observed, so application-versus-automation input attribution remains open. Actual UI sandbox save/relaunch/rollback remains NOT_RUN. Details and source fingerprint are in PRODUCT_UX_VALIDATION.md. No commit/push yet.

Deviation/risk: generic programmatic scene/aircraft remain prototypes; no claim of production art or real flight controller. Usability and physical gamepad tests need humans/devices. Existing Phase 7 source documentation contains historical states, not this batch's validation.

Next: validate Shell A, repair concrete failures, push the independent engineering batch once all required engineering gates pass. Continue only unblocked work; formal asset intake completion waits for source/license/appearance, production scene G0-G4 cannot be signed by automation, Windows/iPad and formal performance require their own hardware evidence.

GitHub: base SHA 108a521b205ec736ada5f5a75ac56ac8b4744525; branch aime/20261016-1435-product-shell; batch commit SHA PENDING.

## Persistent blockers

BLOCKED_ASSET; BLOCKED_USER_CONFIRMATION; BLOCKED_TARGET_HARDWARE_EVIDENCE; Windows NOT_RUN; iPadOS NOT_RUN; human G0-G4 NOT_RUN. No engineering PASS closes these gates.

## Ground flicker repair and background sampling — 2026-10-17

Manual Audio navigation confirmed by the user. Ground flicker reproduced with fixed and moving offscreen Metal samples. The engine OpenWorld template Landscape and runtime floor were loaded together; explicit Entry default plus project-owned sky removes the inherited template terrain. Final same-build comparison captured 24 frames per map, with frame/manifest integrity and pose/state matching. Final stationary ground ROI changed-pixel fraction dropped from 41.819% to 0.000% in these samples. Mac compile, UE Automation 54/54, Cook/Package and both Metal captures PASS; Python 59/59 and ten static validators PASS. Exact fingerprint, metrics, hashes and limits are in PRODUCT_UX_VALIDATION.md.

A separate tested TemporalPackage is available through the desktop demo shortcut. The internal fixed-camera sampling path works without Computer Use window activation; external MCP service code is unchanged. This closes the reproduced template-overlap artifact within the sampled view, not human G0–G4, all-route visual acceptance, formal content, real-device validation or interactive persistence acceptance. Shell A as a whole remains open; no commit/push has been made.

## Coastal cliff scene — 2026-10-17

Sky/clouds/mountain ridge, connected cliff/plateau, sea 65 m below and 240-tree groves added as explicit project-owned prototype geometry. Home includes EXPLORE THE COAST and the Visual Lab offshore view exposes the cliff with a side panel. Obsolete boundary guides are hidden in normal play. Final Mac compile, UE Automation 55/55, Cook/Package/Archive and bounded 24-frame Metal capture PASS; Python 59/59 and ten static validators PASS. Actual overview inspected; source/pose/hash details and limitations are in PRODUCT_UX_VALIDATION.md and adr/PACIFIC_COAST_CLIFF.md. Desktop shortcut updated to CoastalPackage. User appearance/G0–G4, formal content, target platforms, real performance and interactive persistence remain open; no commit/push.

## Coastal naturalism preview — 2026-10-17

UI plus two programmer lanes produced closed continuous terrain, supported flank slopes, richer masked needle trees, draft PBR ground/rock and animated normal water. Root rejected sparse foliage/material repetition, added secondary shoots, bounded distant coverage and cliff macro variation, then reran the complete lane. Final fingerprint 0b6b5f5286b32a3baadf5a2641560dafd016fa5c9b33c7c333e9ae9f6a7d39b5: Mac compile PASS, UE Automation 57/57 PASS, Cook/Package/Archive PASS; Python 66/66 and ten static validators PASS. Five bounded Metal capture sets (24 frames each) pass image/sequence integrity. Exact provenance, hashes and actual visual limits are in PRODUCT_UX_VALIDATION.md.

Preview quality improved, but AAA target is NOT_ACHIEVED; cliff silhouette, mountain dressing, forest-floor detail, shore interaction and real geometry LOD/performance remain open. Draft asset version coast-draft-20261017-v3 is SHA-pinned and unconfirmed. No later independent UI or user art approval. Full diagnostic metric completeness still fails because draw calls are NOT_MEASURED. Shell A native save/relaunch/rollback remains NOT_RUN; whole batch remains ENGINEERING_ACCEPTANCE_OPEN. No commit/push or formal promotion. The desktop preview targets verified NaturalPackage, with older packages retained for rollback.


## Umunhum-inspired coast / prototype v5 — 2026-10-17

Latest user priority: keyboard works; mouse is untested and deferred. Optimize abrupt boardwalk, fake sea and Bay Area / Mt. Umunhum-inspired mountain character. This is a fictional coastal composition, not a geographic reconstruction of the inland preserve.

Replaced115m×9m boardwalk with26m×2.2m grounded timber bridge, human-scale rails, actual sloped approaches and continuous terrain-following trails; seasonal channel is1.8m deep with exposed-bed tint. Replaced separate rounded humps with oblique connected backbone, descending spurs/gullies and slope/drainage vegetation coverage. Distant pine crowns are fuller and grouped rather than uniform stick scattering. Water is actual SingleLayerWater with bounded animated swell, dual moving normals, volume absorption/scattering, shore-tied broken foam and a submerged shelf continuously joining the deep seabed. Cliff shelves/fractures replace the smooth sin-bulge ribs; foam shoreline is computed from actual sampled cliff triangles. Existing sky/clouds remain.

Frozen dirty base `108a521b205ec736ada5f5a75ac56ac8b4744525`; source fingerprint `babe604945719a9fd1225c47f6d873a8424af56fd5e18b97f3d25e8c2dbfb338`; UE5.8.3 / Mac Development / Apple M3 Pro36GB. Build/Cook/Stage/Package/Archive PASS. Python72/72 and ten static validators PASS. UE Automation57 successful:56 clean +1 warning (engine Google generate_204 network probe timed out; no assertion failed). Actual measured tree mesh4,731,723vertices /3,024,413triangles, within retained allocation gates. Closed terrain, route/grade/support, sloped bridge/trail contact and shoreline-vs-actual-triangle3cm tests PASS. This is engineering geometry evidence, not user G0–G4 acceptance.

Matched Metal capture:7 views(overview,bridge,mountain,cliff,forest,south,north)×24frames,1920×1080,High,Clear16:30,seed21630,vFOV60. Every frame identity/manifest/hash PASS. `Artifacts/ProductShell/UmunhumEvidence/v5-review.json` SHA256 `d3169317fad02754134bcdfabe43acb6327d3094fa823dc2d6489467284c089e`. Per-view PNG,manifest,analysis,contact sheet and sampled-motion GIF preserved beside the review; matching build records/logs are in `v5-build/`. Evidence came from the archived app with background offscreen rendering and no repeated window activation. Screenshot readback changes timing; draw calls and formal target performance remain NOT_MEASURED, not PASS.

Scoped visual improvement: prior light box-trail black side edges are absent; north/south underwater hard shelf edges are absent at the sampled views; bridge is smaller and supported; main ridge/gully hierarchy and fuller distant crowns are visible. Remaining: dirt-path continuity is too subtle, distant canopy still sparse and pine-only rather than Bay Area oak/chaparral coverage, long wave trains repeat, cliff silhouette remains coarse and shoreline breaker/foam integration is incomplete. AAA NOT_ACHIEVED; visual G2/G3 and user acceptance pending. Keep DRAFT_NOT_USER_CONFIRMED. UI review is in COAST_VISUAL_REVIEW.md.

Asset manifest remains coast-draft-v4 (material asset set unchanged by v5 geometry follow-up), SHA256 `53302951c5aac5a88e328d90980f2e6b5ce7375ec87a073e44c0e9d7b96bc850`. V4 candidate and original desktop preview are retained separately for rollback. Current larger Shell engineering acceptance remains OPEN for actual UI save/relaunch/rollback; no Git commit/push for this mixed working tree. Formal aircraft, production visuals, user appearance confirmation and Windows/iPad/Shipping performance retain their existing blockers.

Desktop preview updated to archived v5 candidate and opened once. Live MCP inspect returned matching source fingerprint, active application, physical input neutral and possessed player on Home; inspect-only final handoff, no keyboard/mouse causality claim. Previous preview target is preserved in ignored rollback record.


## Launch-site and clean scenery candidate v8 — 2026-10-18

The user's airborne numbers were world-space training/debug text. Normal play now hides gate/zone labels, emissive gate geometry, route bars and scale sticks; hidden diagnostics never retain collision. Mission progression remains active. Explicit Development diagnostic mode can show them; Shipping cannot.

The 32 m square raised slab, tall curbs and oversized markings were replaced with an approximately 8 by 10 m irregular terrain-following gravel apron, a 2 m thin matte launch mat, two supported cases, a windsock and a connected access path. Startup now measures actual landing-support bounds and grounds the aircraft on the mat, preserving that pose for reset. A real normal product flow through Home, Hangar, Mission and Preflight reached Flight through the native command API: actor position (55,79,10.868225075082592) cm, model offset zero, Grounded and neutral. This proves spawn and native navigation, not physical keyboard/mouse causality or manual usability acceptance. See local v8-grounded-mcp.json and v8-grounded-player-camera.png evidence.

Fine gravel uses the verified CC0 Sandy Gravel 02 source at 2.53 m scale, blending base color, normal and roughness into the existing land across a grass collar. V7's visible polygon edge was rejected; v8 fades material before geometry sinks. Distant conifers use smaller irregular groves and low needle regeneration plants; they remain a draft pine treatment rather than Bay Area oak/chaparral habitat. Seven crossing spatially modulated analytic water components replace the previous three regular waves. This is not a spectral ocean or breaking-wave simulation.

Evidence correction: prior offscreen temporal runs paused the whole world through SetSafePaused, so those sequences establish camera/geometry evidence only and cannot establish animated ocean behavior. V8 freezes aircraft/input while explicitly advancing the world clock. All ten capture logs report TEMPORAL_ENVIRONMENT_CLOCK advancing=1 aircraft_paused=1. Normal product pause behavior is unchanged.

Frozen dirty base 108a521b205ec736ada5f5a75ac56ac8b4744525; source fingerprint 7751950fcabe7fe7a2ddf4b543613455e633105d2b78cc89117cd6d0fe041517; build ID AeroSim-Phase7-ProductShellA-20261016-v1; UE 5.8.3 / Mac Development / Apple M3 Pro 36 GB / Metal. Editor compile and Cook/Stage/Package/Archive PASS; UE Automation 58/58 clean successes, zero warnings/failures; Python 72/72 and ten static validators PASS. The importer reports 16 textures and six materials, zero errors/warnings. Current asset manifest coast-draft-20261017-v7 has 22 uassets and 20 source/derived images, SHA256 da3e483270af12fb3f91be275f7a25f692dc280fbfd46dc0cbcc83538145c93a. Actual tree geometry is 4,609,065 vertices and 2,941,043 triangles, within unchanged hard caps; this is not performance acceptance.

Matched packaged captures: launch-operator, launch-low, launch-return, cliff, north, overview, bridge, forest, mountain and south; ten views by 24 frames, 1920 by 1080, High, Clear 16:30, seed 21630, vertical FOV 60. All 240 frame identities/manifests/hashes PASS. Local evidence review v8-review.json SHA256 182bc161a70390e77b7c00da1a246868296f0e7aa86c45ebec5ba78c0b79f9ff. The v8-source-snapshot.zip preserves 162 relative source/asset files with individual hashes, archive SHA256 2ab5e6845663c58d6bc81a352684f97315eda97314b6b6c8574d27c004e04139. Matching logs and reports are in v8-build/. Raw captures, packages and source snapshot remain ignored local artifacts.

Root inspected all ten final PNGs. The prior bright polygonal apron edge is substantially softened, debug numbers/gates no longer dominate and launch equipment scale is credible. Remaining explicit failures: enlarged blurred ground beyond the gravel patch; sparse conifer-only mountain cover; weak erosion/habitat detail; cliff resembling a continuous extruded wall; insufficient wet-rock, foam and breaker interaction. Still images cannot prove motion naturalness, and pixel-change statistics are auxiliary. All-scene G0–G4 and user appearance acceptance remain unsigned, AAA NOT_ACHIEVED, formal named assets BLOCKED_ASSET, performance NOT_MEASURED. This candidate is reviewable development progress. Larger Shell UI save/relaunch/rollback acceptance remains OPEN; no mixed-tree Git commit/push or production promotion.


V8 desktop handoff: the desktop demo link now targets the archived UmunhumPackage v8 candidate; the previous ControlPackage target is recorded in ignored v8-desktop-rollback.json. Opened once with the opt-in control bridge. Native MCP inspect returned matching frozen source fingerprint, PID 38301, Home, possessed player, active mappings, physical neutral and grounded actor (55,79,10.868225075082592) cm. Inspect does not activate a window. No movement was injected at handoff.
