#!/usr/bin/env python3
"""Local MCP stdio server for the opt-in AeroSim Development control bridge."""
from __future__ import annotations
import base64
import fcntl
import io
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import uuid

KEYS = ('W','S','A','D','Q','E','LeftShift','LeftControl','C','Escape','Enter','SpaceBar','F10','H','R')
# Mirrors AAeroSimGameMode::ResolveReviewView; 'player' keeps the live player camera.
VIEWS = ('player','overview','ground','south','north','forest','corridor','cliff','bridge','mountain','shore',
         'launch-low','launch-operator','launch-return')
VIEW_MODES = ('lit','lighting_only','detail_lighting','unlit')
PREVIEW_WIDTH = 1280
SEQUENCE_PREVIEW_WIDTH = 960
MAX_INLINE_BYTES = 4 * 1024 * 1024
EMPTY = {'type':'object','properties':{},'additionalProperties':False}
TOOLS = [
 {'name':'aerosim_inspect','description':'Read live flight page, pause, mapped inputs, Slate focus, physical neutral and actual pose. Does not activate a window.','inputSchema':EMPTY,'annotations':{'readOnlyHint':True}},
 {'name':'aerosim_press_key','description':'Preview then deliver a bounded key press through real Slate/Enhanced Input; automatically release. Returns before/after pose and measured action peaks, not a fabricated flight result.','inputSchema':{'type':'object','properties':{'key':{'type':'string','enum':list(KEYS)},'duration_ms':{'type':'integer','minimum':50,'maximum':2000,'default':250}},'required':['key'],'additionalProperties':False}},
 {'name':'aerosim_ui','description':'Preview then run the same primary/back command as native product UI; does not skip preflight or pause safety.','inputSchema':{'type':'object','properties':{'action':{'type':'string','enum':['primary','back']}},'required':['action'],'additionalProperties':False}},
 {'name':'aerosim_click_primary','description':'Click the actual enabled primary Slate button using its live geometry and normal pointer routing, without OS window activation.','inputSchema':EMPTY},
 {'name':'aerosim_capture','description':'Capture actual viewport pixels (with UI) of the current player view into the fixed application sandbox with the hashed evidence manifest, and return a downscaled JPEG preview of that PNG. Does not activate a window.','inputSchema':EMPTY},
 {'name':'aerosim_capture_view','description':'Capture a fixed review-camera preset (pose equals temporal frame 023) or the live player camera, scene only without HUD/UI, in a chosen engine view mode. lighting_only/detail_lighting show form with neutral albedo (judge shape before materials), unlit shows base color. (Wireframe is unavailable in cooked builds.) frames>1 captures a consecutive held-camera sequence while the world clock runs (synchronous PNG readback makes real spacing at least ~0.3 s; the manifest records actual elapsed time) (water/foliage motion, shimmer), each frame its own hashed PNG. Waits for TAA/streaming to settle, restores camera, HUD and view mode, and returns a JPEG preview plus manifest geometry budget. Visual review only; not performance or acceptance evidence.','inputSchema':{'type':'object','properties':{'view':{'type':'string','enum':list(VIEWS)},'view_mode':{'type':'string','enum':list(VIEW_MODES),'default':'lit'},'frames':{'type':'integer','minimum':1,'maximum':8,'default':1},'interval_ms':{'type':'integer','minimum':100,'maximum':2000,'default':250}},'required':['view'],'additionalProperties':False}},
 {'name':'aerosim_release','description':'Release any bridge-held keys; never resumes flight.','inputSchema':EMPTY},
]
CAPTURE_TOOLS = ('aerosim_capture','aerosim_capture_view')
MANIFEST_SUMMARY = (
    'source_fingerprint','build_id','review_view','view_mode','capture_protocol','image_sha256','rhi','quality_tier','vertical_fov',
    'viewport_width','viewport_height','weather','time_minutes','seed','camera_position_cm','camera_rotation_degrees',
    'scene_frustum_mesh_components','scene_frustum_mesh_instances','scene_frustum_mesh_sections',
    'scene_frustum_triangles_lod0','scene_frustum_other_primitives','scene_geometry_scope',
    'diagnostic_frame_p95_ms','diagnostic_cpu_render_ms','diagnostic_gpu_ms','diagnostic_draw_calls',
    'diagnostic_draw_calls_source','performance_claim','visual_verdict',
    'review_sequence_index','review_sequence_frames','review_sequence_elapsed_seconds','world_paused')


def read_session(path: Path) -> dict:
    # Runtime heartbeat replaces this fixed sandbox file once per second. Its
    # platform-neutral delete/move writer has a short missing-file window.
    # Retry only the read; never replay a command with unknown outcome.
    for attempt in range(5):
        try:
            return json.loads((path/'session.json').read_text(encoding='utf-8-sig'))
        except (FileNotFoundError, json.JSONDecodeError):
            if attempt == 4:
                raise
            time.sleep(.005)
    raise AssertionError('unreachable')


