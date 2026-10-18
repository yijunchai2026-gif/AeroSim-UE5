#!/usr/bin/env python3
"""Run bounded offscreen Metal sampling and inspect real, ordered viewport PNGs.

The external Computer Use MCP is not modified. No window activation or synthetic
input API is used. PNG readback changes frame timing; no performance claim.
Run with the workspace Python (Pillow and numpy), after packaging TemporalPackage.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def validate_sequence(directory: Path) -> list[dict]:
    from verify_product_evidence import verify_screenshot
    frames = sorted(directory.glob('frame-*.png'))
    if len(frames) != 24:
        raise ValueError('EXPECTED_24_FRAMES')
    manifests = []
    identity = None
    for index, frame in enumerate(frames):
        result = verify_screenshot(frame)
        if result.get('integrity_status') != 'PASS':
            raise ValueError('FRAME_INTEGRITY_FAILED')
        envelope = json.loads(Path(str(frame)+'.json').read_text(encoding='utf-8-sig'))
        manifest = json.loads(envelope['canonical_manifest'])
        if (manifest.get('sequence_index') != index
                or manifest.get('capture_protocol') not in ('ground-hold-move-hold/v1','coastal-cliff-overview/v1')):
            raise ValueError('SEQUENCE_ORDER_OR_PROTOCOL_FAILED')
        current = tuple(manifest.get(key) for key in (
            'source_fingerprint', 'source_revision', 'build_id', 'map_package',
            'rhi', 'quality_tier', 'viewport_width', 'viewport_height', 'weather', 'seed',
            'vertical_fov', 'camera_rotation_degrees', 'configuration', 'source_dirty'))
        if identity is not None and identity != current:
            raise ValueError('SEQUENCE_IDENTITY_DRIFT')
        identity = current
        if manifests and manifest['capture_elapsed_seconds'] <= manifests[-1]['capture_elapsed_seconds']:
            raise ValueError('NONMONOTONIC_CAPTURE_TIME')
        manifests.append(manifest)
    poses = [m['camera_position_cm'] for m in manifests]
    if len(set(poses[:8])) != 1 or len(set(poses[16:])) != 1 or len(set(poses[7:16])) != 9:
        raise ValueError('CAMERA_HOLD_MOVE_CONTRACT_FAILED')
    return manifests


def analyze(directory: Path) -> dict:
    import numpy as np
    from PIL import Image, ImageDraw
    manifests = validate_sequence(directory)
    images = [Image.open(path).convert('RGB') for path in sorted(directory.glob('frame-*.png'))]
    width, height = images[0].size
    # Fixed lower-center viewport ROI excludes perimeter HUD and most moving sky.
    roi = (int(width*.15), int(height*.55), int(width*.80), int(height*.88))
    pixels = [np.asarray(im.crop(roi), dtype=np.int16) for im in images]
    holds = {}
    for name, indices in [('initial_hold', range(1, 8)), ('final_hold', range(17, 24))]:
        changes = [np.abs(pixels[i]-pixels[i-1]).mean(axis=2) for i in indices]
        holds[name] = {
            'mean_absolute_rgb_change': float(np.mean(changes)),
            'mean_fraction_pixels_changed_over_20': float(np.mean([np.mean(v > 20) for v in changes])),
        }
    previews = []
    sheet = Image.new('RGB', (4*480, 2*320), '#17232a')
    draw = ImageDraw.Draw(sheet)
    for slot, index in enumerate(range(0, 24, 3)):
        thumb = images[index].copy(); thumb.thumbnail((480, 290))
        x, y = (slot%4)*480, (slot//4)*320
        sheet.paste(thumb, (x, y)); draw.text((x+8,y+294), f'frame {index:02d} / {manifests[index]["camera_position_cm"]}', fill='white')
    sheet.save(directory/'contact-sheet.png')
    for image in images:
        preview = image.copy(); preview.thumbnail((960,600)); previews.append(preview)
    previews[0].save(directory/'sampled-motion.gif', save_all=True, append_images=previews[1:], duration=300, loop=0)
    report = {
        'schema':'aerosim.temporal-capture-analysis/v1', 'integrity_status':'PASS',
        'map_package':manifests[0]['map_package'], 'source_fingerprint':manifests[0]['source_fingerprint'],
        'rhi':manifests[0]['rhi'], 'quality_tier':manifests[0]['quality_tier'],
        'viewport':[width,height], 'weather':manifests[0]['weather'], 'seed':manifests[0]['seed'],
        'vertical_fov':manifests[0]['vertical_fov'],
        'camera_rotations':[m['camera_rotation_degrees'] for m in manifests],
        'camera_positions':[m['camera_position_cm'] for m in manifests],
        'frames':24, 'roi_pixels':roi, 'stationary_pixel_differences':holds,
        'elapsed_seconds':[m['capture_elapsed_seconds'] for m in manifests],
        'image_sha256':[m['image_sha256'] for m in manifests],
        'visual_verdict':'REQUIRES_FRAME_INSPECTION_AND_USER_REVIEW',
        'performance_claim':'NOT_MEASURED', 'capture_overhead':'SYNCHRONOUS_PNG_READBACK',
    }
    (directory/'analysis.json').write_text(json.dumps(report,indent=2)+'\n')
    return report


def run_pair(app: Path, capture_root: Path, overview: bool = False, view: str = "overview") -> dict:
    log_root = ROOT/'Artifacts/ProductShell/TemporalLogs'
    if overview: log_root = log_root/('CoastalOverview' if view == 'overview' else 'NaturalViews/'+view)
    log_root.mkdir(parents=True, exist_ok=True)
    reports = {}
    runs = [('overview','/Engine/Maps/Entry')] if overview else [('template','/Engine/Maps/Templates/OpenWorld'), ('isolated','/Engine/Maps/Entry')]
    for label, map_name in runs:
        before = set(capture_root.glob('temporal-*'))
        with (log_root/f'{label}.log').open('w') as log:
            # The isolated run omits a map override and verifies the actual demo default.
            command = [str(app.resolve())] + ([map_name] if label == 'template' else [])
            if overview: command += ['-AeroSimCoastalOverview', '-AeroSimCoastalView='+view]
            result = subprocess.run(command + ['-RenderOffscreen','-windowed','-ResX=1440','-ResY=900',
                                     '-AeroSimTemporalCapture','-stdout','-FullStdOutLogOutput','-unattended','-nosound'],
                                    stdout=log,stderr=subprocess.STDOUT,timeout=120)
        created = set(capture_root.glob('temporal-*')) - before
        if result.returncode or len(created) != 1:
            raise ValueError('RUNTIME_CAPTURE_FAILED:'+label)
        directory = created.pop()
        reports[label] = {'directory':str(directory), **analyze(directory)}
        if 'Metal' not in reports[label]['rhi']:
            raise ValueError('METAL_REQUIRED:'+label)
        if reports[label]['map_package'] != map_name:
            raise ValueError('UNEXPECTED_MAP:'+label)
        print(label, reports[label]['stationary_pixel_differences'], flush=True)
    if not overview:
        for field in ('source_fingerprint','rhi','quality_tier','viewport','weather','seed','vertical_fov','camera_positions','camera_rotations'):
            if reports['template'][field] != reports['isolated'][field]:
                raise ValueError('COMPARISON_STATE_DRIFT:'+field)
    (log_root/'comparison.json').write_text(json.dumps(reports,indent=2)+'\n')
    return reports


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--analyze',type=Path)
    parser.add_argument('--overview',action='store_true')
    parser.add_argument('--view',choices=('overview','south','north','forest','cliff','bridge','mountain','launch-low','launch-operator','launch-return'),default='overview')
    parser.add_argument('--app',type=Path,default=ROOT/'Artifacts/ProductShell/TemporalPackage/Mac/AeroSim.app/Contents/MacOS/AeroSim')
    parser.add_argument('--capture-root',type=Path,default=Path.home()/'Library/Containers/com.YourCompany.AeroSim/Data/Library/Application Support/Epic/AeroSim/Saved/Screenshots/Product')
    args=parser.parse_args()
    try:
        if args.analyze: print(json.dumps(analyze(args.analyze),indent=2))
        else: run_pair(args.app,args.capture_root,args.overview,args.view)
    except (ValueError,OSError,subprocess.TimeoutExpired) as error:
        print('TEMPORAL_CAPTURE_FAIL',type(error).__name__,str(error)); return 1
    return 0

if __name__=='__main__': raise SystemExit(main())
