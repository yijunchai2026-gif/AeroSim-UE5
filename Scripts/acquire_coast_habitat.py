#!/usr/bin/env python3
"""Acquire fixed CC0 habitat sources and extract bounded attributed meshes."""
from concurrent.futures import ThreadPoolExecutor
import hashlib,json,struct,urllib.request
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'SourceAssets/EnvironmentDraft/HabitatDraft'
UA='AeroSim-asset-research/1.0 (development CC0 attribution audit)'
SELECTIONS={'grass_medium_01': [('SM_CoastGrassSmallDraft',0),('SM_CoastGrassTallDraft',4),('SM_CoastGrassTinyDraft',13)],
            'shrub_03': [('SM_CoastShrubADraft',0),('SM_CoastShrubBDraft',2)],
            'weed_plant_02': [('SM_CoastAnnualForbADraft',0),('SM_CoastAnnualForbBDraft',2),('SM_CoastAnnualForbCDraft',4)],
            'dry_branches_medium_01': [('SM_CoastDeadwoodADraft',0),('SM_CoastDeadwoodBDraft',1),('SM_CoastDeadwoodCDraft',2)]}
def fetch(url,cap=8_000_000):
    if not url.startswith(('https://api.polyhaven.com/','https://dl.polyhaven.org/')):raise ValueError('Unexpected provider host')
    with urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':UA}),timeout=60) as r:data=r.read(cap+1)
    if len(data)>cap:raise ValueError('Source exceeds texture/small-mesh cap')
    return data
