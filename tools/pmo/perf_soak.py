#!/usr/bin/env python3
"""Capture forest/cliff/glint once per minute for N minutes; record p95, render-thread CPU, GPU (diagnostic only)."""
import json, sys, time
from pathlib import Path
ROOT = Path(sys.argv[1]); OUT = Path(sys.argv[2]); MINUTES = int(sys.argv[3]) if len(sys.argv) > 3 else 10
sys.path.insert(0, str(ROOT/'Scripts'))
from aerosim_control_client import Client
c = Client(); rows = []; t0 = time.time()
try:
    while time.time() - t0 < MINUTES * 60:
        for view in ('forest', 'cliff', 'glint'):
            try:
                d = c.call('aerosim_capture_view', view=view, view_mode='lit')
                m = d['frames'][0]['manifest']
                rows.append({'t': round(time.time() - t0), 'view': view, 'p95': m.get('diagnostic_frame_p95_ms'),
                             'cpu_render': m.get('diagnostic_cpu_render_ms'), 'gpu': m.get('diagnostic_gpu_ms'),
                             'paused': m.get('world_paused')})
                print(rows[-1], flush=True)
            except RuntimeError as error:
                rows.append({'t': round(time.time() - t0), 'view': view, 'error': str(error)[:200]})
        time.sleep(max(0, 60 - (time.time() - t0) % 60))
finally:
    c.close(); OUT.mkdir(parents=True, exist_ok=True); (OUT/'soak.json').write_text(json.dumps(rows, indent=1))
