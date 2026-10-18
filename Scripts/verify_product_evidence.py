#!/usr/bin/env python3
"""Check Product Shell PNG/JSON evidence without UE, dependencies, or writes.

Usage: python3 Scripts/verify_product_evidence.py screenshot.png [other.png ...]
Optional --expect-pages checks exact coverage, not names guessed from filenames.
Source/build drift within a batch fails. Expected source/configuration can be
supplied independently; self-declared metadata cannot attest a packaged binary.

Hash the UTF-8 canonical_manifest string *as saved*, without reserializing JSON.
PNG verification reads IHDR, not decoded pixels: synthetic test images are not
runtime evidence. Zero/unavailable diagnostic counters fail closed (zero texture
streaming is valid). PASS means complete diagnostic evidence, never performance,
human visual, user approval, Shipping qualification, or release acceptance.
"""
from __future__ import annotations

import argparse
import binascii
import hashlib
import hmac
import json
import math
import re
import struct
from pathlib import Path

SCHEMA = "aerosim.product-evidence/v1"
RUNTIME = "aerosim.runtime.phase7/v1"
ENGINE = "5.8.3"
PAGE_NAMES = (
    "Home", "Hangar", "Mission", "Preflight", "Flight", "Settings",
    "VisualLab", "Result", "Replay",
)
METRIC_FIELDS = (
    "diagnostic_frame_p95_ms", "diagnostic_samples", "diagnostic_cpu_game_ms",
    "diagnostic_cpu_render_ms", "diagnostic_gpu_ms", "diagnostic_resident_mib",
    "diagnostic_draw_calls", "diagnostic_streaming_mib",
)
TEXT_FIELDS = (
    "build_id", "platform", "hardware", "gpu", "rhi", "camera_contract",
    "diagnostic_scope",
)
IDENTITY_FIELDS = (
    "build_id", "source_revision", "source_fingerprint", "source_dirty",
    "runtime_contract", "ue_version", "platform", "hardware", "gpu", "rhi",
    "scene_id", "configuration",
)
POLICY_FIELDS = {
    "schema": SCHEMA,
    "runtime_contract": RUNTIME,
    "scene_id": "pacific-coast-inspection",
    "visual_verdict": "NOT_RUN - HUMAN G0-G4 REQUIRED",
    "asset_status": "GENERIC PROXY / BLOCKED_ASSET",
    "user_confirmation": "BLOCKED_USER_CONFIRMATION",
    "performance_claim": "NOT_MEASURED",
}
MAX_ITEMS = 64
MAX_JSON_BYTES = 1024 * 1024
MAX_CANONICAL_BYTES = 256 * 1024
MAX_IMAGE_BYTES = 64 * 1024 * 1024
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
UNKNOWN_VALUES = {"", "UNKNOWN", "UNAVAILABLE", "NOT_RECORDED", "NOT_MEASURED",
                  "NOT_RUN", "NONE", "N/A"}


class EvidenceError(ValueError):
    """Only fixed, path-free codes may be exposed to the caller."""


def _reject_constant(_value: str) -> None:
    raise EvidenceError("NONFINITE_JSON")


def _unique_object(pairs: list) -> dict:
    result = {}
    for key, value in pairs:
        if key in result:
            raise EvidenceError("DUPLICATE_JSON_KEY")
        result[key] = value
    return result


def _json_object(text: str) -> dict:
    try:
        value = json.loads(text, object_pairs_hook=_unique_object,
                           parse_constant=_reject_constant)
    except (json.JSONDecodeError, RecursionError) as error:
        raise EvidenceError("INVALID_JSON") from error
    if not isinstance(value, dict):
        raise EvidenceError("JSON_OBJECT_REQUIRED")
    return value


def _hex(value: object, length: int) -> bool:
    return (isinstance(value, str)
            and re.fullmatch(r"[0-9a-f]{%d}" % length, value) is not None
            and value != "0" * length)


def _known_text(value: object) -> bool:
    return (isinstance(value, str) and len(value) <= 512
            and value.strip().upper() not in UNKNOWN_VALUES
            and not any(ord(char) < 32 for char in value))


