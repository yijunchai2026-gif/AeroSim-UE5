# Product Shell A validation

2026-10-16, America/Los_Angeles. Runtime remains `aerosim.runtime.phase7/v1`; this is the first native product-shell engineering batch, not a formal product release candidate.

## Expected behavior and acceptance

Users can see a main action and navigate Home → Hangar → Mission → Preflight → Flight without consulting README. Settings exposes Display, Graphics, Audio, Controls, Camera and Accessibility with staged changes, explicit Preview/Apply/Cancel/Rollback and a 15-second preview recovery. Flight input must never run behind menus; focus loss/disconnect requires explicit resume plus actual physical centering.

Expected → acceptance → actual → deviation → judgment → next step:

- Engineering: all source contracts, Python regression and UE Automation pass; actual compile and packaging must complete on UE 5.8.3 Mac Development. Missing platforms remain NOT_RUN.
- UI: native clickable controls, keyboard/gamepad focus and normal Metal frames; machine capture is evidence, not a human visual sign-off.
- Evidence: one traceable source fingerprint/build, real RHI, camera/environment/viewport metadata, image and canonical-manifest SHA256. Missing diagnostic metrics remain NOT_MEASURED, never replaced with budgets.
- Performance budget: reuse Phase 7 Mac diagnostic contract; no repeated world rebuilding or parallel scene render. Rolling diagnostics do not satisfy 120s warmup, 30min replay and three cold starts.

## Actual execution record

| Check | Current scope/result |
|---|---|
| Baseline Python | PASS, 32/32 before edits |
| Extended Python | PASS, 52/52 at integrated source checks |
| Source/static | PASS, all Phase 0/2/3/4/5/6/7, product UX, shell and legacy/team validators |
| UE 5.8.3 Mac Development compile | PASS for fingerprint `8e50168eefb72cc9cf6c2e4502ac15342454266ba510deb8cc81cb4ff887fb2a`; mouse-event diagnostic rebuild also passed |
| Full UE Automation | PASS, 54/54 for fingerprint `8e50168e…`; diagnostic-source repeat also PASS 54/54 |
| Cook/Stage/Package/Archive | PASS including pixel-capture repair for fingerprint `8e50168e…`; mouse-event diagnostic package also PASS; final acceptance remains open |
| Metal normal launch | PASS for nine normal Metal SM6 page launches, all exit 0; native interaction acceptance remains open |
| Screenshot capture | PASS for nine PNG/manifest integrity checks and expected page coverage at fingerprint `8e50168e…`; full diagnostic verification FAIL because draw-call counter is 0 / NOT_MEASURED |
| Human new-user three-minute first flight | NOT_RUN; no user study or timing data |
| Physical gamepad/controller | NOT_RUN; raw event/neutralization Automation is not hardware sign-off |
| Human normal-view G0-G4 | NOT_RUN; diagnostic image inspection cannot sign for the user |
| Formal aircraft/assets | BLOCKED_ASSET / BLOCKED_USER_CONFIRMATION |
| Windows / iPadOS | NOT_RUN |
| Non-Development target hardware qualification | BLOCKED_TARGET_HARDWARE_EVIDENCE |

Compiler and packaging logs, screenshots, native source stamps, Automation JSON and runtime diagnostics stay in ignored local output folders; they are not committed. Compact final hashes/results will be written here only after completion. Historical Phase 7 evidence is not reused as this batch's PASS.

## Follow-up execution, 2026-10-17 (America/Los_Angeles)

Python regression rerun PASS 52/52. All ten static validators and `git diff --check` passed. Existing final pixel-repair build, Automation (54/54) and Cook/Stage/Package/Archive reports match source fingerprint `8e50168eefb72cc9cf6c2e4502ac15342454266ba510deb8cc81cb4ff887fb2a` at base revision `108a521b205ec736ada5f5a75ac56ac8b4744525` with dirty source explicitly recorded.

