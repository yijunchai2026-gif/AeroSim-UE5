#!/usr/bin/env python3
"""Fail-closed static validation for the first P0 Unity-to-UE5 product UX migration."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    checks = 0
    errors: list[str] = []

    def require(condition: bool, message: str) -> None:
        nonlocal checks
        checks += 1
        if not condition:
            errors.append(message)

    required = (
        "docs/PRODUCT_DESIGN.md", "docs/UNITY_TO_UE5_MIGRATION_MATRIX.md",
        "Source/AeroSim/Public/ProductUX.h", "Source/AeroSim/Private/ProductUX.cpp",
        "Source/AeroSim/Private/Tests/AeroSimProductUXTest.cpp",
    )
    for relative in required:
        require((ROOT / relative).is_file(), f"missing {relative}")

    source = "\n".join((ROOT / p).read_text(encoding="utf-8") for p in (
        "Source/AeroSim/Public/ProductUX.h", "Source/AeroSim/Private/ProductUX.cpp",
        "Source/AeroSim/Private/AeroSimGameMode.cpp", "Source/AeroSim/Private/FlightPawn.cpp",
        "Source/AeroSim/Private/FlightHUD.cpp", "Source/AeroSim/Private/Phase7Product.cpp",
    ))
    for token in (
        "aerosim.product-ux/v1", "1280, 720", "1440, 900", "1920, 1080",
        "Windowed", "Borderless", "Fullscreen", "BeginPreview", "Apply", "Cancel", "Rollback",
        "Enter", "SpaceBar", "Escape", "F10", "debug compatibility only", "WriteCrashSafe",
        "NEXT ACTION", "HOLD LEFT SHIFT", "SAFE PAUSE / INPUT NEUTRAL",
    ):
        require(token in source, f"missing product UX token: {token}")

    runtime = (ROOT / "Source/AeroSim/Public/AeroSimRuntimeContract.h").read_text(encoding="utf-8")
    require('Contract = TEXT("aerosim.runtime.phase7/v1")' in runtime, "phase7 compatibility identity changed")
    require('ProductUxSchema = TEXT("aerosim.product-ux/v1")' in runtime, "product UX schema missing")

    docs = "\n".join((ROOT / p).read_text(encoding="utf-8") for p in required[:2])
    for token in ("Hangar", "Preflight", "Display Settings", "BLOCKED_ASSET", "Windows", "iPad", "NOT_RUN"):
        require(token in docs, f"truthful migration docs missing token: {token}")

    config_and_source = "\n".join(path.read_text(encoding="utf-8", errors="replace") for base in (ROOT / "Config", ROOT / "Source") for path in base.rglob("*") if path.is_file() and path.suffix.lower() in {".ini", ".h", ".cpp", ".cs"})
    require(re.search(r"(?im)^\s*(token|securitytoken|api[_-]?key|password|secret)\s*=\s*[^\s\[]", config_and_source) is None, "credential assignment found")
    require("AndroidFileServer" not in config_and_source, "AndroidFileServer must not be enabled in product migration")

    if errors:
        for error in errors:
            print(f"FAIL: {error}")
        print(f"PRODUCT UX STATIC VALIDATION FAILED: {len(errors)} error(s), {checks} checks")
        return 1
    print(f"PRODUCT UX STATIC VALIDATION PASS: {checks} checks")
    return 0


if __name__ == "__main__":
    sys.exit(main())