def _number(value: object, low: float, high: float, integer: bool = False) -> bool:
    if type(value) not in (int, float):  # bool is not a metric or enum.
        return False
    try:
        return (math.isfinite(value) and low <= value <= high
                and (not integer or int(value) == value))
    except (OverflowError, ValueError):
        return False


def _read_envelope(path: Path) -> dict:
    try:
        with path.open("rb") as stream:
            data = stream.read(MAX_JSON_BYTES + 1)
    except OSError as error:
        raise EvidenceError("METADATA_UNREADABLE") from error
    if len(data) > MAX_JSON_BYTES:
        raise EvidenceError("METADATA_TOO_LARGE")
    try:
        # FString AutoDetect saves may use a BOM; the inner hash is always UTF-8.
        encoding = "utf-16" if data.startswith((b"\xff\xfe", b"\xfe\xff")) else "utf-8-sig"
        return _json_object(data.decode(encoding))
    except UnicodeError as error:
        raise EvidenceError("INVALID_METADATA_ENCODING") from error


def _png_digest(path: Path) -> tuple[str, list[int]]:
    digest = hashlib.sha256()
    try:
        with path.open("rb") as stream:
            header = stream.read(33)
            if (len(header) != 33 or header[:8] != PNG_SIGNATURE
                    or header[8:16] != b"\x00\x00\x00\x0dIHDR"):
                raise EvidenceError("INVALID_PNG_HEADER")
            width, height, depth, color, compression, filtering, interlace = struct.unpack(
                ">IIBBBBB", header[16:29])
            depths = {0: (1, 2, 4, 8, 16), 2: (8, 16), 3: (1, 2, 4, 8),
                      4: (8, 16), 6: (8, 16)}
            if (not 1 <= width <= 16384 or not 1 <= height <= 16384
                    or width * height > 64 * 1024 * 1024):
                raise EvidenceError("INVALID_PNG_DIMENSIONS")
            if (depth not in depths.get(color, ()) or compression != 0
                    or filtering != 0 or interlace not in (0, 1)
                    or binascii.crc32(header[12:29]) != struct.unpack(">I", header[29:33])[0]):
                raise EvidenceError("INVALID_PNG_IHDR")
            digest.update(header)
            total = len(header)
            while chunk := stream.read(1024 * 1024):
                total += len(chunk)
                if total > MAX_IMAGE_BYTES:
                    raise EvidenceError("IMAGE_TOO_LARGE")
                digest.update(chunk)
    except OSError as error:
        raise EvidenceError("IMAGE_UNREADABLE") from error
    return digest.hexdigest(), [width, height]


def _manifest_errors(manifest: dict) -> list[str]:
    errors = []
    for field, expected in POLICY_FIELDS.items():
        if manifest.get(field) != expected:
            errors.append("FIELD_MISMATCH:" + field)
    for field in TEXT_FIELDS:
        if not _known_text(manifest.get(field)):
            errors.append("MISSING_OR_UNKNOWN:" + field)
    for field, length in (("source_revision", 40), ("source_fingerprint", 64),
                          ("image_sha256", 64)):
        if not _hex(manifest.get(field), length):
            errors.append("INVALID_OR_MISSING:" + field)
    if type(manifest.get("source_dirty")) is not bool:
        errors.append("INVALID_OR_MISSING:source_dirty")
    version = manifest.get("ue_version")
    if (not isinstance(version, str) or len(version) > 256
            or re.fullmatch(r"5\.8\.3(?:-[0-9]+(?:\+[A-Za-z0-9_.+-]+)?)?", version) is None):
        errors.append("ENGINE_VERSION_MISMATCH")
    configuration = manifest.get("configuration")
    if (not isinstance(configuration, str) or re.fullmatch(
            r"(Development|Shipping) diagnostic / NOT_RELEASE_EVIDENCE", configuration) is None):
        errors.append("CONFIGURATION_UNKNOWN_OR_RELEASE_CLAIM")
    if isinstance(manifest.get("rhi"), str) and manifest["rhi"].lower() in ("null", "nullrhi"):
        errors.append("PIXEL_RHI_REQUIRED")
    for field, low, high, integer in (
            ("product_page", 0, len(PAGE_NAMES) - 1, True), ("weather", 0, 2, True),
            ("time_minutes", 0, 1439, True), ("seed", 0, 2147483647, True),
            ("quality_tier", 0, 2, True), ("vertical_fov", 35, 100, False)):
        if not _number(manifest.get(field), low, high, integer):
            errors.append("INVALID_OR_MISSING:" + field)
    for field in METRIC_FIELDS:
        integer = field in ("diagnostic_samples", "diagnostic_draw_calls")
        if not _number(manifest.get(field), 0, 1e12, integer):
            errors.append("INVALID_OR_MISSING:" + field)
    return errors


