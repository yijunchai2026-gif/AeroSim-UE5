from __future__ import annotations

import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class ProductUxMigrationTest(unittest.TestCase):
    def test_static_validator(self) -> None:
        run = subprocess.run([sys.executable, str(ROOT / "Scripts/product_ux_static_validate.py")], cwd=ROOT, text=True, capture_output=True)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_primary_flow_does_not_teach_m(self) -> None:
        hud = (ROOT / "Source/AeroSim/Private/FlightHUD.cpp").read_text(encoding="utf-8")
        self.assertIn("ENTER / SPACE", hud)
        self.assertIn("M  debug shortcut only", hud)
        self.assertNotIn("CONFIRM / M", hud)

    def test_phase7_schema_remains_compatible(self) -> None:
        runtime = (ROOT / "Source/AeroSim/Public/AeroSimRuntimeContract.h").read_text(encoding="utf-8")
        self.assertIn('aerosim.runtime.phase7/v1', runtime)
        self.assertIn('aerosim.product-ux/v1', runtime)
        codec = (ROOT / "Source/AeroSim/Private/Phase7Product.cpp").read_text(encoding="utf-8")
        for field in ("resolution_width", "resolution_height", "window_mode"):
            self.assertIn(field, codec)


if __name__ == "__main__":
    unittest.main()
