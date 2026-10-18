#!/usr/bin/env python3
"""Acquire only audited CC0 bark/needle textures; never the high-poly tree model."""
from __future__ import annotations

import argparse
from collections import deque
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import urllib.request

USER_AGENT = "AeroSim-asset-research/1.0 (development CC0 attribution audit)"
FILES_URL = "https://api.polyhaven.com/files/pine_tree_01"
INFO_URL = "https://api.polyhaven.com/info/pine_tree_01"
MAPS = ("twig_diff", "twig_alpha", "twig_nor_dx", "twig_rough", "bark_diff", "bark_nor_dx", "bark_rough")
MAX_FILE_BYTES = 16 * 1024 * 1024


def fetch(url: str, cap: int) -> bytes:
    if not url.startswith(("https://api.polyhaven.com/", "https://dl.polyhaven.org/")):
        raise ValueError("Unexpected provider host")
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(request, timeout=60) as response:
        declared = int(response.headers.get("Content-Length", "0"))
        if declared > cap:
            raise ValueError("Provider response exceeds bounded download size")
        data = response.read(cap + 1)
        if len(data) > cap:
            raise ValueError("Provider response exceeds bounded download size")
        return data


def digest(data: bytes, name: str) -> str:
    return hashlib.new(name, data).hexdigest()


def acquire(output: Path) -> dict:
    output.mkdir(parents=True, exist_ok=True)
    metadata_bytes = fetch(FILES_URL, 2 * 1024 * 1024)
    metadata = json.loads(metadata_bytes)
    info = json.loads(fetch(INFO_URL, 256 * 1024))

    def one_map(name: str) -> dict:
        descriptor = metadata[name]["1k"]["png"]
        size = descriptor["size"]
        if size > MAX_FILE_BYTES:
            raise ValueError(f"Unexpected map size: {name}")
        filename = descriptor["url"].rsplit("/", 1)[1]
        target = output / filename
        data = target.read_bytes() if target.exists() else b""
        if len(data) != size or digest(data, "md5") != descriptor["md5"]:
            data = fetch(descriptor["url"], MAX_FILE_BYTES)
        if len(data) != size or digest(data, "md5") != descriptor["md5"]:
            raise ValueError(f"Provider integrity mismatch: {name}")
        pending = target.with_suffix(".png.partial")
        pending.write_bytes(data)
        pending.replace(target)
        return {"map": name, "file": filename, "url": descriptor["url"], "bytes": len(data),
                "provider_md5": descriptor["md5"], "md5_verified": True, "sha256": digest(data, "sha256"),
                "color_space": "sRGB" if name.endswith("_diff") else "linear",
                "normal_convention": "DirectX tangent-space (-Y relative to OpenGL)" if "nor_dx" in name else None}

    with ThreadPoolExecutor(max_workers=3) as pool:
        files = list(pool.map(one_map, MAPS))
    manifest = {
        "schema": "aerosim.environment-draft.texture-source/v1",
        "status": "DRAFT_NOT_USER_CONFIRMED",
        "asset": "pine_tree_01", "provider": "Poly Haven",
        "asset_page": "https://polyhaven.com/a/pine_tree_01",
        "license": "CC0-1.0", "license_primary_source": "https://polyhaven.com/license",
        "metadata_url": FILES_URL, "info_url": INFO_URL,
        "provider_metadata_sha256": digest(metadata_bytes, "sha256"),
        "authors": info["authors"],
        "scope": "Seven unchanged provider 1k PNG texture maps only. No provider mesh acquired or imported.",
        "use": "Draft two-sided masked needle cards plus rough bark; procedural tree geometry remains project-authored.",
        "texture_requirements": {"twig_alpha": "separate grayscale opacity mask, never opaque material",
                                 "twig_nor_dx": "DirectX tangent-space normal map for UE",
                                 "roughness": "linear grayscale, not sRGB"},
        "files": files,
    }
    (output / "source-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    (output / "LICENSE-SOURCE.md").write_text(
        "# Pine Tree 01 texture draft\n\n"
        "Source: https://polyhaven.com/a/pine_tree_01\n\n"
        "Poly Haven publishes these asset textures under CC0: https://polyhaven.com/license\n\n"
        "Photography: Rob Tuytel. Modeling: Rico Cilliers. Attribution retained for provenance.\n\n"
        "Only seven original 1k PNG texture files were acquired. Provider MD5 and local SHA-256 are recorded in source-manifest.json. "
        "No Poly Haven mesh was acquired; tree geometry remains project-authored. "
        "This isolated development draft has not received user appearance confirmation or production acceptance.\n",
        encoding="utf-8")
    return manifest


