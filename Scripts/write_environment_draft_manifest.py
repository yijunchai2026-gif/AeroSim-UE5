#!/usr/bin/env python3
"""Freeze the fixed development material import after its successful execution."""
import hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
from environment_draft_policy import validate_environment_draft

def main():
    if sys.argv[1:2] == ['--hero-cliff-only'] and len(sys.argv) <= 3:
        from hero_cove_scan_contract import freeze_imported_asset
        changed=freeze_imported_asset(ROOT, sys.argv[2] if len(sys.argv) == 3 else None)
        print('Hero scan manifest appended' if changed else 'Hero scan already registered; no changes')
        return
    if sys.argv[1:]:raise ValueError('Only --hero-cliff-only <sha256> is supported')
    assets=[{'path':str(p.relative_to(ROOT)),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'bytes':p.stat().st_size} for p in sorted((ROOT/'Content/Development/CoastDraft').rglob('*.uasset'))]
    # CoastDraftImport: 34 textures + 7 materials (incl. DEV-SEASKY cirrus); HabitatDraftImport: 41 textures + 11 materials + 9 meshes.
    # Plus, once imported and attested, the single R5 hero-cove scan mesh (fixed intake contract).
    from hero_cove_scan_contract import HERO_ASSET, source_entries, source_contract
    hero=(ROOT/HERO_ASSET).is_file()
    if len(assets)!=116+hero:raise ValueError('Expected exactly 89 textures, 18 materials and 9 meshes (+1 hero scan)')
    sources=[{'path':str(p.relative_to(ROOT)),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'license':'CC0-1.0'} for p in sorted((ROOT/'SourceAssets/EnvironmentDraft').rglob('*'))
             if p.is_file() and 'HeroCoveScan' not in p.relative_to(ROOT/'SourceAssets/EnvironmentDraft').parts and (p.suffix in ('.png','.jpg','.gltf','.bin') or p.name.endswith('.mesh.json'))]
    manifest={'schema':'aerosim.environment-draft.assets/v1','state':'DRAFT_NOT_USER_CONFIRMED','version':'coast-draft-20261018-r1','source_pages':['https://polyhaven.com/a/pine_tree_01','https://polyhaven.com/a/rocky_terrain_02','https://polyhaven.com/a/cliff_side','https://polyhaven.com/a/sandy_gravel_02',
        'https://polyhaven.com/a/withered_grass','https://polyhaven.com/a/sparse_grass','https://polyhaven.com/a/dry_decay_leaves','https://polyhaven.com/a/dry_ground_rocks','https://polyhaven.com/a/dirt','https://polyhaven.com/a/aerial_grass_rock','https://polyhaven.com/a/rocky_trail_02',
        'https://polyhaven.com/a/grass_medium_01','https://polyhaven.com/a/shrub_03','https://polyhaven.com/a/pine_bark','https://polyhaven.com/a/bark_willow_02',
        'https://polyhaven.com/a/jolcham_oak_bark_01','https://polyhaven.com/a/fir_tree_01','https://polyhaven.com/a/island_tree_02','https://polyhaven.com/a/fern_02',
        'https://polyhaven.com/a/wild_rooibos_bush','https://polyhaven.com/a/worn_rock_natural_01','https://polyhaven.com/a/rock_face_03','https://ambientcg.com/view?id=Foam002'],'license':'CC0-1.0','license_source':'https://polyhaven.com/license','importer':'CoastDraftImportCommandlet + HabitatDraftImportCommandlet (fixed whitelists, Editor only)','assets':assets,'sources':sources,'formal_promotion':False,'user_visual_acceptance':'pending_user'}
    if hero:
        manifest['sources'].extend(source_entries());manifest['hero_cove_scan']=source_contract()
        manifest['source_pages'].append('https://polyhaven.com/a/coastal_cliff_01')
    out=ROOT/'docs/environment-draft-assets.json';out.write_text(json.dumps(manifest,indent=2)+'\n')
    errors=validate_environment_draft(ROOT)
    if errors:raise ValueError(errors)
    print('Verified content manifest:',len(assets),'engine assets;',len(sources),'source/derived textures; state DRAFT_NOT_USER_CONFIRMED')
if __name__=='__main__':main()
