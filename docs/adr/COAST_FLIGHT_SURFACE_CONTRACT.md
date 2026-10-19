# Coast flight surface contract — AAA-R3-FLIGHT-02 spike

2026-10-18. Baseline integration 894ab6a; isolated owner DEV-FLIGHT. This freezes the implementation interface before source edits. PMO authorizes FlightPawn, Phase4Mission, necessary GameMode/HUD transforms, Graybox.cpp route-contract section and only TakeoffPadRadiusCm=90 plus coordinate comments in Graybox.h. Environment geometry, trees, ocean, materials and asset manifests are unchanged. No UE build/Cook/package is owned by this lane.

## Coordinates and surface authority

FModel.Position remains centimetres relative to InitialTransform. Runtime surface queries are world-space and explicitly inverse-transform supported actor-origin heights. Model.Advance accepts an explicit deterministic FFlightSurfaceContext: valid support, supported model Z, landability and water contact. Its no-context default describes the legacy flat plane only for existing isolated model tests; the Pawn always supplies real query results, never this fallback. Flight has no launch-height floor. HUD distinguishes above-surface/water clearance from launch-relative altitude. Mission receives resolved world coordinates after movement; task checkpoints, safety and recovery are world-space.

A short downward world-static query checks the footprint, walking normals and actual support. Landing depends on support beneath the vehicle, not a zero-Z plane or a negative-throttle key alone. Unsupported descent remains Flight. Positive throttle can cancel Landing. Ground contact resolves feet to the sampled height; water is explicitly not landable, triggers neutral safe pause/incident, and cannot produce Grounded/Result. Reset still returns to the original bounds-grounded mat pose.

## Movement and safety

Sweep is authoritative. After sweep, synchronize model position/velocity from the actual actor and only then append trace/observe the mission. A blocked move cannot count a checkpoint behind a wall. Preserve pause, focus-loss, disconnect, physical-recenter countdown and semantic input arbitration. Plane-only model tests remain meaningful as explicitly supported-surface tests; new tests must cover missing support and negative world altitude.

PMO approved the generic assisted-quad command change after the old mandatory 5 m/s forward speed proved incompatible with a two-metre mat. Existing semantic axes/keys stay: Pitch forward/back, Roll sideways, Yaw turn, Throttle vertical. Planar target is capped at 1800 cm/s including diagonals, vertical at 400 cm/s, yaw at 50 degrees/s. First-order velocity response is 1.8/s (an 18 m/s neutral release has roughly a 10 m asymptotic braking distance); use staged deceleration. No input targets zero velocity and levels to zero attitude. Presentation tilt targets at most 18 degrees, smoothed with the same response. Takeoff initially rises vertically. Contact landing permits <=40 cm/s planar speed and <=8 degrees pitch/roll, with real support within 45 cm plus one-step descent anticipation. Fast/tilted ground impact is an incident, not a falsely safe Grounded. Positive vertical command aborts Landing. These are prototype controller choices, not validated named-aircraft physics. HUD and actual native Preflight/Controls text describe the same semantics.

Safety is a union of bounded route segments, the existing limited launch/core region, and a bounded forest corridor route, with a finite sea-relative floor and finite top. It is not an enormous all-safe world box. Recovery chooses a nearest legal route point with terrain clearance and never silently teleports the actor. World/model conversion is explicit and tested at nonzero spawn offsets. Out-of-route movement remains a recoverable boundary incident.

## Route and home completion

Keep five ordered inspection stages and retain a genuine low-water target. Freeze a short representative forest segment linked to the existing corridor entrance; retain separate full-corridor geometric/safety coverage and later real traversal. Checkpoint coordinates must correspond to actual terrain/shore geometry, including a sea target 4 m above SeaLevel and outside the cliff foot. No diagnostic camera/review pose is route evidence. Initial route duration remains a 60–90 s target, NOT_MEASURED until real input traversal; full forest loop is a separate test if its length makes the first slice exceed the target.

RTH approach may use a larger region to permit deceleration, but completion requires actual grounded support, low speed and the vehicle footprint within the real mat. The approved 90 cm radius is an origin/home threshold, not a replacement for support validation. A field landing, unsupported Grounded or water contact cannot finish. Existing route logic tests will be updated only where their implicit flat-world/18 m landing assumptions conflict with this contract, with reasons documented.

## Gates

This spike delivers code, source/static verification and tests ready for root's serialized UE train. Source assertions are not real flight PASS. Exit requires engine tests, an actual menu-started semantic-input coast/forest/water/return traversal, distinct collision/water/landing/negative-Z pause tests, and same-build motion evidence. Low water remains an end goal even if the spike exposes further issues. No AAA or user acceptance is inferred.
