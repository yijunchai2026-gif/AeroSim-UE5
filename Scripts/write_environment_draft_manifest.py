#!/usr/bin/env python3
"""Freeze the fixed development material import after its successful execution."""
import hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
from environment_draft_policy import validate_environment_draft

def main():
    assets=[{'path':str(p.relative_to(ROOT)),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'bytes':p.stat().st_size} for p in sorted((ROOT/'Content/Development/CoastDraft').rglob('*.uasset'))]
    if len(assets)!=22:raise ValueError('Expected exactly16 textures and6 materials')
    sources=[{'path':str(p.relative_to(ROOT)),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'license':'CC0-1.0'} for p in sorted((ROOT/'SourceAssets/EnvironmentDraft').rglob('*')) if p.is_file() and p.suffix in ('.png','.jpg')]
    manifest={'schema':'aerosim.environment-draft.assets/v1','state':'DRAFT_NOT_USER_CONFIRMED','version':'coast-draft-20261017-v7','source_pages':['https://polyhaven.com/a/pine_tree_01','https://polyhaven.com/a/rocky_terrain_02','https://polyhaven.com/a/cliff_side','https://polyhaven.com/a/sandy_gravel_02'],'license':'CC0-1.0','license_source':'https://polyhaven.com/license','importer':'CoastDraftImportCommandlet (fixed whitelist, Editor only)','assets':assets,'sources':sources,'formal_promotion':False,'user_visual_acceptance':'pending_user'}
    out=ROOT/'docs/environment-draft-assets.json';out.write_text(json.dumps(manifest,indent=2)+'\n')
    errors=validate_environment_draft(ROOT)
    if errors:raise ValueError(errors)
    print('Verified content manifest:',len(assets),'engine assets;',len(sources),'source/derived textures; state DRAFT_NOT_USER_CONFIRMED')
if __name__=='__main__':main()
