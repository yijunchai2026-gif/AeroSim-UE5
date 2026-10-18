"""Transport/schema checks only; real flight behavior requires packaged MCP execution."""
import base64
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zlib
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import aerosim_control_mcp as server
from aerosim_control_client import Client


def write_png(path: Path, width: int, height: int) -> None:
    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data))
    rows=b''.join(b'\x00'+b''.join(bytes((x%256,y%256,90)) for x in range(width)) for y in range(height))
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,2,0,0,0))
                     +chunk(b'IDAT',zlib.compress(rows))+chunk(b'IEND',b''))


def bridge_tree(root: Path) -> tuple[Path, Path]:
    bridge=root/'Saved/ControlBridge';evidence=root/'Saved/Screenshots/Product'
    bridge.mkdir(parents=True);evidence.mkdir(parents=True)
    return bridge,evidence


class ControlMcpTests(unittest.TestCase):
    def test_real_stdio_initialize_and_discovery(self):
        client=Client()
        try:
            self.assertEqual(len(client.tools),7)
            names={t['name'] for t in client.tools}
            self.assertIn('aerosim_press_key',names)
            self.assertIn('aerosim_capture_view',names)
            self.assertTrue(all(t['inputSchema']['additionalProperties'] is False for t in client.tools))
            schema=next(t for t in client.tools if t['name']=='aerosim_capture_view')['inputSchema']
            self.assertIn('cliff',schema['properties']['view']['enum'])
            self.assertEqual(schema['properties']['view_mode']['enum'],list(server.VIEW_MODES))
        finally:client.close()
    def test_capture_view_allowlist_checked_before_runtime(self):
        with patch.object(server,'directory',side_effect=AssertionError('Must not access runtime')):
            for args in ({},{'view':'Cliff'},{'view':'../cliff'},{'view':'cliff','view_mode':'shadercomplexity'},{'view':'cliff','view_mode':'wireframe'},
                         {'view':'cliff','view_mode':'LIT'},{'view':'cliff','camera':[0,0,0]},{'view':None},
                         {'view':'cliff','frames':0},{'view':'cliff','frames':9},{'view':'cliff','frames':True},{'view':'cliff','frames':2.0},
                         {'view':'cliff','interval_ms':99},{'view':'cliff','interval_ms':2001},{'view':'cliff','interval_ms':'250'}):
                with self.assertRaises(ValueError):server.call('aerosim_capture_view',args)
    def test_capture_view_defaults_to_lit(self):
        self.assertEqual(server.validate('aerosim_capture_view',{'view':'forest'}),
                         ('capture_view',{'view':'forest','view_mode':'lit','frames':1,'interval_ms':250}))
        self.assertEqual(server.validate('aerosim_capture_view',{'view':'cliff','frames':8,'interval_ms':100})[1]['frames'],8)
    def test_evidence_readback_stays_inside_capture_root(self):
        with tempfile.TemporaryDirectory() as scratch:
            bridge,evidence=bridge_tree(Path(scratch))
            inside=evidence/'a.png';write_png(inside,4,4)
            outside=Path(scratch)/'Saved/secret.png';write_png(outside,4,4)
            self.assertEqual(server.evidence_png(bridge,str(inside)),inside.resolve())
            for reported in (str(outside),str(evidence/'../secret.png'),str(evidence/'missing.png'),str(evidence),None,''):
                with self.assertRaises(ValueError):server.evidence_png(bridge,reported)
    def test_capture_returns_inline_jpeg_and_manifest_summary(self):
        with tempfile.TemporaryDirectory() as scratch:
            bridge,evidence=bridge_tree(Path(scratch))
            png=evidence/'shot.png';write_png(png,1600,40)
            manifest={'review_view':'cliff','view_mode':'lighting_only','scene_frustum_triangles_lod0':12,'secret_extra':'x'}
            Path(str(png)+'.json').write_text(json.dumps({'manifest_sha256':'a'*64,'canonical_manifest':json.dumps(manifest)}))
            content=server.capture_content(bridge,{'status':'COMMITTED','evidence_png':str(png),'review_view':'cliff','view_mode':'lighting_only'})
            summary=json.loads(content[0]['text'])
            self.assertEqual(summary['frames'][0]['manifest'],{'review_view':'cliff','view_mode':'lighting_only','scene_frustum_triangles_lod0':12})
            self.assertEqual(content[1]['mimeType'],'image/jpeg')
            jpeg=base64.b64decode(content[1]['data'])
            self.assertTrue(jpeg.startswith(b'\xff\xd8'))
            marker=jpeg.find(b'\xff\xc0')
            if marker<0:marker=jpeg.find(b'\xff\xc2')
            height,width=struct.unpack('>HH',jpeg[marker+5:marker+9])
            self.assertEqual(width,server.PREVIEW_WIDTH)
            self.assertEqual(height,32)
    def test_sequence_returns_one_preview_per_frame(self):
        with tempfile.TemporaryDirectory() as scratch:
            bridge,evidence=bridge_tree(Path(scratch))
            frames=[]
            for index in range(3):
                png=evidence/f'f{index}.png';write_png(png,1920,20)
                Path(str(png)+'.json').write_text(json.dumps({'manifest_sha256':'b'*64,'canonical_manifest':json.dumps({'review_sequence_index':index})}))
                frames.append(str(png))
            content=server.capture_content(bridge,{'status':'COMMITTED','evidence_png':frames[-1],'evidence_pngs':frames})
            summary=json.loads(content[0]['text'])
            self.assertEqual([f['manifest']['review_sequence_index'] for f in summary['frames']],[0,1,2])
            self.assertEqual(len([c for c in content if c['type']=='image']),3)
            self.assertIn('frame_to_frame_mean_abs_rgb',summary)
    def test_sequence_with_one_outside_frame_is_rejected(self):
        with tempfile.TemporaryDirectory() as scratch:
            bridge,evidence=bridge_tree(Path(scratch))
            good=evidence/'g.png';write_png(good,4,4);bad=Path(scratch)/'b.png';write_png(bad,4,4)
            with self.assertRaises(ValueError):server.capture_content(bridge,{'evidence_pngs':[str(good),str(bad)]})
    def test_capture_rejects_runtime_path_outside_root(self):
        with tempfile.TemporaryDirectory() as scratch:
            bridge,_=bridge_tree(Path(scratch))
            outside=Path(scratch)/'elsewhere.png';write_png(outside,4,4)
            with self.assertRaises(ValueError):server.capture_content(bridge,{'evidence_png':str(outside)})
    def test_unknown_key_cannot_reach_mailbox(self):
        with patch.object(server,'directory',side_effect=AssertionError('Must not access runtime')):
            for key in ('ConsoleCommand','/tmp/secret','F1',';quit',None):
                with self.assertRaises(ValueError):server.call('aerosim_press_key',{'key':key})
    def test_bounded_finite_non_boolean_lease(self):
        with patch.object(server,'directory',side_effect=AssertionError('Must not access runtime')):
            for value in (True,0,49,2001,1e9,float('nan'),float('inf'),.5,'100'):
                with self.assertRaises(ValueError):server.call('aerosim_press_key',{'key':'W','duration_ms':value})
    def test_arbitrary_path_code_and_settings_rejected(self):
        with patch.object(server,'directory',side_effect=AssertionError('Must not access runtime')):
            for name,args in [('aerosim_capture',{'path':'/tmp/x'}),('aerosim_ui',{'action':'apply_settings'}),('exec',{'code':'quit'}),('aerosim_inspect',{'actor':'production-map'})]:
                with self.assertRaises(ValueError):server.call(name,args)
    def test_missing_key_and_unknown_properties_rejected(self):
        with self.assertRaises(ValueError):server.call('aerosim_press_key',{})
        with self.assertRaises(ValueError):server.call('aerosim_press_key',{'key':'W','script':'anything'})
    def test_wire_error_does_not_kill_stdio_server(self):
        client=Client()
        try:
            result=client.rpc('tools/call',{'name':'aerosim_press_key','arguments':{'key':'ConsoleCommand'}})
            self.assertTrue(result['isError'])
            self.assertEqual(client.rpc('ping',{}),{})
        finally:client.close()

if __name__=='__main__':unittest.main()
