"""Only audited Phase7 development assets may coexist with the historical C++ slice."""
import hashlib,json
from pathlib import Path
MATERIALS={'M_CoastGroundDraft','M_CoastRockDraft','M_CoastNeedlesDraft','M_CoastBarkDraft','M_CoastWaterDraft','M_CoastGravelDraft'}
TEXTURES={f'T_Coast{part}_{channel}' for part in ('Ground','Rock','Needles','Bark','Gravel','WitheredGrass','SparseGrass','LeafLitter','StonyGround','EarthTrail') for channel in ('BaseColor','Normal','Roughness')}|{'T_CoastNeedles_Alpha','T_CoastDetailDirt_Normal','T_CoastDetailDirt_Roughness','T_CoastMacroGrassRock_BaseColor'}
def validate_environment_draft(root:Path)->list[str]:
    actual={str(p.relative_to(root)) for p in (root/'Content').rglob('*') if p.is_file() and p.suffix in ('.uasset','.umap')}
    if not actual:return []
    try:
        manifest=json.loads((root/'docs/environment-draft-assets.json').read_text())
        if manifest.get('schema')!='aerosim.environment-draft.assets/v1' or manifest.get('state')!='DRAFT_NOT_USER_CONFIRMED':return ['Invalid draft content state']
        entries=manifest['assets'];expected={entry['path'] for entry in entries}
        if len(entries)!=len(expected) or actual!=expected:return ['Unmanifested or missing engine assets']
        for entry in entries:
            path=Path(entry['path']);allowed='Content/Development/CoastDraft/'+('Materials/' if path.stem in MATERIALS else 'Textures/')+path.stem+'.uasset'
            if path.stem not in MATERIALS|TEXTURES or entry['path']!=allowed or (root/path).resolve().is_relative_to((root/'Content/Development/CoastDraft').resolve()) is False:return ['Asset outside the fixed development whitelist']
            if hashlib.sha256((root/path).read_bytes()).hexdigest()!=entry['sha256']:return ['Draft asset hash mismatch: '+entry['path']]
        for source in manifest['sources']:
            path=Path(source['path'])
            if '..' in path.parts or not str(path).startswith('SourceAssets/EnvironmentDraft/') or source.get('license')!='CC0-1.0':return ['Unapproved draft source']
            if hashlib.sha256((root/path).read_bytes()).hexdigest()!=source['sha256']:return ['Draft source hash mismatch: '+str(path)]
        if not manifest['sources']:return ['Draft content requires verified source receipts']
    except (OSError,ValueError,KeyError,TypeError):return ['Missing or malformed draft content manifest']
    return []
