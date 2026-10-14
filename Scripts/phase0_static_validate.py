#!/usr/bin/env python3
"""Static Phase 0 consistency checks requiring only the Python standard library."""

from __future__ import annotations

import fnmatch
import hashlib
import json
import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REQUIRED_DIRS = (
    "Config",
    "Source/AeroSim",
    "Plugins/AeroSimEditor/Source/AeroSimEditor",
    "Migration",
    "SourceAssets",
    "Scripts",
)
JSON_FILES = (
    "AeroSim.uproject",
    "Plugins/AeroSimEditor/AeroSimEditor.uplugin",
    "Migration/asset-migration-manifest.json",
)
DANGEROUS_PATTERNS = (
    re.compile(r"FPlatformProcess\s*::\s*(CreateProc|ExecProcess)"),
    re.compile(r"\b(ConsoleCommand|GEngine\s*->\s*Exec)\b"),
    re.compile(r"\b(system|popen)\s*\("),
    re.compile(r"Process\s*\.\s*Start\s*\("),
    re.compile(r"System\.Diagnostics"),
    re.compile(r"/(bin|usr/bin)/(sh|bash|zsh)\b"),
)
LFS_EXTENSIONS = (
    ".uasset",
    ".umap",
    ".fbx",
    ".png",
    ".jpg",
    ".jpeg",
    ".tga",
    ".tif",
    ".tiff",
    ".exr",
    ".hdr",
    ".psd",
)


class Validator:
    def __init__(self) -> None:
        self.errors: list[str] = []
        self.checks = 0

    def require(self, condition: bool, message: str) -> None:
        self.checks += 1
        if not condition:
            self.errors.append(message)


def load_json(validator: Validator, relative: str) -> object | None:
    path = ROOT / relative
    validator.require(path.is_file(), f"missing JSON file: {relative}")
    if not path.is_file():
        return None
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        validator.require(False, f"invalid JSON {relative}: {error}")
        return None


def validate_layout_and_json(validator: Validator) -> None:
    for relative in REQUIRED_DIRS:
        validator.require((ROOT / relative).is_dir(), f"missing directory: {relative}")

    loaded = {relative: load_json(validator, relative) for relative in JSON_FILES}
    project = loaded["AeroSim.uproject"]
    if isinstance(project, dict):
        validator.require(project.get("EngineAssociation") == "5.8", "EngineAssociation must be 5.8")

    target_text = (ROOT / "Source/AeroSim.Target.cs").read_text(encoding="utf-8")
    validator.require("Process.Start" not in target_text, "TargetRules must not start processes")
    validator.require("System.Diagnostics" not in target_text, "TargetRules must remain declarative")


def validate_dangerous_apis(validator: Validator) -> None:
    roots = (ROOT / "Source", ROOT / "Plugins")
    suffixes = {".h", ".hpp", ".cpp", ".c", ".cs"}
    for root in roots:
        for path in sorted(root.rglob("*")):
            if not path.is_file() or path.suffix.lower() not in suffixes:
                continue
            text = path.read_text(encoding="utf-8", errors="replace")
            for pattern in DANGEROUS_PATTERNS:
                validator.require(
                    pattern.search(text) is None,
                    f"dangerous API pattern {pattern.pattern!r} in {path.relative_to(ROOT)}",
                )


def parse_lfs_rules(validator: Validator) -> list[tuple[str, set[str]]]:
    path = ROOT / ".gitattributes"
    validator.require(path.is_file(), "missing .gitattributes")
    rules: list[tuple[str, set[str]]] = []
    if not path.is_file():
        return rules
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        fields = line.split()
        if len(fields) > 1:
            rules.append((fields[0].lower(), set(fields[1:])))
    return rules


def is_lfs_tracked(relative: str, rules: list[tuple[str, set[str]]]) -> bool:
    lowered = relative.lower()
    for pattern, attrs in rules:
        if fnmatch.fnmatch(lowered, pattern) or fnmatch.fnmatch(Path(lowered).name, pattern):
            if {"filter=lfs", "diff=lfs", "merge=lfs", "-text"}.issubset(attrs):
                return True
    return False


def validate_lfs(validator: Validator, manifest: dict[str, object] | None) -> None:
    rules = parse_lfs_rules(validator)
    for extension in LFS_EXTENSIONS:
        validator.require(is_lfs_tracked(f"probe{extension}", rules), f"missing LFS rule for {extension}")

    if not isinstance(manifest, dict):
        return
    assets = manifest.get("assets")
    if isinstance(assets, list):
        for asset in assets:
            if not isinstance(asset, dict):
                continue
            target = asset.get("targetPath")
            if isinstance(target, str) and Path(target).suffix.lower() in LFS_EXTENSIONS:
                validator.require(is_lfs_tracked(target, rules), f"manifest binary not covered by LFS: {target}")