Nine packaged Metal launches produced Home, Hangar, Mission, Preflight, Flight, Settings, VisualLab, Result and Replay images. Each process exited 0. Image/manifest SHA256, viewport dimensions, declared source/build/configuration and exact page coverage passed. The older `01-home.png` records and depicts Flight, so it is excluded from Home coverage; filenames are not used as page truth. The complete evidence verifier remains FAIL: every capture reports draw calls as zero and therefore `BLOCKED_NOT_MEASURED`. No performance PASS is claimed.

Native interaction is not yet accepted: Enter moved Home to Hangar, but subsequent mouse clicks on the mission CTA and Settings did not navigate. Window Raise and keyboard retry did not resolve it. Event instrumentation was added temporarily to distinguish widget dispatch from window/input-channel failures; the instrumented Editor build passed, packaged build verification passed; native interaction remains open. Mac sandbox Apply/relaunch/Rollback and preview timeout checks remain NOT_RUN in the actual UI; pure transaction Automation does not replace these checks. No commit or push has occurred because this engineering batch has not closed native interaction acceptance.

Event-diagnostic follow-up: source fingerprint `aa4979f273d1` (prefix only; full stamp is retained locally) compiled and Cook/Stage/Package/Archive completed with exit 0. One native mouse down/up pair reached the Slate action button, and Settings was displayed. Earlier click failures therefore do not establish an application defect: the automation channel's coordinates/focus remain unreliable. Rebinding, window Raise, a 200 ms key press and coordinate calibration were attempted. End-to-end native interaction remains NOT_RUN/incomplete pending a reliable input path or manual confirmation; the mouse-event diagnostic instrumentation is not a fix. The earlier failure observation is retained for traceability.

Result and Replay diagnostic launch images both show score 094, one sample, zero duration/distance and digest D9099C59. This checks agreement between the two displayed diagnostic states only; it does not prove a completed flight or real recorded mission. Native save/relaunch/rollback has not been claimed through direct file injection or diagnostic state setup.

Final completed engineering rerun for diagnostic source fingerprint `aa4979f273d136d60022bd4757ef898e2598b3cdcecde8fc71462a7e5c493f5f`: Mac Development build PASS; full UE Automation PASS 54/54; Cook/Stage/Package/Archive PASS. These do not close native UI input or sandbox-save acceptance. Local stage log SHA256:

- build: `1d74fc343be8a15528aa21293870ca56a12fe86b357b67d973f6d85492bb57ec` (exit 0).
- automation: `ffbad0b8286db197d26cf07f94407227f75b8127279486d82aa9ed6eafa65ca5` (exit 0).
- package: `480bb7c125495bf331c3b264a24b431f61ff3c1475d91263dd0db03ce799f016` (exit 0).

## Ground-flicker investigation and temporal capture (2026-10-17)

User confirms manual Audio category switching works. Earlier native automation click failures are not accepted as an application defect. The reported visible issue is persistent ground/scene flicker, not window focus flashing.

New diagnosis hypothesis: the engine default OpenWorld template Landscape overlaps the runtime-generated Pacific Coast floor. The explicit default is now the empty Entry map, with project-owned sky atmosphere; actual moving Metal comparison is pending. A Development-only fixed-camera 24-frame hold/move/hold capture path reads viewport pixels and records ordered, hashed metadata. The external Computer Use MCP service is unchanged. Offscreen operation and flicker outcome remain NOT_RUN until actual execution. See adr/PRODUCT_TEMPORAL_CAPTURE.md. Python sequence regressions and prior suites PASS 59/59; static validators PASS. Final source compile/Automation/package and temporal captures are pending. No user visual PASS, performance PASS, commit or push is inferred.

### Completed temporal comparison

