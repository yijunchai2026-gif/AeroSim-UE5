#!/usr/bin/env python3
"""Generate a deterministic Phase 7 release manifest; missing release gates stay blocked."""
from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD_ID = "AeroSim-Phase7-20261016"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def git_value(*args: str) -> str:
    run = subprocess.run(["git", *args], cwd=ROOT, text=True, capture_output=True, check=False)
    return run.stdout.strip() if run.returncode == 0 else "UNAVAILABLE"


def release_files() -> list[Path]:
    roots = ("Config", "Content", "Plugins", "Scripts", "Source", "SourceAssets", "Build", "docs")
    files = [path for name in roots for path in (ROOT / name).rglob("*") if path.is_file() and "__pycache__" not in path.parts and path.suffix != ".pyc"]
    files.extend(path for path in (ROOT / "AeroSim.uproject", ROOT / "PHASE7.md") if path.is_file())
    return sorted(set(files))


def build_manifest() -> dict:
    files = [{"path": str(path.relative_to(ROOT)), "sha256": sha256(path), "bytes": path.stat().st_size} for path in release_files()]
    formal_asset = ROOT / "SourceAssets/AircraftReference/Mini4/intake-manifest.placeholder.json"
    intake = json.loads(formal_asset.read_text(encoding="utf-8"))
    cliff_source = ROOT / "SourceAssets/CoastReusable/CliffSideCC0/SOURCE.md"
    return {
        "schema": "aerosim.release-manifest/v1",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "build_id": BUILD_ID,
        "runtime_contract": "aerosim.runtime.phase7/v1",
        "source_revision": git_value("rev-parse", "HEAD"),
        "source_dirty": bool(git_value("status", "--porcelain")),
        "engine": "UE 5.8",
        "candidate": {"platform": "Mac", "configuration": "Development", "rhi": "Metal SM6", "status": "PENDING_VALIDATION"},
        "platforms": {
            "Mac": "PENDING_VALIDATION",
            "Windows": "NOT_RUN",
            "iPadOS": "NOT_RUN",
            "iOS": "NOT_RUN",
        },
        "assets": [
            {
                "id": "coast.cliff-side-detail.cc0",
                "license": "CC0",
                "source": "https://polyhaven.com/a/cliff_side",
                "source_record": str(cliff_source.relative_to(ROOT)),
                "source_record_sha256": sha256(cliff_source),
                "status": "SOURCE_RECORDED",
            },
            {
                "id": "aircraft.mini4.formal",
                "license": "NOT_DELIVERED",
                "source_record": str(formal_asset.relative_to(ROOT)),
                "source_record_sha256": sha256(formal_asset),
                "status": "BLOCKED_ASSET" if intake.get("status") == "CONTENT_NOT_DELIVERED" else "REVIEW_REQUIRED",
            },
        ],
        "release_gates": {
            "formal_aircraft": "BLOCKED_ASSET",
            "user_appearance_confirmation": "BLOCKED_USER_CONFIRMATION",
            "target_hardware_performance": "BLOCKED_TARGET_HARDWARE_EVIDENCE",
            "windows_candidate": "NOT_RUN",
            "ipad_device": "NOT_RUN",
            "final_visual_ux": "BLOCKED_USER_CONFIRMATION",
        },
        "files": files,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output if args.output.is_absolute() else ROOT / args.output
    output.parent.mkdir(parents=True, exist_ok=True)
    manifest = build_manifest()
    output.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    digest_path = output.with_suffix(output.suffix + ".sha256")
    digest_path.write_text(f"{sha256(output)}  {output.name}\n", encoding="utf-8")
    print(f"PHASE7 RELEASE MANIFEST GENERATED: {output}")
    print(f"SHA256: {sha256(output)}")
    print("GATES: BLOCKED_ASSET BLOCKED_USER_CONFIRMATION BLOCKED_TARGET_HARDWARE_EVIDENCE Windows=NOT_RUN iPadOS=NOT_RUN")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