def validate_source_contracts(validator: Validator) -> None:
    input_header = (ROOT / "Source/AeroSim/Public/FlightPawn.h").read_text(encoding="utf-8")
    input_source = (ROOT / "Source/AeroSim/Private/FlightPawn.cpp").read_text(encoding="utf-8")
    camera_header = (ROOT / "Source/AeroSim/Public/ReviewCameras.h").read_text(encoding="utf-8")
    camera_source = (ROOT / "Source/AeroSim/Private/ReviewCameras.cpp").read_text(encoding="utf-8")

    input_tokens = (
        "struct AEROSIM_API FFlightInputSuite",
        "IsInputSuiteValid",
        "ValidateConfiguredInputSuite",
        "MappedActions.Contains",
        "bContainsUnexpectedAction",
        "ensureAlwaysMsgf",
    )
    for token in input_tokens:
        validator.require(token in input_header or token in input_source, f"missing atomic input contract token: {token}")

    camera_tokens = (
        "CanGenerateFormalEvidence",
        "IsEvidenceStateWithinTolerance",
        "GetActorLocation()",
        "GetActorRotation()",
        "FieldOfView",
        "LocationToleranceCm",
        "RotationToleranceDegrees",
        "FieldOfViewToleranceDegrees",
    )
    for token in camera_tokens:
        validator.require(token in camera_header or token in camera_source, f"missing evidence camera contract token: {token}")


def safe_manifest_target(validator: Validator, relative: Path) -> Path | None:
    source_root = ROOT / "SourceAssets"
    validator.require(source_root.is_dir(), "SourceAssets root is missing")
    root_is_safe = source_root.is_dir() and not source_root.is_symlink()
    validator.require(root_is_safe, "SourceAssets root must not be a symlink")
    if not root_is_safe:
        return None

    candidate = ROOT / relative
    current = ROOT
    symlink_found = False
    for part in relative.parts:
        current = current / part
        if current.is_symlink():
            validator.require(False, f"manifest target or parent is a symlink: {relative}")
            symlink_found = True
            break
    if symlink_found:
        return None

    try:
        resolved_root = source_root.resolve(strict=True)
        resolved_target = candidate.resolve(strict=True)
    except OSError as error:
        validator.require(False, f"cannot resolve manifest target {relative}: {error}")
        return None

    try:
        common = Path(os.path.commonpath((str(resolved_root), str(resolved_target))))
    except ValueError:
        common = Path()
    validator.require(common == resolved_root, f"manifest target escapes SourceAssets: {relative}")
    if common != resolved_root:
        return None

    validator.require(resolved_target.is_file(), f"manifest target is not a regular file: {relative}")
    return resolved_target if resolved_target.is_file() else None


def validate_manifest(validator: Validator, manifest: object | None) -> None:
    validator.require(isinstance(manifest, dict), "migration manifest must be an object")
    if not isinstance(manifest, dict):
        return

    validator.require(
        manifest.get("schema") == "aerosim.ue5.asset-migration-manifest/v1",
        "unexpected migration manifest schema",
    )
    validator.require(manifest.get("hashAlgorithm") == "SHA-256", "manifest hash algorithm must be SHA-256")
    assets = manifest.get("assets")
    validator.require(isinstance(assets, list) and bool(assets), "manifest assets must be a non-empty list")
    if not isinstance(assets, list):
        return

    ids: set[str] = set()
    targets: set[str] = set()
    required = ("id", "sourcePath", "targetPath", "sha256", "sizeBytes", "type", "status")
    for index, asset in enumerate(assets):
        validator.require(isinstance(asset, dict), f"manifest asset {index} must be an object")
        if not isinstance(asset, dict):
            continue
        for field in required:
            validator.require(field in asset, f"manifest asset {index} missing {field}")

        asset_id = asset.get("id")
        target = asset.get("targetPath")
        validator.require(isinstance(asset_id, str) and asset_id not in ids, f"duplicate/invalid asset id: {asset_id}")
        validator.require(isinstance(target, str) and target not in targets, f"duplicate/invalid targetPath: {target}")
        if isinstance(asset_id, str):
            ids.add(asset_id)
        if not isinstance(target, str):
            continue
        targets.add(target)

        relative = Path(target)
        path_is_safe = not relative.is_absolute() and ".." not in relative.parts
        path_is_in_source_assets = bool(relative.parts) and relative.parts[0] == "SourceAssets"
        validator.require(path_is_safe, f"unsafe targetPath: {target}")
        validator.require(path_is_in_source_assets, f"target outside SourceAssets: {target}")
        if not path_is_safe or not path_is_in_source_assets:
            continue
        file_path = safe_manifest_target(validator, relative)
        if file_path is None:
            continue

        data = file_path.read_bytes()
        validator.require(len(data) == asset.get("sizeBytes"), f"size mismatch: {target}")
        validator.require(hashlib.sha256(data).hexdigest() == asset.get("sha256"), f"SHA-256 mismatch: {target}")


def main() -> int:
    validator = Validator()
    validate_layout_and_json(validator)
    manifest = load_json(validator, "Migration/asset-migration-manifest.json")
    validate_dangerous_apis(validator)
    validate_source_contracts(validator)
    validate_manifest(validator, manifest)
    validate_lfs(validator, manifest if isinstance(manifest, dict) else None)

    if validator.errors:
        for error in validator.errors:
            print(f"FAIL: {error}")
        print(f"STATIC VALIDATION FAILED: {len(validator.errors)} error(s), {validator.checks} checks")
        return 1

    print(f"STATIC VALIDATION PASS: {validator.checks} checks")
    return 0


if __name__ == "__main__":
    sys.exit(main())
