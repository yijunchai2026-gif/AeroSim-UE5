#!/usr/bin/env python3
"""One local UE build lane with explicit source provenance and bounded stage logs."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Artifacts/ProductShell'


def source_fingerprint() -> str:
    digest = hashlib.sha256()
    files = [ROOT / 'AeroSim.uproject']
    files += [ROOT/'Scripts'/name for name in ('aerosim_control_mcp.py','aerosim_control_client.py') if (ROOT/'Scripts'/name).is_file()]
    draft_manifest = ROOT/'docs/environment-draft-assets.json'
    if draft_manifest.is_file(): files.append(draft_manifest)
    for folder in ('Source', 'Plugins', 'Config'):
        files += [p for p in (ROOT/folder).rglob('*') if p.is_file() and p.suffix in {'.h', '.cpp', '.cs', '.ini', '.uplugin'}
                  and 'Intermediate' not in p.parts and 'Binaries' not in p.parts]
    files += [p for p in (ROOT/'Content').rglob('*.uasset') if p.is_file()]
    files += [p for p in (ROOT/'SourceAssets/EnvironmentDraft').rglob('*') if p.is_file() and p.suffix in {'.json','.png','.jpg'}]
    for path in sorted(files):
        digest.update(str(path.relative_to(ROOT)).encode('utf-8') + b'\0')
        digest.update(path.read_bytes())
        digest.update(b'\0')
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('stage', choices=('build', 'automation', 'package'))
    parser.add_argument('--package-name', choices=('Package', 'TemporalPackage', 'CoastalPackage', 'NaturalPackage', 'ControlPackage', 'UmunhumPackage'), default='Package')
    parser.add_argument('--engine', type=Path, default=Path(os.environ['AEROSIM_UE_ROOT']) if os.environ.get('AEROSIM_UE_ROOT') else None)
    args = parser.parse_args()
    if args.engine is None:
        parser.error('provide --engine or AEROSIM_UE_ROOT for the UE 5.8.3 installation')
    engine = args.engine.resolve()
    version = json.loads((engine/'Engine/Build/Build.version').read_text())
    if (version['MajorVersion'], version['MinorVersion'], version['PatchVersion']) != (5, 8, 3):
        print('BLOCKED_ENGINE_VERSION: requires UE 5.8.3'); return 2
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT/'Logs').mkdir(exist_ok=True)
    revision = subprocess.check_output(['git','rev-parse','HEAD'], cwd=ROOT, text=True).strip()
    fingerprint = source_fingerprint()
    dirty = bool(subprocess.check_output(['git','status','--porcelain'], cwd=ROOT, text=True).strip())
    metadata = {'schema':'aerosim.build-source/v1','source_revision':revision,'source_fingerprint':fingerprint,
                'source_dirty':dirty, 'build_id':'AeroSim-Phase7-ProductShellA-20261016-v1',
                'ue_version':'5.8.3','platform':'Mac','configuration':'Development',
                'generated_utc':datetime.now(timezone.utc).isoformat()}
    stamp = OUT/'source-stamp.json'
    if args.stage == 'build': stamp.write_text(json.dumps(metadata, indent=2)+'\n')
    elif not stamp.is_file() or json.loads(stamp.read_text())['source_fingerprint'] != fingerprint:
        print('FAIL_SOURCE_DRIFT: rebuild after source changes'); return 2
    env = {k:v for k,v in os.environ.items() if not any(t in k.upper() for t in ('TOKEN','SECRET','PASSWORD','API_KEY','APIKEY','CREDENTIAL'))}
    env.update(AEROSIM_SOURCE_REVISION=revision, AEROSIM_SOURCE_FINGERPRINT=fingerprint, AEROSIM_SOURCE_DIRTY=str(int(dirty)))
    project = str(ROOT/'AeroSim.uproject')
    if args.stage == 'build':
        command = [str(engine/'Engine/Build/BatchFiles/Mac/Build.sh'),'AeroSimEditor','Mac','Development',project,'-WaitMutex','-MaxParallelActions=4']
    elif args.stage == 'automation':
        command = [str(engine/'Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor'),project,
                   '-unattended','-nop4','-nosplash','-NullRHI','-NoSound','-stdout','-FullStdOutLogOutput',
                   '-ExecCmds=Automation RunTests AeroSim;Quit','-TestExit=Automation Test Queue Empty',
                   f'-ReportExportPath={OUT / "Automation"}',f'-abslog={OUT / "Logs/automation-engine.log"}']
    else:
        command = [str(engine/'Engine/Build/BatchFiles/RunUAT.sh'),'BuildCookRun',f'-project={project}','-noP4',
                   '-platform=Mac','-clientconfig=Development','-build','-cook','-allmaps','-stage','-pak','-package',
                   '-archive','-UbtArgs=-MaxParallelActions=4',f'-archivedirectory={OUT / args.package_name}','-AdditionalCookerOptions=-SkipZenStore','-unattended']
    log = OUT/'Logs'/f'{args.stage}.log'
    print(f'Running {args.stage}: UE 5.8.3 Mac Development, source {revision[:7]}, fingerprint {fingerprint[:12]}', flush=True)
    with log.open('w') as output:
        result = subprocess.run(command, cwd=ROOT, env=env, stdout=output, stderr=subprocess.STDOUT)
    print(f'{args.stage} exit_code={result.returncode}; log={log.relative_to(ROOT)}', flush=True)
    print('\n'.join(log.read_text(errors='replace').splitlines()[-18:]))
    report = dict(metadata, stage=args.stage, exit_code=result.returncode,
                  log_sha256=hashlib.sha256(log.read_bytes()).hexdigest(), status='PASS' if result.returncode == 0 else 'FAIL')
    (OUT/f'{args.stage}-result.json').write_text(json.dumps(report, indent=2)+'\n')
    return result.returncode

if __name__ == '__main__': sys.exit(main())