Final source fingerprint `cb93b49f7f63eb49ff5282c0ce60e0aa11aabed08dc95c5c08d6424f30ade03c`, base revision `108a521b205ec736ada5f5a75ac56ac8b4744525`, dirty source explicitly recorded; UE 5.8.3, Mac Development, Metal, 1440×900. Build PASS, full UE Automation PASS 54/54, Cook/Stage/Package/Archive PASS. Python PASS 59/59 and all ten static validators PASS. The separately archived TemporalPackage preserves the earlier demo archive.

Both actual offscreen processes exited 0 and produced complete ordered sets of 24 PNG/manifest pairs, with verified hashes, constant capture identity, monotonic timestamps and the fixed hold/move/hold poses. The template run explicitly loaded `/Engine/Maps/Templates/OpenWorld`; the isolated run used no map override and actually loaded `/Engine/Maps/Entry`, verifying the new normal demo default. Both runs have the same source/build, RHI, quality, resolution, environment, vertical FOV and camera pose sequence. Capture directly reads viewport pixels; no Computer Use window/screenshot/input API was invoked during this comparison. This implements a project-owned internal sampling path; the external MCP service was not changed.

At the final stationary hold, the lower-center ground ROI (x 15–80%, y 55–88%) has mean absolute RGB change **24.699 → 0.518** (0–255 scale). Mean fraction of pixels changing by more than 20 is **41.819% → 0.000%** across seven adjacent sample pairs. Contact-sheet inspection shows the template ground pattern shifting at identical camera poses, while the isolated floor remains stable. This reproduces the reported artifact and supports overlapping template/runtime terrain as its cause. It is a bounded diagnostic finding, not a universal flicker guarantee or human G0–G4 sign-off. Camera-step motion blur is present in moving samples and is not classified as ground flicker.

Each requested sample is 0.3 seconds apart at minimum; actual timestamps are retained. PNG readback changes timing, so no performance claim is made. Draw calls remain NOT_MEASURED where zero; the full metric-completeness verifier is not relabeled PASS. Formal assets, appearance confirmation, target hardware and actual interactive save/relaunch/rollback acceptance remain open.

Local ignored outputs: `Artifacts/ProductShell/TemporalLogs/comparison.json` and `Artifacts/ProductShell/TemporalEvidence/` (comparison animation/contact sheets). Original frames and manifests remain in the Mac application's sandbox. Desktop demo shortcut now points at the tested TemporalPackage; an already running earlier process must be quit and reopened to pick up the fix.

Comparison report SHA256: `fea7e516b8a6cd23fbf99369582e6e7ea5a1f580fc27ef06b1ee2674779a7be1`.

- Temporal build log SHA256: `530cb9e03574d34abfc1914b3d07674c228246e3c6a8b8fca47b52b5ff2114cb` (exit 0).
- Temporal automation log SHA256: `b6f4d6ccb25540d762153d1733fc7a8e0d6e9461777c9fc76da67edf90ad43bd` (exit 0).
- Temporal package log SHA256: `4b29e21e9cfb6e104509f76b8deb3e22b83cb3cda826ebfa8dd7fb9b22c0db8c` (exit 0).

## Coastal cliff prototype — 2026-10-17

User-requested sky, clouds, mountains, cliff, sea below and woodland now have project-owned scene components in the Entry map. Plateau and cliff rings meet continuously; sea is 65 m below route level; 240 seeded trees avoid the training/boardwalk corridors. Old core guide bars are hidden and non-colliding in gameplay. Home adds EXPLORE THE COAST; Visual Lab uses an offshore camera and side panel so the cliff face is visible before a mission. Details/limits: adr/PACIFIC_COAST_CLIFF.md.

Base revision `108a521b205ec736ada5f5a75ac56ac8b4744525`, dirty source fingerprint `aabd97009774f1e625e0e568caaa360908133980b34fdaa19e27c420fa9b1bc0`, build `AeroSim-Phase7-ProductShellA-20261016-v1`, UE 5.8.3 Mac Development. Compile PASS, full UE Automation PASS 55/55 including actual material parameter, clockwise topology, sea clearance, route setback and tree-count checks. Cook/Package/Archive PASS, Python PASS 59/59 and ten static validators PASS.

