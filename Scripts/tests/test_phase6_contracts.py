from __future__ import annotations

import json
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class Phase6ContractsTest(unittest.TestCase):
    def test_static_validator(self) -> None:
        run = subprocess.run([sys.executable, str(ROOT / "Scripts/phase6_static_validate.py")], cwd=ROOT, text=True, capture_output=True)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_runtime_and_editor_separation(self) -> None:
        runtime = (ROOT / "Source/AeroSim/Public/AeroSimRuntimeContract.h").read_text(encoding="utf-8")
        self.assertIn("aerosim.runtime.phase6/v1", runtime)
        self.assertNotIn("AeroSimEditor", (ROOT / "Source/AeroSim/AeroSim.Build.cs").read_text(encoding="utf-8"))
        descriptor = json.loads((ROOT / "Plugins/AeroSimEditor/AeroSimEditor.uplugin").read_text(encoding="utf-8"))
        self.assertEqual([module["Type"] for module in descriptor["Modules"]], ["Editor"])

    def test_finite_transaction_surface(self) -> None:
        code = (ROOT / "Plugins/AeroSimEditor/Source/AeroSimEditor/Private/AeroSimPhase6Transaction.cpp").read_text(encoding="utf-8")
        dispatcher = (ROOT / "Plugins/AeroSimEditor/Source/AeroSimEditor/Private/AeroSimEditorMcp.cpp").read_text(encoding="utf-8")
        for operation in ("transaction_dry_run", "transaction_snapshot", "transaction_apply", "transaction_validate", "transaction_evidence", "transaction_commit", "transaction_rollback"):
            self.assertIn(operation, dispatcher)
        for contract in ("aerosim.scene-patch/v1", "PENDING_REVIEW_SANDBOX", "production_touched", "snapshot_sha256", "SKIPPED_UPSTREAM_FAILURE"):
            self.assertIn(contract, code)

    def test_no_phase7_or_dangerous_surface(self) -> None:
        code = "\n".join(p.read_text(encoding="utf-8", errors="replace") for base in (ROOT / "Source", ROOT / "Plugins") for p in base.rglob("*") if p.suffix in {".h", ".cpp", ".cs"})
        for forbidden in ("FPlatformProcess::CreateProc", "GEngine->Exec", "StaticLoadObject", "ProcessEvent(", "FHttpModule"):
            self.assertNotIn(forbidden, code)
        transaction = (ROOT / "Plugins/AeroSimEditor/Source/AeroSimEditor/Private/AeroSimPhase6Transaction.cpp").read_text(encoding="utf-8")
        for phase7 in ("optimize_lod", "ai_layout", "optimize_lighting"):
            self.assertNotIn(phase7, transaction)


if __name__ == "__main__":
    unittest.main()
