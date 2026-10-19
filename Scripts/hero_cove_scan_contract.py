"""Fixed R5 CC0 hero-cove scan intake. No general glTF/import path support."""
import hashlib
import json
import math
import struct
from pathlib import Path

HERO_NAME = 'SM_CoastHeroCliffScanDraft'
HERO_ASSET = 'Content/Development/CoastDraft/Meshes/' + HERO_NAME + '.uasset'
HERO_SOURCE = 'SourceAssets/EnvironmentDraft/HeroCoveScan/'
RECEIPT = HERO_SOURCE + 'source-receipt.json'
RECEIPT_SHA256 = '27cb4ab652d29d40e2689d55468599708e3e80a9d6016600b4c15cc34b0ae571'
LOD_COUNTS = ((27145, 49995), (7435, 11997), (2918, 3591))
FILES = (('coastal-cliff-40m-lod0.gltf', 1755, '099cae75b2f82bd5d10ffe86922705e662bb8cf4fc072bf5497f929185424812'), ('coastal-cliff-40m-lod0.bin', 1468580, '56c1f67209c0d53e27a0bb744ee8b764201715256f03184c63de6fcb602a8323'), ('coastal-cliff-40m-lod1.gltf', 1746, '2d8d903ec1e6a6d5cb2a2086b6a12d48acf54ee3dacc91b7e45e824cc609c590'), ('coastal-cliff-40m-lod1.bin', 381884, '6ee6034e8e2c2bd52a24b72248139d31e0813fa6cfae09dbbbbc29dd6746246b'), ('coastal-cliff-40m-lod2.gltf', 1743, 'ab72b18f5a233c5fa6febb4cfdc9e8dba4aae61665036334dfc50360816be3da'), ('coastal-cliff-40m-lod2.bin', 136468, '73619882ce484cdad80544e6166b999edbe95fa7039936f937d3660f580218fd'))


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def parse_lod(document, blob, lod):
    """Validate this fixed, tightly packed triangle format, independently of hash checks."""
    vertices, triangles = LOD_COUNTS[lod]
    if document.get('asset', {}).get('version') != '2.0':
        raise ValueError('Hero glTF version')
    if set(document) != {'asset', 'scene', 'scenes', 'nodes', 'meshes', 'buffers', 'bufferViews', 'accessors'}:
        raise ValueError('Hero glTF external dependencies/extensions are forbidden')
    if document['scene'] != 0 or document['scenes'] != [{'nodes': [0]}]:
        raise ValueError('Hero scene')
    if document['nodes'] != [{'mesh': 0, 'name': f'coastal-cliff-40m-lod{lod}'}]:
        raise ValueError('Hero node transforms are forbidden')
    if document['meshes'] != [{'primitives': [{'attributes': {'POSITION': 0, 'NORMAL': 1, 'TEXCOORD_0': 2}, 'indices': 3, 'mode': 4}]}]:
        raise ValueError('Hero primitive/attributes')
    if document['buffers'] != [{'uri': f'coastal-cliff-40m-lod{lod}.bin', 'byteLength': len(blob)}]:
        raise ValueError('Hero buffer URI/length')
    views, accessors = document['bufferViews'], document['accessors']
    if len(views) != 4 or len(accessors) != 4:
        raise ValueError('Hero view/accessor count')
    offset = 0
    for i, (count, components, kind, component_type) in enumerate(((vertices, 3, 'VEC3', 5126), (vertices, 3, 'VEC3', 5126), (vertices, 2, 'VEC2', 5126), (triangles * 3, 1, 'SCALAR', 5125))):
        size = count * components * 4
        if views[i] != {'buffer': 0, 'byteOffset': offset, 'byteLength': size, 'target': 34963 if i == 3 else 34962}:
            raise ValueError('Hero buffer offset/stride/range')
        expected = {'bufferView': i, 'componentType': component_type, 'count': count, 'type': kind}
        if {k: v for k, v in accessors[i].items() if k not in ('min', 'max')} != expected:
            raise ValueError('Hero accessor layout/sparse/stride')
        offset += size
    if offset != len(blob):
        raise ValueError('Hero buffer bounds')
    p_offset, n_offset, uv_offset, index_offset = (view['byteOffset'] for view in views)
    points = list(struct.iter_unpack('<3f', blob[p_offset:n_offset]))
    normals = list(struct.iter_unpack('<3f', blob[n_offset:uv_offset]))
    uv = list(struct.iter_unpack('<2f', blob[uv_offset:index_offset]))
    indices = struct.unpack('<' + str(triangles * 3) + 'I', blob[index_offset:])
    if any(not all(math.isfinite(x) for x in point) or max(map(abs, point)) > 50 for point in points):
        raise ValueError('Hero nonfinite/out-of-range position')
    if any(not all(math.isfinite(x) for x in normal) or not .99 < sum(x*x for x in normal) < 1.01 for normal in normals):
        raise ValueError('Hero invalid normal')
    if any(not all(math.isfinite(x) and 0 <= x <= 1 for x in value) for value in uv):
        raise ValueError('Hero invalid UV')
    if max(indices) >= vertices:
        raise ValueError('Hero index bounds')
    # Validate every actual face and UV area; axis permutation preserves areas.
    for i in range(0, len(indices), 3):
        a, b, c = (points[j] for j in indices[i:i+3])
        ab = tuple(b[j]-a[j] for j in range(3)); ac = tuple(c[j]-a[j] for j in range(3))
        cross = (ab[1]*ac[2]-ab[2]*ac[1], ab[2]*ac[0]-ab[0]*ac[2], ab[0]*ac[1]-ab[1]*ac[0])
        if sum(x*x for x in cross) <= 1e-24:
            raise ValueError('Hero zero-area triangle')
        a, b, c = (uv[j] for j in indices[i:i+3])
        if abs((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])) <= 2e-14:
            raise ValueError('Hero zero-area UV triangle')
    minimum = [min(p[axis] for p in points) for axis in range(3)]
    maximum = [max(p[axis] for p in points) for axis in range(3)]
    if not abs(maximum[0]-minimum[0]-40) < .001 or not 9.8 < maximum[1]-minimum[1] < 10.2:
        raise ValueError('Hero physical dimensions')
    if accessors[0].get('min') != minimum or accessors[0].get('max') != maximum:
        raise ValueError('Hero declared bounds')
    return {'vertices': vertices, 'triangles': triangles, 'ue_bounds_min_cm': [100*minimum[2], 100*minimum[0], 100*minimum[1]], 'ue_bounds_max_cm': [100*maximum[2], 100*maximum[0], 100*maximum[1]]}


