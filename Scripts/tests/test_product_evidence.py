"""Synthetic fixtures validate the verifier; they are not UE/pixel evidence."""
from __future__ import annotations

import binascii
import hashlib
import importlib.util
import json
import re
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "Scripts/verify_product_evidence.py"
SPEC = importlib.util.spec_from_file_location("verify_product_evidence", SCRIPT)
VERIFIER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VERIFIER)


def dummy_png(width: int = 32, height: int = 18) -> bytes:
    """Valid IHDR + dummy bytes; intentionally no claim of decoded pixel data."""
    ihdr = b"IHDR" + struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + struct.pack(">I", 13) + ihdr
            + struct.pack(">I", binascii.crc32(ihdr)) + b"synthetic-not-rendered")


class ProductEvidenceTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory(prefix="evidence-private-")
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.image = self.directory / "private-name.png"
        self.image.write_bytes(dummy_png())
        self.manifest = {
            "schema": "aerosim.product-evidence/v1",
            "build_id": "SYNTHETIC-NOT-A-UE-BUILD",
            "source_revision": "a" * 40, "source_fingerprint": "b" * 64,
            "source_dirty": True, "runtime_contract": "aerosim.runtime.phase7/v1",
            "ue_version": "5.8.3-0+++UE5+Release-5.8", "platform": "Mac",
            "hardware": "Synthetic CPU", "gpu": "Synthetic GPU", "rhi": "Metal",
            "scene_id": "pacific-coast-inspection",
            "configuration": "Development diagnostic / NOT_RELEASE_EVIDENCE",
            "product_page": 0, "weather": 0, "time_minutes": 990, "seed": 21630,
            "quality_tier": 0, "vertical_fov": 60,
            "camera_contract": "Context+Family+Preset / vertical FOV truth",
            "viewport_width": 32, "viewport_height": 18,
            "visual_verdict": "NOT_RUN - HUMAN G0-G4 REQUIRED",
            "asset_status": "GENERIC PROXY / BLOCKED_ASSET",
            "user_confirmation": "BLOCKED_USER_CONFIRMATION",
            "performance_claim": "NOT_MEASURED",
            "diagnostic_frame_p95_ms": 16.7, "diagnostic_samples": 120,
            "diagnostic_cpu_game_ms": 4.0, "diagnostic_cpu_render_ms": 3.0,
            "diagnostic_gpu_ms": 8.0, "diagnostic_resident_mib": 512,
            "diagnostic_draw_calls": 17, "diagnostic_streaming_mib": 0,
            "diagnostic_scope": "rolling window + latest engine counters / not qualification",
            "image_file": self.image.name,
            "image_sha256": hashlib.sha256(self.image.read_bytes()).hexdigest(),
        }

    def write(self, manifest: dict | None = None, image: Path | None = None,
              canonical: str | None = None, encoding: str = "utf-8") -> Path:
        image = image or self.image
        if canonical is None:
            canonical = json.dumps(manifest or self.manifest, indent=2, ensure_ascii=False)
        envelope = {"canonical_manifest": canonical,
                    "manifest_sha256": hashlib.sha256(canonical.encode("utf-8")).hexdigest()}
        metadata = Path(str(image) + ".json")
        metadata.write_text(json.dumps(envelope, ensure_ascii=False), encoding=encoding)
        return metadata

    def assert_failure(self, code: str) -> dict:
        result = VERIFIER.verify_screenshot(self.image)
        self.assertEqual(result["status"], "FAIL", result)
        self.assertIn(code, result["errors"])
        return result

    def test_valid_exact_string_and_header_only(self) -> None:
        self.write()
        report = VERIFIER.verify_screenshots([self.image], expected_pages=["Home"])
        self.assertEqual(report["status"], "PASS", report)
        self.assertEqual(report["pages_recorded"], ["Home"])
        self.assertEqual(report["evidence_class"], "NOT_RELEASE_EVIDENCE")
        item = report["items"][0]
        self.assertEqual(item["image_dimensions"], [32, 18])
        self.assertEqual(item["pixel_validation"], "PNG_HEADER_ONLY")
        self.assertEqual(item["recorded_configuration"], "Development")
        self.assertEqual(report["performance_claim"], "NOT_MEASURED")
        self.assertEqual(report["visual_verdict"], "NOT_RUN")
        self.assertIn("NOT_BINARY_VERIFIED", report["configuration_verification"])

    def test_bom_encodings_and_unicode_preserve_inner_utf8_hash(self) -> None:
        self.manifest["hardware"] = "Synthetic CPU \u03b1"
        for encoding in ("utf-8-sig", "utf-16"):
            with self.subTest(encoding=encoding):
                metadata = self.write(encoding=encoding)
                self.assertEqual(VERIFIER.verify_screenshot(metadata)["status"], "PASS")

    def test_missing_or_invalid_hash_fields_fail(self) -> None:
        for field in ("manifest_sha256", "canonical_manifest"):
            for value in (None, "", 3, "UNKNOWN"):
                with self.subTest(field=field, value=value):
                    metadata = self.write()
                    envelope = json.loads(metadata.read_text())
                    envelope[field] = value
                    metadata.write_text(json.dumps(envelope))
                    self.assertEqual(VERIFIER.verify_screenshot(self.image)["status"], "FAIL")
        del self.manifest["image_sha256"]
        self.write()
        self.assert_failure("INVALID_OR_MISSING:image_sha256")

    def test_manifest_tampering_and_whitespace_are_not_reserialized(self) -> None:
        metadata = self.write()
        envelope = json.loads(metadata.read_text())
        envelope["canonical_manifest"] += "\n"
        metadata.write_text(json.dumps(envelope))
        self.assert_failure("MANIFEST_SHA256_MISMATCH")
        self.write(canonical=json.dumps(self.manifest, separators=(",", ":")) + "\n")
        self.assertEqual(VERIFIER.verify_screenshot(self.image)["status"], "PASS")

    def test_image_tampering_and_absence(self) -> None:
        self.write()
        self.image.write_bytes(dummy_png() + b"changed")
        self.assert_failure("IMAGE_SHA256_MISMATCH")
        self.image.unlink()
        self.assert_failure("IMAGE_UNREADABLE")

    def test_metadata_absence_and_invalid_json(self) -> None:
        self.assert_failure("METADATA_UNREADABLE")
        metadata = Path(str(self.image) + ".json")
        for text in ("{", "[]", "null", '{"private": NaN}'):
            metadata.write_text(text)
            self.assertEqual(VERIFIER.verify_screenshot(self.image)["status"], "FAIL")

    def test_missing_unknown_and_malformed_provenance(self) -> None:
        original = self.manifest.copy()
        for field in ("source_revision", "source_fingerprint", "source_dirty", "ue_version",
                      "runtime_contract", "build_id", "platform", "hardware", "gpu", "rhi"):
            for value in (None, "UNKNOWN", "", 1 if field == "source_dirty" else True):
                with self.subTest(field=field, value=value):
                    self.manifest = {**original, field: value}
                    self.write()
                    self.assertEqual(VERIFIER.verify_screenshot(self.image)["status"], "FAIL")
        for revision in ("a" * 39, "a" * 41, "0" * 40, "A" * 40, "g" * 40):
            self.write({**original, "source_revision": revision})
            self.assert_failure("INVALID_OR_MISSING:source_revision")

    def test_wrong_engine_runtime_or_false_pass_policy(self) -> None:
        for version in ("5.8", "5.8.2", "5.8.30", "5.9.3", "UNKNOWN"):
            self.write({**self.manifest, "ue_version": version})
            self.assert_failure("ENGINE_VERSION_MISMATCH")
        for field in ("runtime_contract", "performance_claim", "visual_verdict",
                      "asset_status", "user_confirmation"):
            self.write({**self.manifest, field: "PASS"})
            self.assert_failure("FIELD_MISMATCH:" + field)

    def test_each_missing_invalid_and_unavailable_metric_fails(self) -> None:
        for field in VERIFIER.METRIC_FIELDS:
            for value in (None, "1", True, -1, float("nan"), float("inf")):
                with self.subTest(field=field, value=value):
                    self.write({**self.manifest, field: value})
                    self.assertEqual(VERIFIER.verify_screenshot(self.image)["status"], "FAIL")
            missing = self.manifest.copy()
            del missing[field]
            self.write(missing)
            self.assert_failure("INVALID_OR_MISSING:" + field)
            if field != "diagnostic_streaming_mib":
                self.write({**self.manifest, field: 0})
                result = self.assert_failure("METRIC_NOT_MEASURED:" + field)
                self.assertEqual(result["integrity_status"], "PASS")
                self.assertEqual(result["metrics_status"], "BLOCKED_NOT_MEASURED")

    def test_pages_come_from_manifest_not_private_filename(self) -> None:
        for page_id, name in enumerate(VERIFIER.PAGE_NAMES):
            self.write({**self.manifest, "product_page": page_id})
            result = VERIFIER.verify_screenshot(self.image)
            self.assertEqual(result["page"], name)
            self.assertEqual(result["page_id"], page_id)
        for page_id in (-1, 9, "Home", True, 1.5):
            self.write({**self.manifest, "product_page": page_id})
            self.assert_failure("INVALID_OR_MISSING:product_page")

    def test_exact_multi_page_coverage_and_all_identity_drift(self) -> None:
        self.write()
        second = self.directory / "other.png"
        second.write_bytes(self.image.read_bytes())
        next_manifest = {**self.manifest, "image_file": second.name, "product_page": 5}
        self.write(next_manifest, second)
        report = VERIFIER.verify_screenshots([self.image, second], expected_pages=["Home", "Settings"])
        self.assertEqual(report["status"], "PASS", report)
        report = VERIFIER.verify_screenshots([self.image, second], expected_pages=["Home", "Replay"])
        self.assertEqual(report["status"], "FAIL")
        self.assertIn("EXPECTED_PAGE_MISSING:Replay", report["errors"])
        self.assertIn("UNEXPECTED_PAGE:Settings", report["errors"])
        changes = {"build_id": "OTHER-SYNTHETIC-BUILD", "source_revision": "c" * 40,
                   "source_fingerprint": "d" * 64, "source_dirty": False,
                   "ue_version": "5.8.3-1+++UE5+Release-5.8", "platform": "Windows",
                   "hardware": "Other CPU", "gpu": "Other GPU", "rhi": "D3D12",
                   "configuration": "Shipping diagnostic / NOT_RELEASE_EVIDENCE"}
        for field, value in changes.items():
            with self.subTest(field=field):
                self.write({**next_manifest, field: value}, second)
                report = VERIFIER.verify_screenshots([self.image, second])
                self.assertEqual(report["status"], "FAIL")
                self.assertIn("BATCH_PROVENANCE_OR_BUILD_DRIFT", report["items"][1]["errors"])

    def test_independent_source_and_shipping_expectations(self) -> None:
        self.write()
        report = VERIFIER.verify_screenshots([self.image], expected_revision="c" * 40,
                                            expected_fingerprint="d" * 64,
                                            expected_configuration="Shipping")
        self.assertEqual(report["status"], "FAIL")
        self.assertIn("EXPECTED_CONFIGURATION_MISMATCH", report["items"][0]["errors"])
        self.assertIn("EXPECTED_VALUE_MISMATCH:source_revision", report["items"][0]["errors"])
        self.assertIn("EXPECTED_VALUE_MISMATCH:source_fingerprint", report["items"][0]["errors"])
        self.manifest["configuration"] = "Shipping diagnostic / NOT_RELEASE_EVIDENCE"
        self.write()
        report = VERIFIER.verify_screenshots([self.image], expected_configuration="Shipping")
        self.assertEqual(report["status"], "PASS")
        self.assertEqual(report["evidence_class"], "NOT_RELEASE_EVIDENCE")
        for configuration in ("Shipping", "Release PASS", "UNKNOWN", "Development"):
            self.write({**self.manifest, "configuration": configuration})
            self.assert_failure("CONFIGURATION_UNKNOWN_OR_RELEASE_CLAIM")

    def test_png_dimensions_crc_and_viewport_drift(self) -> None:
        for data, code in ((b"not png", "INVALID_PNG_HEADER"),
                           (dummy_png(0, 18), "INVALID_PNG_DIMENSIONS"),
                           (dummy_png(32, 18)[:29] + b"bad!", "INVALID_PNG_IHDR")):
            self.image.write_bytes(data)
            self.write({**self.manifest, "image_sha256": hashlib.sha256(data).hexdigest()})
            self.assert_failure(code)
        self.image.write_bytes(dummy_png(33, 18))
        self.write({**self.manifest, "image_sha256": hashlib.sha256(self.image.read_bytes()).hexdigest()})
        self.assert_failure("VIEWPORT_DIMENSION_MISMATCH")

    def test_filename_traversal_duplicate_keys_and_nullrhi(self) -> None:
        for filename in ("../private.png", str(self.image), "other.png", "folder\\private.png"):
            self.write({**self.manifest, "image_file": filename})
            self.assert_failure("IMAGE_FILENAME_MISMATCH")
        canonical = json.dumps(self.manifest)[:-1] + ', "product_page": 8}'
        self.write(canonical=canonical)
        self.assert_failure("DUPLICATE_JSON_KEY")
        metadata = self.write()
        text = metadata.read_text()[:-1] + ', "manifest_sha256": "' + "f" * 64 + '"}'
        metadata.write_text(text)
        self.assert_failure("DUPLICATE_JSON_KEY")
        self.write({**self.manifest, "rhi": "NullRHI"})
        self.assert_failure("PIXEL_RHI_REQUIRED")

    def test_bounded_output_no_paths_or_untrusted_values_and_exit_codes(self) -> None:
        self.manifest["hardware"] = str(self.directory / "private-hardware-name")
        self.manifest["build_id"] = str(self.directory / "private-build-name")
        self.write()
        run = subprocess.run([sys.executable, "-B", str(SCRIPT), str(self.image),
                              "--expect-pages", "Home"], capture_output=True, text=True)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        self.assertEqual(json.loads(run.stdout)["status"], "PASS")
        self.assertEqual(run.stderr, "")
        self.assertNotIn(str(self.directory), run.stdout)
        self.assertNotIn(self.image.name, run.stdout)
        self.assertNotIn("private-build-name", run.stdout)
        metadata = self.write({**self.manifest, "source_revision": str(self.directory)})
        run = subprocess.run([sys.executable, "-B", str(SCRIPT), str(metadata)], capture_output=True, text=True)
        self.assertEqual(run.returncode, 1)
        self.assertEqual(json.loads(run.stdout)["status"], "FAIL")
        self.assertNotIn(str(self.directory), run.stdout + run.stderr)
        report = VERIFIER.verify_screenshots([self.image] * VERIFIER.MAX_ITEMS)
        self.assertLess(len(json.dumps(report)), 100000)
        for paths in ([], [self.image] * (VERIFIER.MAX_ITEMS + 1)):
            self.assertEqual(VERIFIER.verify_screenshots(paths)["status"], "FAIL")

    def test_invalid_cli_arguments_are_json_not_pathful_tracebacks(self) -> None:
        for arguments in ([], [str(self.image), "--expect-configuration", str(self.directory)]):
            run = subprocess.run([sys.executable, "-B", str(SCRIPT), *arguments], capture_output=True, text=True)
            self.assertEqual(run.returncode, 1)
            self.assertEqual(json.loads(run.stdout)["errors"], ["INVALID_ARGUMENTS"])
            self.assertEqual(run.stderr, "")

    def test_schema_fixture_tracks_actual_producer_and_page_enum(self) -> None:
        producer = (ROOT / "Source/AeroSim/Private/AeroSimGameMode.cpp").read_text()
        fields = set(re.findall(r'Manifest->Set(?:String|Number|Bool)Field\(TEXT\("([a-z0-9_]+)"\)', producer))
        required = set(VERIFIER.METRIC_FIELDS + VERIFIER.TEXT_FIELDS + VERIFIER.IDENTITY_FIELDS)
        required.update(VERIFIER.POLICY_FIELDS)
        required.update(("image_file", "image_sha256", "product_page", "weather", "time_minutes", "seed",
                         "quality_tier", "vertical_fov"))
        self.assertFalse(required - fields, "Producer renamed a required evidence key")
        header = (ROOT / "Source/AeroSim/Public/ProductShell.h").read_text()
        enum = re.search(r"enum class EProductPage\s*:\s*uint8\s*\{([^}]+)\}", header)
        names = tuple(name.strip() for name in enum.group(1).split(","))
        self.assertEqual(names, VERIFIER.PAGE_NAMES, "Page enum order drift changes evidence meaning")


if __name__ == "__main__":
    unittest.main()