def directory() -> Path:
    # Configuration is fixed here, not accepted from MCP requests.
    home=Path.home()
    candidates=[home/'Library/Containers/com.YourCompany.AeroSim/Data/Library/Application Support/Epic/AeroSim/Saved/ControlBridge',
                Path(__file__).resolve().parents[1]/'Saved/ControlBridge']
    fresh=[]
    for path in candidates:
        try:
            session=read_session(path)
            if abs(time.time()-session['heartbeat_unix'])<=8: fresh.append(path)
        except (OSError,ValueError,KeyError,TypeError): pass
    if len(fresh)!=1: raise ValueError('LIVE_BRIDGE_REQUIRED: launch one Development demo with -AeroSimControlBridge')
    return fresh[0]


def send(path: Path, session: str, operation: str, timeout: float = 7, **arguments) -> dict:
    request={'request_id':uuid.uuid4().hex,'session':session,'target':'player:0','operation':operation,
             'operator':'codex-local-mcp','expires_unix':time.time()+8,**arguments}
    temp=path/'request.client.tmp'
    temp.write_text(json.dumps(request,allow_nan=False),encoding='utf-8')
    os.replace(temp,path/'request.json')
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        try:
            response=json.loads((path/'response.json').read_text(encoding='utf-8-sig'))
            if response.get('request_id')==request['request_id']:
                if response.get('status') in ('REJECTED','ROLLED_BACK'):raise ValueError(json.dumps(response))
                return response
        except (OSError,json.JSONDecodeError): pass
        time.sleep(.025)
    raise TimeoutError('RUNTIME_TIMEOUT; bounded runtime key lease still auto-releases')


def evidence_png(bridge: Path, reported: object) -> Path:
    """Only a PNG inside this bridge's own Saved/Screenshots/Product root may be read back."""
    root=(bridge.parent/'Screenshots/Product').resolve()
    if not isinstance(reported,str) or not reported:raise ValueError('EVIDENCE_PATH_MISSING')
    path=Path(reported).resolve()
    if path.suffix.lower()!='.png' or root not in path.parents or not path.is_file():raise ValueError('EVIDENCE_PATH_NOT_ALLOWED')
    return path


def jpeg_preview(png: Path, width: int = PREVIEW_WIDTH) -> bytes:
    """Downscaled JPEG for model viewing; derived pixels are never evidence. Pillow if present, else macOS sips."""
    try:
        from PIL import Image
        with Image.open(png) as image:
            image=image.convert('RGB')
            if image.width>width:
                image=image.resize((width,round(image.height*width/image.width)),Image.LANCZOS)
            buffer=io.BytesIO();image.save(buffer,'JPEG',quality=85);return buffer.getvalue()
    except ImportError:
        pass
    with tempfile.TemporaryDirectory() as scratch:
        output=Path(scratch)/'preview.jpg'
        subprocess.run(['/usr/bin/sips','-s','format','jpeg','-s','formatOptions','85','--resampleWidth',str(width),
                        str(png),'--out',str(output)],check=True,capture_output=True,timeout=30)
        return output.read_bytes()


def read_manifest(png: Path) -> tuple[dict | str, str | None]:
    try:
        envelope=json.loads(Path(str(png)+'.json').read_text(encoding='utf-8-sig'))
        manifest=json.loads(envelope['canonical_manifest'])
        return {key:manifest[key] for key in MANIFEST_SUMMARY if key in manifest},envelope.get('manifest_sha256')
    except (OSError,ValueError,KeyError,TypeError):
        return 'UNREADABLE',None


def frame_differences(pngs: list[Path]) -> list[float] | str:
    """Auxiliary mean absolute RGB change between consecutive frames at 320 px; not a naturalness verdict."""
    try:
        from PIL import Image, ImageChops, ImageStat
    except ImportError:
        return 'UNAVAILABLE_NO_PILLOW'
    small=[]
    for png in pngs:
        with Image.open(png) as image:
            small.append(image.convert('RGB').resize((320,round(image.height*320/image.width))))
    return [round(sum(ImageStat.Stat(ImageChops.difference(a,b)).mean)/3,3) for a,b in zip(small,small[1:])]


def capture_content(bridge: Path, result: dict) -> list[dict]:
    reported=result.get('evidence_pngs') or [result.get('evidence_png')]
    pngs=[evidence_png(bridge,path) for path in reported]
    summary={'status':result.get('status'),'preview':'JPEG derived from evidence PNG; not evidence','frames':[]}
    for key in ('review_view','view_mode','settle_frames','settle_seconds','streaming_wanting_at_capture'):
        if key in result:summary[key]=result[key]
    for png in pngs:
        manifest,digest=read_manifest(png)
        summary['frames'].append({'evidence_png':str(png),'manifest_sha256':digest,'manifest':manifest})
    if len(pngs)>1:summary['frame_to_frame_mean_abs_rgb']=frame_differences(pngs)
    content=[{'type':'text','text':''}]
    width=PREVIEW_WIDTH if len(pngs)==1 else SEQUENCE_PREVIEW_WIDTH
    total=0
    for png in pngs:
        try:
            image=jpeg_preview(png,width)
        except (OSError,ValueError,subprocess.SubprocessError) as error:
            summary['preview']='PREVIEW_FAILED:'+type(error).__name__;break
        total+=len(image)
        if total>MAX_INLINE_BYTES:
            summary['preview']='PREVIEW_TOO_LARGE';break
        content.append({'type':'image','data':base64.b64encode(image).decode('ascii'),'mimeType':'image/jpeg'})
    content[0]['text']=json.dumps(summary,allow_nan=False)
    return content


