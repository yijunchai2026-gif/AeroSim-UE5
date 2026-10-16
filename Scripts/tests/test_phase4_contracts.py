from __future__ import annotations

import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class Phase4ContractsTest(unittest.TestCase):
    def test_static_validator(self) -> None:
        completed = subprocess.run(
            [sys.executable, str(ROOT / "Scripts/phase4_static_validate.py")],
            cwd=ROOT,
            text=True,
            capture_output=True,
            check=False,
        )
        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)

    def test_runtime_and_product_loop_contract(self) -> None:
        runtime = (ROOT / "Source/AeroSim/Public/AeroSimRuntimeContract.h").read_text(encoding="utf-8")
        mission = (ROOT / "Source/AeroSim/Private/Phase4Mission.cpp").read_text(encoding="utf-8")
        self.assertIn("aerosim.runtime.phase4/v1", runtime)
        for stage in ("Takeoff", "Inspection", "ReturnToHome", "Landing", "Result", "Replay"):
            self.assertIn(f"EPhase4MissionStage::{stage}", mission)
        for dimension in ("Score.Safety", "Score.Precision", "Score.Efficiency", "Score.Smoothness"):
            self.assertIn(dimension, mission)

    def test_recovery_and_accessibility_are_not_color_only(self) -> None:
        mission = (ROOT / "Source/AeroSim/Private/Phase4Mission.cpp").read_text(encoding="utf-8")
        hud = (ROOT / "Source/AeroSim/Private/FlightHUD.cpp").read_text(encoding="utf-8")
        for cue in ("INWARD CHEVRONS", "STOP OCTAGON", "CHECKMARK", "HOME DIAMOND"):
            self.assertIn(cue, mission)
        self.assertIn("SHAPE:", hud)
        self.assertIn("central flight corridor is always unobstructed", hud)

    def test_formal_asset_and_edit_surfaces_stay_blocked(self) -> None:
        all_source = "\n".join(
            path.read_text(encoding="utf-8", errors="replace")
            for root in (ROOT / "Source", ROOT / "Plugins")
            for path in root.rglob("*")
            if path.suffix.lower() in {".h", ".cpp"}
        )
        self.assertIn("BLOCKED_ASSET / GENERIC PROXY", all_source)
        for prohibited in ("ExecutePython", "CreateProcess(", "AIEdit", "ApplyTransform"):
            self.assertNotIn(prohibited, all_source)
        engine = (ROOT / "Config/DefaultEngine.ini").read_text(encoding="utf-8")
        self.assertNotIn("AndroidFileServer", engine)
        self.assertNotRegex(engine, r"(?i)(token|securitytoken)\s*=")

    def test_phase4_automation_suite_is_declared(self) -> None:
        tests = (ROOT / "Source/AeroSim/Private/Tests/AeroSimPhase4Test.cpp").read_text(encoding="utf-8")
        names = {line.split('"')[1] for line in tests.splitlines() if '"AeroSim.Phase4.' in line}
        self.assertGreaterEqual(len(names), 5)
        self.assertTrue(any("ScoreReplayDeterminism" in name for name in names))
        self.assertTrue(any("RecoverBoundaryCollisionMissedGate" in name for name in names))


if __name__ == "__main__":
    unittest.main()
