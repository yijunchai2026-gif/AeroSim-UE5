#!/usr/bin/env python3
"""Wall-clock Phase 7 packaged replay/soak runner with fail-closed JSON evidence."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import time
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SENSITIVE_ENV_TOKENS = ("TOKEN", "SECRET", "PASSWORD", "API_KEY", "APIKEY", "CREDENTIAL")


def clean_environment() -> dict[str, str]:
    return {key: value for key, value in os.environ.items() if not any(token in key.upper() for token in SENSITIVE_ENV_TOKENS)}


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--project", type=Path)
    parser.add_argument("--seconds", type=int, choices=range(1, 1801), required=True)
    parser.add_argument("--rhi", choices=("metal", "nullrhi"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output if args.output.is_absolute() else ROOT / args.output
    output.parent.mkdir(parents=True, exist_ok=True)
    log_path = output.with_suffix(".log")
    command = [str(args.executable)]
    if args.project:
        command.append(str(args.project))
    command.extend([f"-Phase7SoakSeconds={args.seconds}", "-unattended", "-nosplash", "-stdout", "-FullStdOutLogOutput"])
    if args.rhi == "nullrhi":
        command.append("-NullRHI")
    started = time.monotonic()
    status = "BLOCKED_EXECUTABLE_MISSING"
    return_code: int | None = None
    text = ""
    if args.executable.is_file():
        try:
            run = subprocess.run(command, cwd=ROOT, env=clean_environment(), text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=args.seconds + 180)
            return_code = run.returncode
            text = run.stdout
            status = "PASS" if return_code == 0 and "PHASE7_SOAK_PASS" in text and "PHASE7_SOAK_FAIL" not in text else "FAIL"
        except subprocess.TimeoutExpired as error:
            text = (error.stdout or "") if isinstance(error.stdout, str) else ""
            status = "FAIL_TIMEOUT"
    log_path.write_text(text, encoding="utf-8")
    report = {
        "schema": "aerosim.soak-report/v1",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "build_id": "AeroSim-Phase7-20261016",
        "runtime_contract": "aerosim.runtime.phase7/v1",
        "requested_seconds": args.seconds,
        "wall_seconds": round(time.monotonic() - started, 3),
        "rhi": "Metal SM6" if args.rhi == "metal" else "NullRHI",
        "status": status,
        "return_code": return_code,
        "pass_marker": "PHASE7_SOAK_PASS" in text,
        "fail_marker": "PHASE7_SOAK_FAIL" in text,
        "log": log_path.name,
        "log_sha256": digest(log_path),
        "performance_claim": "NOT_MEASURED",
    }
    output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    return 0 if status == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
