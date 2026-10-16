from __future__ import annotations

import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class LegacyTeamContractTest(unittest.TestCase):
    def test_legacy_and_team_static_validator(self) -> None:
        run = subprocess.run([sys.executable, str(ROOT / "Scripts/legacy_team_static_validate.py")], cwd=ROOT, text=True, capture_output=True)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_legacy_directory_contains_markdown_sources_only(self) -> None:
        root = ROOT / "docs" / "legacy-unity"
        suffixes = {path.suffix for path in root.rglob("*") if path.is_file()}
        self.assertEqual(suffixes, {".md"})
        self.assertFalse(any(part in {"Saved", "Intermediate", ".aime", "skills"} for path in root.rglob("*") for part in path.parts))


if __name__ == "__main__":
    unittest.main()