def validate_sources(root):
    directory = root / HERO_SOURCE
    expected = {name for name, _, _ in FILES} | {'source-receipt.json'}
    parent = root
    for part in Path(HERO_SOURCE).parts:
        parent = parent / part
        if parent.is_symlink(): raise ValueError('Hero source path contains a symlink')
    if directory.is_symlink() or {p.name for p in directory.iterdir()} != expected:
        raise ValueError('Hero source directory is not the fixed seven-file intake')
    for name, size, sha in FILES:
        path = directory / name
        if path.is_symlink() or not path.resolve().is_relative_to(directory.resolve()) or path.stat().st_size != size or digest(path) != sha:
            raise ValueError('Hero fixed source hash/size mismatch: ' + name)
    if (root/RECEIPT).is_symlink() or digest(root / RECEIPT) != RECEIPT_SHA256:
        raise ValueError('Hero receipt hash mismatch')
    receipt = json.loads((root / RECEIPT).read_text())
    if receipt['license'] != 'CC0-1.0' or receipt['asset'] != HERO_NAME:
        raise ValueError('Hero source identity/license')
    return [parse_lod(json.loads((directory/f'coastal-cliff-40m-lod{lod}.gltf').read_text()), (directory/f'coastal-cliff-40m-lod{lod}.bin').read_bytes(), lod) for lod in range(3)]


def source_entries():
    return [{'path': HERO_SOURCE+name, 'sha256': sha, 'license': 'CC0-1.0'} for name, _, sha in FILES]


def source_contract():
    return {'schema': 'aerosim.hero-cove-scan-intake/v1', 'asset': HERO_ASSET, 'source_receipt': RECEIPT, 'source_receipt_sha256': RECEIPT_SHA256,
            'source_page': 'https://polyhaven.com/a/coastal_cliff_01', 'license': 'CC0-1.0',
            'lod_triangles': [count[1] for count in LOD_COUNTS], 'state': 'DRAFT_NOT_USER_CONFIRMED'}


