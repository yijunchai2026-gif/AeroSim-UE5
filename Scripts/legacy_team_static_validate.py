#!/usr/bin/env python3
"""Validate legacy Unity source indexing and the fixed 15-lane virtual team contract."""
from __future__ import annotations

import hashlib
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LEGACY = ROOT / "docs" / "legacy-unity"
EXPECTED = (
    "AGENTS.md", "ART_DIRECTION_V2.md", "DEVELOPMENT_PLAN.md", "飞行模拟器产品设计文档.md",
    "docs/V125_PRD_CONFLICT_REVIEW.md", "docs/V125_PRD_REORGANIZATION_QA.md",
    "docs/V125_UI_MOBILE_AUDIT.md", "docs/V125_UX_FLOW_AUDIT.md",
    "docs/V125_UX_UI_OPTIMIZATION_RESEARCH.md", "docs/V125_UX_UI_RESEARCH_QA.md",
    "docs/V1_SCOPE_REVIEW.md", "docs/V1_SCREEN_SPEC.md", "docs/V1_TECHNICAL_PLAN.md",
)


def main() -> int:
    checks = 0
    errors: list[str] = []

    def require(condition: bool, message: str) -> None:
        nonlocal checks
        checks += 1
        if not condition:
            errors.append(message)

    index = LEGACY / "README.md"
    require(index.is_file(), "legacy Unity README index missing")
    index_text = index.read_text(encoding="utf-8") if index.is_file() else ""
    require("历史产品意图与迁移输入，不覆盖 UE5 当前事实" in index_text, "legacy precedence warning missing")
    require("不收录" in index_text and "内部 skill" in index_text, "legacy inclusion/exclusion policy missing")

    old_root = ROOT.parent / "AeroSim-runtime-work"
    for relative in EXPECTED:
        copied = LEGACY / relative
        require(copied.is_file() and copied.stat().st_size > 0, f"missing legacy original: {relative}")
        require(relative in index_text, f"legacy index missing entry: {relative}")
        source = old_root / relative
        if source.is_file() and copied.is_file():
            require(hashlib.sha256(source.read_bytes()).digest() == hashlib.sha256(copied.read_bytes()).digest(), f"legacy original changed: {relative}")

    team = (ROOT / "docs" / "TEAM_EXECUTION_PLAN.md").read_text(encoding="utf-8")
    acceptance = (ROOT / "docs" / "PHASE0_ACCEPTANCE.md").read_text(encoding="utf-8")
    current_docs = team + "\n" + acceptance
    for token in ("PMO-01", "RD-01..10", "QA-01..03", "UI-01", "15 条虚拟职责泳道", "UX 由 UI-01 与 PMO-01"):
        require(token in current_docs, f"team contract token missing: {token}")
    for forbidden in ("QA-04", "QA-05", "UI-02", "UI-10", "UX-01", "UX-02", "5 QA", "10 UI", "2 UX"):
        require(forbidden not in current_docs, f"obsolete team lane remains: {forbidden}")

    matrix = (ROOT / "docs" / "UNITY_TO_UE5_MIGRATION_MATRIX.md").read_text(encoding="utf-8")
    for relative in EXPECTED:
        require(f"`{relative}`" in matrix, f"migration matrix missing legacy status: {relative}")
    require("BLOCKED_ASSET" in matrix and "NOT_RUN" in matrix, "truthful blocked/not-run boundaries missing")

    if errors:
        for error in errors:
            print(f"FAIL: {error}")
        print(f"LEGACY/TEAM STATIC VALIDATION FAILED: {len(errors)} error(s), {checks} checks")
        return 1
    print(f"LEGACY/TEAM STATIC VALIDATION PASS: {checks} checks")
    return 0


if __name__ == "__main__":
    sys.exit(main())
