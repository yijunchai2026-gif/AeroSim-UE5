"""Only audited Phase7 development assets may coexist with the historical C++ slice."""
import hashlib,json
from pathlib import Path
from hero_cove_scan_contract import HERO_NAME, validate_manifest_contract
MATERIALS={'M_CoastGroundDraft','M_CoastRockDraft','M_CoastNeedlesDraft','M_CoastBarkDraft','M_CoastWaterDraft','M_CoastGravelDraft','M_CoastCirrusDraft'}
TEXTURES={f'T_Coast{part}_{channel}' for part in ('Ground','Rock','Needles','Bark','Gravel','WitheredGrass','SparseGrass','LeafLitter','StonyGround','EarthTrail','WornRock','RockFace','Foam') for channel in ('BaseColor','Normal','Roughness')}|{'T_CoastNeedles_Alpha','T_CoastDetailDirt_Normal','T_CoastDetailDirt_Roughness','T_CoastMacroGrassRock_BaseColor','T_CoastWitheredGrass_Height','T_CoastSparseGrass_Height','T_CoastLeafLitter_Height','T_CoastStonyGround_Height','T_CoastFoam_Opacity'}
# DEV-FLORA habitat/tree intake (HabitatDraftImportCommandlet, fixed whitelist).
MATERIALS|={'M_CoastDryGrassDraft','M_CoastBroadShrubDraft','M_FloraFernDraft','M_FloraRooibosDraft','M_FloraPineNeedlesDraft','M_FloraFirNeedlesDraft',
            'M_FloraOakLeavesDraft','M_FloraPineBarkDraft','M_FloraFirBarkDraft','M_FloraOakBarkDraft','M_FloraTrunkLitterDraft'}
TEXTURES|={f'T_Coast{part}_{channel}' for part in ('DryGrass','BroadShrub') for channel in ('BaseColor','Alpha','Normal','Roughness')}
TEXTURES|={f'T_Flora{part}_{channel}' for part in ('PineBark','FirBark','OakBark') for channel in ('BaseColor','Normal','Roughness')}
TEXTURES|={f'T_Flora{part}_{channel}' for part in ('PineTwig','FirTwig','OakLeaf') for channel in ('BaseColor','Alpha','Normal','Roughness')}
TEXTURES|={f'T_Flora{part}_{channel}' for part in ('Fern','Rooibos') for channel in ('BaseColor','Alpha','Normal','ARM')}
TEXTURES|={f'T_FloraTrunkLitter_{channel}' for channel in ('BaseColor','Alpha','Normal','Roughness')}
MESHES={'SM_CoastGrassSmallDraft','SM_CoastGrassTallDraft','SM_CoastGrassTinyDraft','SM_CoastShrubADraft','SM_CoastShrubBDraft',
        'SM_FloraFernADraft','SM_FloraFernBDraft','SM_FloraRooibosADraft','SM_FloraRooibosBDraft'}
MESHES.add(HERO_NAME)
def _validate_wildlife_assets(root:Path, actual:set[str])->list[str]:
    prefix='Content/ThirdParty/Wildlife/'
    wildlife={path for path in actual if path.startswith(prefix)}
    if not wildlife:return []
    try:
        manifest=json.loads((root/'docs/third-party-wildlife-assets.json').read_text())
        if manifest.get('schema')!='aerosim.third-party-wildlife.assets/v1':return ['Invalid wildlife asset manifest']
        if manifest.get('license')!='CC-BY-4.0':return ['Unapproved wildlife asset license']
        entries=manifest['assets'];expected={entry['path'] for entry in entries}
        if len(entries)!=len(expected) or wildlife!=expected:return ['Unmanifested or missing wildlife assets']
        for entry in entries:
            path=Path(entry['path'])
            if path.suffix!='.uasset' or '..' in path.parts or not entry['path'].startswith(prefix):return ['Wildlife asset outside the fixed third-party path']
            if hashlib.sha256((root/path).read_bytes()).hexdigest()!=entry['sha256']:return ['Wildlife asset hash mismatch: '+entry['path']]
        sources=manifest['sources']
        if {source['model_uid'] for source in sources}!={'3adff88544344a58a8c93b10aaf05451','347e5d4fbee14e53811c5df2221be8d0','f4912be9163a4f45b2480df8ccb8b2c6','8fb150270adb45aa9653e857f3fa351b'}:return ['Wildlife source receipts are incomplete']
        if any(source.get('license')!='CC-BY-4.0' or len(source.get('archive_sha256',''))!=64 for source in sources):return ['Unapproved wildlife source receipt']
    except (OSError,ValueError,KeyError,TypeError):return ['Missing or malformed wildlife asset manifest']
    return []

def validate_environment_draft(root:Path, manifest_override=None)->list[str]:
    all_assets={str(p.relative_to(root)) for p in (root/'Content').rglob('*') if p.is_file() and p.suffix in ('.uasset','.umap')}
    wildlife_errors=_validate_wildlife_assets(root, all_assets)
    if wildlife_errors:return wildlife_errors
    actual={path for path in all_assets if not path.startswith('Content/ThirdParty/Wildlife/')}
    if not actual:return []
    try:
        manifest=manifest_override if manifest_override is not None else json.loads((root/'docs/environment-draft-assets.json').read_text())
        if manifest.get('schema')!='aerosim.environment-draft.assets/v1' or manifest.get('state')!='DRAFT_NOT_USER_CONFIRMED':return ['Invalid draft content state']
        entries=manifest['assets'];expected={entry['path'] for entry in entries}
        if len(entries)!=len(expected) or actual!=expected:return ['Unmanifested or missing engine assets']
        for entry in entries:
            path=Path(entry['path']);allowed='Content/Development/CoastDraft/'+('Materials/' if path.stem in MATERIALS else 'Meshes/' if path.stem in MESHES else 'Textures/')+path.stem+'.uasset'
            if path.stem not in MATERIALS|TEXTURES|MESHES or entry['path']!=allowed or (root/path).resolve().is_relative_to((root/'Content/Development/CoastDraft').resolve()) is False:return ['Asset outside the fixed development whitelist']
            if hashlib.sha256((root/path).read_bytes()).hexdigest()!=entry['sha256']:return ['Draft asset hash mismatch: '+entry['path']]
        for source in manifest['sources']:
            path=Path(source['path'])
            if '..' in path.parts or not str(path).startswith('SourceAssets/EnvironmentDraft/') or source.get('license')!='CC0-1.0':return ['Unapproved draft source']
            if hashlib.sha256((root/path).read_bytes()).hexdigest()!=source['sha256']:return ['Draft source hash mismatch: '+str(path)]
        if not manifest['sources']:return ['Draft content requires verified source receipts']
        validate_manifest_contract(root, manifest)
    except (OSError,ValueError,KeyError,TypeError):return ['Missing or malformed draft content manifest']
    return []
