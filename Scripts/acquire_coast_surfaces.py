#!/usr/bin/env python3
"""Acquire size-bounded, hash-verified CC0 draft surface maps from their publisher."""
from concurrent.futures import ThreadPoolExecutor
import hashlib,json
from pathlib import Path
import urllib.request
ROOT=Path(__file__).resolve().parents[1]
UA='AeroSim-asset-research/1.0 (development CC0 attribution audit)'
def fetch(url,cap):
    if not url.startswith(('https://api.polyhaven.com/','https://dl.polyhaven.org/')):raise ValueError('Unapproved source host')
    req=urllib.request.Request(url,headers={'User-Agent':UA})
    with urllib.request.urlopen(req,timeout=60) as r:data=r.read(cap+1)
    if len(data)>cap:raise ValueError('Source size exceeds bound')
    return data

def main():
    jobs=[];sources=[]
    for folder,asset in [('Ground','rocky_terrain_02'),('Rock','cliff_side')]:
        files=json.loads(fetch('https://api.polyhaven.com/files/'+asset,2_000_000))
        for key,name in [('Diffuse','basecolor.jpg'),('nor_dx','normal.jpg'),('Rough','roughness.jpg')]:
            entry=files[key]['2k']['jpg'];jobs.append((folder,asset,name,entry))
    def acquire(job):
        folder,asset,name,entry=job;p=ROOT/'SourceAssets/EnvironmentDraft'/folder/name;p.parent.mkdir(parents=True,exist_ok=True)
        data=p.read_bytes() if p.exists() else fetch(entry['url'],16*1024*1024)
        if len(data)!=entry['size'] or hashlib.md5(data).hexdigest()!=entry['md5']:raise ValueError('Publisher hash/size mismatch')
        p.write_bytes(data)
        return {'path':str(p.relative_to(ROOT)),'sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data),'publisher_md5':entry['md5'],'url':entry['url'],'asset':'https://polyhaven.com/a/'+asset,'license':'CC0-1.0','role':'DirectX normal, linear' if name=='normal.jpg' else 'sRGB base color' if name=='basecolor.jpg' else 'linear roughness'}
    with ThreadPoolExecutor(max_workers=3) as pool:sources=list(pool.map(acquire,jobs))
    manifest={'schema':'aerosim.environment-source/v1','state':'DRAFT_NOT_USER_CONFIRMED','license_source':'https://polyhaven.com/license','physical_width_cm':{'Ground':9000,'Rock':180},'sources':sources}
    out=ROOT/'SourceAssets/EnvironmentDraft/surface-source-manifest.json';out.write_text(json.dumps(manifest,indent=2)+'\n')
    print('Verified draft surfaces',len(sources),'bytes',sum(s['bytes'] for s in sources))
if __name__=='__main__':main()
