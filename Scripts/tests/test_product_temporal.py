"""Synthetic manifests exercise sequence rejection; never runtime/visual evidence."""
import importlib.util
import json
from pathlib import Path
import unittest
import sys
import test_product_evidence as fixtures

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'Scripts'))
SPEC=importlib.util.spec_from_file_location('product_temporal_capture',ROOT/'Scripts/product_temporal_capture.py')
MODULE=importlib.util.module_from_spec(SPEC); SPEC.loader.exec_module(MODULE)

class TemporalSequenceTests(unittest.TestCase):
    def setUp(self):
        self.fixture=fixtures.ProductEvidenceTests(); self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.directory=self.fixture.directory
        for i in range(24):
            image=self.directory/f'frame-{i:03d}.png'
            image.write_bytes(self.fixture.image.read_bytes())
            m=dict(self.fixture.manifest,image_file=image.name,
                   sequence_index=i,capture_protocol='ground-hold-move-hold/v1',
                   map_package='/Engine/Maps/Entry',capture_elapsed_seconds=8+i*.3,
                   camera_position_cm=str(max(0,min(8,i-7))))
            self.fixture.write(m,image=image)

    def rewrite(self,index,**changes):
        image=self.directory/f'frame-{index:03d}.png'
        m=json.loads(json.loads(Path(str(image)+'.json').read_text())['canonical_manifest'])
        m.update(changes); self.fixture.write(m,image=image)

    def test_complete_ordered_hold_move_sequence(self):
        self.assertEqual(len(MODULE.validate_sequence(self.directory)),24)

    def test_missing_frame_rejected(self):
        (self.directory/'frame-012.png').unlink()
        with self.assertRaisesRegex(ValueError,'EXPECTED_24_FRAMES'): MODULE.validate_sequence(self.directory)

    def test_duplicate_index_rejected(self):
        self.rewrite(12,sequence_index=11)
        with self.assertRaisesRegex(ValueError,'SEQUENCE_ORDER'): MODULE.validate_sequence(self.directory)

    def test_source_or_map_drift_rejected(self):
        self.rewrite(12,map_package='/Engine/Maps/Templates/OpenWorld')
        with self.assertRaisesRegex(ValueError,'IDENTITY_DRIFT'): MODULE.validate_sequence(self.directory)

    def test_time_reversal_rejected(self):
        self.rewrite(12,capture_elapsed_seconds=8)
        with self.assertRaisesRegex(ValueError,'NONMONOTONIC'): MODULE.validate_sequence(self.directory)

    def test_missing_camera_motion_rejected(self):
        self.rewrite(12,camera_position_cm='0')
        with self.assertRaisesRegex(ValueError,'CAMERA_HOLD_MOVE'): MODULE.validate_sequence(self.directory)

    def test_runtime_default_does_not_inherit_template_terrain(self):
        config=(ROOT/'Config/DefaultEngine.ini').read_text()
        self.assertIn('GameDefaultMap=/Engine/Maps/Entry',config)
