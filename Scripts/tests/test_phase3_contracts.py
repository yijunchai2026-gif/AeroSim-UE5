from __future__ import annotations

import json
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class Phase3ContractsTest(unittest.TestCase):
    def test_static_validator(self) -> None:
        completed = subprocess.run(
            [sys.executable, str(ROOT / "Scripts/phase3_static_validate.py")],
            cwd=ROOT,
            text=True,
            capture_output=True,
            check=False,
        )
        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)

    def test_manifest_fails_closed_without_formal_asset(self) -> None:
        manifest = json.loads(
            (ROOT / "SourceAssets/AircraftReference/phase3-aircraft-definition.json").read_text(encoding="utf-8")
        )
        definition = manifest["definition"]
        self.assertEqual(definition["status"]["engineering"], "BLOCKED_ASSET")
        self.assertEqual(definition["status"]["formalContent"], "CONTENT_NOT_DELIVERED")
        self.assertIsNone(definition["source"]["sha256"])
        self.assertIsNone(definition["identity"]["namedProductionModel"])
        self.assertFalse(definition["identity"]["trademarkTextures"])
        self.assertIn("USER_CONFIRMED", definition["qualityGates"]["blocked"])

    def test_camera_and_aircraft_automation_contracts(self) -> None:
        tests = (ROOT / "Source/AeroSim/Private/Tests/AeroSimPhase3Test.cpp").read_text(encoding="utf-8")
        names = {
            line.split('"')[1]
            for line in tests.splitlines()
            if '"AeroSim.Phase3.' in line
        }
        self.assertGreaterEqual(len(names), 5)
        self.assertTrue(all(name.startswith("AeroSim.Phase3.") for name in names))
        for required in ("VisibilityCollisionLodFov", "ScaleCenterSupport", "FixedCameraContracts", "GimbalLimitsSmoothingHorizon"):
            self.assertTrue(any(required in name for name in names), required)

    def test_runtime_contract_and_no_android_credentials(self) -> None:
        runtime = (ROOT / "Source/AeroSim/Public/AeroSimRuntimeContract.h").read_text(encoding="utf-8")
        self.assertIn("aerosim.runtime.phase3/v1", runtime)
        engine_ini = (ROOT / "Config/DefaultEngine.ini").read_text(encoding="utf-8")
        self.assertNotIn("AndroidFileServer", engine_ini)
        self.assertNotRegex(engine_ini, r"(?i)(token|securitytoken)\s*=")


if __name__ == "__main__":
    unittest.main()
