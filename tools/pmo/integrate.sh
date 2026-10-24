#!/bin/bash
# Round integration: build/test/package the integration worktree. Merging is done by PMO beforehand.
# Usage: integrate.sh <round>   e.g. integrate.sh R1
set -uo pipefail
ROUND=$1
I=${AEROSIM_INTEGRATION:-/Users/bytedance/Downloads/AeroSim-worktrees/integration}
E="/Users/Shared/Epic Games/UE_5.8"
OUT=$I/Artifacts/ProductShell/AAA/$ROUND
mkdir -p "$OUT/logs"
cd "$I"
REV=$(git rev-parse HEAD)
FP=$(python3 - "$I" <<'EOF'
import sys,importlib.util,pathlib
spec=importlib.util.spec_from_file_location('b',sys.argv[1]+'/Scripts/product_shell_build.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
m.ROOT=pathlib.Path(sys.argv[1]);print(m.source_fingerprint())
EOF
)
DIRTY=$([ -z "$(git status --porcelain)" ] && echo 0 || echo 1)
export AEROSIM_SOURCE_REVISION=$REV AEROSIM_SOURCE_FINGERPRINT=$FP AEROSIM_SOURCE_DIRTY=$DIRTY
echo "ROUND $ROUND rev ${REV:0:9} fingerprint ${FP:0:12} dirty $DIRTY" | tee "$OUT/logs/identity.txt"
python3 -m unittest discover -s Scripts/tests > "$OUT/logs/python.log" 2>&1; echo "PYTHON $(grep -E '^(Ran|OK|FAILED)' "$OUT/logs/python.log" | tr '\n' ' ')"
for v in Scripts/*static_validate.py Scripts/product_shell_validate.py; do python3 "$v" 2>&1 | tail -1; done > "$OUT/logs/static.log"; echo "STATIC $(grep -c PASS "$OUT/logs/static.log")/10 PASS"
"$E/Engine/Build/BatchFiles/Mac/Build.sh" AeroSimEditor Mac Development "$I/AeroSim.uproject" -WaitMutex -MaxParallelActions=6 > "$OUT/logs/build.log" 2>&1 || { echo "BUILD FAIL"; grep -n "error:" "$OUT/logs/build.log" | head -20; exit 1; }
echo "BUILD PASS"
"$E/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$I/AeroSim.uproject" -unattended -nop4 -nosplash -NullRHI -NoSound "-ExecCmds=Automation RunTests AeroSim;Quit" "-TestExit=Automation Test Queue Empty" "-ReportExportPath=$OUT/automation" > "$OUT/logs/automation.log" 2>&1
python3 - "$OUT/automation/index.json" <<'EOF'
import json,sys
r=json.load(open(sys.argv[1],encoding='utf-8-sig'))
print('AUTOMATION succeeded',r['succeeded'],'warnings',r['succeededWithWarnings'],'failed',r['failed'],'notRun',r['notRun'])
for t in r['tests']:
    if t['state']!='Success': print('  ',t['state'],t['fullTestPath'],[e['event']['message'][:160] for e in t['entries'] if e['event']['type']=='Error'][:2])
EOF
PKG_OK=0
for attempt in 1 2 3; do
  # UAT's multi-target UBT call ignores -WaitMutex; wait for other worktrees' builds to release the global mutex.
  while pgrep -f "UnrealBuildTool.dll" >/dev/null; do sleep 15; done
  "$E/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun "-project=$I/AeroSim.uproject" -noP4 -platform=Mac -clientconfig=Development -build -cook -allmaps -stage -pak -package -archive "-UbtArgs=-WaitMutex -MaxParallelActions=6" "-archivedirectory=$OUT/Package" -AdditionalCookerOptions=-SkipZenStore -unattended > "$OUT/logs/package.log" 2>&1 && { PKG_OK=1; break; }
  grep -q "ConflictingInstance\|conflicting instance" "$OUT/logs/package.log" || break
  echo "PACKAGE retry $attempt after UBT mutex conflict"
done
[ $PKG_OK = 1 ] && echo "PACKAGE PASS" || { echo "PACKAGE FAIL"; grep -n "Error\|error:" "$OUT/logs/package.log" | tail -15; exit 1; }
if grep -q "Failed to compile Material" "$OUT/logs/package.log"; then echo "SHADER FAIL (cooked materials fell back to default):"; grep "Failed to compile Material" "$OUT/logs/package.log" | sed 's/.*Material //' | sort -u; grep -E "Material.ush:[0-9]+:[0-9]+: error" "$OUT/logs/package.log" | sort -u | head -5; exit 1; fi
echo "SHADERS PASS"
# UAT may write AndroidFileServer settings/tokens into config; AGENTS.md requires removing them.
if grep -q "AndroidFileServer" Config/DefaultEngine.ini; then echo "WARNING: AndroidFileServer section written into Config/DefaultEngine.ini — remove before commit"; fi
