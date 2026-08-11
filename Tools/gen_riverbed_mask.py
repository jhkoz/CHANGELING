"""Bake a world-aligned RIVERBED FOOTPRINT mask from the ACTUAL placed water
geometry (water_geom.json: real river splines, pool centers/radii, moat disk).
1=riverbed, 0=biome, feathered banks.  Sampled in the material by world-XY -> UV,
so it follows the channels at any elevation (decouples riverbed from absolute Z).
Landscape: origin (-100800,-100800), 100800 uu across; center (-50400,-50400)."""
import os, json, numpy as np
from PIL import Image
T = r"G:\personal_projects\CHANGELING\Tools"
RES=1024; EXTENT=100800.0; ORIGIN=-100800.0; CX=CY=-50400.0
geom=json.load(open(os.path.join(T,"water_geom.json")))
ax=ORIGIN+(np.arange(RES)+0.5)/RES*EXTENT
WX,WY=np.meshgrid(ax,ax)                 # WX: col->X, WY: row->Y
D=np.sqrt((WX-CX)**2+(WY-CY)**2)
bed=np.zeros((RES,RES),np.float32)

# irregular-edge noise: perturb the distance fields so the shoreline breaks up
def _noise(cells,seed):
    g=np.random.default_rng(seed).standard_normal((cells,cells)).astype(np.float32)
    im=Image.fromarray(((g-g.min())/(g.max()-g.min()+1e-6)*255).astype(np.uint8)).resize((RES,RES),Image.BICUBIC)
    return np.asarray(im,np.float32)/255.0*2-1
NZ=0.65*_noise(80,1)+0.35*_noise(210,2); NZ/=np.abs(NZ).max()
NAMP=420.0   # world-unit jitter applied to every riverbed edge

# moat ring (disk-lake R~17500, island pokes through ~R12000 -> submerged ring)
lo,hi,f=geom["moat"]["inner"],geom["moat"]["outer"],1000.0
Dn=D+NZ*NAMP
bed=np.maximum(bed, np.clip((Dn-(lo-f))/f,0,1)*np.clip(((hi+f)-Dn)/f,0,1))

# rivers: stamp each real spline polyline
def stamp_river(mask,pts,halfw=1700.0,feather=900.0,BLK=16):
    pts=np.asarray(pts,np.float64)
    Rpx=(halfw+feather+NAMP)/EXTENT*RES+2
    pc=(pts[:,0]-ORIGIN)/EXTENT*RES; pr=(pts[:,1]-ORIGIN)/EXTENT*RES
    c0=max(0,int(pc.min()-Rpx)); c1=min(RES-1,int(pc.max()+Rpx))
    r0=max(0,int(pr.min()-Rpx)); r1=min(RES-1,int(pr.max()+Rpx))
    cols=np.arange(c0,c1+1)
    for rb in range(r0,r1+1,BLK):
        rb2=min(r1+1,rb+BLK); rows=np.arange(rb,rb2)
        GC,GR=np.meshgrid(cols,rows)
        gx=ORIGIN+(GC+0.5)/RES*EXTENT; gy=ORIGIN+(GR+0.5)/RES*EXTENT
        ddx=gx[...,None]-pts[None,None,:,0]; ddy=gy[...,None]-pts[None,None,:,1]
        dmin=np.sqrt((ddx*ddx+ddy*ddy).min(axis=2))+NZ[rb:rb2,c0:c1+1]*NAMP
        m=np.clip((halfw+feather-dmin)/feather,0,1)
        mask[rb:rb2,c0:c1+1]=np.maximum(mask[rb:rb2,c0:c1+1],m)
for r in geom["rivers"]: stamp_river(bed, r["pts"])

# pools: disks at real centers/radii
for p in geom["pools"]:
    cx,cy=p["c"]; rr=p["r"]; fe=1000.0
    pd=np.sqrt((WX-cx)**2+(WY-cy)**2)+NZ*NAMP
    bed=np.maximum(bed, np.clip((rr+fe-pd)/fe,0,1))

bed=np.clip(bed,0,1)
Image.fromarray((bed*255).astype(np.uint8)).save(os.path.join(T,"riverbed_mask.png"))
print("wrote riverbed_mask.png %dx%d coverage=%.1f%% (from actual water geom)"%(RES,RES,bed.mean()*100))
