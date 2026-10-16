from __future__ import annotations

import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]


def load_script(name: str):
    path = ROOT / "Scripts" / name
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader
    spec.loader.exec_module(module)
    return module


class Phase7ContractsTest(unittest.TestCase):
    def test_static_validator(self) -> None:
        run = subprocess.run([sys.executable, str(ROOT / "Scripts/phase7_static_validate.py")], cwd=ROOT, text=True, capture_output=True)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_release_manifest_is_fail_closed(self) -> None:
        module = load_script("phase7_release_manifest.py")
        manifest = module.build_manifest()
        self.assertEqual(manifest["runtime_contract"], "aerosim.runtime.phase7/v1")
        self.assertEqual(manifest["platforms"]["Windows"], "NOT_RUN")
        self.assertEqual(manifest["platforms"]["iPadOS"], "NOT_RUN")
        self.assertEqual(manifest["release_gates"]["formal_aircraft"], "BLOCKED_ASSET")
        self.assertEqual(manifest["release_gates"]["user_appearance_confirmation"], "BLOCKED_USER_CONFIRMATION")
        self.assertTrue(all(len(entry["sha256"]) == 64 for entry in manifest["files"]))

    def test_soak_runner_missing_executable_is_blocked(self) -> None:
        module = load_script("phase7_soak.py")
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "report.json"
            argv = ["phase7_soak.py", "--executable", str(Path(directory) / "missing"), "--seconds", "1", "--rhi", "nullrhi", "--output", str(output)]
            with mock.patch.object(sys, "argv", argv):
                self.assertEqual(module.main(), 1)
            report = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(report["status"], "BLOCKED_EXECUTABLE_MISSING")
            self.assertEqual(report["performance_claim"], "NOT_MEASURED")

    def test_no_runtime_editor_dependency(self) -> None:
        build = (ROOT / "Source/AeroSim/AeroSim.Build.cs").read_text(encoding="utf-8")
        self.assertNotIn("AeroSimEditor", build)
        runtime = (ROOT / "Source/AeroSim/Public/AeroSimRuntimeContract.h").read_text(encoding="utf-8")
        self.assertIn("aerosim.runtime.phase7/v1", runtime)
        transaction = (ROOT / "Plugins/AeroSimEditor/Source/AeroSimEditor/Private/AeroSimPhase6Transaction.cpp").read_text(encoding="utf-8")
        self.assertIn("PENDING_REVIEW_SANDBOX", transaction)
        self.assertIn("production_touched", transaction)


if __name__ == "__main__":
    unittest.main()
