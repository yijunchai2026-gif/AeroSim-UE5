import copy
import hashlib
import json
from pathlib import Path
import shutil
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from hero_cove_scan_contract import (HERO_ASSET, HERO_SOURCE, FILES, LOD_COUNTS, parse_lod,
    validate_sources, freeze_imported_asset, output_action, IMPORT_RECEIPT, import_receipt_contract)
from environment_draft_policy import validate_environment_draft
ROOT = Path(__file__).resolve().parents[2]


class HeroCoveScanTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.document = json.loads((ROOT/HERO_SOURCE/'coastal-cliff-40m-lod0.gltf').read_text())
        cls.binary = (ROOT/HERO_SOURCE/'coastal-cliff-40m-lod0.bin').read_bytes()

    def fixture(self, root):
        shutil.copytree(ROOT/HERO_SOURCE, root/HERO_SOURCE)
        p = root/'Content/Development/CoastDraft/Textures/T_CoastGround_BaseColor.uasset'
        p.parent.mkdir(parents=True); p.write_bytes(b'preexisting-reviewed-binary')
        source = root/'SourceAssets/EnvironmentDraft/Ground/basecolor.jpg'
        source.parent.mkdir(); source.write_bytes(b'preexisting-source')
        manifest = {'schema': 'aerosim.environment-draft.assets/v1', 'state': 'DRAFT_NOT_USER_CONFIRMED',
                    'assets': [{'path': str(p.relative_to(root)), 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()}],
                    'sources': [{'path': str(source.relative_to(root)), 'sha256': hashlib.sha256(source.read_bytes()).hexdigest(), 'license': 'CC0-1.0'}],
                    'user_visual_acceptance': 'pending_user', 'formal_promotion': False}
        f = root/'docs/environment-draft-assets.json'; f.parent.mkdir(); f.write_text(json.dumps(manifest))
        return p, manifest, f

    def test_real_fixed_six_files_and_all_faces(self):
        results = validate_sources(ROOT)
        self.assertEqual([r['triangles'] for r in results], [49995, 11997, 3591])
        for result in results:
            self.assertAlmostEqual(result['ue_bounds_max_cm'][1]-result['ue_bounds_min_cm'][1], 4000)
            self.assertTrue(980 < result['ue_bounds_max_cm'][2]-result['ue_bounds_min_cm'][2] < 1020)

    def test_reject_external_buffer_uri_and_material(self):
        for uri in ['../../outside.bin', 'https://example.test/mesh.bin', 'data:application/octet-stream,0']:
            doc = copy.deepcopy(self.document); doc['buffers'][0]['uri'] = uri
            with self.assertRaises(ValueError): parse_lod(doc, self.binary, 0)
        doc = copy.deepcopy(self.document); doc['materials'] = [{}]
        with self.assertRaises(ValueError): parse_lod(doc, self.binary, 0)

    def test_reject_stride_sparse_and_out_of_bounds_buffer(self):
        for mutate in [lambda d: d['bufferViews'][0].update(byteStride=16),
                       lambda d: d['accessors'][0].update(sparse={}),
                       lambda d: d['bufferViews'][0].update(byteOffset=4)]:
            doc = copy.deepcopy(self.document); mutate(doc)
            with self.assertRaises(ValueError): parse_lod(doc, self.binary, 0)
        with self.assertRaises(ValueError): parse_lod(self.document, self.binary[:-4], 0)

    def test_reject_nonfinite_normal_uv_and_index_outside_vertex_buffer(self):
        for offset, value, kind in [(self.document['bufferViews'][1]['byteOffset'], float('nan'), 'f'),
                                    (self.document['bufferViews'][2]['byteOffset'], 1.1, 'f'),
                                    (self.document['bufferViews'][3]['byteOffset'], LOD_COUNTS[0][0], 'I')]:
            blob = bytearray(self.binary); struct.pack_into('<'+kind, blob, offset, value)
            with self.assertRaises(ValueError): parse_lod(self.document, blob, 0)

    def test_reject_repeated_index_zero_area_and_rescaled_positions(self):
        blob = bytearray(self.binary); offset = self.document['bufferViews'][3]['byteOffset']
        blob[offset+4:offset+8] = blob[offset:offset+4]
        with self.assertRaises(ValueError): parse_lod(self.document, blob, 0)
        blob = bytearray(self.binary)
        for i in range(LOD_COUNTS[0][0]):
            value = struct.unpack_from('<f', blob, i*12)[0]
            struct.pack_into('<f', blob, i*12, value*.5)
        with self.assertRaises(ValueError): parse_lod(self.document, blob, 0)

    def test_pending_sources_do_not_register_a_missing_engine_asset(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); _, manifest, f = self.fixture(root); original = f.read_bytes()
            self.assertEqual(validate_environment_draft(root), [])
            self.assertEqual(output_action(root, manifest), 'CREATE_NEW')
            with self.assertRaises(ValueError): freeze_imported_asset(root)
            self.assertEqual(f.read_bytes(), original)

    def import_receipt(self, root, mesh):
        file = root/IMPORT_RECEIPT; file.parent.mkdir(parents=True)
        file.write_text(json.dumps(import_receipt_contract(hashlib.sha256(mesh.read_bytes()).hexdigest())))

    def test_missing_or_wrong_engine_import_receipt_does_not_bless_same_name(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp); _, _, f=self.fixture(root); original=f.read_bytes()
            mesh=root/HERO_ASSET; mesh.parent.mkdir(); mesh.write_bytes(b'unknown-same-name')
            with self.assertRaises(ValueError): freeze_imported_asset(root)
            self.import_receipt(root, mesh); mesh.write_bytes(b'changed-after-import')
            with self.assertRaises(ValueError): freeze_imported_asset(root)
            self.assertEqual(f.read_bytes(), original)

    def test_append_real_asset_preserves_baseline_and_repeat_is_no_op(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); _, baseline, f = self.fixture(root)
            mesh = root/HERO_ASSET; mesh.parent.mkdir(); mesh.write_bytes(b'new-engine-generated-draft')
            self.import_receipt(root, mesh)
            with self.assertRaises(ValueError): output_action(root, baseline)
            self.assertTrue(freeze_imported_asset(root))
            final = json.loads(f.read_text())
            self.assertEqual(final['assets'][:-1], baseline['assets'])
            self.assertEqual(final['sources'][:-6], baseline['sources'])
            self.assertEqual(final['user_visual_acceptance'], 'pending_user')
            self.assertFalse(final['formal_promotion'])
            self.assertEqual(validate_environment_draft(root), [])
            frozen = f.read_bytes()
            self.assertEqual(output_action(root, final), 'NO_OP')
            self.assertFalse(freeze_imported_asset(root)); self.assertEqual(f.read_bytes(), frozen)
            mesh.write_bytes(b'user-modified-engine-output')
            with self.assertRaises(ValueError): output_action(root, final)
            with self.assertRaises(ValueError): freeze_imported_asset(root)
            self.assertEqual(f.read_bytes(), frozen)

    def test_changed_baseline_or_extra_asset_blocks_manifest_without_writes(self):
        for bad in ('changed', 'extra'):
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); old, _, f = self.fixture(root); original = f.read_bytes()
                mesh = root/HERO_ASSET; mesh.parent.mkdir(); mesh.write_bytes(b'new')
                self.import_receipt(root, mesh)
                if bad == 'changed': old.write_bytes(b'user-unreviewed')
                else: (root/'Content/Unapproved.uasset').write_bytes(b'not-from-this-import')
                with self.assertRaises(ValueError): freeze_imported_asset(root)
                self.assertEqual(f.read_bytes(), original)

    def test_source_hash_changes_or_unlisted_files_rejected(self):
        for bad in ('changed', 'extra', 'symlink'):
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); self.fixture(root); directory = root/HERO_SOURCE
                target = directory/FILES[0][0]
                if bad == 'changed': target.write_bytes(target.read_bytes()+b' ')
                elif bad == 'extra': (directory/'unexpected.bin').write_bytes(b'unlisted')
                else:
                    data=target.read_bytes(); target.unlink(); other=root/'external.gltf';other.write_bytes(data);target.symlink_to(other)
                with self.assertRaises(ValueError): validate_sources(root)


if __name__ == '__main__': unittest.main()
