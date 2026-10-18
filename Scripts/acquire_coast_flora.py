#!/usr/bin/env python3
"""Stage fixed, publisher-hash-verified CC0 flora sources (bark, foliage cards, understory).

Only fixed Poly Haven asset IDs (discovered through api.polyhaven.com) are fetched with an
identifying User-Agent and bounded sizes. Every texture is at most 2048 px on its longest
side. No 17M-triangle tree mesh is acquired; only small understory glTF models are kept.
Nothing is imported into Unreal by this script.
"""
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import argparse
import hashlib
import json
from pathlib import Path
import struct
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
UA = "AeroSim-asset-research/1.0 (development CC0 attribution audit)"
CANONICAL_FOLDER = "SourceAssets/EnvironmentDraft/FloraDraft"
MAX_TEXTURE_BYTES = 12 * 1024 * 1024
MAX_MESH_BYTES = 4 * 1024 * 1024
MAX_TOTAL_BYTES = 250 * 1000 * 1000
MAX_TEXTURE_PIXELS = 2048
STD_MAPS = ("Diffuse", "nor_dx", "Rough", "Displacement")

# (asset_id, resolution, provider map keys, role, complaint)
TEXTURES = (
    ("pine_bark", "2k", STD_MAPS, "Monterey pine trunk bark: plated red-brown ridges, deep furrows", "U1"),
    ("bark_willow_02", "2k", STD_MAPS, "Douglas-fir proxy bark: deep vertical furrows, grey-brown corky ridges", "U1"),
    ("jolcham_oak_bark_01", "1k", STD_MAPS, "Coast live oak bark (primary): grey furrowed oak bark; 1k is 1024x2048", "U1"),
    ("tree_bark_03", "2k", STD_MAPS, "Coast live oak bark (alternate): light grey blocky ridges with lichen", "U1"),
    ("forest_leaves_04", "2k", STD_MAPS, "Pine needle and cone forest-floor litter", "U1/U2"),
    ("dry_decay_leaves", "2k", STD_MAPS, "Dry broadleaf (oak-like) leaf litter", "U1/U2"),
    ("fir_tree_01", "2k", ("twig_diff", "twig_alpha", "twig_nor_dx", "twig_rough"), "Fir needle spray card atlas (Douglas-fir proxy); texture only, no 7.8M-tri mesh", "U1/U2"),
    ("pine_tree_01", "2k", ("twig_diff", "twig_alpha", "twig_nor_dx", "twig_rough"), "Pine needle fascicle + twig card atlas, 2K upgrade of PineTree01 1K set; texture only, no 17.4M-tri mesh", "U1/U2"),
    ("island_tree_02", "2k", ("leaves_diff", "leaves_alpha", "leaves_nor_dx", "leaves_rough"), "Broadleaf oval leaf card atlas (oak canopy proxy); texture only", "U1/U2"),
    ("searsia_lucida", "2k", ("Diffuse", "Alpha", "nor_dx", "Rough"), "Leathery leaf + twig atlas (chaparral / live-oak leaf proxy); texture only", "U2"),
)
# (asset_id, extra 1k maps beyond the glTF bundle, role, complaint)
MODELS = (
    ("fern_02", ("Alpha", "nor_dx"), "Sword-fern-like understory fern clumps", "U2"),
    ("shrub_04", ("Alpha", "nor_dx"), "Low leafy shrub variants (understory filler)", "U2"),
    ("wild_rooibos_bush", ("Alpha", "nor_dx"), "Fine-leaved dry shrub (chaparral / coyote-brush proxy)", "U2"),
    ("dry_branches_medium_01", ("nor_dx",), "Fallen dead branches / twigs for forest floor", "U1/U2"),
)


def fetch(url: str, cap: int) -> bytes:
    if not url.startswith(("https://api.polyhaven.com/", "https://dl.polyhaven.org/")):
        raise ValueError("Unexpected provider host")
    request = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(request, timeout=90) as response:
        if int(response.headers.get("Content-Length", "0")) > cap:
            raise ValueError("Declared provider response exceeds the download cap")
        data = response.read(cap + 1)
    if len(data) > cap:
        raise ValueError("Provider response exceeds the download cap")
    return data


