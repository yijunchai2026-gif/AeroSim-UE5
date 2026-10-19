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
import io
import struct
import urllib.parse
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
UA = "AeroSim-asset-research/1.0 (development CC0 attribution audit)"
CANONICAL_FOLDER = "SourceAssets/EnvironmentDraft/FloraDraft"
MAX_TEXTURE_BYTES = 12 * 1024 * 1024
MAX_MESH_BYTES = 4 * 1024 * 1024
MAX_TOTAL_BYTES = 250 * 1000 * 1000
MAX_ZIP_BYTES = 40 * 1000 * 1000
ALLOWED_PREFIXES = ("https://api.polyhaven.com/", "https://dl.polyhaven.org/", "https://ambientcg.com/api/v2/",
                    "https://ambientcg.com/get?file=", "https://acg-download.struffelproductions.com/file/ambientCG-Web/download/")
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
    ("metasequoia_bark", "2k", STD_MAPS, "R2: coast redwood proxy bark: deep vertical fibrous furrows (dawn redwood, same subfamily)", "U1"),
)
# R2 ambientCG (CC0, https://docs.ambientcg.com/license/): (asset_id, role, complaint). 2K-JPG zip; only
# Color/NormalDX/Roughness/Opacity/Displacement members are extracted, zip itself is not kept.
AMBIENTCG = (
    ("Bark014", "R2: Douglas-fir bark (ambientCG tags it fir): deep vertical furrows, 1.2 m tile, photogrammetry", "U1"),
    ("LeafSet024", "R2: small oval leaf atlas (9 leaves, 40 cm atlas) - closest CC0 proxy for coast live oak leaves", "U1/U2"),
    ("LeafSet005", "R2: small serrated leaves on a twig - twig-card source for oak/chaparral sprigs", "U1/U2"),
    ("LeafSet019", "R2: three dense conifer needle sprays (opacity) - high-coverage crown / sapling cards", "U1/U2"),
    ("PineNeedles001", "R2: loose fallen pine needles atlas with opacity - litter decal / scatter cards", "U1/U2"),
)
ACG_MAPS = ("Color", "NormalDX", "Roughness", "Opacity", "Displacement")
# (asset_id, extra 1k maps beyond the glTF bundle, role, complaint)
MODELS = (
    ("fern_02", ("Alpha", "nor_dx"), "Sword-fern-like understory fern clumps", "U2"),
    ("shrub_04", ("Alpha", "nor_dx"), "Low leafy shrub variants (understory filler)", "U2"),
    ("wild_rooibos_bush", ("Alpha", "nor_dx"), "Fine-leaved dry shrub (chaparral / coyote-brush proxy)", "U2"),
    ("dry_branches_medium_01", ("nor_dx",), "Fallen dead branches / twigs for forest floor", "U1/U2"),
)