def derive_card(output: Path) -> None:
    """Extract the real needle island, excluding opaque pinecones and atlas padding."""
    from PIL import Image, ImageFilter

    prefix = "pine_tree_01_twig_"
    alpha = Image.open(output / (prefix + "alpha_1k.png")).convert("L")
    pixels = alpha.load()
    seed = (134, 300)  # Pixel on the vertical twig's connected central stem.
    connected = {seed}
    queue = deque([seed])
    while queue:
        x, y = queue.popleft()
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                neighbor = (x + dx, y + dy)
                if (0 <= neighbor[0] < alpha.width and 0 <= neighbor[1] < alpha.height
                        and neighbor not in connected and pixels[neighbor] > 24):
                    connected.add(neighbor)
                    queue.append(neighbor)
    if not 20000 < len(connected) < 30000:
        raise ValueError("Provider twig topology changed; inspect the new atlas before extraction")
    selection = Image.new("L", alpha.size, 0)
    selected_pixels = selection.load()
    for point in connected:
        selected_pixels[point] = 255
    # One pixel of dilation preserves antialiased edge values from the original.
    selection = selection.filter(ImageFilter.MaxFilter(3))
    isolated = Image.new("L", alpha.size, 0)
    isolated.paste(alpha, mask=selection)
    bbox = isolated.getbbox()
    crop = (bbox[0] - 8, bbox[1] - 8, bbox[2] + 8, bbox[3] + 8)
    target_size = (256, 512)
    cropped_alpha = isolated.crop(crop).resize(target_size, Image.Resampling.LANCZOS)
    # Dilate only fully transparent RGB padding with nearest needle colors. This
    # keeps distance mips from averaging opaque foliage against black atlas space.
    nearest = Image.new("I", target_size, -1)
    nearest_pixels = nearest.load()
    mask_pixels = cropped_alpha.load()
    pending = deque()
    for y in range(target_size[1]):
        for x in range(target_size[0]):
            if mask_pixels[x,y] > 0:
                nearest_pixels[x,y] = y*target_size[0]+x
                pending.append((x,y))
    while pending:
        x,y = pending.popleft()
        for dx,dy in ((-1,0),(1,0),(0,-1),(0,1)):
            nx,ny=x+dx,y+dy
            if 0<=nx<target_size[0] and 0<=ny<target_size[1] and nearest_pixels[nx,ny]<0:
                nearest_pixels[nx,ny]=nearest_pixels[x,y]
                pending.append((nx,ny))
    derived = []
    for kind in ("diff", "nor_dx", "rough"):
        image = Image.open(output / (prefix + kind + "_1k.png")).convert("RGB").crop(crop).resize(target_size, Image.Resampling.LANCZOS)
        rgb = image.load()
        original = image.copy().load()
        for y in range(target_size[1]):
            for x in range(target_size[0]):
                if mask_pixels[x,y] == 0:
                    index = nearest_pixels[x,y]
                    rgb[x,y] = original[index%target_size[0],index//target_size[0]]
        if kind == "diff":
            image = image.convert("RGBA")
            image.putalpha(cropped_alpha)
        path = output / ("needle_" + ("normal" if kind == "nor_dx" else kind) + ".png")
        image.save(path)
        derived.append({"file": path.name, "sha256": digest(path.read_bytes(), "sha256")})
    path = output / "needle_alpha.png"
    cropped_alpha.save(path)
    derived.append({"file": path.name, "sha256": digest(path.read_bytes(), "sha256")})
    manifest_path = output / "source-manifest.json"
    manifest = json.loads(manifest_path.read_text())
    manifest["derived_card"] = {
        "algorithm": "8-connected component alpha>24 seeded at (134,300), one-pixel dilation, original alpha preserved, eight-pixel crop margin, Lanczos resize, nearest-needle color/normal padding only where alpha=0",
        "source_needle_component_pixels": len(connected), "source_crop_xyxy": crop,
        "output_size": target_size, "uv_rectangle": [0, 0, 1, 1],
        "geometry_aspect_height_over_width": 2,
        "notes": "One actual needle twig only; pinecones, stretched UV padding and all other atlas islands excluded. Derived albedo RGBA alpha is the opacity mask.",
        "files": derived,
    }
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parents[1] / "SourceAssets/EnvironmentDraft/PineTree01")
    parser.add_argument("--derive-cards", action="store_true", help="Extract one atlas needle island; requires Pillow")
    arguments = parser.parse_args()
    result = acquire(arguments.output)
    if arguments.derive_cards:
        derive_card(arguments.output)
    print(json.dumps({"status": "ACQUIRED_TEXTURE_DRAFT", "files": len(result["files"]),
                      "bytes": sum(item["bytes"] for item in result["files"]), "output": str(arguments.output)}, indent=2))
