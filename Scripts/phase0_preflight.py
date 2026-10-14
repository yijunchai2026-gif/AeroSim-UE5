#!/usr/bin/env python3
"""Fixed, read-only macOS gate that must pass before explicit UE Cook/Package."""

from __future__ import annotations

import argparse
import json
import platform
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Sequence

MINIMUM_FREE_BYTES = 100 * 1024**3
DEFAULT_UE_EDITOR = Path(
    "/Applications/Epic Games/UE_5.8/Engine/Binaries/Mac/"
    "UnrealEditor.app/Contents/MacOS/UnrealEditor"
)
XCRUN = Path("/usr/bin/xcrun")
XCODE_SELECT = Path("/usr/bin/xcode-select")


def fixed_command(argv: Sequence[str]) -> tuple[bool, str]:
    """Run an allow-listed fixed executable with fixed arguments; never invokes a shell."""
    try:
        result = subprocess.run(
            list(argv),
            check=False,
            capture_output=True,
            text=True,
            timeout=30,
            shell=False,
        )
    except (OSError, subprocess.SubprocessError) as error:
        return False, str(error)

    output = result.stdout.strip() or result.stderr.strip()
    return result.returncode == 0, output


def locate_engine_root(editor: Path) -> Path | None:
    """Derive the owning Engine directory from the editor path; never searches other installs."""
    for parent in editor.parents:
        if parent.name == "Engine":
            return parent
    return None


def validate_engine_layout(editor_path: Path) -> tuple[bool, str]:
    editor = editor_path.expanduser().resolve()
    if not editor.is_file() or editor.name != "UnrealEditor":
        return False, f"UnrealEditor executable not found: {editor}"

    engine_root = locate_engine_root(editor)
    if engine_root is None:
        return False, f"editor is not inside an Engine directory: {editor}"

    build_version = engine_root / "Build" / "Build.version"
    run_uat = engine_root / "Build" / "BatchFiles" / "RunUAT.sh"
    if not build_version.is_file() or build_version.is_symlink():
        return False, f"regular Build.version missing under editor Engine root: {build_version}"
    if not run_uat.is_file() or run_uat.is_symlink():
        return False, f"regular RunUAT.sh missing under editor Engine root: {run_uat}"

    try:
        version = json.loads(build_version.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        return False, f"invalid Build.version: {error}"

    major = version.get("MajorVersion") if isinstance(version, dict) else None
    minor = version.get("MinorVersion") if isinstance(version, dict) else None
    if major != 5 or minor != 8:
        return False, f"wrong Unreal Engine version {major}.{minor}; required 5.8"

    return True, f"UE 5.8 root={engine_root}; RunUAT={run_uat}"


def report(name: str, passed: bool, detail: str) -> bool:
    print(f"{'PASS' if passed else 'FAIL'} {name}: {detail}")
    return passed


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="AeroSim Phase 0 fixed preflight for explicit UE5.8 Cook/Package."
    )
    parser.add_argument(
        "--ue-editor",
        type=Path,
        default=DEFAULT_UE_EDITOR,
        help="Path to the UE5.8 UnrealEditor executable (a path only, never a command).",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    project_root = Path(__file__).resolve().parents[1]
    results: list[bool] = []

    results.append(report("platform", platform.system() == "Darwin", platform.platform()))

    engine_ok, engine_detail = validate_engine_layout(args.ue_editor)
    results.append(report("ue_engine_5_8", engine_ok, engine_detail))

    free_bytes = shutil.disk_usage(project_root).free
    free_gib = free_bytes / 1024**3
    results.append(
        report(
            "disk_space",
            free_bytes >= MINIMUM_FREE_BYTES,
            f"{free_gib:.1f} GiB free; minimum is 100.0 GiB",
        )
    )

    xcode_ok, xcode_output = fixed_command((str(XCODE_SELECT), "-p"))
    xcode_path = Path(xcode_output) if xcode_ok else None
    xcode_valid = bool(xcode_path and xcode_path.is_dir())
    results.append(report("xcode_developer_dir", xcode_ok and xcode_valid, xcode_output))

    sdk_ok, sdk_output = fixed_command((str(XCRUN), "--sdk", "macosx", "--show-sdk-path"))
    sdk_path = Path(sdk_output) if sdk_ok else None
    sdk_valid = bool(sdk_path and sdk_path.is_dir())
    results.append(report("macos_sdk", sdk_ok and sdk_valid, sdk_output))

    metallib_ok, metallib_output = fixed_command((str(XCRUN), "--find", "metallib"))
    metallib_path = Path(metallib_output) if metallib_ok else None
    metallib_valid = bool(metallib_path and metallib_path.is_file())
    results.append(report("metallib", metallib_ok and metallib_valid, metallib_output))

    if all(results):
        print("PREFLIGHT PASS: explicit UE Build/Cook/Package may proceed.")
        return 0

    print("PREFLIGHT BLOCKED: do not run UE Cook/Package until every check passes.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