Actual Metal run PASS: 24 ordered PNG/manifest pairs under `coastal-cliff-overview/v1`, actual map `/Engine/Maps/Entry`, viewport `[1920, 1080]`, clear 16:30, seed 21630, High. Image hashes and pose/identity/timestamps pass the sequence verifier. Offshore camera starts (40000,-42000,16000) cm, rotation (-17,145,0), moves eight steps (-300,+600,0) cm. Captures use viewport pixels with no Computer Use activation. Final stationary lower-center ROI mean absolute RGB change 0.197125; changed fraction over 20/255 is 0.000008360. This is a different scene/camera from the earlier flicker comparison and is not a comparative performance claim.

Actual rendered overview inspected: sky/clouds/ridge/forest/cliff/sea all visible, obsolete suspended guide rectangle absent. G0–G4 human acceptance and user visual sign-off remain NOT_RUN; simplified terrain/materials/water are prototypes. PNG readback affects timing, draw calls remain NOT_MEASURED, Low cloud fallback remains visually NOT_RUN. Full-route sea-level flight behavior, Windows/iPad, production content and interactive save/relaunch/rollback remain open. No whole-Shell engineering acceptance or Git commit/push is inferred.

Ignored local outputs: `Artifacts/ProductShell/CoastalPackage`, `CoastalEvidence/preview.png`, `sampled-motion.gif`, `contact-sheet.png`, and `TemporalLogs/CoastalOverview/comparison.json`; originals/manifests remain in the application's sandbox. Desktop shortcut now targets the tested CoastalPackage. Already running older processes require reopening.

- build log SHA256: `1d05d6fe46710b8116b2ceb81fe78f70e2209fd753391ef111632967ef1b9949`.
- automation log SHA256: `c66e92fa03c8aa86928a5ef0c938a2e8deef0f6b9e423f7b770261d1034facbb`.
- package log SHA256: `300d59068be3766a2eb94e5cc34cea1d4973a4d9c26945af4a39838c6caf4445`.

Additional actual packaged Metal Visual Lab capture exited 0; screenshot/manifest integrity PASS. Inspected native side panel and offshore scene together at 1920×1080. This verifies the rendered layout through the diagnostic entry, not physical mouse activation of the new Home action.

## Coastal naturalism development revision — 2026-10-17

The user explicitly requested one UI reviewer and two programmers. UI recorded independent rejection of the first three actual packaged revisions; programmers implemented closed terrain/foothills, branch/needle geometry and fixed draft asset import. Root integrated, inspected subsequent near/far/side images and corrected further failures. No later independent UI approval is claimed. This remains DRAFT_NOT_USER_CONFIRMED, not AAA quality or formal content acceptance.

Current source fingerprint `0b6b5f5286b32a3baadf5a2641560dafd016fa5c9b33c7c333e9ae9f6a7d39b5`, base revision `108a521b205ec736ada5f5a75ac56ac8b4744525`, dirty source explicitly recorded; UE 5.8.3, Mac Development. Compile PASS, full UE Automation PASS 57/57, Cook/Stage/Package/Archive PASS. Python PASS 66/66; ten static validators PASS; fixed draft asset local-path scan PASS. Draft environment version coast-draft-20261017-v3: 13 texture assets, 5 authored materials, 17 source/derived image files, all SHA-pinned. Environment manifest SHA256 `95bb95d7b7efb2b88ed09402dfc5319ac7a081559a721ff328372d6822ec92d1`. Source/author/license/physical scales are recorded in ENVIRONMENT_ASSET_SOURCES.md.

