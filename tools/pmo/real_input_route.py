#!/usr/bin/env python3
"""Closed-loop REAL-INPUT route smoke over the MCP bridge: only aerosim_ui / aerosim_press_key, no teleports.

Reads the live guide point from aerosim_inspect and pulses W/S (X), D/A (Y), LeftShift/LeftControl (Z),
one key per call, never Q/E (heading stays +X). Logs every step; captures player frames periodically.
"""
import json, sys, time
from pathlib import Path

ROOT = Path(sys.argv[1]); OUT = Path(sys.argv[2]); OUT.mkdir(parents=True, exist_ok=True)
sys.path.insert(0, str(ROOT/'Scripts'))
from aerosim_control_client import Client

log = []
c = Client()

def state():
    return c.call('aerosim_inspect')['state']

def vec(s, p):
    return [s.get(f'{p}_x', 0.0), s.get(f'{p}_y', 0.0), s.get(f'{p}_z', 0.0)]

def press(key, ms):
    ms = int(max(50, min(2000, ms)))
    try:
        c.call('aerosim_press_key', key=key, duration_ms=ms)
    except RuntimeError as error:
        log.append({'t': time.time() - t0, 'press_error': str(error)[:300]})

t0 = time.time()
s = state(); log.append({'t': 0, 'start_page': s['page']})
# Normal product flow to Flight via the same commands as native UI.
for _ in range(8):
    if s['page'] == 'Flight': break
    try: c.call('aerosim_ui', action='primary')
    except RuntimeError as error: log.append({'ui_error': str(error)[:300]})
    time.sleep(0.6); s = state()
log.append({'t': time.time() - t0, 'page': s['page']})
if s['page'] != 'Flight':
    (OUT/'route-log.json').write_text(json.dumps(log, indent=1)); sys.exit('did not reach Flight')

import atexit
atexit.register(lambda: (OUT/'route-log.json').write_text(json.dumps(log, indent=1)))
stuck_hist = []; enters = 0
flight_start = time.time(); last_pos = None; last_t = None; frames = 0; next_capture = 0
while time.time() - flight_start < 600:
    s = state(); now = time.time() - flight_start
    pos = vec(s, 'actor_cm'); leg0 = (s.get('guide_leg_id') or '').lower()
    vertical0 = any(k in leg0 for k in ('takeoff', 'descent', 'climb', 'land'))
    goal = vec(s, 'guide_point_cm') if (vertical0 or s.get('guide_target_cm_x') is None) else vec(s, 'guide_target_cm')
    vel = [0, 0, 0]
    if last_pos is not None and now > last_t:
        vel = [(a - b) / (now - last_t) for a, b in zip(pos, last_pos)]
    last_pos, last_t = pos, now
    entry = {'t': round(now, 2), 'page': s['page'], 'leg': s.get('guide_leg_id'), 'leg_index': s.get('guide_leg_index'),
             'recovery': s.get('guide_recovery'), 'pos': [round(v) for v in pos], 'guide': [round(v) for v in goal],
             'vel': [round(v) for v in vel], 'phase': s.get('phase'), 'paused': s.get('world_paused'),
             'resume_pending': s.get('resume_pending')}
    log.append(entry)
    if s['page'] != 'Flight':
        break
    if s.get('world_paused') or s.get('phase') == 4:
        # Documented tester procedure for focus-loss safe pause: release, confirm neutral, Enter, let the countdown finish.
        entry['safe_pause'] = {'app_active': s.get('application_active'), 'neutral': s.get('physical_neutral'),
                               'resume_pending': s.get('resume_pending'), 'resume_seconds': s.get('resume_seconds')}
        try: c.call('aerosim_release')
        except RuntimeError: pass
        if not s.get('resume_pending'):
            enters += 1
            if enters > 3:
                entry['abort'] = 'repeated pause; not navigating menus blind'; break
            press('Enter', 100)
        time.sleep(1.0); continue
    if now >= next_capture:
        try:
            cap = c.call('aerosim_capture')
            entry['capture'] = cap.get('frames', [{}])[0].get('evidence_png'); frames += 1
        except RuntimeError as error:
            entry['capture_error'] = str(error)[:200]
        next_capture = now + 15
    d = [g - p for g, p in zip(goal, pos)]
    leg = (entry['leg'] or '').lower()
    vertical_leg = any(k in leg for k in ('vertical', 'descent', 'climb', 'land', 'takeoff', 'column'))
    dist = sum(v * v for v in d) ** 0.5; spd = sum(v * v for v in vel) ** 0.5
    # Damping: close to the goal, let the model's neutral brake stop us before any new pulse.
    if dist < 500 and spd > 90:
        time.sleep(0.45); continue
    if dist < 60 and spd < 90:
        time.sleep(0.3); continue
    weights = [abs(d[0]), abs(d[1]), abs(d[2]) * (1.6 if vertical_leg else 1.0)]
    planar = (d[0] ** 2 + d[1] ** 2) ** 0.5
    landing = 'land' in leg
    planar_speed = (vel[0] ** 2 + vel[1] ** 2) ** 0.5
    if vertical_leg and (max(abs(d[0]), abs(d[1])) > (30 if landing else 120) or (landing and planar_speed > 20)):
        weights[2] = 0
    # Grounded off the pad (90 cm radius): a pilot lifts off and re-centres; the grounded drone does not slide.
    if landing and planar > 45 and abs(d[2]) < 30 and spd < 20:
        entry['key'] = ['LeftShift', 250]; press('LeftShift', 250); time.sleep(0.5); continue
    axis = max(range(3), key=lambda i: weights[i])
    err = d[axis]; speed = abs(vel[axis])
    stopping = speed * speed / (2 * (700 if axis == 2 else 900))
    if vel[axis] * err > 0 and stopping > abs(err) * 0.7:
        time.sleep(0.35); continue
    key = [('W', 'S'), ('D', 'A'), ('LeftShift', 'LeftControl')][axis][0 if err > 0 else 1]
    if abs(err) > 3000: ms = 2000
    elif abs(err) > 1200: ms = 600
    elif abs(err) > 400: ms = 200
    else: ms = 60
    if axis == 2 and abs(err) < 600: ms = min(ms, 90)
    # Blind-driver obstacle escape: if several pressed steps produced no motion, back off and sidestep (as a pilot would).
    stuck_hist.append(tuple(round(v / 20) for v in pos))
    if len(stuck_hist) > 6 and len(set(stuck_hist[-6:])) == 1:
        back = {'W': 'S', 'S': 'W', 'D': 'A', 'A': 'D', 'LeftShift': 'LeftControl', 'LeftControl': 'LeftShift'}[key]
        side = ['D', 'A', 'LeftShift'][len(stuck_hist) % 3] if axis != 1 else ['W', 'S', 'LeftShift'][len(stuck_hist) % 3]
        entry['escape'] = [back, side]
        press(back, 450); time.sleep(0.4); press(side, 350); time.sleep(0.4)
        stuck_hist.clear(); continue
    entry['key'] = [key, ms]
    press(key, ms)

final = state()
log.append({'t': round(time.time() - flight_start, 2), 'final_page': final['page'], 'final_leg': final.get('guide_leg_id')})
c.close()
(OUT/'route-log.json').write_text(json.dumps(log, indent=1))
print('final page', final['page'], 'leg', final.get('guide_leg_id'), 'elapsed', round(time.time() - flight_start, 1), 'frames', frames)
