#!/usr/bin/env python3
"""Fail-closed static validation for the Phase 6 controlled Editor transaction boundary."""
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
        "PHASE6.md", "docs/EDITOR_MCP_PROTOCOL.md", "docs/PHASE6_THREAT_MODEL.md",
        "docs/PHASE6_VALIDATION.md", "Plugins/AeroSimEditor/Source/AeroSimEditor/Public/AeroSimPhase6Transaction.h",
        "Plugins/AeroSimEditor/Source/AeroSimEditor/Private/AeroSimPhase6Transaction.cpp",
        "Plugins/AeroSimEditor/Source/AeroSimEditor/Private/Tests/AeroSimPhase6TransactionTest.cpp",
    )
    for name in required:
        require((ROOT / name).is_file(), f"missing {name}")

    plugin_root = ROOT / "Plugins/AeroSimEditor"
    plugin_files = [p for p in plugin_root.rglob("*") if p.suffix in {".h", ".cpp", ".cs", ".uplugin"}]
    plugin_text = "\n".join(p.read_text(encoding="utf-8", errors="replace") for p in plugin_files)
    transaction = (plugin_root / "Source/AeroSimEditor/Private/AeroSimPhase6Transaction.cpp").read_text(encoding="utf-8")
    dispatcher = (plugin_root / "Source/AeroSimEditor/Private/AeroSimEditorMcp.cpp").read_text(encoding="utf-8")

    for token in (
        "aerosim.runtime.phase7/v1", "aerosim.scene-patch/v1", "aerosim.snapshot-manifest/v1",
        "PENDING_REVIEW_SANDBOX", "DRAFT_NOT_USER_CONFIRMED", "base_revision", "base_hash",
        "snapshot_sha256", "manifest_sha256", "draft_hash", "SHA256", "REQUEST_ID_REUSE",
        "TRANSACTION_LOCKED", "TIMEOUT", "IdleExpirySeconds", "REVISION_CONFLICT", "HASH_CONFLICT",
        "VALIDATION_FAILED_ROLLED_BACK", "ROLLBACK_TRUST_FAILURE", "SKIPPED_UPSTREAM_FAILURE",
        "transaction_dry_run", "transaction_snapshot", "transaction_apply", "transaction_validate",
        "transaction_evidence", "transaction_commit", "transaction_rollback", "transaction_undo", "transaction_redo",
        "spawn_approved", "set_transform", "set_material_scalar", "add_tag", "remove_tag", "delete_draft",
        "/Game/AeroSim/Sandbox/Maps/AI_Review_Sandbox", "AeroSim_AI_Draft", "primitive.cube",
        "near-follow", "EDITOR_CAPTURE_REQUIRED", "production_touched",
    ):
        require(token in plugin_text, f"missing Phase 6 contract/security token: {token}")

    for gate in ("G0", "G1", "G2", "G3", "G4"):
        require(f'TEXT("{gate}")' in transaction, f"missing ordered gate {gate}")
    require(transaction.index('TEXT("G0")') < transaction.index('TEXT("G1")') < transaction.index('TEXT("G2")') < transaction.index('TEXT("G3")') < transaction.index('TEXT("G4")'), "G0-G4 must remain ordered")

    descriptor = json.loads((plugin_root / "AeroSimEditor.uplugin").read_text(encoding="utf-8"))
    require(all(m.get("Type") == "Editor" for m in descriptor["Modules"]), "transaction module must be Editor-only")
    uproject = json.loads((ROOT / "AeroSim.uproject").read_text(encoding="utf-8"))
    editor_plugin = next((p for p in uproject["Plugins"] if p["Name"] == "AeroSimEditor"), {})
    require(editor_plugin.get("TargetAllowList") == ["Editor"], "AeroSimEditor must have Editor-only target allowlist")
    for dangerous in ("PythonScriptPlugin", "EditorScriptingUtilities"):
        entry = next((p for p in uproject["Plugins"] if p["Name"] == dangerous), None)
        require(entry is None or entry.get("Enabled") is False, f"{dangerous} must remain disabled")

    runtime = (ROOT / "Source/AeroSim/Public/AeroSimRuntimeContract.h").read_text(encoding="utf-8")
    require('Phase = TEXT("Phase7")' in runtime, "runtime phase must advance to Phase7 without changing the Phase6 Editor boundary")
    require('Contract = TEXT("aerosim.runtime.phase7/v1")' in runtime, "runtime contract must be phase7/v1")
    require("AeroSimEditor" not in (ROOT / "Source/AeroSim/AeroSim.Build.cs").read_text(encoding="utf-8"), "runtime must not depend on Editor transaction module")

    prohibited_calls = (
        "FPlatformProcess::CreateProc", "FPlatformProcess::ExecProcess", "GEngine->Exec", "GetMutableDefault",
        "StaticLoadObject", "LoadObject<", "ProcessEvent(", "IFileManager::", "FFileHelper::Save",
        "IPythonScriptPlugin", "PythonScriptPlugin", "FHttpModule", "FSocket", "ImportObject",
    )
    code = "\n".join(p.read_text(encoding="utf-8", errors="replace") for base in (ROOT / "Source", ROOT / "Plugins") for p in base.rglob("*") if p.suffix in {".h", ".cpp", ".cs"})
    for call in prohibited_calls:
        require(call not in code, f"prohibited executable/arbitrary write surface: {call}")

    require("MaximumActionsPerPatch = 64" in plugin_text, "patch quota missing")
    require("MaximumDraftActors = 256" in plugin_text, "actor quota missing")
    require("TransactionTimeoutMs = 2000" in plugin_text, "transaction timeout missing")
    require("IdempotentResponses.Num() >= 512" in dispatcher, "idempotency cache quota missing")

    ini = (ROOT / "Config/DefaultEngine.ini").read_text(encoding="utf-8")
    require("AndroidFileServer" not in ini, "AndroidFileServer config must be absent")
    require(re.search(r"(?i)(token|securitytoken|apikey|api_key)\s*=", ini) is None, "credential/token entry must be absent")

    if errors:
        for error in errors:
            print(f"FAIL: {error}")
        print(f"PHASE6 STATIC VALIDATION FAILED: {len(errors)} error(s), {checks} checks")
        return 1
    print(f"PHASE6 STATIC VALIDATION PASS: {checks} checks")
    return 0


if __name__ == "__main__":
    sys.exit(main())
