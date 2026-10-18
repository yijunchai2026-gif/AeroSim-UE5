#!/usr/bin/env python3
"""Static Phase 4 product/mission contract checks using only the standard library."""
from __future__ import annotations
from environment_draft_policy import validate_environment_draft

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    errors: list[str] = []
    checks = 0

    def require(condition: bool, message: str) -> None:
        nonlocal checks
        checks += 1
        if not condition:
            errors.append(message)

    required = (
        "PHASE4.md",
        "docs/PHASE4_VALIDATION.md",
        "Source/AeroSim/Public/Phase4Mission.h",
        "Source/AeroSim/Private/Phase4Mission.cpp",
        "Source/AeroSim/Private/Tests/AeroSimPhase4Test.cpp",
    )
    for relative in required:
        require((ROOT / relative).is_file(), f"missing {relative}")

    source = "\n".join(
        path.read_text(encoding="utf-8", errors="replace")
        for root in (ROOT / "Source", ROOT / "Plugins", ROOT / "Config")
        for path in root.rglob("*")
        if path.is_file() and path.suffix.lower() in {".h", ".cpp", ".ini"}
    )
    tokens = (
        "aerosim.runtime.phase4/v1",
        "FPhase4TaskDefinition",
        "pacific-coast-inspection",
        "CurrentVersion = 1",
        "FPhase4Checkpoint",
        "SafeHalfLengthCm",
        "FPhase4PreflightChecklist",
        "EPhase4MissionStage::Takeoff",
        "EPhase4MissionStage::Inspection",
        "EPhase4MissionStage::ReturnToHome",
        "EPhase4MissionStage::Landing",
        "EPhase4MissionStage::Result",
        "EPhase4MissionStage::Replay",
        "EPhase4Incident::Boundary",
        "EPhase4Incident::Collision",
        "EPhase4Incident::MissedCheckpoint",
        "GetRecoveryPoint",
        "Safety",
        "Precision",
        "Efficiency",
        "Smoothness",
        "ReplayDigest",
        "GetProgressiveHint",
        "INWARD CHEVRONS",
        "STOP OCTAGON",
        "Gamepad_DPad_Down",
        "PauseSettings",
        "ResumeCentered",
        "BLOCKED_ASSET / GENERIC PROXY",
        "AeroSim.Phase4",
        "Intentionally exposes no Python, shell, console-command, file-write, or process APIs",
    )
    for token in tokens:
        require(token in source, f"missing Phase 4 token: {token}")

    runtime = (ROOT / "Source/AeroSim/Public/AeroSimRuntimeContract.h").read_text(encoding="utf-8")
    require(any(f'Phase = TEXT("Phase{phase}")' in runtime for phase in (4, 5, 6, 7)), "runtime phase regressed below Phase4")
    require('ContractVersion = 1' in runtime, "runtime version is not v1")

    hud = (ROOT / "Source/AeroSim/Private/FlightHUD.cpp").read_text(encoding="utf-8")
    require("central flight corridor is always unobstructed" in hud, "HUD center-view safety contract missing")
    require("SHIELD  SAFETY" in hud and "TARGET  PRECISION" in hud, "accessible score labels missing")
    require("SHAPE:" in hud, "incident shape cue missing")

    engine_ini = (ROOT / "Config/DefaultEngine.ini").read_text(encoding="utf-8")
    require("AndroidFileServer" not in engine_ini, "AndroidFileServer section must be removed")
    require(re.search(r"(?i)(token|securitytoken)\s*=", engine_ini) is None, "token entry must be removed")
    require(not validate_environment_draft(ROOT), "Only manifested, hash-verified Phase7 development assets are permitted")

    prohibited = ("ExecutePython", "CreateProcess(", "FPlatformProcess::ExecProcess", "AIEdit", "ApplyTransform")
    for token in prohibited:
        require(token not in source, f"prohibited MCP/AI edit surface found: {token}")

    if errors:
        for error in errors:
            print(f"FAIL: {error}")
        print(f"PHASE4 STATIC VALIDATION FAILED: {len(errors)} error(s), {checks} checks")
        return 1
    print(f"PHASE4 STATIC VALIDATION PASS: {checks} checks")
    return 0


if __name__ == "__main__":
    sys.exit(main())
