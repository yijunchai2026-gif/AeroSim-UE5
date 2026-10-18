# Pacific Coast cliff prototype — scene ownership and layout

2026-10-17. User requests sky, clouds, mountains, a cliff with sea below, and woodland. Retain the empty Entry map: all visible land, cliff, sea, distant mountain silhouettes and trees have one project owner; do not restore the overlapping template Landscape.

Layout: existing training pad/route remains on a level headland at z=0. The irregular cliff edge lies east of the route (minimum x>12,000 cm), falling to a sea surface at z=-6,500 cm. A single continuous plateau mesh meets a banded cliff mesh at shared boundary coordinates. A distant inland mountain ridge frames the woodland behind the plateau. Seeded conifer groves sit on sampled land heights, outside the route corridor. Existing boardwalk and task markers remain prototypes. No named aircraft, imported production asset, or user visual approval is implied.

Implementation: UE 5.8.3 bundled ProceduralMeshComponent plugin, enabled explicitly; bounded native meshes created once, instanced tree trunks/canopies, engine-provided color-parameter material, and engine volumetric-cloud material. No new project binary assets, scripts exposed through MCP, duplicate scene rendering, per-frame geometry rebuild or external asset source. The project explicitly hides volumetric clouds outside Desktop High; Low still needs actual validation. Coast geometry remains a prototype pending normal-view G0–G4/user review.

Validation: topology/sea clearance/route setback/tree support tests, actual engine material-parameter check, static/Python, Mac compile/Automation/Cook/Package and Metal overview/moving captures. Reuse the opt-in offscreen capture harness; label new scene/camera protocol separately from the earlier ground-flicker comparison. Preserve the existing demo archive until a tested replacement is ready.

Presentation: Home adds an EXPLORE THE COAST entry. Visual Lab opens an offshore camera looking northwest and uses a side panel even before a mission, so the cliff face remains visible. The old core-acceptance rectangle is hidden and non-colliding in gameplay because its eastern edge would otherwise hang over the sea. Logical validation dimensions remain unchanged.

Limits: sea waves are static prototype geometry, not a water simulation. Existing flight altitude/floor behavior is unchanged and does not establish safe descent to the sea surface. Clouds use the engine material; production weather art, physical coast interactions, full-route human review and formal assets remain open.
