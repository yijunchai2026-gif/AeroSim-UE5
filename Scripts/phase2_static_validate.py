#!/usr/bin/env python3
"""Static Phase 2 contract checks requiring only Python's standard library."""
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
        "PHASE2.md",
        "Source/AeroSim/Public/Phase2Systems.h",
        "Source/AeroSim/Private/Phase2Systems.cpp",
        "Source/AeroSim/Private/Tests/AeroSimPhase2Test.cpp",
    )
    for relative in required:
        require((ROOT / relative).is_file(), f"missing {relative}")

    source = "\n".join(
        path.read_text(encoding="utf-8", errors="replace")
        for root in (ROOT / "Source", ROOT / "Config")
        for path in root.rglob("*")
        if path.is_file() and path.suffix.lower() in {".h", ".cpp", ".ini"}
    )
    tokens = (
        "aerosim.runtime.phase3/v1",
        "EAeroSimMissionState",
        "Hangar",
        "Preflight",
        "Result",
        "Replay",
        "FVerticalFovTransaction",
        "VerticalToHorizontal",
        "bDoCollisionTest",
        "FEnvironmentState",
        "TimeMinutes = 16 * 60 + 30",
        "Seed = 21630",
        "ResumeCountdownSeconds",
        "FDeterministicFlightTrace",
        "AeroSim.Phase2",
        "CoreWidthCm = 30000.0F",
        "CoreLengthCm = 50000.0F",
    )
    for token in tokens:
        require(token in source, f"missing Phase 2 token: {token}")

    require(not validate_environment_draft(ROOT), "Only manifested, hash-verified Phase7 development assets are permitted")
    engine_ini = (ROOT / "Config/DefaultEngine.ini").read_text(encoding="utf-8")
    require("AndroidFileServer" not in engine_ini, "AndroidFileServer section must be removed")
    require(re.search(r"(?i)(token|securitytoken)\s*=", engine_ini) is None, "token entry must be removed")

    if errors:
        for error in errors:
            print(f"FAIL: {error}")
        print(f"PHASE2 STATIC VALIDATION FAILED: {len(errors)} error(s), {checks} checks")
        return 1
    print(f"PHASE2 STATIC VALIDATION PASS: {checks} checks")
    return 0


if __name__ == "__main__":
    sys.exit(main())
