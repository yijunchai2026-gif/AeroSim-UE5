#!/usr/bin/env python3
"""Fail-closed static validation for the Phase 5 read-only Editor MCP boundary."""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    errors: list[str] = []
    checks = 0

    def require(value: bool, message: str) -> None:
        nonlocal checks
        checks += 1
        if not value:
            errors.append(message)

    required = (
        "PHASE5.md", "docs/EDITOR_MCP_PROTOCOL.md", "docs/PHASE5_THREAT_MODEL.md",
        "docs/PHASE5_VALIDATION.md", "Plugins/AeroSimEditor/Source/AeroSimEditor/Public/AeroSimEditorMcp.h",
        "Plugins/AeroSimEditor/Source/AeroSimEditor/Private/AeroSimEditorMcp.cpp",
        "Plugins/AeroSimEditor/Source/AeroSimEditor/Private/Tests/AeroSimEditorMcpTest.cpp",
    )
    for name in required:
        require((ROOT / name).is_file(), f"missing {name}")

    plugin_root = ROOT / "Plugins/AeroSimEditor"
    plugin_text = "\n".join(p.read_text(encoding="utf-8", errors="replace") for p in plugin_root.rglob("*") if p.suffix in {".h", ".cpp", ".cs", ".uplugin"})
    all_code = "\n".join(p.read_text(encoding="utf-8", errors="replace") for base in (ROOT / "Source", ROOT / "Plugins") for p in base.rglob("*") if p.suffix in {".h", ".cpp", ".cs"})

    for token in (
        "aerosim.editor.mcp.request/v1", "aerosim.editor.mcp.response/v1", "aerosim.editor.mcp.capabilities/v1",
        "READ_ONLY_DENIED", "SCHEMA_UNSUPPORTED", "INVALID_REQUEST_ID", "PATH_NOT_ALLOWED", "TIMEOUT",
        "heartbeat", "inspect_project", "inspect_world", "list_actors", "inspect_actor", "inspect_component",
        "inspect_asset_metadata", "inspect_data_layers", "inspect_world_partition", "validate_scene", "capture_request",
        "MaximumPageSize = 100", "MaximumTimeoutMs = 2000", "MaximumResponseBytes = 65536",
        "append-only", "EDITOR_CAPTURE_REQUIRED", "PENDING_MANUAL_REVIEW",
    ):
        require(token in plugin_text, f"missing protocol/security token: {token}")

    descriptor = json.loads((plugin_root / "AeroSimEditor.uplugin").read_text(encoding="utf-8"))
    require(all(m.get("Type") == "Editor" for m in descriptor["Modules"]), "MCP plugin module must be Editor-only")
    uproject = json.loads((ROOT / "AeroSim.uproject").read_text(encoding="utf-8"))
    editor_plugin = next((p for p in uproject["Plugins"] if p["Name"] == "AeroSimEditor"), {})
    require(editor_plugin.get("TargetAllowList") == ["Editor"], "AeroSimEditor must have Editor-only target allowlist")
    for dangerous in ("PythonScriptPlugin", "EditorScriptingUtilities"):
        entry = next((p for p in uproject["Plugins"] if p["Name"] == dangerous), None)
        require(entry is None or entry.get("Enabled") is False, f"{dangerous} must not be enabled")

    runtime = (ROOT / "Source/AeroSim/Public/AeroSimRuntimeContract.h").read_text(encoding="utf-8")
    require('Phase = TEXT("Phase5")' in runtime, "runtime phase must be Phase5")
    require('Contract = TEXT("aerosim.runtime.phase5/v1")' in runtime, "runtime contract must be phase5/v1")
    require("AeroSimEditor" not in (ROOT / "Source/AeroSim/AeroSim.Build.cs").read_text(encoding="utf-8"), "runtime must not depend on Editor MCP")

    prohibited_calls = (
        "FPlatformProcess::CreateProc", "FPlatformProcess::ExecProcess", "GEngine->Exec", "GetMutableDefault",
        "StaticLoadObject", "LoadObject<", "ProcessEvent(", "IFileManager::", "FFileHelper::Save",
        "IPythonScriptPlugin", "PythonScriptPlugin",
    )
    for call in prohibited_calls:
        require(call not in all_code, f"prohibited executable/mutation surface: {call}")

    ini = (ROOT / "Config/DefaultEngine.ini").read_text(encoding="utf-8")
    require("AndroidFileServer" not in ini, "AndroidFileServer config must be absent")
    require(re.search(r"(?i)(token|securitytoken)\s*=", ini) is None, "token entry must be absent")

    if errors:
        for error in errors:
            print(f"FAIL: {error}")
        print(f"PHASE5 STATIC VALIDATION FAILED: {len(errors)} error(s), {checks} checks")
        return 1
    print(f"PHASE5 STATIC VALIDATION PASS: {checks} checks")
    return 0


if __name__ == "__main__":
    sys.exit(main())
