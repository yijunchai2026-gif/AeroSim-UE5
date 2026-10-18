#!/usr/bin/env python3
"""Acquire two fixed CC0 plant sources and extract five bounded attributed meshes."""
from concurrent.futures import ThreadPoolExecutor
import hashlib,json,struct,urllib.request
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'SourceAssets/EnvironmentDraft/HabitatDraft'
UA='AeroSim-asset-research/1.0 (development CC0 attribution audit)'
SELECTIONS={'grass_medium_01': [('SM_CoastGrassSmallDraft',0),('SM_CoastGrassTallDraft',4),('SM_CoastGrassTinyDraft',13)],
            'shrub_03': [('SM_CoastShrubADraft',0),('SM_CoastShrubBDraft',2)]}
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
        for key,name in [('dry_diff' if asset.startswith('grass') else 'Diffuse','basecolor.jpg'),('Alpha','alpha.jpg'),('nor_dx','normal.jpg'),('Rough','roughness.jpg')]:jobs.append((asset+'/'+name,files[key]['1k']['jpg']))
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
    manifest={'schema':'aerosim.habitat-source/v1','state':'DRAFT_NOT_USER_CONFIRMED','license':'CC0-1.0','license_source':'https://polyhaven.com/license','providers':providers,'sources':receipts,'meshes':meshes,'limits':'Generic published dry grass and small oval-leaved plants; no Bay Area oak identity or production acceptance. Original glTF and all linked files preserved; custom DirectX/alpha/roughness maps are explicitly assigned by the isolated importer. Mesh node display offsets discarded; each mesh bottom is grounded at local Z=0.'}
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
# Coast live oak twig cards: deterministic composite of real LeafSet024 (ambientCG CC0) leaves on
# drawn twigs, 4 cells of 1024 px (a 45-55 cm card shows 28-36 leaves of ~5-7 cm). Normals (DirectX)
# are rotated with each leaf. Requires Pillow; local only.
def derive_oak(seed=20261018):
    import math,random
    from PIL import Image,ImageDraw
    src=FLORA/'LeafSet024';rng=random.Random(seed)
    color=Image.open(src/'LeafSet024_2K-JPG_Color.jpg').convert('RGB');alpha=Image.open(src/'LeafSet024_2K-JPG_Opacity.jpg').convert('L')
    normal=Image.open(src/'LeafSet024_2K-JPG_NormalDX.jpg').convert('RGB');rough=Image.open(src/'LeafSet024_2K-JPG_Roughness.jpg').convert('L')
    boxes=[(95,13,595,671),(799,8,1250,660),(1473,14,1945,663),(108,705,573,1342),(797,721,1260,1353),(1471,695,1931,1355)]
    N=2048;out_c=Image.new('RGB',(N,N),(52,66,30));out_a=Image.new('L',(N,N),0);out_n=Image.new('RGB',(N,N),(128,128,255));out_r=Image.new('L',(N,N),150)
    def rotate_normal(img,angle):
        rot=img.rotate(angle,resample=Image.BICUBIC,expand=True,fillcolor=(128,128,255));px=rot.load();a=math.radians(angle);ca,sa=math.cos(a),math.sin(a)
        for y in range(rot.size[1]):
            for x in range(rot.size[0]):
                r,g,b=px[x,y];nx,ny=r/127.5-1,g/127.5-1
                px[x,y]=(int(max(0,min(255,(nx*ca+ny*sa+1)*127.5))),int(max(0,min(255,(-nx*sa+ny*ca+1)*127.5))),b)
        return rot
    for cell in range(4):
        ox,oy=(cell%2)*1024,(cell//2)*1024
        draw_c=ImageDraw.Draw(out_c);draw_a=ImageDraw.Draw(out_a)
        stems=[];base=(ox+512+rng.randint(-40,40),oy+1010);tip=(ox+512+rng.randint(-160,160),oy+90)
        main=[(base[0]+(tip[0]-base[0])*t+40*math.sin(t*3+cell),base[1]+(tip[1]-base[1])*t) for t in [i/12 for i in range(13)]]
        stems.append(main)
        for side in (-1,1):
            k=rng.randint(4,7);start=main[k];ang=math.radians(-90+side*rng.uniform(30,50));L=rng.uniform(300,420)
            stems.append([(start[0]+math.cos(ang)*L*t,start[1]+math.sin(ang)*L*t) for t in [i/8 for i in range(9)]])
        for stem in stems:
            for i in range(len(stem)-1):
                w=int(9-6*i/len(stem));draw_c.line([stem[i],stem[i+1]],fill=(70,52,38),width=w);draw_a.line([stem[i],stem[i+1]],fill=255,width=w)
        leaves=rng.randint(28,36)
        for leaf in range(leaves):
            stem=stems[0] if leaf%3 else stems[1+leaf%2]
            t=rng.uniform(.12,1.0);i=min(int(t*(len(stem)-1)),len(stem)-2);f=t*(len(stem)-1)-i
            px=stem[i][0]+(stem[i+1][0]-stem[i][0])*f;py=stem[i][1]+(stem[i+1][1]-stem[i][1])*f
            dx,dy=stem[i+1][0]-stem[i][0],stem[i+1][1]-stem[i][1];stem_ang=math.degrees(math.atan2(-dy,dx))
            side=1 if leaf%2 else -1;ang=stem_ang+side*rng.uniform(25,65)-90 # leaf tip points along this direction
            b=boxes[rng.randrange(len(boxes))];length=rng.uniform(105,150);scale=length/(b[3]-b[1])
            size=(max(8,int((b[2]-b[0])*scale)),max(8,int((b[3]-b[1])*scale)))
            lc=color.crop(b).resize(size,Image.LANCZOS);la=alpha.crop(b).resize(size,Image.LANCZOS);ln=normal.crop(b).resize(size,Image.LANCZOS);lr=rough.crop(b).resize(size,Image.LANCZOS)
            lc=lc.rotate(ang,resample=Image.BICUBIC,expand=True);la=la.rotate(ang,resample=Image.BICUBIC,expand=True);lr=lr.rotate(ang,resample=Image.BICUBIC,expand=True);ln=rotate_normal(ln,ang)
            # petiole sits at the stem: offset the leaf centre outward along its own axis
            ra=math.radians(ang+90);cx=px+math.cos(ra)*length*.5;cy=py-math.sin(ra)*length*.5
            pos=(int(cx-lc.size[0]/2),int(cy-lc.size[1]/2))
            clip=Image.new('L',(N,N),0);clip.paste(Image.new('L',(1024,1024),255),(ox,oy))
            region=(pos[0],pos[1],pos[0]+lc.size[0],pos[1]+lc.size[1]);mask=Image.composite(la,Image.new('L',la.size,0),clip.crop(region))
            out_c.paste(lc,pos,mask);out_n.paste(ln,pos,mask);out_r.paste(lr,pos,mask);out_a.paste(Image.new('L',la.size,255),pos,mask)
    target=OUT/'OakTwigCards';target.mkdir(parents=True,exist_ok=True)
    out_c.save(target/'basecolor.png');out_a.save(target/'alpha.png');out_n.save(target/'normal.png');out_r.save(target/'roughness.png')
    receipts=[{'path':str(p.relative_to(ROOT)),'sha256':digest(p.read_bytes()),'license':'CC0-1.0','derived_from':'SourceAssets/EnvironmentDraft/FloraDraft/LeafSet024','seed':seed} for p in sorted(target.glob('*.png'))]
    manifest=json.loads((OUT/'source.json').read_text());manifest['oak_twig_cards']=receipts;(OUT/'source.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps(receipts,indent=2))
if __name__=='__main__':
    import sys
    args=sys.argv[1:]
    if args==['--derive-flora']:derive_flora()
    elif args==['--derive-oak']:derive_oak()
    else:acquire()
