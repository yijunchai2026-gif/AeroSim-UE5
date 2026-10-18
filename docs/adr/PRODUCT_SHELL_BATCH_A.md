# Product Shell A: interface and verification boundary

2026-10-16. Source baseline: 108a521. UE installation verified as 5.8.3.

This batch replaces menu presentation with a native CommonUI activatable widget backed by Slate. GameMode owns application commands; ProductShell owns presentation navigation only. FlightModel, mission, camera, settings codec and Editor transactions retain their existing ownership. No binary project assets or external UI plugins are added.

Shared interface: ProductShell.h defines pages, settings categories/fields/actions and input kind. AeroSimGameMode.h exposes read-only snapshots and finite commands. UI never mutates mission or simulation structs directly. Existing Canvas remains a flight/perimeter fallback, not a second menu owner.

Write leases: UI-01/RD-07 owns new AeroSimProductWidget files; RD-01 owns Phase7Product and ProductUX settings contracts/tests; RD-02 owns FlightPawn and new PlayerController files/input tests; PMO integration owns GameMode, ProductShell, Build.cs, HUD adaptation, evidence and documentation. One local UE build/package lane only.

Expected behavior: clickable Home/Hangar/Mission/Preflight/Settings/Visual Lab/Result/Replay; visible primary action; keyboard/gamepad focus; safe settings and pause; 15-second preview recovery; settings persistence; explicit generic proxy and blocked named assets. Visual Lab MVP is a fixed-state, sequential quality A/B, not a claimed split-screen or production art tool.

Acceptance: static/Python and full UE Automation, UE 5.8.3 Mac Development compile, Cook/Stage/Package, actual Metal screenshots, input neutralization regression. UI must not allow flight controls behind menus. No automatic resume from focus loss/disconnect. User usability and human G0-G4 are separate NOT_RUN gates.

Budget: no added Tick-owned world/actor rebuild, no screenshot every frame, no duplicate scene render targets; existing Mac Development High budget remains diagnostic only. Source/build screenshot provenance is captured, not performance compliance.

Excluded: formal aircraft intake completion, production landscape/water conversion, FPV link simulation, Windows/iPad execution, Shipping hardware performance, user appearance/visual sign-off. Asset and platform blockers remain explicit.
