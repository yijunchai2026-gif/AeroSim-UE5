#!/bin/bash
set -euo pipefail
ROOT="/private/tmp/aime-agent-shared-dir/4e3f96ddfdaf/AeroSim-UE5"
APP="$ROOT/Artifacts/Phase4/Package/Mac/AeroSim.app/Contents/MacOS/AeroSim"
OUT="$ROOT/Artifacts/Phase4/Screenshots"
LOGS="$ROOT/Artifacts/Phase4/Logs"
WINDOW_HELPER="$ROOT/Scripts/phase4_window_id.swift"
mkdir -p "$OUT" "$LOGS"
pkill -x AeroSim >/dev/null 2>&1 || true
sleep 2

capture() {
    local view="$1"
    local file="$2"
    "$APP" -windowed -ResX=1440 -ResY=900 -AeroSimPhase4View="$view" -abslog="$LOGS/Capture-$view.log" >/dev/null 2>&1 &
    local pid=$!
    sleep 8
    local record
    if ! record="$(swift "$WINDOW_HELPER")"; then
        kill "$pid" >/dev/null 2>&1 || true
        wait "$pid" >/dev/null 2>&1 || true
        return 1
    fi
    local window_id="${record%%|*}"
    printf 'capture %s: %s\n' "$view" "$record"
    screencapture -x -l "$window_id" "$OUT/$file"
    sips -g pixelWidth -g pixelHeight "$OUT/$file" | tail -2
    shasum -a 256 "$OUT/$file"
    kill "$pid" >/dev/null 2>&1 || true
    wait "$pid" >/dev/null 2>&1 || true
    sleep 2
}

capture Hangar 01-hangar.png
capture Preflight 02-preflight.png
capture FlightHint 03-flight-hint.png
capture BoundaryRecovery 04-boundary-recovery.png
capture Result 05-result.png
capture Replay 06-replay.png

pkill -x AeroSim >/dev/null 2>&1 || true
