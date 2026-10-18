#!/usr/bin/env python3
"""Acquire the RD-02 v2 Pacific Coast surface set: bounded, hash-verified CC0 maps only.

Conventions follow acquire_coast_surfaces.py / acquire_coast_gravel.py: fixed asset IDs,
fixed provider hosts, identifying User-Agent, bounded downloads, publisher MD5 check
(Poly Haven), local MD5+SHA256 receipts, and provider physical scale recorded.

Scale guard: Poly Haven's API ``dimensions`` field is in MILLIMETRES (see
https://api.polyhaven.com/api-docs/swagger.json). The earlier Sandy Gravel 02 intake
misread 2529.99 as metres. Every asset below carries an expected millimetre range;
a provider value outside it aborts instead of silently writing a wrong world scale.

ambientCG publishes no per-file MD5 and no physical size for the foam set; the receipt
records the declared zip size, our own MD5/SHA256 of the zip and of every extracted
map, and ``physical_size: not published by provider``.

Engine water normals are NOT copied (engine content is not CC0). Their engine-relative
paths and SHA256 are recorded as references when --engine-dir is given.
"""
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import argparse
import hashlib
import io
import json
from pathlib import Path
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
UA = "AeroSim-asset-research/1.0 (development CC0 attribution audit)"
DRAFT = "SourceAssets/EnvironmentDraft"
POLYHAVEN_HOSTS = ("https://api.polyhaven.com/", "https://dl.polyhaven.org/")
AMBIENTCG_HOSTS = ("https://ambientcg.com/", "https://acg-download.struffelproductions.com/")
MAP_CAP = 8 * 1024 * 1024
ZIP_CAP = 40 * 1024 * 1024
ZIP_MEMBER_CAP = 16 * 1024 * 1024  # ambientCG 2K JPG normals are high-quality (~10 MB)
TOTAL_CAP = 300 * 1024 * 1024

# Poly Haven map key -> (file suffix role, colour space)
PH_MAPS = {
    "Diffuse": "sRGB base color",
    "nor_dx": "DirectX tangent-space normal, linear",
    "Rough": "linear roughness",
    "Displacement": "linear height (8-bit JPG), for height-lerp layer blending only",
}
BASE = ("Diffuse", "nor_dx", "Rough", "Displacement")

# (folder, asset_id, maps, expected mm range, blend role, complaint, intended use)
POLYHAVEN_ASSETS = (
    ("Ground", "withered_grass", BASE, (1500, 2500), "base", "U5,U6",
     "Dry golden grassland ground for sun-facing California hill slopes and launch surroundings."),
    ("Ground", "sparse_grass", BASE, (1500, 2500), "base", "U5,U6",
     "Green-brown grass/soil for shaded slopes, swales and drainage lines."),
    ("Ground", "rocky_trail_02", BASE, (1500, 2500), "base", "U5",
     "Bare compacted dirt/trail soil for trails and worn launch access."),
    ("Ground", "dry_ground_rocks", BASE, (3500, 4500), "base", "U5,U6",
     "Chaparral soil: dry brown soil with small stones beneath scrub on steep slopes."),
    ("Ground", "forest_leaves_04", BASE, (1200, 1800), "base", "U5",
     "Pine-needle forest floor under the conifer stand and forest training corridor."),
    ("Ground", "dry_decay_leaves", BASE, (1500, 2500), "base", "U5,U6",
     "Oak leaf litter for shaded oak woodland in canyons and north-facing slopes."),
    ("Ground", "dirt", ("nor_dx", "Rough"), (1500, 2500), "detail", "U5",
     "Neutral fine-grain soil normal/roughness used only as a tileable close-range detail layer."),
    ("Ground", "aerial_grass_rock", ("Diffuse", "nor_dx", "Rough"), (12000, 18000), "macro", "U5,U6",
     "Aerial grass/rock macro variation map to break base-layer tiling at mid/far distance."),
    ("Rock", "worn_rock_natural_01", BASE, (1500, 2500), "base", "U6,U3",
     "Weathered tan sandstone for steep exposed hill rock and upper coastal cliff faces."),
    ("Rock", "rock_face_03", BASE, (2200, 3200), "base", "U6",
     "Fractured tan rock face for road cuts, gully walls and steep outcrops."),
    ("Rock", "seaside_rock", BASE, (1500, 2500), "base", "U3",
     "Dark weathered seaside rock for the splash/wet zone at the cliff toe."),
    ("Rock", "low_tide_rocks", BASE, (1700, 2700), "base", "U3",
     "Tidal rocky sand/gravel for the intertidal band below the cliff."),
    ("Ground", "damp_beach_sand", BASE, (1500, 2500), "base", "U3",
     "Damp compacted beach sand for cove floors and swash zone."),
    ("Ground", "clean_pebbles", BASE, (1500, 2500), "base", "U3",
     "Rounded shingle/pebbles for cove back-beach and storm berm."),
)

