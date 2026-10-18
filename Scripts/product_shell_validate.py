#!/usr/bin/env python3
"""Bounded Product Shell source/architecture checks; never a visual PASS."""
from pathlib import Path
import sys
ROOT = Path(__file__).resolve().parents[1]

def main():
    errors = []
    checks = 0
    def require(condition, message):
        nonlocal checks
        checks += 1
        if not condition: errors.append(message)
    required = ["Source/AeroSim/Public/ProductShell.h", "Source/AeroSim/Private/ProductShell.cpp",
                "Source/AeroSim/Public/AeroSimProductWidget.h", "Source/AeroSim/Private/AeroSimProductWidget.cpp",
                "Source/AeroSim/Public/AeroSimPlayerController.h", "Source/AeroSim/Private/AeroSimPlayerController.cpp",
                "Source/AeroSim/Private/ProductEvidence.cpp", "docs/adr/PRODUCT_SHELL_BATCH_A.md"]
    for path in required: require((ROOT/path).is_file(), f"missing {path}")
    combined = "\n".join((ROOT/p).read_text() for p in required if (ROOT/p).is_file())
    for token in ["UCommonActivatableWidget", "SButton", "Home", "Hangar", "Mission", "Preflight", "VisualLab", "Result", "Replay",
                  "Display", "Graphics", "Audio", "Controls", "Camera", "Accessibility", "BLOCKED_ASSET"]:
        require(token in combined, f"missing shell token {token}")
    pawn = (ROOT/"Source/AeroSim/Private/FlightPawn.cpp").read_text()
    require("IsPhysicalInputNeutral" in pawn, "physical neutral resume gate missing")
    require("MapKey(InputSuite.PitchAction, EKeys::MouseY)" not in pawn, "passive mouse pitch mapping unsafe")
    require("MapKey(InputSuite.RollAction, EKeys::MouseX)" not in pawn, "passive mouse roll mapping unsafe")
    game = (ROOT/"Source/AeroSim/Private/AeroSimGameMode.cpp").read_text()
    for token in ["AdvancePreview", "SaveProductSettings", "SAVE FAILED", "source_fingerprint", "image_sha256", "NOT_MEASURED", "OnScreenshotRequestProcessed"]:
        require(token in game, f"missing runtime safety/evidence {token}")
    runtime = (ROOT/"Source/AeroSim/Public/AeroSimRuntimeContract.h").read_text()
    require('Contract = TEXT("aerosim.runtime.phase7/v1")' in runtime, "runtime schema changed")
    for error in errors: print(f"FAIL: {error}")
    print(f"PRODUCT SHELL STATIC {'FAIL' if errors else 'PASS'}: {checks} checks")
    return int(bool(errors))
if __name__ == "__main__": sys.exit(main())
