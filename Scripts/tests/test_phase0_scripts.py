#!/usr/bin/env python3
"""Standard-library regression tests for Phase 0 static gates."""

from __future__ import annotations

import importlib.util
import json
import os
import shutil
import tempfile
import unittest
from pathlib import Path

SCRIPTS = Path(__file__).resolve().parents[1]
PROJECT_ROOT = SCRIPTS.parent


def load_module(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class PreflightFixtureTests(unittest.TestCase):
    def test_wrong_engine_version_is_rejected_and_5_8_is_accepted(self) -> None:
        preflight = load_module("phase0_preflight_fixture", SCRIPTS / "phase0_preflight.py")
        with tempfile.TemporaryDirectory() as temporary:
            engine = Path(temporary) / "UE" / "Engine"
            editor = engine / "Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
            build_version = engine / "Build/Build.version"
            run_uat = engine / "Build/BatchFiles/RunUAT.sh"
            editor.parent.mkdir(parents=True)
            run_uat.parent.mkdir(parents=True)
            editor.write_text("fixture", encoding="utf-8")
            run_uat.write_text("fixture", encoding="utf-8")

            build_version.write_text(
                json.dumps({"MajorVersion": 5, "MinorVersion": 7}), encoding="utf-8"
            )
            valid, detail = preflight.validate_engine_layout(editor)
            self.assertFalse(valid)
            self.assertIn("wrong Unreal Engine version 5.7", detail)

            build_version.write_text(
                json.dumps({"MajorVersion": 5, "MinorVersion": 8}), encoding="utf-8"
            )
            valid, detail = preflight.validate_engine_layout(editor)
            self.assertTrue(valid, detail)
            self.assertIn(str(run_uat), detail)


class StaticValidatorFixtureTests(unittest.TestCase):
    def test_manifest_file_parent_and_root_symlinks_are_rejected(self) -> None:
        validator_module = load_module("phase0_static_symlink_fixture", SCRIPTS / "phase0_static_validate.py")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "project"
            source_assets = root / "SourceAssets"
            outside = Path(temporary) / "outside"
            source_assets.mkdir(parents=True)
            outside.mkdir()
            (outside / "asset.bin").write_bytes(b"fixture")
            os.symlink(outside, source_assets / "Escape")
            os.symlink(outside / "asset.bin", source_assets / "file-link.bin")

            original_root = validator_module.ROOT
            validator_module.ROOT = root
            try:
                parent_validator = validator_module.Validator()
                parent_result = validator_module.safe_manifest_target(
                    parent_validator, Path("SourceAssets/Escape/asset.bin")
                )
                file_validator = validator_module.Validator()
                file_result = validator_module.safe_manifest_target(
                    file_validator, Path("SourceAssets/file-link.bin")
                )

                shutil.rmtree(source_assets)
                os.symlink(outside, source_assets)
                root_validator = validator_module.Validator()
                root_result = validator_module.safe_manifest_target(
                    root_validator, Path("SourceAssets/asset.bin")
                )
            finally:
                validator_module.ROOT = original_root

            self.assertIsNone(parent_result)
            self.assertTrue(any("symlink" in error for error in parent_validator.errors))
            self.assertIsNone(file_result)
            self.assertTrue(any("symlink" in error for error in file_validator.errors))
            self.assertIsNone(root_result)
            self.assertTrue(any("symlink" in error for error in root_validator.errors))

    def test_source_contract_mutations_are_detected(self) -> None:
        validator_module = load_module("phase0_static_contract_fixture", SCRIPTS / "phase0_static_validate.py")
        relative_files = (
            "Source/AeroSim/Public/FlightPawn.h",
            "Source/AeroSim/Private/FlightPawn.cpp",
            "Source/AeroSim/Public/ReviewCameras.h",
            "Source/AeroSim/Private/ReviewCameras.cpp",
        )
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "project"
            for relative in relative_files:
                destination = root / relative
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(PROJECT_ROOT / relative, destination)

            original_root = validator_module.ROOT
            validator_module.ROOT = root
            try:
                valid_validator = validator_module.Validator()
                validator_module.validate_source_contracts(valid_validator)
                self.assertEqual([], valid_validator.errors)

                input_source = root / "Source/AeroSim/Private/FlightPawn.cpp"
                input_source.write_text(
                    input_source.read_text(encoding="utf-8").replace(
                        "MappedActions.Contains", "MappedActions.Find"
                    ),
                    encoding="utf-8",
                )
                camera_source = root / "Source/AeroSim/Private/ReviewCameras.cpp"
                camera_source.write_text(
                    camera_source.read_text(encoding="utf-8").replace(
                        "GetActorLocation()", "FVector::ZeroVector"
                    ),
                    encoding="utf-8",
                )
                invalid_validator = validator_module.Validator()
                validator_module.validate_source_contracts(invalid_validator)
            finally:
                validator_module.ROOT = original_root

            self.assertTrue(any("MappedActions.Contains" in error for error in invalid_validator.errors))
            self.assertTrue(any("GetActorLocation()" in error for error in invalid_validator.errors))


if __name__ == "__main__":
    unittest.main()