def fetch(url: str, cap: int) -> bytes:
    if not url.startswith(ALLOWED_PREFIXES):
        raise ValueError("Unexpected provider host")
    request = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(request, timeout=120) as response:
        if not response.geturl().startswith(ALLOWED_PREFIXES):
            raise ValueError("Provider redirected to an unexpected host")
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
    acg_providers = []
    if AMBIENTCG:
        ids = ",".join(a for a, _, _ in AMBIENTCG)
        api = "https://ambientcg.com/api/v2/full_json?" + urllib.parse.urlencode({"id": ids, "include": "downloadData,dimensionsData,displayData", "limit": 50})
        api_raw = fetch(api, 2_000_000)
        found = {a["assetId"]: a for a in json.loads(api_raw)["foundAssets"]}
        for asset, role, complaint in AMBIENTCG:
            meta = found[asset]
            download = next(d for d in meta["downloadFolders"]["default"]["downloadFiletypeCategories"]["zip"]["downloads"] if d["attribute"] == "2K-JPG")
            if download["size"] > MAX_ZIP_BYTES:
                raise ValueError("ambientCG zip exceeds cap: " + asset)
            folder = output / asset
            folder.mkdir(parents=True, exist_ok=True)
            receipt_path = folder / "zip-receipt.json"
            previous = json.loads(receipt_path.read_text()) if receipt_path.exists() else {}
            members = {m: folder / (asset + "_2K-JPG_" + m + ".jpg") for m in ACG_MAPS}
            have = previous.get("zip_bytes") == download["size"] and all(
                p.exists() and hashlib.sha256(p.read_bytes()).hexdigest() == previous["members"].get(m) for m, p in members.items() if m in previous.get("members", {}))
            if not have:
                blob = fetch(download["downloadLink"], MAX_ZIP_BYTES)
                if len(blob) != download["size"]:
                    raise ValueError("ambientCG declared size mismatch: " + asset)
                archive = zipfile.ZipFile(io.BytesIO(blob))
                names = {i.filename: i for i in archive.infolist()}
                previous = {"zip_url": download["downloadLink"], "zip_bytes": len(blob), "zip_md5": hashlib.md5(blob).hexdigest(),
                            "zip_sha256": hashlib.sha256(blob).hexdigest(), "zip_members": sorted(names), "members": {}}
                for m, p in members.items():
                    name = asset + "_2K-JPG_" + m + ".jpg"
                    if name not in names:
                        continue
                    if "/" in name or names[name].file_size > MAX_TEXTURE_BYTES:
                        raise ValueError("Unexpected zip member: " + name)
                    data = archive.read(name)
                    p.with_name(p.name + ".partial").write_bytes(data)
                    p.with_name(p.name + ".partial").replace(p)
                    previous["members"][m] = hashlib.sha256(data).hexdigest()
                receipt_path.write_text(json.dumps(previous, indent=2) + "\n")
            for m, p in members.items():
                if m not in previous["members"]:
                    continue
                data = p.read_bytes()
                w, h = image_size(data)
                if max(w, h) > MAX_TEXTURE_PIXELS:
                    raise ValueError("Texture exceeds 2K: " + p.name)
                sources.append({"path": CANONICAL_FOLDER + "/" + asset + "/" + p.name, "asset": asset, "provider": "ambientCG",
                                "provider_key": m, "url": previous["zip_url"], "zip_member": p.name,
                                "zip_md5": previous["zip_md5"], "zip_sha256": previous["zip_sha256"], "zip_bytes": previous["zip_bytes"],
                                "bytes": len(data), "md5": hashlib.md5(data).hexdigest(), "sha256": hashlib.sha256(data).hexdigest(),
                                "license": "CC0-1.0", "role": role, "complaint": complaint, "pixels": [w, h],
                                "color_space": "sRGB" if m == "Color" else "linear",
                                **({"normal_convention": "DirectX tangent-space (UE native)"} if m == "NormalDX" else {})})
            acg_providers.append({"asset": asset, "provider": "ambientCG", "page": "https://ambientcg.com/view?id=" + asset,
                                  "authors": {"ambientCG (Lennart Demes)": "All"}, "license_source": "https://docs.ambientcg.com/license/",
                                  "creation_method": meta.get("creationMethod"), "physical_dimensions_cm": [meta.get("dimensionX"), meta.get("dimensionY")],
                                  "zip_url": previous["zip_url"], "zip_bytes": previous["zip_bytes"], "zip_md5": previous["zip_md5"], "zip_sha256": previous["zip_sha256"],
                                  "api_sha256": hashlib.sha256(api_raw).hexdigest(),
                                  "integrity_note": "ambientCG publishes no per-file MD5; declared zip size verified, zip and member MD5/SHA256 recorded locally"})
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
        "license": "CC0-1.0", "license_source": "https://polyhaven.com/license; https://docs.ambientcg.com/license/",
        "physical_scale_unit_source": "https://api.polyhaven.com/api-docs/swagger.json (dimensions in millimetres)",
        "providers": providers + acg_providers, "meshes": meshes,
        "total_bytes": sum(s["bytes"] for s in sources), "file_count": len(sources),
        "download_bytes": sum(s["bytes"] for s in sources if s.get("provider") != "ambientCG") + sum(p["zip_bytes"] for p in acg_providers),
        "limits": "Published CC0 scans of non-Californian species used as visual proxies (no Monterey pine, Douglas-fir or coast live oak identity claimed). Model scene dimensions cover all variants laid out side by side, not one plant. No engine import, material, placement or visual acceptance is claimed. Spec: docs/designs/flora-asset-spec.md.",
        "sources": sources,
    }
    if manifest["download_bytes"] > MAX_TOTAL_BYTES:
        raise ValueError("Flora download budget exceeded")
    previous = output / "source.json"
    for key in ("derived", "derived_r3"):
        if previous.exists() and key in json.loads(previous.read_text()):
            manifest[key] = json.loads(previous.read_text())[key]
    (output / "source.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return manifest


DERIVE_SEED = 20261018


def derive(output: Path) -> list:
    """R2 project-derived cards (deterministic; requires numpy, scipy, Pillow). Sources stay CC0."""
    import numpy as np
    from PIL import Image
    from scipy import ndimage

    def load(path, mode):
        return np.asarray(Image.open(output / path).convert(mode), dtype=np.float32) / 255.0

    def pad_colors(rgb, alpha):
        # Fill transparent texels with the nearest opaque colour so distance mips do not darken edges.
        _, (iy, ix) = ndimage.distance_transform_edt(alpha < 0.02, return_indices=True)
        return rgb[iy, ix]

    def save(name, array, mode):
        path = output / "Derived" / name
        path.parent.mkdir(parents=True, exist_ok=True)
        Image.fromarray(np.clip(array * 255.0 + 0.5, 0, 255).astype(np.uint8), mode).save(path, optimize=True)
        data = path.read_bytes()
        return {"path": CANONICAL_FOLDER + "/Derived/" + name, "bytes": len(data), "md5": hashlib.md5(data).hexdigest(),
                "sha256": hashlib.sha256(data).hexdigest(), "license": "CC0-1.0", "pixels": list(Image.open(path).size)}

    results = []
    # 1) Dense pine clump card atlas from the two real needle sprigs in pine_tree_01 twig 2K.
    prefix = "pine_tree_01/pine_tree_01_twig_"
    alpha = load(prefix + "alpha_2k.jpg", "L")
    diff, nrm, rough = load(prefix + "diff_2k.jpg", "RGB"), load(prefix + "nor_dx_2k.jpg", "RGB"), load(prefix + "rough_2k.jpg", "L")
    labels, _ = ndimage.label(alpha > 40 / 255, structure=np.ones((3, 3)))
    sprigs = []
    for (x0, x1, y0, y1) in ((72, 450, 89, 905), (1334, 1918, 700, 1061)):
        window = labels[y0:y1, x0:x1]
        ids, counts = np.unique(window[window > 0], return_counts=True)
        keep = ndimage.binary_dilation(window == ids[counts.argmax()], iterations=2)
        if not 60000 < (window == ids[counts.argmax()]).sum() < 100000:
            raise ValueError("Provider twig atlas changed; re-inspect sprig islands")
        a = alpha[y0:y1, x0:x1] * keep
        sprigs.append((a, diff[y0:y1, x0:x1], nrm[y0:y1, x0:x1], rough[y0:y1, x0:x1]))
    # Normalise orientation: stem base at bottom, tip up. Sprig 0 is vertical; sprig 1 horizontal -> rotate 90.
    def to_img(a, d, n, r):
        rgba = np.dstack([d, a])
        return [Image.fromarray((np.clip(c, 0, 1) * 255).astype(np.uint8)) for c in (rgba, n, r)]
    base = [to_img(*sprigs[0]), [im.rotate(90, expand=True) for im in to_img(*sprigs[1])]]
    base_angle = [0.0, 90.0]
    rng = np.random.default_rng(DERIVE_SEED)
    cell = 1024
    atlas_rgba = np.zeros((2048, 2048, 4), np.float32)
    atlas_n = np.zeros((2048, 2048, 3), np.float32); atlas_n[...] = (0.5, 0.5, 1.0)
    atlas_r = np.ones((2048, 2048), np.float32)
    layouts = {
        "A_radial_clump": [(i * 360 / 26 + rng.uniform(-10, 10), rng.uniform(0.50, 0.66) * (1.0 if i % 2 else 0.75), (512 + rng.uniform(-30, 30), 512 + rng.uniform(-30, 30))) for i in range(26)],
        "B_tip_fan": [(rng.uniform(-70, 70), rng.uniform(0.56, 0.74), (512 + rng.uniform(-50, 50), 1000)) for _ in range(24)],
        "C_branch_spray": [((90 if i % 2 else -90) + rng.uniform(-40, 40), rng.uniform(0.42, 0.55), (150 + i * 31, 512 + rng.uniform(-40, 40))) for i in range(24)],
        "D_sapling_whorls": [((-1) ** i * rng.uniform(30, 85), rng.uniform(0.40, 0.62) * (1.2 - i / 30), (512 + rng.uniform(-15, 15), 1000 - i * 34)) for i in range(28)],
    }
    coverage = {}
    for index, (name, placements) in enumerate(layouts.items()):
        ox, oy = (index % 2) * cell, (index // 2) * cell
        for angle, scale, (px, py) in placements:
            which = int(rng.integers(0, 2))
            rgba_im, n_im, r_im = base[which]
            size = (max(1, int(rgba_im.width * scale)), max(1, int(rgba_im.height * scale)))
            ims = [im.resize(size, Image.Resampling.LANCZOS).rotate(angle, resample=Image.Resampling.BICUBIC, expand=True) for im in (rgba_im, n_im, r_im)]
            rgba = np.asarray(ims[0], np.float32) / 255; n = np.asarray(ims[1].convert("RGB"), np.float32) / 255; r = np.asarray(ims[2].convert("L"), np.float32) / 255
            # Rotate DirectX tangent vectors by the same image-space rotation (y down).
            theta = np.radians(angle + base_angle[which])
            vx, vy = n[..., 0] * 2 - 1, n[..., 1] * 2 - 1
            n[..., 0] = (vx * np.cos(theta) + vy * np.sin(theta) + 1) / 2
            n[..., 1] = (-vx * np.sin(theta) + vy * np.cos(theta) + 1) / 2
            h, w = rgba.shape[:2]
            # Anchor the sprig's stem end (bottom-centre before rotation) at the placement point.
            ax, ay = px - w / 2 - np.sin(np.radians(angle)) * size[1] / 2, py - h / 2 - np.cos(np.radians(angle)) * size[1] / 2
            x0, y0 = int(ox + ax), int(oy + ay)
            cx0, cy0, cx1, cy1 = max(x0, ox), max(y0, oy), min(x0 + w, ox + cell), min(y0 + h, oy + cell)
            if cx1 <= cx0 or cy1 <= cy0:
                continue
            src = (slice(cy0 - y0, cy1 - y0), slice(cx0 - x0, cx1 - x0)); dst = (slice(cy0, cy1), slice(cx0, cx1))
            a = rgba[src][..., 3:4]
            atlas_rgba[dst][..., :3] = rgba[src][..., :3] * a + atlas_rgba[dst][..., :3] * (1 - a)
            atlas_rgba[dst][..., 3:4] = a + atlas_rgba[dst][..., 3:4] * (1 - a)
            atlas_n[dst] = n[src] * a + atlas_n[dst] * (1 - a)
            atlas_r[dst] = r[src] * a[..., 0] + atlas_r[dst] * (1 - a[..., 0])
        ramp = np.clip(np.minimum(np.arange(cell), np.arange(cell)[::-1]) / 32.0, 0, 1)
        atlas_rgba[oy:oy + cell, ox:ox + cell, 3] *= np.minimum.outer(ramp, ramp)  # 32px fade: no clip seams / mip bleed across cells
        block = atlas_rgba[oy:oy + cell, ox:ox + cell, 3]
        coverage[name] = round(float((block > 0.35).mean()), 3)
    a = atlas_rgba[..., 3]
    rgb = pad_colors(atlas_rgba[..., :3], a)
    nrm_out = pad_colors(atlas_n, a); rough_out = pad_colors(atlas_r[..., None], a)[..., 0]
    files = [save("T_PineClumpCards_BaseColor.png", np.dstack([rgb, a]), "RGBA"), save("T_PineClumpCards_Alpha.png", a, "L"),
             save("T_PineClumpCards_Normal.png", nrm_out, "RGB"), save("T_PineClumpCards_Roughness.png", rough_out, "L")]
    results.append({"name": "T_PineClumpCards", "sources": [CANONICAL_FOLDER + "/" + prefix + k + "_2k.jpg" for k in ("alpha", "diff", "nor_dx", "rough")],
                    "algorithm": "8-connected alpha>40 islands of the two real needle sprigs (fixed bboxes, size-checked), 2px dilation; seeded (%d) compositing of 24-28 scaled/rotated sprigs per 1024 cell, DirectX normal XY rotated with each sprig, alpha-over, 32px alpha fade at every cell border, nearest-colour padding where alpha<0.02" % DERIVE_SEED,
                    "cells": {"A_radial_clump": [0, 0, 0.5, 0.5], "B_tip_fan": [0.5, 0, 1, 0.5], "C_branch_spray": [0, 0.5, 0.5, 1], "D_sapling_whorls": [0.5, 0.5, 1, 1]},
                    "alpha_coverage_fraction_gt_0.35": coverage, "suggested_cell_world_size_cm": 80, "files": files})
    # 2) Trunk-base litter decal from forest_leaves_04 (1.5 m tile) with an irregular radial alpha.
    prefix = "forest_leaves_04/forest_leaves_04_"
    d2, n2, r2, h2 = load(prefix + "diff_2k.jpg", "RGB"), load(prefix + "nor_dx_2k.jpg", "RGB"), load(prefix + "rough_2k.jpg", "L"), load(prefix + "disp_2k.jpg", "L")
    small = lambda arr, mode: np.asarray(Image.fromarray((arr * 255).astype(np.uint8), mode).resize((1024, 1024), Image.Resampling.LANCZOS), np.float32) / 255
    d2, n2, r2, h2 = small(d2, "RGB"), small(n2, "RGB"), small(r2, "L"), small(h2, "L")
    yy, xx = np.mgrid[0:1024, 0:1024]
    radius = np.hypot(xx - 511.5, yy - 511.5) / 512
    noise = ndimage.gaussian_filter(np.random.default_rng(DERIVE_SEED + 1).random((1024, 1024)).astype(np.float32), 24)
    noise = (noise - noise.min()) / (noise.max() - noise.min())
    edge = np.clip((0.92 - radius + (noise - 0.5) * 0.35) / 0.30, 0, 1)
    decal_alpha = np.clip(edge * (0.75 + 0.6 * (h2 - h2.mean())), 0, 1)
    decal_alpha[radius >= 0.99] = 0
    files = [save("T_TrunkBaseLitter_BaseColor.png", np.dstack([d2, decal_alpha]), "RGBA"), save("T_TrunkBaseLitter_Alpha.png", decal_alpha, "L"),
             save("T_TrunkBaseLitter_Normal.png", n2, "RGB"), save("T_TrunkBaseLitter_Roughness.png", r2, "L")]
    results.append({"name": "T_TrunkBaseLitter", "sources": [CANONICAL_FOLDER + "/" + prefix + k + "_2k.jpg" for k in ("diff", "nor_dx", "rough", "disp")],
                    "algorithm": "Full 1.5 m forest_leaves_04 tile Lanczos 2048->1024; alpha = radial falloff (0.92 radius, 0.30 soft band) perturbed by seeded gaussian noise, modulated by displacement so raised needles/cones survive the edge; zero outside unit circle",
                    "world_size_cm": 150, "files": files})
    path = output / "source.json"
    manifest = json.loads(path.read_text())
    manifest["derived"] = {"state": "PROJECT_DERIVED_FROM_CC0_DRAFT", "seed": DERIVE_SEED, "items": results}
    path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return results


def derive_r3(output: Path) -> list:
    """R3 derived drafts: live-oak twig cards and hemi-octahedral sapling impostors (numpy, scipy, Pillow)."""
    import math
    import numpy as np
    from PIL import Image, ImageDraw
    from scipy import ndimage

    derived = output / "Derived"

    def arr(path, mode):
        return np.asarray(Image.open(path).convert(mode), dtype=np.float32) / 255.0

    def save(name, array, mode):
        path = derived / name
        Image.fromarray(np.clip(array * 255.0 + 0.5, 0, 255).astype(np.uint8), mode).save(path, optimize=True)
        data = path.read_bytes()
        return {"path": CANONICAL_FOLDER + "/Derived/" + name, "bytes": len(data), "md5": hashlib.md5(data).hexdigest(),
                "sha256": hashlib.sha256(data).hexdigest(), "license": "CC0-1.0", "pixels": list(Image.open(path).size)}

    def pad(rgb, alpha):
        _, (iy, ix) = ndimage.distance_transform_edt(alpha < 0.02, return_indices=True)
        return rgb[iy, ix]

    def over(dst_rgba, dst_n, dst_r, x0, y0, rgba, n, r):
        h, w = rgba.shape[:2]; H, W = dst_rgba.shape[:2]
        cx0, cy0, cx1, cy1 = max(x0, 0), max(y0, 0), min(x0 + w, W), min(y0 + h, H)
        if cx1 <= cx0 or cy1 <= cy0:
            return
        src = (slice(cy0 - y0, cy1 - y0), slice(cx0 - x0, cx1 - x0)); dst = (slice(cy0, cy1), slice(cx0, cx1))
        a = rgba[src][..., 3:4]
        dst_rgba[dst][..., :3] = rgba[src][..., :3] * a + dst_rgba[dst][..., :3] * (1 - a)
        dst_rgba[dst][..., 3:4] = a + dst_rgba[dst][..., 3:4] * (1 - a)
        dst_n[dst] = n[src] * a + dst_n[dst] * (1 - a)
        dst_r[dst] = r[src] * a[..., 0] + dst_r[dst] * (1 - a[..., 0])

    results = []
    # ---------- (1) Live-oak twig cards from the nine real LeafSet024 leaves ----------
    seed = DERIVE_SEED + 3
    rng = np.random.default_rng(seed)
    src = output / "LeafSet024"
    la, lc = arr(src / "LeafSet024_2K-JPG_Opacity.jpg", "L"), arr(src / "LeafSet024_2K-JPG_Color.jpg", "RGB")
    ln, lr = arr(src / "LeafSet024_2K-JPG_NormalDX.jpg", "RGB"), arr(src / "LeafSet024_2K-JPG_Roughness.jpg", "L")
    labels, count = ndimage.label(la > 0.5)
    leaves = []
    for index, box in enumerate(ndimage.find_objects(labels)):
        keep = ndimage.binary_dilation(labels[box] == index + 1, iterations=3)
        if keep.sum() < 100000:
            continue
        a = la[box] * keep
        # Live-oak read: darker, more saturated olive upper side; cupped (convex) leaf bent into the normal.
        dome = ndimage.gaussian_filter(ndimage.distance_transform_edt(a > 0.5), 18)
        gy, gx = np.gradient(dome)
        n = ln[box] * 2 - 1
        n[..., 0] += gx * 0.06; n[..., 1] += gy * 0.06
        n /= np.linalg.norm(n, axis=2, keepdims=True)
        # Pre-scale each leaf to 200 px length (cards use 77-140 px) to keep the compositing loop fast.
        k = 200 / a.shape[0]; size = (max(8, int(a.shape[1] * k)), 200)
        rs = lambda x, mode: np.asarray(Image.fromarray((np.clip(x, 0, 1) * 255).astype(np.uint8), mode).resize(size, Image.Resampling.LANCZOS), np.float32) / 255
        leaves.append((rs(a, "L"), rs(lc[box], "RGB"), rs((n + 1) / 2, "RGB"), rs(lr[box], "L")))
    if len(leaves) != 9:
        raise ValueError("LeafSet024 topology changed; re-inspect leaf islands")
    bark = arr(output / "tree_bark_03/tree_bark_03_diff_2k.jpg", "RGB")
    twig_rgb = np.array([0.30, 0.27, 0.23], np.float32)
    N, cell = 2048, 1024
    oak_rgba = np.zeros((N, N, 4), np.float32); oak_n = np.zeros((N, N, 3), np.float32); oak_n[...] = (0.5, 0.5, 1.0); oak_r = np.full((N, N), 0.6, np.float32)

    def leaf_sprite(length_px, angle, underside):
        a, c, n, r = leaves[int(rng.integers(0, len(leaves)))]
        scale = length_px / a.shape[0]
        size = (max(6, int(a.shape[1] * scale)), max(8, int(a.shape[0] * scale)))
        tone = np.array([0.42, 0.50, 0.30], np.float32) * rng.uniform(0.85, 1.1) if not underside else np.array([0.62, 0.66, 0.50], np.float32)
        gray = c.mean(axis=2, keepdims=True)
        col = np.clip((c * 0.35 + gray * 0.65) * tone / max(gray.mean(), 1e-3) * (0.30 if not underside else 0.40), 0, 1)
        rough = np.clip(r * 0.55 + (0.12 if not underside else 0.35), 0, 1)  # glossy cuticle above, matte felted underside
        ims = [Image.fromarray((np.dstack([col, a]) * 255).astype(np.uint8), "RGBA"), Image.fromarray((n * 255).astype(np.uint8), "RGB"), Image.fromarray((rough * 255).astype(np.uint8), "L")]
        ims = [im.resize(size, Image.Resampling.LANCZOS).rotate(angle, resample=Image.Resampling.BICUBIC, expand=True) for im in ims]
        rgba = np.asarray(ims[0], np.float32) / 255; nn = np.asarray(ims[1], np.float32) / 255; rr = np.asarray(ims[2], np.float32) / 255
        t = math.radians(angle); vx, vy = nn[..., 0] * 2 - 1, nn[..., 1] * 2 - 1
        nn = nn.copy(); nn[..., 0] = (vx * math.cos(t) + vy * math.sin(t) + 1) / 2; nn[..., 1] = (-vx * math.sin(t) + vy * math.cos(t) + 1) / 2
        return rgba, nn, rr

    layouts = ("A_terminal_cluster", "B_twig_spray", "C_dense_rosette", "D_drooping_twig")
    coverage = {}
    for ci, name in enumerate(layouts):
        ox, oy = (ci % 2) * cell, (ci // 2) * cell
        c_rgba = np.zeros((cell, cell, 4), np.float32); c_n = np.zeros((cell, cell, 3), np.float32); c_n[...] = (0.5, 0.5, 1.0); c_r = np.full((cell, cell), 0.6, np.float32)
        # Short twigs as polylines (stem base at the card bottom for A/B/D, centre for C).
        if name == "C_dense_rosette":
            twigs = [[(512, 512), (512 + math.cos(k) * 200, 512 + math.sin(k) * 200), (512 + math.cos(k + 0.15) * 400, 512 + math.sin(k + 0.15) * 400)]
                     for k in np.linspace(0, 2 * math.pi, 8, endpoint=False) + rng.uniform(-0.2, 0.2, 8)]
        else:
            droop = name == "D_drooping_twig"
            base = (512 + rng.uniform(-30, 30), 1000)
            tip = (512 + rng.uniform(-100, 100), 130) if not droop else (260 + rng.uniform(0, 60), 300)
            mid = ((base[0] + tip[0]) / 2 + rng.uniform(-60, 60) + (220 if droop else 0), (base[1] + tip[1]) / 2 - (260 if droop else 0))
            main = [base, mid, tip]
            twigs = [main]
            sides = 9 if name == "B_twig_spray" else 7
            for k in range(sides):
                t0 = 0.22 + 0.73 * k / max(sides - 1, 1)
                seg = 0 if t0 < 0.5 else 1; f = t0 * 2 - seg
                sx, sy = main[seg][0] + (main[seg + 1][0] - main[seg][0]) * f, main[seg][1] + (main[seg + 1][1] - main[seg][1]) * f
                ang = math.radians(-90 + (1 if k % 2 else -1) * rng.uniform(35, 75) + (25 if droop else 0)); L = rng.uniform(180, 320) * (1.15 - 0.4 * t0)
                sag = 90 if droop else rng.uniform(-20, 20)
                twigs.append([(sx, sy), (sx + math.cos(ang) * L * 0.55, sy + math.sin(ang) * L * 0.55 + sag * 0.25), (sx + math.cos(ang) * L, sy + math.sin(ang) * L + sag)])
        twig_layer = Image.new("L", (cell, cell), 0); draw = ImageDraw.Draw(twig_layer)
        for twig in twigs:
            for i in range(len(twig) - 1):
                draw.line([twig[i], twig[i + 1]], fill=255, width=int((14 if twig is twigs[0] else 9) - 3 * i))
        ta = np.asarray(twig_layer, np.float32) / 255
        tex = bark[:cell, :cell] * 0.5 + twig_rgb * 0.5
        c_rgba[..., :3] = tex; c_rgba[..., 3] = ta; c_r[:] = 0.8
        # Leaves along each twig: alternate, 3-6 cm (77-154 px at 40 cm per cell), denser toward tips.
        target = 0.46 if name == "D_drooping_twig" else 0.42
        attempts = 0
        while (c_rgba[..., 3] > 0.35).mean() < target and attempts < 5000:
            attempts += 1
            twig = twigs[int(rng.integers(0, len(twigs)))]
            t = rng.uniform(0.25 if twig is twigs[0] else 0.1, 1.0) ** 0.6
            seg = min(int(t * (len(twig) - 1)), len(twig) - 2); f = t * (len(twig) - 1) - seg
            px = twig[seg][0] + (twig[seg + 1][0] - twig[seg][0]) * f; py = twig[seg][1] + (twig[seg + 1][1] - twig[seg][1]) * f
            stem_ang = math.degrees(math.atan2(-(twig[seg + 1][1] - twig[seg][1]), twig[seg + 1][0] - twig[seg][0]))
            angle = stem_ang - 90 + (1 if attempts % 2 else -1) * rng.uniform(25, 70)
            length = rng.uniform(77, 140)
            rgba, nn, rr = leaf_sprite(length, angle, underside=rng.random() < 0.18)
            h, w = rgba.shape[:2]
            ra = math.radians(angle + 90)
            cx, cy = px + math.cos(ra) * length * 0.5, py - math.sin(ra) * length * 0.5
            over(c_rgba, c_n, c_r, int(cx - w / 2), int(cy - h / 2), rgba, nn, rr)
        ramp = np.clip(np.minimum(np.arange(cell), np.arange(cell)[::-1]) / 32.0, 0, 1)
        c_rgba[..., 3] *= np.minimum.outer(ramp, ramp)
        coverage[name] = round(float((c_rgba[..., 3] > 0.35).mean()), 3)
        oak_rgba[oy:oy + cell, ox:ox + cell] = c_rgba; oak_n[oy:oy + cell, ox:ox + cell] = c_n; oak_r[oy:oy + cell, ox:ox + cell] = c_r
    a = oak_rgba[..., 3]
    files = [save("T_LiveOakTwigCards_BaseColor.png", np.dstack([pad(oak_rgba[..., :3], a), a]), "RGBA"), save("T_LiveOakTwigCards_Alpha.png", a, "L"),
             save("T_LiveOakTwigCards_Normal.png", pad(oak_n, a), "RGB"), save("T_LiveOakTwigCards_Roughness.png", pad(oak_r[..., None], a)[..., 0], "L")]
    results.append({"name": "T_LiveOakTwigCards", "seed": seed,
                    "sources": [CANONICAL_FOLDER + "/LeafSet024/LeafSet024_2K-JPG_" + k + ".jpg" for k in ("Opacity", "Color", "NormalDX", "Roughness")] + [CANONICAL_FOLDER + "/tree_bark_03/tree_bark_03_diff_2k.jpg"],
                    "algorithm": "9 real LeafSet024 islands (opacity>0.5, 3px dilation); per-leaf darken/desaturate to olive (upper) or pale (18% undersides), roughness remap 0.55r+0.12 upper / +0.35 under, cupped-leaf dome normal from blurred distance field; seeded alternate placement along a main twig + 7-9 short side twigs (8 radial for C) drawn with tree_bark_03 colour until coverage>=0.42 (0.46 for D) before the border fade; 32px cell-border alpha fade; nearest-colour padding",
                    "cells": {"A_terminal_cluster": [0, 0, 0.5, 0.5], "B_twig_spray": [0.5, 0, 1, 0.5], "C_dense_rosette": [0, 0.5, 0.5, 1], "D_drooping_twig": [0.5, 0.5, 1, 1]},
                    "cell_world_size_cm": 40, "leaf_length_cm": [3, 6], "alpha_coverage_fraction_gt_0.35": coverage, "files": files})

    # ---------- (2) Hemi-octahedral sapling impostors (orthographic software render of card saplings) ----------
    def render_impostor(cards, stem, frames=4, frame_px=512):
        pts = np.array([c["center"] for c in cards] + [stem["base"], stem["top"]])
        pivot = np.array([0.0, 0.0, (pts[:, 2].max()) / 2])
        extent = 2.0 * max(np.linalg.norm(pts - pivot, axis=1).max() + max(max(c["size"]) for c in cards) / 2, 0.1)
        ppm = frame_px / extent
        out_rgba = np.zeros((frames * frame_px, frames * frame_px, 4), np.float32)
        out_n = np.zeros((frames * frame_px, frames * frame_px, 3), np.float32); out_n[...] = 0.5
        out_d = np.full((frames * frame_px, frames * frame_px), 0.5, np.float32)
        for row in range(frames):
            for col in range(frames):
                g, h = 2 * (col + 0.5) / frames - 1, 2 * (row + 0.5) / frames - 1
                ax, ay = (g + h) / 2, (g - h) / 2
                d = np.array([ax, ay, 1 - abs(ax) - abs(ay)]); d /= np.linalg.norm(d)
                right = np.cross([0, 0, 1], d); right = right / np.linalg.norm(right) if np.linalg.norm(right) > 1e-4 else np.array([1.0, 0, 0])
                up = np.cross(d, right)
                f_rgba = np.zeros((frame_px, frame_px, 4), np.float32); f_n = np.zeros((frame_px, frame_px, 3), np.float32); f_n[...] = 0.5
                f_d = np.full((frame_px, frame_px), 0.5, np.float32)
                screen = lambda P: ((P - pivot) @ right * ppm + frame_px / 2, -((P - pivot) @ up) * ppm + frame_px / 2)
                items = []
                # Stem: tapered camera-facing quad with cylindrical normals.
                items.append((float((stem["base"] + stem["top"]) / 2 @ d), "stem", None))
                for c in cards:
                    items.append((float(np.asarray(c["center"]) @ d), "card", c))
                for depth, kind, c in sorted(items, key=lambda x: x[0]):
                    if kind == "stem":
                        b, t = np.asarray(stem["base"], float), np.asarray(stem["top"], float)
                        bx, by = screen(b); tx, ty = screen(t)
                        steps = int(max(abs(ty - by), abs(tx - bx))) + 1
                        for k in range(steps):
                            u = k / max(steps - 1, 1)
                            x, y = bx + (tx - bx) * u, by + (ty - by) * u
                            rad = (stem["r0"] * (1 - u) + stem["r1"] * u) * ppm
                            x0, x1 = int(x - rad), int(x + rad) + 1; yi = int(y)
                            if 0 <= yi < frame_px and x1 > 0 and x0 < frame_px:
                                xs = np.arange(max(x0, 0), min(x1, frame_px)); s = np.clip((xs - x) / max(rad, 0.5), -1, 1)
                                nw = s[:, None] * right + np.sqrt(1 - s ** 2)[:, None] * (d - d[2] * np.array([0, 0, 1.0])) / max(np.linalg.norm(d[:2]), 1e-3)
                                nw /= np.linalg.norm(nw, axis=1, keepdims=True)
                                f_rgba[yi, xs, :3] = stem["color"] * (0.55 + 0.45 * np.sqrt(1 - s ** 2))[:, None]; f_rgba[yi, xs, 3] = 1
                                f_n[yi, xs] = nw * 0.5 + 0.5; f_d[yi, xs] = 0.5 + float((np.array([x, y]) * 0).sum()) + float(((b + (t - b) * u) - pivot) @ d) / extent
                        continue
                    U, V = np.asarray(c["U"], float), np.asarray(c["V"], float)
                    Nf = np.cross(V, U)
                    flip = 1.0 if Nf @ d >= 0 else -1.0
                    tex_rgba, tex_n = c["tex"]
                    th, tw = tex_rgba.shape[:2]
                    C = np.asarray(c["center"], float); w, hgt = c["size"]
                    # forward affine: texel (u,v) -> screen
                    p0 = screen(C - 0.5 * w * U - 0.5 * hgt * V); pu = screen(C + 0.5 * w * U - 0.5 * hgt * V); pv = screen(C - 0.5 * w * U + 0.5 * hgt * V)
                    M = np.array([[(pu[0] - p0[0]) / tw, (pv[0] - p0[0]) / th, p0[0]], [(pu[1] - p0[1]) / tw, (pv[1] - p0[1]) / th, p0[1]], [0, 0, 1]])
                    if abs(np.linalg.det(M[:2, :2])) < 1e-6:
                        continue
                    inv = np.linalg.inv(M)[:2].ravel()
                    warp = lambda im: np.asarray(im.transform((frame_px, frame_px), Image.Transform.AFFINE, tuple(inv), resample=Image.Resampling.BILINEAR), np.float32) / 255
                    rgba = warp(Image.fromarray((tex_rgba * 255).astype(np.uint8), "RGBA"))
                    tn = warp(Image.fromarray((tex_n * 255).astype(np.uint8), "RGB")) * 2 - 1
                    world = (tn[..., 0:1] * U + tn[..., 1:2] * V + tn[..., 2:3] * Nf) * flip
                    world /= np.maximum(np.linalg.norm(world, axis=2, keepdims=True), 1e-4)
                    a = rgba[..., 3:4] * (rgba[..., 3:4] > 0.35)
                    shade = 0.75 + 0.25 * flip  # back faces slightly darker (transmission not modelled)
                    f_rgba[..., :3] = rgba[..., :3] * shade * a + f_rgba[..., :3] * (1 - a)
                    f_rgba[..., 3:4] = np.maximum(f_rgba[..., 3:4], a)
                    f_n = world * 0.5 * a + 0.5 * a + f_n * (1 - a)
                    f_d = np.where(a[..., 0] > 0, 0.5 + float((C - pivot) @ d) / extent, f_d)
                ys, xs = slice(row * frame_px, (row + 1) * frame_px), slice(col * frame_px, (col + 1) * frame_px)
                out_rgba[ys, xs] = f_rgba; out_n[ys, xs] = f_n; out_d[ys, xs] = f_d
        return out_rgba, out_n, out_d, extent, pivot

    def cell_tex(rgba, n, cell_index, cells=2):
        size = rgba.shape[0] // cells; x0, y0 = (cell_index % cells) * size, (cell_index // cells) * size
        sub = lambda im: im[y0:y0 + size, x0:x0 + size]
        small = lambda im: np.asarray(Image.fromarray((sub(im) * 255).astype(np.uint8)).resize((256, 256), Image.Resampling.LANCZOS), np.float32) / 255
        return small(rgba), small(n)

    pine = (arr(derived / "T_PineClumpCards_BaseColor.png", "RGBA"), arr(derived / "T_PineClumpCards_Normal.png", "RGB"))
    oak = (oak_rgba * 0 + np.dstack([pad(oak_rgba[..., :3], oak_rgba[..., 3]), oak_rgba[..., 3]]), pad(oak_n, oak_rgba[..., 3]))
    pine_cells = [cell_tex(*pine, i) for i in range(4)]; oak_cells = [cell_tex(*oak, i) for i in range(4)]
    srng = np.random.default_rng(DERIVE_SEED + 4)

    def card(center, U, V, size, tex):
        U = np.asarray(U, float); U /= np.linalg.norm(U); V = np.asarray(V, float); V -= (V @ U) * U; V /= np.linalg.norm(V)
        return {"center": center, "U": U, "V": V, "size": size, "tex": tex}

    conifer = []
    height = 2.0
    for wi, z in enumerate(np.linspace(0.40, 1.80, 6)):
        L = 0.85 - 0.10 * wi
        for k in range(6):
            yaw = k * 2 * math.pi / 6 + wi * 0.6 + srng.uniform(-0.2, 0.2)
            out = np.array([math.cos(yaw), math.sin(yaw), 0.0]); side = np.cross([0, 0, 1], out)
            roll = srng.uniform(0.3, 0.9)
            plane_v = side * math.cos(roll) + np.array([0, 0, 1.0]) * math.sin(roll)
            # cell C is a horizontal branch spray: card U along the branch, V across it
            conifer.append(card(list(out * L / 2 + [0, 0, z - 0.05]), out + [0, 0, 0.05], -plane_v, (L, L * 0.6), pine_cells[2]))
            conifer.append(card(list(out * L * 0.35 + [0, 0, z + 0.05]), out, np.array([0, 0, -1.0]), (L * 0.7, L * 0.45), pine_cells[0]))
    for k in range(3):
        yaw = k * math.pi / 3
        conifer.append(card([0, 0, height - 0.15], [math.cos(yaw), math.sin(yaw), 0], [0, 0, -1], (0.45, 0.6), pine_cells[3]))
    pine_stem = {"base": np.array([0, 0, 0.0]), "top": np.array([0, 0, height]), "r0": 0.035, "r1": 0.008, "color": np.array([0.33, 0.24, 0.17])}

    oakcards = []
    for k in range(64):
        u, v = srng.uniform(0, 2 * math.pi), srng.uniform(-0.4, 1.57)
        rr = 0.55 * srng.uniform(0.05, 1.0) ** 0.33
        pos = np.array([math.cos(u) * math.cos(v) * rr, math.sin(u) * math.cos(v) * rr, 1.05 + math.sin(v) * 0.42 * rr / 0.55])
        out = pos - [0, 0, 0.95]; out /= np.linalg.norm(out)
        normal = out + srng.normal(0, 0.7, 3) + [0, 0, 0.4]; normal /= np.linalg.norm(normal)
        helper = np.array([0, 0, 1.0]) if abs(normal[2]) < 0.9 else np.array([1.0, 0, 0])
        U = np.cross(helper, normal); U /= np.linalg.norm(U)
        V = np.cross(normal, U)  # U x V = -normal -> front face (V x U) points along normal
        roll = srng.uniform(0, 2 * math.pi)
        U, V = U * math.cos(roll) + V * math.sin(roll), -U * math.sin(roll) + V * math.cos(roll)
        oakcards.append(card(list(pos), U, V, (0.40, 0.40), oak_cells[k % 4]))
    oak_stem = {"base": np.array([0, 0, 0.0]), "top": np.array([0.05, 0.02, 1.15]), "r0": 0.03, "r1": 0.012, "color": np.array([0.36, 0.34, 0.31])}

    for name, cards, stem, species in (("T_ImpostorConiferSapling", conifer, pine_stem, "young Monterey pine / Douglas-fir proxy, 2.0 m"),
                                       ("T_ImpostorOakSapling", oakcards, oak_stem, "young coast live oak proxy, 1.6 m")):
        rgba, nrm, depth, extent, pivot = render_impostor(cards, stem)
        a = rgba[..., 3]
        files = [save(name + "_BaseColor.png", np.dstack([pad(rgba[..., :3], a), a]), "RGBA"), save(name + "_Alpha.png", a, "L"),
                 save(name + "_NormalWS.png", pad(nrm, a), "RGB"), save(name + "_Depth.png", depth, "L")]
        results.append({"name": name, "species": species, "type": "hemi-octahedral impostor", "frames": [4, 4], "frame_px": 512,
                        "frame_world_extent_m": round(extent, 4), "pivot_m": [round(float(v), 4) for v in pivot],
                        "direction_from_frame": "g=2*(col+0.5)/4-1, h=2*(row+0.5)/4-1; a=(g+h)/2, b=(g-h)/2; dir_to_camera=normalize(a, b, 1-|a|-|b|) in X,Y,Z-up (row 0 = top of atlas)",
                        "normal_encoding": "world-space unit normal *0.5+0.5, X/Y/Z-up (linear, not tangent space)",
                        "depth_encoding": "0.5 + (point-pivot).dir_to_camera / frame_world_extent_m (per-card constant, linear)",
                        "cards": len(cards), "card_sources": "Derived/T_PineClumpCards_* or Derived/T_LiveOakTwigCards_* (cells resampled to 256 px)",
                        "files": files})
    path = output / "source.json"
    manifest = json.loads(path.read_text())
    manifest["derived_r3"] = {"state": "PROJECT_DERIVED_FROM_CC0_DRAFT", "items": results}
    path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return results


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / CANONICAL_FOLDER)
    parser.add_argument("--derive", action="store_true", help="Also build R2 derived cards/decal (numpy, scipy, Pillow)")
    args = parser.parse_args()
    result = acquire(args.output)
    if args.derive:
        for item in derive(args.output) + derive_r3(args.output):
            print(item["name"], item.get("alpha_coverage_fraction_gt_0.35", ""))
    print(json.dumps({"state": result["intake_state"], "files": result["file_count"], "bytes": result["total_bytes"],
                      "meshes": [(m["asset"], sum(x["triangles"] for x in m["meshes"]), len(m["meshes"])) for m in result["meshes"]]}, indent=2))
