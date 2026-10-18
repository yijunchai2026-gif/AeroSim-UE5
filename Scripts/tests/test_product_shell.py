from pathlib import Path
import subprocess
import sys
import unittest
ROOT = Path(__file__).resolve().parents[2]

class ProductShellSourceTests(unittest.TestCase):
    def test_shell_static_contract(self):
        result = subprocess.run([sys.executable, str(ROOT/'Scripts/product_shell_validate.py')], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_mcp_remains_editor_only(self):
        import json
        descriptor = json.loads((ROOT/'Plugins/AeroSimEditor/AeroSimEditor.uplugin').read_text())
        self.assertTrue(all(m['Type'] == 'Editor' for m in descriptor['Modules']))
        runtime = (ROOT/'Source/AeroSim/AeroSim.Build.cs').read_text()
        self.assertNotIn('"AeroSimEditor"', runtime)

    def test_save_failure_does_not_become_applied_success(self):
        source = (ROOT/'Source/AeroSim/Private/AeroSimGameMode.cpp').read_text()
        self.assertIn('AppliedSettings = Before;', source)
        self.assertIn('SAVE FAILED - PREVIOUS APPLIED CONFIGURATION RESTORED', source)
        self.assertNotIn('APPLIED / SAVE FAILED', source)
