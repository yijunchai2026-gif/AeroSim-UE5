#!/usr/bin/env python3
"""Fail-closed Phase 7 static contract validation."""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    checks = 0
    errors: list[str] = []

    def require(condition: bool, message: str) -> None:
        nonlocal checks
        checks += 1
        if not condition:
            errors.append(message)

    required = (
        "PHASE7.md", "docs/PHASE7_RELEASE_CHECKLIST.md", "docs/PHASE7_PERFORMANCE_REPORT.md",
        "docs/PHASE7_VALIDATION.md", "Config/DefaultDeviceProfiles.ini",
        "Source/AeroSim/Public/Phase7Product.h", "Source/AeroSim/Private/Phase7Product.cpp",
        "Source/AeroSim/Private/Tests/AeroSimPhase7Test.cpp",
        "Plugins/AeroSimEditor/Source/AeroSimEditor/Public/AeroSimPhase7Optimization.h",
        "Plugins/AeroSimEditor/Source/AeroSimEditor/Private/AeroSimPhase7Optimization.cpp",
        "Plugins/AeroSimEditor/Source/AeroSimEditor/Private/Tests/AeroSimPhase7OptimizationTest.cpp",
        "Scripts/phase7_release_manifest.py", "Scripts/phase7_soak.py",
    )
    for relative in required:
        require((ROOT / relative).is_file(), f"missing {relative}")

    runtime = (ROOT / "Source/AeroSim/Public/AeroSimRuntimeContract.h").read_text(encoding="utf-8")
    require('Phase = TEXT("Phase7")' in runtime, "runtime Phase7 identity missing")
    require('Contract = TEXT("aerosim.runtime.phase7/v1")' in runtime, "runtime phase7/v1 missing")
    require('PreviousContract = TEXT("aerosim.runtime.phase6/v1")' in runtime, "Phase6 lineage missing")

    product = (ROOT / "Source/AeroSim/Private/Phase7Product.cpp").read_text(encoding="utf-8")
    header = (ROOT / "Source/AeroSim/Public/Phase7Product.h").read_text(encoding="utf-8")
    for token in (
        "FrameP95Ms", "GameThreadP95Ms", "RenderThreadP95Ms", "GpuP95Ms", "ResidentMemoryMiB",
        "DrawCalls", "StreamingPoolMiB", "StreamingSpikeP95Ms", "bWorldPartition", "bHlod", "bNanite",
        "BLOCKED_MISSING_EVIDENCE", "NON_DEVELOPMENT_BUILD_REQUIRED", "INCOMPLETE_QUALIFICATION_PROTOCOL",
        "WriteCrashSafe", "DecodeAndMigrate", "UNSUPPORTED_VERSION", "Rollback", "Redact",
        "FPhase7DeterministicSoak", "StepsPerSecond = 60",
    ):
        require(token in header or token in product, f"missing product/performance contract token: {token}")

    advisor = (ROOT / "Plugins/AeroSimEditor/Source/AeroSimEditor/Private/AeroSimPhase7Optimization.cpp").read_text(encoding="utf-8")
    dispatcher = (ROOT / "Plugins/AeroSimEditor/Source/AeroSimEditor/Private/AeroSimEditorMcp.cpp").read_text(encoding="utf-8")
    for token in ("Layout", "Lighting", "Lod", "Performance", "DRAFT_NOT_USER_CONFIRMED", "SANDBOX_ONLY", "suggest_optimization", "requires_human_review"):
        require(token in advisor or token in dispatcher, f"missing controlled optimization token: {token}")
    require("bApplied = false" in (ROOT / "Plugins/AeroSimEditor/Source/AeroSimEditor/Public/AeroSimPhase7Optimization.h").read_text(encoding="utf-8"), "suggestions must default to unapplied")

    profiles = (ROOT / "Config/DefaultDeviceProfiles.ini").read_text(encoding="utf-8")
    for token in ("[Mac DeviceProfile]", "[Windows DeviceProfile]", "[IOS DeviceProfile]", "r.Nanite=0", "HLOD is authored", "r.Streaming.PoolSize"):
        require(token in profiles, f"missing quality profile token: {token}")

    docs = "\n".join((ROOT / name).read_text(encoding="utf-8") for name in required[:4])
    for token in ("Mac", "Development", "Windows", "iPad", "NOT_RUN", "BLOCKED_ASSET", "BLOCKED_USER_CONFIRMATION", "CPU", "GPU", "draw calls", "streaming", "LOD", "HLOD", "Nanite"):
        require(token in docs, f"missing truthful release documentation token: {token}")

    descriptor = json.loads((ROOT / "Plugins/AeroSimEditor/AeroSimEditor.uplugin").read_text(encoding="utf-8"))
    require(all(module.get("Type") == "Editor" for module in descriptor["Modules"]), "Editor MCP must remain Editor-only")
    uproject = json.loads((ROOT / "AeroSim.uproject").read_text(encoding="utf-8"))
    plugin = next(item for item in uproject["Plugins"] if item["Name"] == "AeroSimEditor")
    require(plugin.get("TargetAllowList") == ["Editor"], "Editor target allowlist regressed")

    searchable = [p for base in (ROOT / "Config", ROOT / "Source", ROOT / "Plugins", ROOT / "Scripts") for p in base.rglob("*") if p.is_file() and p.suffix.lower() in {".ini", ".h", ".cpp", ".cs", ".py", ".json"}]
    content = "\n".join(path.read_text(encoding="utf-8", errors="replace") for path in searchable)
    require(re.search(r"(?im)^\s*(token|securitytoken|api[_-]?key|password|secret)\s*=\s*[^\s\[]", content) is None, "credential assignment found")
    implementation = "\n".join(path.read_text(encoding="utf-8", errors="replace") for base in (ROOT / "Source", ROOT / "Plugins") for path in base.rglob("*") if path.is_file() and path.suffix.lower() in {".h", ".cpp", ".cs"})
    for forbidden in ("FPlatformProcess::CreateProc", "GEngine->Exec", "StaticLoadObject", "ProcessEvent(", "FHttpModule"):
        require(forbidden not in implementation, f"Phase6 security regression: {forbidden}")

    if errors:
        for error in errors:
            print(f"FAIL: {error}")
        print(f"PHASE7 STATIC VALIDATION FAILED: {len(errors)} error(s), {checks} checks")
        return 1
    print(f"PHASE7 STATIC VALIDATION PASS: {checks} checks")
    return 0


if __name__ == "__main__":
    sys.exit(main())
