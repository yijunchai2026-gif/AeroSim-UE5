# R3 bounded engineering batch: runtime UV metadata and exterior sea depth

2026-10-18. State: scoped engineering verification PASS; AAA NOT_ACHIEVED; user appearance pending_user. Flight/safety/mission reachability remains an open P0 and is being implemented separately. This batch does not certify complete task playability, general ocean realism, geometry art, motion/LOD, production assets or target performance.

Built dirty base: 894ab6a15955409dd72a5f37bb855a5a0d226c91. Frozen source fingerprint: 3c34d3eb6a25bc571ed2b4bd9b55c28a0bdbda0720d147c9bc5b547c6f9a6754. Build ID AeroSim-Phase7-ProductShellA-20261016-v1, UE 5.8.3, Mac Development, Apple M3 Pro/36 GB, Metal SM6. Round/source fingerprint disambiguates the reused historical build ID. No asset bytes changed; the existing 106-asset/210-source whitelist and hashes remain valid.

Runtime fast-built tree archetypes now derive UV0 density from actual LOD0 positions/UVs and initialize both material slots before BuildFromMeshDescriptions. All 13 archetypes /26 slots are covered; invalid topology/nonfinite data fails closed. No texture streaming or ensure suppression, no geometry, materials, density or LOD artwork changes. Regressions exercise units/UV scaling/degeneracy and actual mesh/ISM streaming access.

Ocean depth masks now default to the real 175 m sea-floor depth, replacing that value only inside supported shelf/land footprints. Former landward exterior ocean was unconditionally assigned 250 cm shallow water, producing the rectangular colour edge. Regression includes negative-X north/south/west exterior, shelf-end continuity and legitimate shallow shoreline. The same floor elevation is used by the actual floor/shelf geometry. No opaque-water or fog workaround.

Verification:

- Editor compile PASS; Python79 PASS; ten applicable static validators PASS (there is no separate phase1_static_validate.py in this tree).
- Full NullRHI Automation:68 clean +3warning,0failed. Two warnings are engine HTTP probes; one explicitly marks Metal material test NOT_RUN under NullRHI.
- Separate real Metal SM6 material Automation:1 clean PASS,0warnings/failed/notRun; water, ground, rock, gravel and cloud resources all compile with zero material errors.
- Cook/Stage/Package/Archive PASS; actual packaged Metal startup/capture PASS.
- Three fresh background packaged starts each log26 initialized positive UV-density slots, with no former UVChannelData ensure, no FLORA_UV_STREAMING_INVALID or other handled ensure. Normal Home→Hangar→Mission→Preflight→Flight→Settings→Flight was exercised in the first successful run. Flight→Home was not exercised; back opens safe Settings and is not a Home command.
- Same-build MCP captures:9 viewpoints,29frames, including north/south/shore/forest six-frame unpaused sequences,1920×1080 High Clear16:30 seed21630 vFOV60. All original/canonical-name copied images pass integrity/hash verification. Root inspected actual north/south/cliff/shore; separate pixel review is in AAA-R3-VISUAL_REVIEW.md. The prior rectangular edge is absent at sampled north/south views. Parallel wave trains, weak surf, smooth bay trough and flat crowns remain visibly open.
- First capture attempt completed29frames but a subsequent inspect returned transient missing session.json; the QA harness stopped its own app in finally. This is not evidence of app crash, and a transport heartbeat-read race is not yet a proven root cause. The first attempt is preserved as qa-attempt-1; the successful rerun and three-cold-start evidence are separate. Do not silently relabel the failed protocol attempt PASS.

Local ignored evidence: Artifacts/ProductShell/AAA/R3/build/, MetalAutomation/, qa/, qa-attempt-1/, logs. Successful qa/results.json SHA256 a90bcc1207e2685b5ec497f13bea7f06c2c7a7febd664f633fb8e356455858ef. No raw captures/logs/packages/caches/credentials belong in Git. Snapshot readback affects timing; formal performance NOT_MEASURED.

Source changes are four explicit C++ files: PacificCoastTrees.cpp, PacificCoastOcean.cpp, AeroSimCoastalFloraStreamingTest.cpp and AeroSimCoastalSeaSkyTest.cpp. Documentation is committed as the planning/evidence record. Any later flight/hero-bay/archetype changes require a new fingerprint and validation batch; R3 evidence cannot cover them.
