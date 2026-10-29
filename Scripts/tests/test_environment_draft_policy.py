import hashlib,json,sys,tempfile,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from environment_draft_policy import validate_environment_draft
class DraftContentPolicyTests(unittest.TestCase):
    def fixture(self,root):
        p=root/'Content/Development/CoastDraft/Textures/T_CoastGround_BaseColor.uasset';p.parent.mkdir(parents=True);p.write_bytes(b'audited-engine-asset')
        s=root/'SourceAssets/EnvironmentDraft/Ground/basecolor.jpg';s.parent.mkdir(parents=True);s.write_bytes(b'verified-source')
        m={'schema':'aerosim.environment-draft.assets/v1','state':'DRAFT_NOT_USER_CONFIRMED','assets':[{'path':str(p.relative_to(root)),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}],'sources':[{'path':str(s.relative_to(root)),'sha256':hashlib.sha256(s.read_bytes()).hexdigest(),'license':'CC0-1.0'}]}
        f=root/'docs/environment-draft-assets.json';f.parent.mkdir();f.write_text(json.dumps(m));return p,s,m,f
    def test_empty_cpp_slice_stays_valid(self):
        with tempfile.TemporaryDirectory() as d:self.assertEqual(validate_environment_draft(Path(d)),[])
    def test_repository_wildlife_receipts_stay_valid(self):
        self.assertEqual(validate_environment_draft(Path(__file__).resolve().parents[2]),[])
    def test_exact_hashed_draft_is_allowed(self):
        with tempfile.TemporaryDirectory() as d:
            r=Path(d);self.fixture(r);self.assertEqual(validate_environment_draft(r),[])
    def test_changed_binary_is_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            r=Path(d);p,_,_,_=self.fixture(r);p.write_bytes(b'changed');self.assertTrue(validate_environment_draft(r))
    def test_unmanifested_map_is_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            r=Path(d);self.fixture(r);(r/'Content/Official.umap').write_bytes(b'unapproved');self.assertTrue(validate_environment_draft(r))
    def test_source_changes_are_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            r=Path(d);_,s,_,_=self.fixture(r);s.write_bytes(b'changed');self.assertTrue(validate_environment_draft(r))
    def test_unapproved_license_or_promoted_state_is_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            r=Path(d);_,_,m,f=self.fixture(r);m['sources'][0]['license']='UNKNOWN';f.write_text(json.dumps(m));self.assertTrue(validate_environment_draft(r))
            m['sources'][0]['license']='CC0-1.0';m['state']='PRODUCTION';f.write_text(json.dumps(m));self.assertTrue(validate_environment_draft(r))
    def test_path_escape_is_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            r=Path(d);_,_,m,f=self.fixture(r);m['sources'][0]['path']='SourceAssets/EnvironmentDraft/../../private.jpg';f.write_text(json.dumps(m));self.assertTrue(validate_environment_draft(r))
