"""Denoise the captured terrain grid and emit a 505x505 16-bit heightmap to
re-import (kills high-freq noise -> no slope stretching; keeps island/moat/
rivers/pools so water stays aligned).  Landscape Z-scale=100, actorZ=0 ->
16-bit value V = 32768 + Z*1.28 ; worldZ = (V-32768)/128*100."""
import os, json, numpy as np
from PIL import Image
T=r"G:\personal_projects\CHANGELING\Tools"
d=json.load(open(os.path.join(T,"heightgrid.json")))
Z=np.array(d["z"],np.float32); G=d["G"]

def gauss1d(s):
    r=max(1,int(3*s)); x=np.arange(-r,r+1); k=np.exp(-x*x/(2*s*s)); return k/k.sum()
def smooth(a,s):
    k=gauss1d(s); r=len(k)//2
    ap=np.pad(a,((r,r),(r,r)),mode='edge')
    tmp=np.array([np.convolve(row,k,'valid') for row in ap])
    return np.array([np.convolve(col,k,'valid') for col in tmp.T]).T

Zs=smooth(Z,2.0)                                   # denoise (keep macro layout)
im=Image.fromarray(Zs.astype(np.float32),mode='F').resize((505,505),Image.BICUBIC)
Zu=np.asarray(im,np.float32)                        # 505x505 smooth heights
V=np.clip(32768.0+Zu*1.28,0,65535).astype(np.uint16)
Image.fromarray(V).save(os.path.join(T,"heightmap_denoised_505.png"))      # 16-bit
prev=((Zu-Zu.min())/(Zu.max()-Zu.min()+1e-6)*255).astype(np.uint8)
Image.fromarray(prev).save(os.path.join(T,"heightmap_denoised_preview.png"))
print("denoised 505 heightmap written")
print("Z range %.0f..%.0f  -> V16 %d..%d"%(Zu.min(),Zu.max(),V.min(),V.max()))