def digest(data):return hashlib.sha256(data).hexdigest()
def acquire():
    OUT.mkdir(parents=True,exist_ok=True);receipts=[];meshes=[];providers=[]
    for asset,selections in SELECTIONS.items():
        info_raw=fetch('https://api.polyhaven.com/info/'+asset);files_raw=fetch('https://api.polyhaven.com/files/'+asset)
        info,files=json.loads(info_raw),json.loads(files_raw);providers.append({'asset':asset,'authors':info['authors'],'info_sha256':digest(info_raw),'files_sha256':digest(files_raw),'page':'https://polyhaven.com/a/'+asset})
        entry=files['gltf']['1k']['gltf'];jobs=[(asset+'/'+asset+'.gltf',entry)]
        for filename,descriptor in entry['include'].items():jobs.append((asset+'/'+filename,descriptor))
        texture_maps=[('dry_diff' if asset.startswith('grass') else 'Diffuse','basecolor.jpg'),('nor_dx','normal.jpg'),('Rough','roughness.jpg')]
        if 'Alpha' in files:texture_maps.insert(1,('Alpha','alpha.jpg'))
        for key,name in texture_maps:jobs.append((asset+'/'+name,files[key]['1k']['jpg']))
        def one(job):
            relative,e=job;p=OUT/relative;p.parent.mkdir(parents=True,exist_ok=True)
            data=p.read_bytes() if p.exists() else b''
            if len(data)!=e['size'] or hashlib.md5(data).hexdigest()!=e['md5']:data=fetch(e['url'])
            if len(data)!=e['size'] or hashlib.md5(data).hexdigest()!=e['md5']:raise ValueError('Provider MD5/size mismatch')
            p.write_bytes(data)
            return {'path':str(p.relative_to(ROOT)),'url':e['url'],'sha256':digest(data),'publisher_md5':e['md5'],'bytes':len(data),'license':'CC0-1.0','asset':'https://polyhaven.com/a/'+asset}
        with ThreadPoolExecutor(max_workers=3) as pool:receipts.extend(pool.map(one,jobs))
        gltf=json.loads((OUT/asset/(asset+'.gltf')).read_text());blob=(OUT/asset/gltf['buffers'][0]['uri']).read_bytes()
        def accessor(index):
            a=gltf['accessors'][index];v=gltf['bufferViews'][a['bufferView']];types={5126:'f',5125:'I',5123:'H',5121:'B'};width={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']]
            fmt='<'+types[a['componentType']]*width;stride=v.get('byteStride',struct.calcsize(fmt));start=v.get('byteOffset',0)+a.get('byteOffset',0)
            return [list(struct.unpack_from(fmt,blob,start+i*stride)) for i in range(a['count'])]
        for name,index in selections:
            source_mesh=gltf['meshes'][index];primitive=source_mesh['primitives'][0]
            if len(source_mesh['primitives'])!=1 or primitive.get('mode',4)!=4:raise ValueError('Unexpected source topology')
            positions=accessor(primitive['attributes']['POSITION']);normals=accessor(primitive['attributes']['NORMAL']);uv=accessor(primitive['attributes']['TEXCOORD_0']);indices=[i[0] for i in accessor(primitive['indices'])]
            ground=min(p[1] for p in positions)
            # glTF meters, Y-up right-handed -> UE cm, Z-up, clockwise faces.
            positions=[[p[0]*100,-p[2]*100,(p[1]-ground)*100] for p in positions];normals=[[n[0],-n[2],n[1]] for n in normals]
            for triangle in range(0,len(indices),3):indices[triangle+1],indices[triangle+2]=indices[triangle+2],indices[triangle+1]
            geometry={'schema':'aerosim.habitat-mesh/v1','asset':name,'provider':asset,'provider_mesh_index':index,'provider_mesh_name':source_mesh['name'],'units':'centimeters','ground_offset_source_m':ground,'positions':positions,'normals':normals,'uv':uv,'indices':indices}
            p=OUT/(name+'.mesh.json');p.write_text(json.dumps(geometry,separators=(',',':'))+'\n')
            receipt={'path':str(p.relative_to(ROOT)),'sha256':digest(p.read_bytes()),'license':'CC0-1.0','derived_from':asset,'triangles':len(indices)//3,'vertices':len(positions),'asset':name,'dimensions_cm':[max(v[a] for v in positions)-min(v[a] for v in positions) for a in range(3)]}
            receipts.append(receipt);meshes.append(receipt)
    manifest={'schema':'aerosim.habitat-source/v1','state':'DRAFT_NOT_USER_CONFIRMED','license':'CC0-1.0','license_source':'https://polyhaven.com/license','providers':providers,'sources':receipts,'meshes':meshes,'limits':'Generic published dry grass, annual forbs, shrubs and deadwood; no exact Bay Area botanical identity or production acceptance. Original glTF and all linked files preserved; custom DirectX/alpha/roughness maps are explicitly assigned by the isolated importer. Mesh node display offsets discarded; each mesh bottom is grounded at local Z=0.'}
    (OUT/'source.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps({'sources':len(receipts),'meshes':meshes},indent=2))
# ART-FLORA's hash-verified FloraDraft glTF bundles (no network). Each selected node is
# converted to UE cm/Z-up/clockwise and grounded at local Z=0. All primitives share one
# texture set, so they are merged into a single foliage section.
FLORA=ROOT/'SourceAssets/EnvironmentDraft/FloraDraft'
FLORA_SELECTIONS={'fern_02':[('SM_FloraFernADraft',0),('SM_FloraFernBDraft',2)],
                  'wild_rooibos_bush':[('SM_FloraRooibosADraft',1),('SM_FloraRooibosBDraft',3)]}
def derive_flora():
    receipts=[]
    for asset,selections in FLORA_SELECTIONS.items():
        gltf=json.loads((FLORA/asset/(asset+'.gltf')).read_text());blob=(FLORA/asset/gltf['buffers'][0]['uri']).read_bytes()
        def accessor(index):
            a=gltf['accessors'][index];v=gltf['bufferViews'][a['bufferView']];types={5126:'f',5125:'I',5123:'H',5121:'B'};width={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']]
            fmt='<'+types[a['componentType']]*width;stride=v.get('byteStride',struct.calcsize(fmt));start=v.get('byteOffset',0)+a.get('byteOffset',0)
            return [list(struct.unpack_from(fmt,blob,start+i*stride)) for i in range(a['count'])]
        for name,index in selections:
            positions=[];normals=[];uv=[];indices=[]
            for primitive in gltf['meshes'][index]['primitives']:
                if primitive.get('mode',4)!=4:raise ValueError('Unexpected source topology')
                base=len(positions)
                positions+=accessor(primitive['attributes']['POSITION']);normals+=accessor(primitive['attributes']['NORMAL'])
                uv+=accessor(primitive['attributes']['TEXCOORD_0']);indices+=[i[0]+base for i in accessor(primitive['indices'])]
            ground=min(p[1] for p in positions)
            positions=[[p[0]*100,-p[2]*100,(p[1]-ground)*100] for p in positions];normals=[[n[0],-n[2],n[1]] for n in normals]
            for triangle in range(0,len(indices),3):indices[triangle+1],indices[triangle+2]=indices[triangle+2],indices[triangle+1]
            geometry={'schema':'aerosim.habitat-mesh/v1','asset':name,'provider':asset,'provider_mesh_index':index,'units':'centimeters','ground_offset_source_m':ground,'positions':[[round(c,3) for c in p] for p in positions],'normals':[[round(c,5) for c in n] for n in normals],'uv':[[round(c,5) for c in t] for t in uv],'indices':indices}
            p=OUT/(name+'.mesh.json');p.write_text(json.dumps(geometry,separators=(',',':'))+'\n')
            receipts.append({'path':str(p.relative_to(ROOT)),'sha256':digest(p.read_bytes()),'license':'CC0-1.0','derived_from':'SourceAssets/EnvironmentDraft/FloraDraft/'+asset,'triangles':len(indices)//3,'vertices':len(positions),'asset':name,'dimensions_cm':[round(max(v[a] for v in positions)-min(v[a] for v in positions),2) for a in range(3)]})
    manifest=json.loads((OUT/'source.json').read_text())
    manifest['flora_derived']=receipts
    (OUT/'source.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps(receipts,indent=2))
if __name__=='__main__':
    import sys
    args=sys.argv[1:]
    if args==['--derive-flora']:derive_flora()
    else:
        acquire()
        derive_flora()