def image_size(data: bytes) -> tuple:
    if data[:8] == b"\x89PNG\r\n\x1a\n":
        return struct.unpack(">II", data[16:24])
    if data[:2] == b"\xff\xd8":
        i = 2
        while i < len(data):
            marker, length = data[i + 1], struct.unpack(">H", data[i + 2:i + 4])[0]
            if marker in (0xC0, 0xC1, 0xC2):
                h, w = struct.unpack(">HH", data[i + 5:i + 9])
                return (w, h)
            i += 2 + length
    raise ValueError("Unrecognized image header")


def mesh_stats(gltf_path: Path) -> list:
    gltf = json.loads(gltf_path.read_text())
    out = []
    for mesh in gltf["meshes"]:
        tris, lo, hi = 0, [1e9] * 3, [-1e9] * 3
        for primitive in mesh["primitives"]:
            if primitive.get("mode", 4) != 4:
                raise ValueError("Unexpected non-triangle topology")
            index = primitive.get("indices")
            count = gltf["accessors"][index]["count"] if index is not None else gltf["accessors"][primitive["attributes"]["POSITION"]]["count"]
            tris += count // 3
            pos = gltf["accessors"][primitive["attributes"]["POSITION"]]
            lo = [min(a, b) for a, b in zip(lo, pos["min"])]
            hi = [max(a, b) for a, b in zip(hi, pos["max"])]
        # glTF metres, Y-up -> report UE-style X/Y/Z(up) centimetres.
        size = [hi[0] - lo[0], hi[2] - lo[2], hi[1] - lo[1]]
        out.append({"mesh": mesh["name"], "triangles": tris, "bounds_cm_xyz_up": [round(v * 100, 1) for v in size]})
    return out


