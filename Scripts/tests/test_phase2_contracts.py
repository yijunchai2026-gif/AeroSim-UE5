from __future__ import annotations

import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class Phase2ContractsTest(unittest.TestCase):
    def test_phase2_static_validator(self) -> None:
        completed = subprocess.run(
            [sys.executable, str(ROOT / "Scripts/phase2_static_validate.py")],
            cwd=ROOT,
            text=True,
            capture_output=True,
            check=False,
        )
        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)

    def test_phase2_contract_and_automation_names(self) -> None:
        contract = (ROOT / "Source/AeroSim/Public/AeroSimRuntimeContract.h").read_text(encoding="utf-8")
        tests = (ROOT / "Source/AeroSim/Private/Tests/AeroSimPhase2Test.cpp").read_text(encoding="utf-8")
        self.assertIn("aerosim.runtime.phase2/v1", contract)
        names = {
            line.split('"')[1]
            for line in tests.splitlines()
            if '"AeroSim.Phase2.' in line
        }
        self.assertGreaterEqual(len(names), 7)
        self.assertTrue(all(name.startswith("AeroSim.Phase2.") for name in names))

    def test_no_binary_content_or_android_token(self) -> None:
        self.assertEqual(list((ROOT / "Content").rglob("*.uasset")), [])
        engine_ini = (ROOT / "Config/DefaultEngine.ini").read_text(encoding="utf-8")
        self.assertNotIn("AndroidFileServer", engine_ini)
        self.assertNotIn("SecurityToken=", engine_ini)


if __name__ == "__main__":
    unittest.main()