Terrain uses a shared closed surface: every indexed edge has two incident faces; north/south/west perimeter descends below water. The old detached land slab and disconnected mountains are removed. Trees sample the same height field, clear actual flight/boardwalk routes and use 720 hidden simple trunk proxies; visible tree meshes have no triangle collision. Actual tree geometry is 3,614,408 vertices / 2,306,588 triangles, including 1,511,696 needle triangles. This is an allocation diagnostic, not a performance result. High and Low still share geometry; Low only reduces cloud/water/shadow features. Proper instancing/geometry LOD, Shipping performance, memory/long-run qualification and target Windows/iPad evidence remain open.

An intermediate erosion run failed 81 shared-height assertions because inward cliff vertices entered the land-height footprint. The geometry was corrected to stay outside the shared rim; the assertion was preserved and the full suite rerun successfully. First textured forest captures were rejected for bare-looking trees. Six secondary shoots and 5–8 twigs per shoot improve near canopy coverage; bounded distance-mask compensation and macro cliff projections are the current experiment. The earlier isolated bright mountain material island is removed. Water uses opaque blue PBR plus animated normals, without fluid, breakers or shore-depth acceptance.

Final packaged Metal captures and visual findings are recorded below after inspection. Full diagnostic completeness is not PASS where draw calls remain zero/NOT_MEASURED. PNG readback changes frame timing; no FPS claim or all-route flicker guarantee. Formal assets, human G0–G4, user appearance acceptance, native UI sandbox save/relaunch/rollback and complete Shell A acceptance remain open. No Git commit/push or production promotion is inferred.

- build log SHA256: `c3fcab80a750eec726c09e1d024c77c57481b49888df50ad84cb1acac2bf68c0` (exit 0).
- automation log SHA256: `cbbbd8f0806f9f185dd32126058ea94d61650ab46f940345a774a44dfd1303b7` (exit 0).
- package log SHA256: `32be8857a741fa28c3bc7521555db03ee9f9dd9a99958c630dd9c40924dc1b84` (exit 0).

### Final v3 packaged view inspection

Five actual offscreen Metal runs exited 0, each producing 24 ordered PNG/manifest pairs with source, map, camera, environment, resolution, hash and timestamp identity checks PASS. All use the source fingerprint above, Apple M3 Pro, Metal, High, Entry map, Clear 16:30, seed 21630, vertical FOV 60 degrees, actual 1920×1080 pixels. Exact per-view poses/timestamps/image hashes and original sandbox directories are frozen in ignored local `NaturalEvidence/naturalism-v3-review.json` and V3Logs. Capture called no external Computer Use screenshot/input/focus API.

Root inspected final overview, forest, low cliff, north and south frames plus the forest hold/move/hold contact sheet. Distant groves now have readable needle crowns instead of bare sticks; near crowns retain sky gaps and photographed needle structure without the former solid ceiling. The bright hillside material island is absent. Broad cliff colour/detail breaks up the previous uniform tiny horizontal stripe pattern. Both side views show supported land descending into sea; no previous elevated open slab appears in these views. The seed remains fixed but changed geometry consumes a different deterministic random sequence, so revisions do not preserve individual tree placements for pixel comparison.

AAA target remains NOT_ACHIEVED: rounded mountains remain under-dressed, the cliff's large outline still resembles extruded/repeated curtains, flat forest floor lacks grounded undergrowth/litter, and the water lacks shore-depth/wet-edge/breaking-wave treatment. Independent UI re-review and user acceptance remain NOT_RUN/pending_user. These observed improvements support a development preview, not G0–G4 art sign-off. Full metric completeness still FAIL/NOT_MEASURED for draw calls, with no performance acceptance. The tested new NaturalPackage is a preview; previous CoastalPackage and NaturalPackage-v2 remain available for rollback. Desktop preview link is updated only after this capture set is verified. No Shell-wide acceptance, Git commit/push or formal content promotion.

Review report SHA256: `a4b92def8d42463751a41ef28ba463fd3c8164b1785f0e84628d0301c051c20b`.


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