def acquire(output: Path) -> dict:
    output.mkdir(parents=True, exist_ok=True)
    providers, jobs = [], []
    for asset, *_ in TEXTURES + MODELS:
        if any(p["asset"] == asset for p in providers):
            continue
        info_raw, files_raw = fetch("https://api.polyhaven.com/info/" + asset, 256_000), fetch("https://api.polyhaven.com/files/" + asset, 4_000_000)
        info = json.loads(info_raw)
        providers.append({"asset": asset, "page": "https://polyhaven.com/a/" + asset, "authors": info["authors"],
                          "provider_type": {0: "hdri", 1: "texture", 2: "model"}[info["type"]],
                          "physical_dimensions_mm": info.get("dimensions"), "provider_polycount": info.get("polycount"),
                          "info_sha256": hashlib.sha256(info_raw).hexdigest(), "files_sha256": hashlib.sha256(files_raw).hexdigest(),
                          "_files": json.loads(files_raw)})
    lookup = {p["asset"]: p for p in providers}
    for asset, res, keys, role, complaint in TEXTURES:
        for key in keys:
            jobs.append((asset, "textures", key, lookup[asset]["_files"][key][res]["jpg"], role, complaint, MAX_TEXTURE_BYTES))
    for asset, extras, role, complaint in MODELS:
        bundle = lookup[asset]["_files"]["gltf"]["1k"]["gltf"]
        jobs.append((asset, "model", "gltf", bundle, role, complaint, MAX_MESH_BYTES))
        for relative, descriptor in bundle["include"].items():
            jobs.append((asset, "model", relative, descriptor, role, complaint, MAX_TEXTURE_BYTES))
        for key in extras:
            jobs.append((asset, "model", key, lookup[asset]["_files"][key]["1k"]["jpg"], role, complaint, MAX_TEXTURE_BYTES))
    if sum(j[3]["size"] for j in jobs) > MAX_TOTAL_BYTES:
        raise ValueError("Declared provider total exceeds the flora download budget")

    def one(job: tuple) -> dict:
        asset, kind, key, descriptor, role, complaint, cap = job
        if descriptor["size"] > cap:
            raise ValueError("Provider file exceeds cap: " + asset + "/" + key)
        if kind == "model" and key == "gltf":
            relative = asset + "/" + asset + ".gltf"
        elif kind == "model" and "/" in key or key.endswith(".bin"):
            relative = asset + "/" + key
        else:
            relative = asset + "/" + descriptor["url"].rsplit("/", 1)[1]
        path = output / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        data = path.read_bytes() if path.exists() else b""
        if len(data) != descriptor["size"] or hashlib.md5(data).hexdigest() != descriptor["md5"]:
            data = fetch(descriptor["url"], cap)
        if len(data) != descriptor["size"] or hashlib.md5(data).hexdigest() != descriptor["md5"]:
            raise ValueError("Provider size or MD5 mismatch: " + descriptor["url"])
        receipt = {"path": CANONICAL_FOLDER + "/" + relative, "asset": asset, "provider_key": key, "url": descriptor["url"],
                   "bytes": len(data), "publisher_md5": descriptor["md5"], "publisher_md5_verified": True,
                   "sha256": hashlib.sha256(data).hexdigest(), "license": "CC0-1.0", "role": role, "complaint": complaint}
        if relative.endswith((".jpg", ".png")):
            w, h = image_size(data)
            if max(w, h) > MAX_TEXTURE_PIXELS:
                raise ValueError("Texture exceeds 2K: " + relative)
            low = key.lower()
            receipt["pixels"] = [w, h]
            receipt["color_space"] = "sRGB" if ("diff" in low) else "linear"
            if "nor_dx" in low:
                receipt["normal_convention"] = "DirectX tangent-space (UE native)"
            elif "nor_gl" in low:
                receipt["normal_convention"] = "OpenGL tangent-space (glTF bundle; flip green or use nor_dx sibling in UE)"
        pending = path.with_name(path.name + ".partial")
        pending.write_bytes(data)
        pending.replace(path)
        return receipt

    with ThreadPoolExecutor(max_workers=3) as pool:
        sources = list(pool.map(one, jobs))
    meshes = []
    for asset, _, role, complaint in MODELS:
        meshes.append({"asset": asset, "role": role, "complaint": complaint,
                       "gltf": CANONICAL_FOLDER + "/" + asset + "/" + asset + ".gltf",
                       "meshes": mesh_stats(output / asset / (asset + ".gltf"))})
    for provider in providers:
        provider.pop("_files")
    manifest = {
        "schema": "aerosim.flora-source/v1", "state": "DRAFT_NOT_USER_CONFIRMED",
        "intake_state": "STAGED_NOT_IMPORTED_OR_APPLIED",
        "acquired_utc": datetime.now(timezone.utc).isoformat(),
        "license": "CC0-1.0", "license_source": "https://polyhaven.com/license",
        "physical_scale_unit_source": "https://api.polyhaven.com/api-docs/swagger.json (dimensions in millimetres)",
        "providers": providers, "meshes": meshes,
        "total_bytes": sum(s["bytes"] for s in sources), "file_count": len(sources),
        "limits": "Published CC0 scans of non-Californian species used as visual proxies (no Monterey pine, Douglas-fir or coast live oak identity claimed). Model scene dimensions cover all variants laid out side by side, not one plant. No engine import, material, placement or visual acceptance is claimed. Spec: docs/designs/flora-asset-spec.md.",
        "sources": sources,
    }
    (output / "source.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return manifest


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / CANONICAL_FOLDER)
    args = parser.parse_args()
    result = acquire(args.output)
    print(json.dumps({"state": result["intake_state"], "files": result["file_count"], "bytes": result["total_bytes"],
                      "meshes": [(m["asset"], sum(x["triangles"] for x in m["meshes"]), len(m["meshes"])) for m in result["meshes"]]}, indent=2))
