#!/usr/bin/env python3
"""Static Phase 3 contract checks requiring only Python's standard library."""
from __future__ import annotations

import json
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
        "PHASE3.md",
        "docs/PHASE3_VALIDATION.md",
        "Source/AeroSim/Public/Phase3Aircraft.h",
        "Source/AeroSim/Private/Phase3Aircraft.cpp",
        "Source/AeroSim/Private/Tests/AeroSimPhase3Test.cpp",
        "SourceAssets/AircraftReference/phase3-aircraft-definition.json",
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
        "FAircraftDefinition",
        "EAircraftAssetStatus::BlockedAsset",
        "MakeGenericGolden",
        "FAircraftVariant",
        "FAircraftLodContract",
        "FAircraftMaterialSlot",
        "FGimbalController",
        "MinimumPitchDegrees = -90.0F",
        "MaximumYawDegrees = 75.0F",
        "EFlightCameraMode::Gimbal",
        "SetUsingAbsoluteRotation",
        "bDoCollisionTest",
        "FVerticalFovTransaction",
        "FPhase3EvidenceCameraContract",
        "AeroSim.Phase3",
        "BLOCKED_ASSET / GENERIC PROXY",
        "NavigationLights",
        "LandingSupports",
        "LOD1_Proxy",
    )
    for token in tokens:
        require(token in source, f"missing Phase 3 token: {token}")

    manifest_path = ROOT / "SourceAssets/AircraftReference/phase3-aircraft-definition.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    definition = manifest["definition"]
    require(manifest["schema"] == "aerosim.aircraft-definition/phase3-v1", "wrong manifest schema")
    require(manifest["runtimeContract"] == "aerosim.runtime.phase3/v1", "wrong manifest runtime contract")
    require(definition["status"]["engineering"] == "BLOCKED_ASSET", "generic aircraft must stay BLOCKED_ASSET")
    require(definition["status"]["formalContent"] == "CONTENT_NOT_DELIVERED", "formal content must remain undelivered")
    require(definition["identity"]["namedProductionModel"] is None, "generic proxy cannot name a production model")
    require(definition["identity"]["trademarkTextures"] is False, "trademark textures are forbidden")
    require(definition["source"]["sha256"] is None, "programmatic proxy cannot invent a source model hash")
    require(len(definition["materialSlots"]) >= 5, "material slot contract incomplete")
    require(len(definition["lod"]) >= 2 and any(item["proxy"] for item in definition["lod"]), "LOD proxy contract incomplete")
    require("USER_CONFIRMED" in definition["qualityGates"]["blocked"], "user confirmation gate must remain blocked")

    engine_ini = (ROOT / "Config/DefaultEngine.ini").read_text(encoding="utf-8")
    require("AndroidFileServer" not in engine_ini, "AndroidFileServer section must be removed")
    require(re.search(r"(?i)(token|securitytoken)\s*=", engine_ini) is None, "token entry must be removed")
    require(not list((ROOT / "Content").rglob("*.uasset")), "Phase 3 pure-C++ delivery must not add binary uassets")

    if errors:
        for error in errors:
            print(f"FAIL: {error}")
        print(f"PHASE3 STATIC VALIDATION FAILED: {len(errors)} error(s), {checks} checks")
        return 1
    print(f"PHASE3 STATIC VALIDATION PASS: {checks} checks")
    return 0


if __name__ == "__main__":
    sys.exit(main())
