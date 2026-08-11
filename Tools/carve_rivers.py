"""Carve 4 meandering rivers from the moat out to the map corners, into the
hill+moat build. Width varies 2500..3500 along each river; beds are
monotonically descending (running-min of terrain-depth) so they can never float."""
import os, numpy as np
from PIL import Image
T = r"G:\personal_projects\CHANGELING\Tools"
def load(n): return np.array(Image.open(os.path.join(T, n))).astype(np.float64)
def save16(a, n): Image.fromarray(np.clip(a,0,65535).astype(np.uint16)).save(os.path.join(T, n))
ZS = 24.0/128.0
def v2z(v): return (v-32768.0)*ZS
def z2v(z): return 32768.0 + z/ZS

UPP      = 200.0
MOAT_R   = 14500.0
INSET    = 6000.0
DEPTH    = 300.0
HALF_BASE= 1500.0      # 3000 wide center of range
HALF_VAR = 250.0       # +/-250 -> half 1250..1750 -> width 2500..3500
BANK     = 1000.0

build = v2z(load("heightmap_build.png"))      # 505 world-Z (hill+moat)
N = build.shape[0]; c = (N-1)/2.0
center = np.array([c, c])
CORNERS = [(0,0),(N-1,0),(0,N-1),(N-1,N-1)]
# (meanderAmp_u, meanderFreq, meanderPhase, widthFreq, widthPhase)
PARAMS  = [(5200,2.5,0.3,1.5,0.0),(4600,3.0,1.7,1.8,2.0),
           (4800,2.0,3.1,1.3,4.0),(5000,2.7,0.9,1.6,1.0)]

def make(corner, amp,mf,mp,wf,wp, M=700):
    cor=np.array(corner,float); d=cor-center; L=np.linalg.norm(d); dv=d/L
    S=center+dv*(MOAT_R/UPP); E=cor-dv*(INSET/UPP)
    t=np.linspace(0,1,M)
    base=S[None,:]+t[:,None]*(E-S)[None,:]
    perp=np.array([-dv[1],dv[0]])
    off=(amp/UPP)*np.sin(2*np.pi*mf*t+mp)*np.sin(np.pi*t)
    path=base+perp[None,:]*off[:,None]
    # bed: monotonic descent
    px=np.clip(path[:,0].round().astype(int),0,N-1); py=np.clip(path[:,1].round().astype(int),0,N-1)
    terr=build[py,px]
    ramp=np.clip(t/0.08,0,1); ramp=3*ramp**2-2*ramp**3
    bed=np.minimum.accumulate(terr-DEPTH*ramp)
    # width (pixels): varies 2500..3500
    halfw=(HALF_BASE+HALF_VAR*np.sin(2*np.pi*wf*t+wp))/UPP
    return path, bed, halfw

def carve(path, bed, halfw):
    Rpx=halfw.max()+BANK/UPP+2
    x0=max(0,int(path[:,0].min()-Rpx)); x1=min(N-1,int(path[:,0].max()+Rpx))
    y0=max(0,int(path[:,1].min()-Rpx)); y1=min(N-1,int(path[:,1].max()+Rpx))
    pxs=path[:,0]; pys=path[:,1]; bankpx=BANK/UPP
    for ry in range(y0,y1+1,32):
        ry2=min(y1+1,ry+32)
        ys=np.arange(ry,ry2); xs=np.arange(x0,x1+1)
        GX,GY=np.meshgrid(xs,ys)
        dx=GX[...,None]-pxs[None,None,:]; dy=GY[...,None]-pys[None,None,:]
        d2=dx*dx+dy*dy; idx=d2.argmin(axis=2); dpx=np.sqrt(d2.min(axis=2))
        bh=bed[idx]; hw=halfw[idx]
        ex=build[ry:ry2,x0:x1+1]
        s=np.clip((dpx-hw)/bankpx,0,1); s=3*s**2-2*s**3
        carved=np.where(dpx<=hw, bh, np.where(dpx<=hw+bankpx, bh*(1-s)+ex*s, ex))
        build[ry:ry2,x0:x1+1]=np.minimum(ex,carved)

rep=[]
for corner,pr in zip(CORNERS,PARAMS):
    path,bed,halfw=make(corner,*pr); carve(path,bed,halfw)
    inc=int(np.sum(np.diff(bed)>0.5))
    rep.append("  corner px%s: width %.0f..%.0f  bed %.0f->%.0f  uphill=%d" %
               (corner, 2*halfw.min()*UPP, 2*halfw.max()*UPP, bed[0], bed[-1], inc))

vb=z2v(build); save16(vb,"heightmap_build.png")
bb=vb[0:504,0:504]; save16(bb,"basewp_build_full.png")
QUAD={'x0_y0':(slice(0,252),slice(0,252)),'x1_y0':(slice(0,252),slice(252,504)),
      'x0_y1':(slice(252,504),slice(0,252)),'x1_y1':(slice(252,504),slice(252,504))}
for nm,(rs,cs) in QUAD.items(): save16(bb[rs,cs],"basewp_build_%s.png"%nm)
pv=((vb-vb.min())/max(1,(vb.max()-vb.min()))*255).astype(np.uint8)
Image.fromarray(pv).save(os.path.join(T,"heightmap_build_preview.png"))
print("RIVERS carved (variable width 2500..3500, monotonic):")
print("\n".join(rep))
print("retiled basewp_build_* ; preview updated")
