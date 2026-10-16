from __future__ import annotations

import json
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class Phase5ContractsTest(unittest.TestCase):
    def test_static_validator(self) -> None:
        run = subprocess.run([sys.executable, str(ROOT / "Scripts/phase5_static_validate.py")], cwd=ROOT, text=True, capture_output=True)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_runtime_and_editor_separation(self) -> None:
        runtime = (ROOT / "Source/AeroSim/Public/AeroSimRuntimeContract.h").read_text(encoding="utf-8")
        self.assertIn("aerosim.runtime.phase6/v1", runtime)
        self.assertIn("aerosim.runtime.phase5/v1", runtime)
        self.assertNotIn("AeroSimEditor", (ROOT / "Source/AeroSim/AeroSim.Build.cs").read_text(encoding="utf-8"))
        descriptor = json.loads((ROOT / "Plugins/AeroSimEditor/AeroSimEditor.uplugin").read_text(encoding="utf-8"))
        self.assertEqual([module["Type"] for module in descriptor["Modules"]], ["Editor"])

    def test_required_protocol_and_denials(self) -> None:
        code = (ROOT / "Plugins/AeroSimEditor/Source/AeroSimEditor/Private/AeroSimEditorMcp.cpp").read_text(encoding="utf-8")
        for operation in ("heartbeat", "inspect_project", "inspect_world", "list_actors", "inspect_actor", "inspect_component", "inspect_asset_metadata", "inspect_data_layers", "inspect_world_partition", "validate_scene", "capture_request"):
            self.assertIn(operation, code)
        for denial in ("READ_ONLY_DENIED", "shell", "python", "console", "network", "arbitrary_file", "arbitrary_uobject"):
            self.assertIn(denial, code)

    def test_no_sensitive_config(self) -> None:
        config = (ROOT / "Config/DefaultEngine.ini").read_text(encoding="utf-8")
        self.assertNotIn("AndroidFileServer", config)
        self.assertNotRegex(config, r"(?i)(token|securitytoken)\s*=")


if __name__ == "__main__":
    unittest.main()
