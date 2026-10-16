#!/bin/bash
set -euo pipefail
ROOT="/private/tmp/aime-agent-shared-dir/4e3f96ddfdaf/AeroSim-UE5"
APP="$ROOT/Artifacts/ProductUX/Mac/AeroSim.app/Contents/MacOS/AeroSim"
OUT="$ROOT/Artifacts/ProductUX/Screenshots"
LOGS="$ROOT/Artifacts/ProductUX/Logs"
WINDOW_HELPER="$ROOT/Scripts/phase4_window_id.swift"
mkdir -p "$OUT" "$LOGS"
pkill -x AeroSim >/dev/null 2>&1 || true
sleep 2

capture() {
    local view="$1"
    local review="$2"
    local file="$3"
    "$APP" -windowed -ResX=1440 -ResY=900 -AeroSimProductUxView="$view" -AeroSimReview="$review" -abslog="$LOGS/Capture-$view.log" >/dev/null 2>&1 &
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

capture Hangar Aircraft 01-hangar.png
capture Preflight Aircraft 02-preflight.png
capture DisplaySettings Aircraft 03-display-settings.png
capture Flight FPV 04-flight-hud.png

pkill -x AeroSim >/dev/null 2>&1 || true