def validate_manifest_contract(root, manifest):
    """Pending sources do not imply an imported mesh. A registered mesh requires all receipts."""
    entries = [entry for entry in manifest['assets'] if entry['path'] == HERO_ASSET]
    sources = [entry for entry in manifest['sources'] if entry['path'].startswith(HERO_SOURCE)]
    if not entries:
        if sources or 'hero_cove_scan' in manifest:
            raise ValueError('Hero manifest claims sources/contract without an imported asset')
        return
    validate_sources(root)
    if len(entries) != 1 or sources != source_entries() or manifest.get('hero_cove_scan') != source_contract():
        raise ValueError('Hero manifest contract differs from fixed intake')


def output_action(root, manifest):
    """Create once; a registered unchanged result is idempotent; never overwrite an asset."""
    if manifest.get('schema') != 'aerosim.environment-draft.assets/v1' or manifest.get('state') != 'DRAFT_NOT_USER_CONFIRMED':
        raise ValueError('Hero requires the existing unconfirmed manifest')
    matches = [entry for entry in manifest['assets'] if entry['path'] == HERO_ASSET]
    path = root / HERO_ASSET
    if len(matches) > 1:
        raise ValueError('Hero duplicate registration')
    if path.exists():
        if path.is_symlink() or not matches or digest(path) != matches[0]['sha256']:
            raise ValueError('Hero same-name asset is unregistered or modified; refusing replacement')
        validate_manifest_contract(root, manifest)
        return 'NO_OP'
    if matches:
        raise ValueError('Hero registered asset is missing')
    return 'CREATE_NEW'


def validate_attestation(asset, attested_sha256):
    """The importer logs HERO_CLIFF_OUTPUT_SHA256 for the package it just saved; only that exact
    engine output may be registered (no file-writing receipt, per the phase5/6 write-surface rules)."""
    if not attested_sha256 or len(attested_sha256) != 64:
        raise ValueError('Hero engine output attestation is missing; refusing to register same-name output')
    if digest(asset) != attested_sha256:
        raise ValueError('Hero output differs from the engine-attested import')


def freeze_imported_asset(root, attested_sha256=None):
    """Append only this real output; retain every existing asset and source entry byte-for-value."""
    from environment_draft_policy import validate_environment_draft
    validate_sources(root)
    file = root/'docs/environment-draft-assets.json'
    manifest = json.loads(file.read_text())
    target = root / HERO_ASSET
    if not target.is_file() or target.is_symlink():
        raise ValueError('Hero asset NOT_IMPORTED; refusing to write manifest')
    if any(entry['path'] == HERO_ASSET for entry in manifest['assets']):
        if output_action(root, manifest) != 'NO_OP':
            raise ValueError('Hero output conflict')
        errors = validate_environment_draft(root)
        if errors: raise ValueError(errors)
        return False
    validate_attestation(target, attested_sha256)
    # Validate the baseline against a virtual view excluding the one new output.
    if manifest.get('schema') != 'aerosim.environment-draft.assets/v1' or manifest.get('state') != 'DRAFT_NOT_USER_CONFIRMED':
        raise ValueError('Hero manifest state')
    actual = {str(p.relative_to(root)) for p in (root/'Content').rglob('*') if p.is_file() and p.suffix in ('.uasset', '.umap')}
    baseline = {entry['path'] for entry in manifest['assets']}
    if len(baseline) != len(manifest['assets']) or actual != baseline | {HERO_ASSET}:
        raise ValueError('Hero import changed the fixed baseline asset set')
    updated = json.loads(json.dumps(manifest))
    if any(entry['path'].startswith(HERO_SOURCE) for entry in updated['sources']) or 'hero_cove_scan' in updated:
        raise ValueError('Hero source entries already exist without the asset')
    updated['assets'].append({'path': HERO_ASSET, 'sha256': digest(target), 'bytes': target.stat().st_size})
    updated['sources'].extend(source_entries())
    updated['hero_cove_scan'] = source_contract()
    updated['source_pages'] = list(dict.fromkeys(updated.get('source_pages', []) + ['https://polyhaven.com/a/coastal_cliff_01']))
    # Prevalidate without changing the registered manifest or other files.
    errors = validate_environment_draft(root, manifest_override=updated)
    if errors: raise ValueError(errors)
    # One atomic manifest replacement, after validation. No asset writes or deletions.
    temporary = file.with_suffix('.json.hero-scan-tmp')
    with temporary.open('x') as output: output.write(json.dumps(updated, indent=2)+'\n')
    temporary.replace(file)
    return True