AMBIENTCG_FOAM = {
    "folder": "Water", "asset_id": "Foam002", "resolution": "2K-JPG",
    "api": "https://ambientcg.com/api/v2/full_json?id=Foam002&include=downloadData,tagData",
    "members": {"Foam002_2K-JPG_Color.jpg": "sRGB foam color/mask",
                "Foam002_2K-JPG_NormalDX.jpg": "DirectX tangent-space normal, linear",
                "Foam002_2K-JPG_Roughness.jpg": "linear roughness",
                "Foam002_2K-JPG_Opacity.jpg": "linear foam coverage/opacity mask"},
}

ENGINE_REFERENCES = (
    ("Plugins/Experimental/Water/Content/Textures/Normals/T_Water_TilingNormal_Waves_02.uasset",
     "/Water/Textures/Normals/T_Water_TilingNormal_Waves_02", "mid-scale swell/chop normal"),
    ("Plugins/Experimental/Water/Content/Textures/Normals/T_Water_TilingNormal_With_Height_02_Softened.uasset",
     "/Water/Textures/Normals/T_Water_TilingNormal_With_Height_02_Softened", "large-scale soft wave normal"),
    ("Content/Functions/Engine_MaterialFunctions02/ExampleContent/Textures/water_n.uasset",
     "/Engine/Functions/Engine_MaterialFunctions02/ExampleContent/Textures/water_n", "fine ripple normal (already in v8 draft)"),
    ("Plugins/Experimental/Water/Content/Textures/Foam/T_WaterFlow_01_Foam_Tiled.uasset",
     "/Water/Textures/Foam/T_WaterFlow_01_Foam_Tiled", "engine alternative foam mask"),
    ("Plugins/Experimental/Water/Content/Textures/Foam/T_WaterFlow_01_Foam_Tiled_N.uasset",
     "/Water/Textures/Foam/T_WaterFlow_01_Foam_Tiled_N", "engine alternative foam normal"),
)


def fetch(url: str, cap: int, hosts: tuple) -> bytes:
    if not url.startswith(hosts):
        raise ValueError("Unexpected provider host: " + url)
    request = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(request, timeout=120) as response:
        if not response.geturl().startswith(hosts):
            raise ValueError("Redirected to unexpected host: " + response.geturl())
        if int(response.headers.get("Content-Length", "0")) > cap:
            raise ValueError("Declared provider response exceeds the download cap")
        data = response.read(cap + 1)
    if len(data) > cap:
        raise ValueError("Provider response exceeds the download cap")
    return data