def _verify(path: Path) -> tuple[dict, dict | None]:
    result = {"status": "FAIL", "integrity_status": "FAIL",
              "metrics_status": "NOT_MEASURED", "errors": []}
    try:
        if path.name.lower().endswith(".png.json"):
            path = path.with_name(path.name[:-5])
        if path.suffix.lower() != ".png":
            raise EvidenceError("PNG_PATH_REQUIRED")
        envelope = _read_envelope(Path(str(path) + ".json"))
        canonical = envelope.get("canonical_manifest")
        if not isinstance(canonical, str):
            raise EvidenceError("CANONICAL_MANIFEST_MISSING")
        try:
            canonical_bytes = canonical.encode("utf-8")
        except UnicodeError as error:
            raise EvidenceError("INVALID_CANONICAL_ENCODING") from error
        if len(canonical_bytes) > MAX_CANONICAL_BYTES:
            raise EvidenceError("CANONICAL_MANIFEST_TOO_LARGE")
        claimed = envelope.get("manifest_sha256")
        if not _hex(claimed, 64):
            raise EvidenceError("MANIFEST_SHA256_MISSING_OR_INVALID")
        digest = hashlib.sha256(canonical_bytes).hexdigest()
        if not hmac.compare_digest(claimed, digest):
            raise EvidenceError("MANIFEST_SHA256_MISMATCH")
        manifest = _json_object(canonical)
        errors = _manifest_errors(manifest)
        filename = manifest.get("image_file")
        if (not isinstance(filename, str) or filename != path.name
                or any(char in filename for char in ("/", "\\", ":", "\x00"))):
            errors.append("IMAGE_FILENAME_MISMATCH")
        if errors:
            result["errors"] = errors
            return result, None
        image_digest, dimensions = _png_digest(path)
        if not hmac.compare_digest(image_digest, manifest["image_sha256"]):
            raise EvidenceError("IMAGE_SHA256_MISMATCH")
        # New capture fields are checked when supplied, never invented for old captures.
        if "viewport_width" in manifest or "viewport_height" in manifest:
            viewport = [manifest.get("viewport_width"), manifest.get("viewport_height")]
            if (not all(_number(value, 1, 16384, True) for value in viewport)
                    or viewport != dimensions):
                raise EvidenceError("VIEWPORT_DIMENSION_MISMATCH")
        page = int(manifest["product_page"])
        result.update({
            "integrity_status": "PASS", "page": PAGE_NAMES[page], "page_id": page,
            "source_revision": manifest["source_revision"],
            "source_fingerprint": manifest["source_fingerprint"],
            "source_dirty": manifest["source_dirty"], "ue_version": ENGINE,
            "runtime_contract": RUNTIME,
            "recorded_configuration": manifest["configuration"].split(" ", 1)[0],
            "manifest_sha256": digest, "image_sha256": image_digest,
            "image_dimensions": dimensions, "pixel_validation": "PNG_HEADER_ONLY",
        })
        missing = ["METRIC_NOT_MEASURED:" + field for field in METRIC_FIELDS
                   if field != "diagnostic_streaming_mib" and manifest[field] <= 0]
        result["errors"] = missing
        result["metrics_status"] = "BLOCKED_NOT_MEASURED" if missing else "COMPLETE_DIAGNOSTIC_ONLY"
        result["status"] = "FAIL" if missing else "PASS"
        return result, manifest
    except EvidenceError as error:
        result["errors"].append(str(error))
        return result, None


def verify_screenshot(path: str | Path) -> dict:
    """Read one PNG and its .png.json sibling; never expose input paths or text."""
    return _verify(Path(path))[0]


