#!/usr/bin/env python3
"""Stage three bounded, publisher-hash-verified CC0 close-range ground maps."""
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import argparse
import hashlib
import json
from pathlib import Path
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
ASSET = "sandy_gravel_02"
UA = "AeroSim-asset-research/1.0 (development CC0 attribution audit)"
INFO_URL = "https://api.polyhaven.com/info/" + ASSET
FILES_URL = "https://api.polyhaven.com/files/" + ASSET
CANONICAL_FOLDER = "SourceAssets/EnvironmentDraft/GravelDraft"
MAPS = (("Diffuse", "basecolor.jpg", "sRGB base color"),
        ("nor_dx", "normal.jpg", "DirectX tangent-space normal, linear"),
        ("Rough", "roughness.jpg", "linear roughness"))


def fetch(url: str, cap: int) -> bytes:
    if not url.startswith(("https://api.polyhaven.com/", "https://dl.polyhaven.org/")):
        raise ValueError("Unexpected provider host")
    request = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(request, timeout=60) as response:
        if int(response.headers.get("Content-Length", "0")) > cap:
            raise ValueError("Declared provider response exceeds the download cap")
        data = response.read(cap + 1)
    if len(data) > cap:
        raise ValueError("Provider response exceeds the download cap")
    return data


def acquire(output: Path) -> dict:
    info_bytes = fetch(INFO_URL, 256_000)
    files_bytes = fetch(FILES_URL, 2_000_000)
    info, files = json.loads(info_bytes), json.loads(files_bytes)
    # Poly Haven's official API schema defines dimensions in millimeters.
    dimensions_mm = info["dimensions"]
    if len(dimensions_mm) != 2 or not all(2000 <= item <= 4000 for item in dimensions_mm):
        raise ValueError("Provider scale changed; review actual physical dimensions")
    output.mkdir(parents=True, exist_ok=True)

    def one_map(job: tuple) -> dict:
        key, filename, role = job
        descriptor = files[key]["2k"]["jpg"]
        if descriptor["size"] > 8 * 1024 * 1024:
            raise ValueError("Individual source map exceeds the texture-only cap")
        path = output / filename
        data = path.read_bytes() if path.exists() else b""
        if len(data) != descriptor["size"] or hashlib.md5(data).hexdigest() != descriptor["md5"]:
            data = fetch(descriptor["url"], 8 * 1024 * 1024)
        if len(data) != descriptor["size"] or hashlib.md5(data).hexdigest() != descriptor["md5"]:
            raise ValueError("Provider size or MD5 mismatch: " + key)
        pending = path.with_suffix(".jpg.partial")
        pending.write_bytes(data)
        pending.replace(path)
        return {"path": CANONICAL_FOLDER + "/" + filename, "sha256": hashlib.sha256(data).hexdigest(),
                "bytes": len(data), "publisher_md5": descriptor["md5"], "publisher_md5_verified": True,
                "url": descriptor["url"], "asset": "https://polyhaven.com/a/" + ASSET,
                "license": "CC0-1.0", "role": role, "source_resolution": "2k"}

    with ThreadPoolExecutor(max_workers=3) as pool:
        sources = list(pool.map(one_map, MAPS))
    manifest = {
        "schema": "aerosim.environment-source/v1", "state": "DRAFT_NOT_USER_CONFIRMED",
        "intake_state": "STAGED_NOT_IMPORTED_OR_APPLIED",
        "acquired_utc": datetime.now(timezone.utc).isoformat(),
        "asset_id": ASSET, "asset_page": "https://polyhaven.com/a/" + ASSET,
        "license": "CC0-1.0", "license_source": "https://polyhaven.com/license",
        "authors": info["authors"], "physical_dimensions_mm": dimensions_mm,
        "physical_width_cm": dimensions_mm[0] / 10,
        "physical_scale_source": INFO_URL,
        "physical_scale_unit_source": "https://api.polyhaven.com/api-docs/swagger.json",
        "millimeters_per_texel_at_2k": dimensions_mm[0] / 2048,
        "provider_info_sha256": hashlib.sha256(info_bytes).hexdigest(),
        "provider_files_sha256": hashlib.sha256(files_bytes).hexdigest(),
        "intended_use": "Candidate fine sandy gravel detail for draft launch work surface and walking trail, tiled at measured 2.53m width.",
        "limits": "Source is a published sandy-gravel PBR texture. Capture technique, site identity, engineering compaction, and suitability for the final Pacific Coast scene are not independently verified. No engine asset imported, no material applied, and no visual acceptance claimed.",
        "proposed_engine_assets": ["T_CoastGravel_BaseColor", "T_CoastGravel_Normal", "T_CoastGravel_Roughness", "M_CoastGravelDraft"],
        "sources": sources,
    }
    (output / "source.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return manifest


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    # Stage outside SourceAssets while a frozen build is in progress. Promotion is
    # a separate explicit root-agent action after the real G0–G2 review.
    parser.add_argument("--output", type=Path, default=ROOT / "Artifacts/ProductShell/GravelIntake/GravelDraft")
    args = parser.parse_args()
    result = acquire(args.output)
    print(json.dumps({"state": result["intake_state"], "maps": len(result["sources"]),
                      "bytes": sum(item["bytes"] for item in result["sources"]),
                      "width_cm": result["physical_width_cm"], "output": str(args.output)}, indent=2))
