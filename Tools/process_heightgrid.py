"""Load the traced terrain grid, visualize current vs denoised, and write a
smoothed 16-bit heightmap.  Denoise = low-pass that removes jagged high-freq
noise (the stretching source) while keeping macro layout (island/moat/rivers)."""
import os, json, numpy as np
from PIL import Image
T=r"G:\personal_projects\CHANGELING\Tools"
d=json.load(open(os.path.join(T,"heightgrid.json")))
Z=np.array(d["z"],np.float32); G=d["G"]
print("loaded %dx%d  Z %.0f..%.0f"%(G,G,Z.min(),Z.max()))

def gauss1d(s):
    r=max(1,int(3*s)); x=np.arange(-r,r+1); k=np.exp(-x*x/(2*s*s)); return k/k.sum()
def smooth(a,s):
    k=gauss1d(s); r=len(k)//2
    ap=np.pad(a,((r,r),(r,r)),mode='edge')
    tmp=np.array([np.convolve(row,k,'valid') for row in ap])
    return np.array([np.convolve(col,k,'valid') for col in tmp.T]).T

def viz(a,name):
    n=(np.clip((a-Z.min())/(Z.max()-Z.min()),0,1)*255).astype(np.uint8)
    Image.fromarray(n).save(os.path.join(T,name))

Zs=smooth(Z,1.6)   # ~500uu sigma: kills sub-~1.5k noise, keeps moat(R3400)/rivers(~2k)
viz(Z,"terr_current.png"); viz(Zs,"terr_denoised.png")

# slope comparison
def slopestats(a):
    cs=d["cell"]; gx=np.abs(np.diff(a,axis=1)); gy=np.abs(np.diff(a,axis=0))
    ang=np.degrees(np.arctan2(np.maximum(gx[:-1,:],gy[:,:-1]),cs))
    return ang.mean(), ang.max(), (ang>40).mean()*100
print("current  slope mean/max/steep%%: %.1f / %.1f / %.1f"%slopestats(Z))
print("denoised slope mean/max/steep%%: %.1f / %.1f / %.1f"%slopestats(Zs))
print("wrote terr_current.png, terr_denoised.png")