def verify_screenshots(paths: list, *, expected_pages: list | None = None,
                       expected_revision: str | None = None,
                       expected_fingerprint: str | None = None,
                       expected_configuration: str | None = None) -> dict:
    report = {
        "schema": "aerosim.product-evidence-verification/v1", "status": "FAIL",
        "evidence_class": "NOT_RELEASE_EVIDENCE", "performance_claim": "NOT_MEASURED",
        "visual_verdict": "NOT_RUN", "configuration_verification": "MANIFEST_ONLY_NOT_BINARY_VERIFIED",
        "provenance_verification": "DECLARED_HASHES_NOT_REPOSITORY_ATTESTED",
        "items": [], "pages_recorded": [], "errors": [],
    }
    if not 1 <= len(paths) <= MAX_ITEMS:
        report["errors"].append("SCREENSHOT_COUNT_OUT_OF_RANGE")
        return report
    if ((expected_pages is not None and (not expected_pages or any(page not in PAGE_NAMES for page in expected_pages)))
            or (expected_revision is not None and not _hex(expected_revision, 40))
            or (expected_fingerprint is not None and not _hex(expected_fingerprint, 64))
            or (expected_configuration is not None and expected_configuration not in ("Development", "Shipping"))):
        report["errors"].append("INVALID_EXPECTATION")
        return report
    baseline = None
    pages = set()
    for index, path in enumerate(paths, start=1):
        item, manifest = _verify(Path(path))
        item["index"] = index
        if manifest is not None:
            pages.add(item["page"])
            identity = tuple(manifest[field] for field in IDENTITY_FIELDS)
            if baseline is None:
                baseline = identity
            elif identity != baseline:
                item["errors"].append("BATCH_PROVENANCE_OR_BUILD_DRIFT")
            for field, expected in (("source_revision", expected_revision),
                                    ("source_fingerprint", expected_fingerprint)):
                if expected is not None and manifest[field] != expected:
                    item["errors"].append("EXPECTED_VALUE_MISMATCH:" + field)
            if expected_configuration is not None and item["recorded_configuration"] != expected_configuration:
                item["errors"].append("EXPECTED_CONFIGURATION_MISMATCH")
            if item["errors"]:
                item["status"] = "FAIL"
        report["items"].append(item)
    report["pages_recorded"] = [page for page in PAGE_NAMES if page in pages]
    if expected_pages is not None:
        missing = set(expected_pages) - pages
        unexpected = pages - set(expected_pages)
        report["errors"].extend("EXPECTED_PAGE_MISSING:" + page for page in PAGE_NAMES if page in missing)
        report["errors"].extend("UNEXPECTED_PAGE:" + page for page in PAGE_NAMES if page in unexpected)
    report["passed"] = sum(item["status"] == "PASS" for item in report["items"])
    report["failed"] = len(report["items"]) - report["passed"]
    report["status"] = "PASS" if not report["errors"] and not report["failed"] else "FAIL"
    return report


class _Parser(argparse.ArgumentParser):
    def error(self, _message: str) -> None:
        raise EvidenceError("INVALID_ARGUMENTS")


def main(argv: list[str] | None = None) -> int:
    parser = _Parser(prog="verify_product_evidence.py", description=__doc__)
    parser.add_argument("screenshots", nargs="+")
    parser.add_argument("--expect-pages", nargs="+", choices=PAGE_NAMES)
    parser.add_argument("--expect-source-revision")
    parser.add_argument("--expect-source-fingerprint")
    parser.add_argument("--expect-configuration", choices=("Development", "Shipping"))
    try:
        args = parser.parse_args(argv)
        report = verify_screenshots(args.screenshots, expected_pages=args.expect_pages,
                                    expected_revision=args.expect_source_revision,
                                    expected_fingerprint=args.expect_source_fingerprint,
                                    expected_configuration=args.expect_configuration)
    except EvidenceError:
        report = {"status": "FAIL", "evidence_class": "NOT_RELEASE_EVIDENCE",
                  "errors": ["INVALID_ARGUMENTS"]}
    print(json.dumps(report, ensure_ascii=True, allow_nan=False, separators=(",", ":")))
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