def write_atomic(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    pending = path.with_name(path.name + ".partial")
    pending.write_bytes(data)
    pending.replace(path)


def receipt(rel: str, data: bytes, **extra) -> dict:
    return {"path": rel, "bytes": len(data), "md5": hashlib.md5(data).hexdigest(),
            "sha256": hashlib.sha256(data).hexdigest(), **extra}


def polyhaven_asset(root: Path, spec: tuple) -> dict:
    folder, asset, maps, (lo, hi), role, complaint, use = spec
    info_url, files_url = "https://api.polyhaven.com/info/" + asset, "https://api.polyhaven.com/files/" + asset
    info_bytes, files_bytes = fetch(info_url, 256_000, POLYHAVEN_HOSTS), fetch(files_url, 2_000_000, POLYHAVEN_HOSTS)
    info, files = json.loads(info_bytes), json.loads(files_bytes)
    dims_mm = info["dimensions"]
    if len(dims_mm) != 2 or not all(lo <= v <= hi for v in dims_mm):
        raise ValueError(f"{asset}: provider scale {dims_mm} mm outside expected {lo}-{hi} mm; review units")
    rel_dir = f"{DRAFT}/{folder}/{asset}"
    sources = []
    for key in maps:
        descriptor = files[key]["2k"]["jpg"]
        if descriptor["size"] > MAP_CAP:
            raise ValueError(f"{asset}: {key} exceeds per-map cap")
        name = descriptor["url"].rsplit("/", 1)[1]
        path = root / rel_dir / name
        data = path.read_bytes() if path.exists() else b""
        if len(data) != descriptor["size"] or hashlib.md5(data).hexdigest() != descriptor["md5"]:
            data = fetch(descriptor["url"], MAP_CAP, POLYHAVEN_HOSTS)
        if len(data) != descriptor["size"] or hashlib.md5(data).hexdigest() != descriptor["md5"]:
            raise ValueError(f"{asset}: provider size or MD5 mismatch for {key}")
        write_atomic(path, data)
        sources.append(receipt(f"{rel_dir}/{name}", data, publisher_md5=descriptor["md5"],
                               publisher_md5_verified=True, url=descriptor["url"], role=PH_MAPS[key],
                               source_resolution="2k", license="CC0-1.0"))
    width_mm = dims_mm[0]
    manifest = {
        "schema": "aerosim.environment-source/v1", "state": "DRAFT_NOT_USER_CONFIRMED",
        "intake_state": "STAGED_NOT_IMPORTED_OR_APPLIED", "provider": "Poly Haven",
        "asset_id": asset, "asset_page": "https://polyhaven.com/a/" + asset, "name": info.get("name"),
        "license": "CC0-1.0", "license_source": "https://polyhaven.com/license",
        "authors": info["authors"], "physical_dimensions_mm": dims_mm,
        "physical_width_m": round(width_mm / 1000, 4), "physical_width_cm": round(width_mm / 10, 2),
        "millimeters_per_texel_at_2k": round(width_mm / 2048, 4),
        "physical_scale_source": info_url,
        "physical_scale_unit_source": "https://api.polyhaven.com/api-docs/swagger.json (dimensions in millimetres)",
        "provider_info_sha256": hashlib.sha256(info_bytes).hexdigest(),
        "provider_files_sha256": hashlib.sha256(files_bytes).hexdigest(),
        "blend_role": role, "serves": complaint.split(","), "intended_use": use,
        "limits": "Published CC0 PBR scan. Site identity (not a Bay Area capture), geometry, collision and visual suitability are not verified. No engine asset imported, no material applied, no visual acceptance claimed.",
        "sources": sources,
    }
    write_atomic(root / rel_dir / "source.json", (json.dumps(manifest, indent=2) + "\n").encode())
    return manifest


def ambientcg_foam(root: Path) -> dict:
    spec = AMBIENTCG_FOAM
    api_bytes = fetch(spec["api"], 512_000, AMBIENTCG_HOSTS)
    asset = json.loads(api_bytes)["foundAssets"][0]
    if asset["assetId"] != spec["asset_id"]:
        raise ValueError("ambientCG returned an unexpected asset")
    download = next(d for d in asset["downloadFolders"]["default"]["downloadFiletypeCategories"]["zip"]["downloads"]
                    if d["attribute"] == spec["resolution"])
    if download["size"] > ZIP_CAP:
        raise ValueError("ambientCG zip exceeds the cap")
    rel_dir = f"{DRAFT}/{spec['folder']}/{spec['asset_id']}"
    out_dir = root / rel_dir
    existing = [out_dir / m for m in spec["members"]]
    old = json.loads((out_dir / "source.json").read_text()) if (out_dir / "source.json").exists() else None
    if old and all(p.exists() for p in existing) and all(
            hashlib.sha256((root / s["path"]).read_bytes()).hexdigest() == s["sha256"] for s in old["sources"]):
        return old
    blob = fetch(download["downloadLink"], ZIP_CAP, AMBIENTCG_HOSTS)
    if len(blob) != download["size"]:
        raise ValueError("ambientCG zip size differs from the provider-declared size")
    sources = []
    with zipfile.ZipFile(io.BytesIO(blob)) as archive:
        listing = archive.namelist()
        for member, role in spec["members"].items():
            if member not in listing or "/" in member or "\\" in member:
                raise ValueError("Expected zip member missing or unsafe: " + member)
            if archive.getinfo(member).file_size > ZIP_MEMBER_CAP:
                raise ValueError("Zip member exceeds per-map cap: " + member)
            data = archive.read(member)
            write_atomic(out_dir / member, data)
            sources.append(receipt(f"{rel_dir}/{member}", data, role=role, source_resolution="2k",
                                   url=download["downloadLink"], zip_member=member, publisher_md5="not published",
                                   license="CC0-1.0"))
    manifest = {
        "schema": "aerosim.environment-source/v1", "state": "DRAFT_NOT_USER_CONFIRMED",
        "intake_state": "STAGED_NOT_IMPORTED_OR_APPLIED", "provider": "ambientCG",
        "asset_id": spec["asset_id"], "asset_page": "https://ambientcg.com/view?id=" + spec["asset_id"],
        "license": "CC0-1.0", "license_source": "https://docs.ambientcg.com/license/",
        "authors": {"Lennart Demes (ambientCG)": "All"}, "provider_tags": asset.get("tags"),
        "physical_size": "not published by provider (dimensionX/Y = 0); see docs/designs/surface-asset-spec.md for the chosen world scale",
        "archive": {"url": download["downloadLink"], "declared_bytes": download["size"], "bytes": len(blob),
                    "md5": hashlib.md5(blob).hexdigest(), "sha256": hashlib.sha256(blob).hexdigest(),
                    "members": sorted(listing), "retained": list(spec["members"])},
        "provider_api_sha256": hashlib.sha256(api_bytes).hexdigest(),
        "blend_role": "detail", "serves": ["U3"],
        "intended_use": "Broken sea-foam/surf mask, normal and roughness for rock-contact and shallows foam.",
        "limits": "Provider tags include 'soap'; this is a generic foam surface, not a captured ocean surf sequence. No engine asset imported or visual acceptance claimed.",
        "sources": sources,
    }
    write_atomic(out_dir / "source.json", (json.dumps(manifest, indent=2) + "\n").encode())
    return manifest


def engine_references(engine_dir: Path | None) -> list:
    refs = []
    for rel, package, role in ENGINE_REFERENCES:
        entry = {"engine_relative_path": rel, "package_path": package, "role": role,
                 "engine": "Unreal Engine 5.8.3 (Build.version Changelist 58210709)",
                 "license": "Unreal Engine EULA engine content; NOT CC0; referenced in-engine, not copied into SourceAssets"}
        if engine_dir is not None:
            data = (engine_dir / rel).read_bytes()
            entry.update(bytes=len(data), sha256=hashlib.sha256(data).hexdigest())
        refs.append(entry)
    return refs


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--engine-dir", type=Path, default=None,
                        help="UE install Engine/ directory, to hash the referenced engine water textures")
    args = parser.parse_args()
    with ThreadPoolExecutor(max_workers=3) as pool:
        assets = list(pool.map(lambda s: polyhaven_asset(args.root, s), POLYHAVEN_ASSETS))
    assets.append(ambientcg_foam(args.root))
    total = sum(s["bytes"] for a in assets for s in a["sources"])
    if total > TOTAL_CAP:
        raise ValueError("Surface v2 set exceeds the 300 MB budget")
    index = {
        "schema": "aerosim.environment-source-index/v1", "state": "DRAFT_NOT_USER_CONFIRMED",
        "intake_state": "STAGED_NOT_IMPORTED_OR_APPLIED", "lane": "RD-02 ART-SURFACE",
        "generated_utc": datetime.now(timezone.utc).isoformat(), "total_bytes": total,
        "spec": "docs/designs/surface-asset-spec.md",
        "assets": [{"asset_id": a["asset_id"], "provider": a["provider"], "license": a["license"],
                    "authors": a["authors"], "receipt": str(Path(a["sources"][0]["path"]).parent / "source.json"),
                    "physical_width_m": a.get("physical_width_m"), "blend_role": a["blend_role"],
                    "serves": a["serves"], "bytes": sum(s["bytes"] for s in a["sources"]),
                    "sources": [{"path": s["path"], "sha256": s["sha256"], "license": s["license"]} for s in a["sources"]]}
                   for a in assets],
        "engine_references": engine_references(args.engine_dir),
    }
    write_atomic(args.root / DRAFT / "surface-v2-source-manifest.json", (json.dumps(index, indent=2) + "\n").encode())
    print(json.dumps({"assets": len(assets), "maps": sum(len(a["sources"]) for a in assets), "bytes": total}, indent=2))


if __name__ == "__main__":
    main()
