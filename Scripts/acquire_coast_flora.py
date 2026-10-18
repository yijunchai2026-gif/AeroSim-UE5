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
    if previous.exists() and "derived" in json.loads(previous.read_text()):
        manifest["derived"] = json.loads(previous.read_text())["derived"]
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


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / CANONICAL_FOLDER)
    parser.add_argument("--derive", action="store_true", help="Also build R2 derived cards/decal (numpy, scipy, Pillow)")
    args = parser.parse_args()
    result = acquire(args.output)
    if args.derive:
        for item in derive(args.output):
            print(item["name"], item.get("alpha_coverage_fraction_gt_0.35", ""))
    print(json.dumps({"state": result["intake_state"], "files": result["file_count"], "bytes": result["total_bytes"],
                      "meshes": [(m["asset"], sum(x["triangles"] for x in m["meshes"]), len(m["meshes"])) for m in result["meshes"]]}, indent=2))