def validate(name: str, arguments: dict) -> tuple[str, dict]:
    if not isinstance(arguments,dict):raise ValueError('Arguments must be an object')
    if name not in {t['name'] for t in TOOLS}:raise ValueError('Unknown tool')
    if name=='aerosim_press_key':
        if set(arguments)-{'key','duration_ms'} or arguments.get('key') not in KEYS:raise ValueError('KEY_NOT_ALLOWED')
        duration=arguments.get('duration_ms',250)
        if type(duration) is not int or not 50<=duration<=2000:raise ValueError('LEASE_NOT_ALLOWED')
        return 'key',{'key':arguments['key'],'duration_ms':duration}
    if name=='aerosim_ui':
        if set(arguments)!={'action'} or arguments['action'] not in ('primary','back'):raise ValueError('ACTION_NOT_ALLOWED')
        return arguments['action'],{}
    if name=='aerosim_capture_view':
        if set(arguments)-{'view','view_mode','frames','interval_ms'} or arguments.get('view') not in VIEWS:raise ValueError('VIEW_NOT_ALLOWED')
        mode=arguments.get('view_mode','lit')
        if mode not in VIEW_MODES:raise ValueError('VIEW_MODE_NOT_ALLOWED')
        frames=arguments.get('frames',1);interval=arguments.get('interval_ms',250)
        if type(frames) is not int or not 1<=frames<=8 or type(interval) is not int or not 100<=interval<=2000:
            raise ValueError('SEQUENCE_NOT_ALLOWED')
        return 'capture_view',{'view':arguments['view'],'view_mode':mode,'frames':frames,'interval_ms':interval}
    if arguments:raise ValueError('Unexpected arguments')
    return {'aerosim_inspect':'inspect','aerosim_capture':'capture','aerosim_release':'release','aerosim_click_primary':'click_primary'}[name],{}


def run(name: str, arguments: dict) -> tuple[Path, dict]:
    operation,payload=validate(name,arguments)
    path=directory()
    with (path/'client.lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX)
        session=read_session(path)['session']
        opening=send(path,session,'inspect')
        if operation=='inspect':return path,opening
        preview=send(path,session,operation,mode='preview',**payload)
        # Review captures settle for up to 6 s plus readback; the runtime deadline is 15 s.
        timeout=7
        if operation=='capture_view':timeout=20+payload['frames']*(payload['interval_ms']/1000+1.5)
        result=send(path,session,operation,timeout=timeout,
                    mode='apply',expected_page=preview['expected_page'],**payload)
        result['preview']=preview
        return path,result


def call(name: str, arguments: dict) -> dict:
    return run(name,arguments)[1]


def tool_content(name: str, arguments: dict) -> list[dict]:
    path,data=run(name,arguments)
    if name in CAPTURE_TOOLS:return capture_content(path,data)
    return [{'type':'text','text':json.dumps(data,allow_nan=False)}]


def main() -> None:
    for line in sys.stdin:
        request={}
        try:
            if len(line)>16384:raise ValueError('Request too large')
            request=json.loads(line)
            if not isinstance(request,dict) or request.get('jsonrpc')!='2.0':raise ValueError('JSON-RPC 2.0 object required')
            method=request.get('method');identifier=request.get('id')
            if method=='initialize':
                result={'protocolVersion':'2024-11-05','capabilities':{'tools':{}},'serverInfo':{'name':'aerosim-control','version':'1.1.0'}}
            elif method=='ping':result={}
            elif method=='tools/list':result={'tools':TOOLS}
            elif method=='tools/call':
                params=request.get('params',{})
                try:
                    result={'content':tool_content(params.get('name',''),params.get('arguments',{})),'isError':False}
                except (ValueError,OSError,TimeoutError,KeyError) as error:
                    result={'content':[{'type':'text','text':str(error)}],'isError':True}
            elif str(method).startswith('notifications/') or identifier is None:continue
            else:raise ValueError('Unsupported MCP method')
            response={'jsonrpc':'2.0','id':identifier,'result':result}
        except (ValueError,TypeError) as error:
            response={'jsonrpc':'2.0','id':request.get('id') if isinstance(request,dict) else None,'error':{'code':-32602,'message':str(error)}}
        sys.stdout.write(json.dumps(response,allow_nan=False)+'\n');sys.stdout.flush()

if __name__=='__main__':main()
