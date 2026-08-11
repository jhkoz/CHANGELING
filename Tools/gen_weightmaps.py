"""Generate landscape layer weightmaps for the biome paint.

M_MasterLandscape = BlendMaterialAttributes(A=5-layer biome blend, B=riverbed,
Alpha=MF_AutoFadeZ).  The RIVERBED is height-blended by the material (painted
below TopZ), so it is NOT a weightmap layer.  The 5 paintable layers are biomes:
  Balanced = central island (inside the moat)
  Air(E) / Earth(N) / Fire(S) / Water(W) = cardinal wedges, fill everything else.
Compass verified in-viewport against the sun (lat 42.1N geographic sun):
East=+X, South=+Y, West=-X, North=-Y  =>  Earth=N, Air=E, Fire=S, Water=W.
Balanced + 4 wedges sum to 1 everywhere.
Output full 505 per layer + WP tiles + color preview."""
import os, math, numpy as np
from PIL import Image
T = r"G:\personal_projects\CHANGELING\Tools"
N = 505; UPP = 200.0; c = (N-1)/2.0
yy, xx = np.mgrid[0:N, 0:N].astype(np.float64)
dx = (xx-c)*UPP; dy = (yy-c)*UPP
D = np.sqrt(dx*dx + dy*dy)
ang = np.degrees(np.arctan2(dy, dx)) % 360.0          # 0=+X(E),90=+Y(S),180=-X(W),270=-Y(N)

# ---------- central-island mask (Balanced) ----------
# Moat centerline ~R14500 (carve_moat RING_R).  island=1 well inside the moat,
# crosses 0.5 at the moat centerline (hidden under water), 0 past the outer bank.
RING_R = 14500.0; FEATHER = 2500.0
island = np.clip((RING_R - D)/FEATHER + 0.5, 0.0, 1.0)

# ---------- cardinal wedges (the outer land) ----------
CARD={'Air':0.0,'Fire':90.0,'Water':180.0,'Earth':270.0}; FEA=12.0   # +X=E,+Y=S,-X=W,-Y=N
def angd(a,cc): return np.abs(((a-cc+180)%360)-180)
wedge={k: np.clip((45.0+FEA-angd(ang,cc))/(2*FEA),0,1) for k,cc in CARD.items()}
tot=sum(wedge.values()); tot[tot==0]=1.0
for k in wedge: wedge[k]=wedge[k]/tot

# ---------- compose 5 biome layers ----------
layers={'Balanced':island}
for k in ('Air','Earth','Water','Fire'): layers[k]=(1.0-island)*wedge[k]

QUAD={'x0_y0':(slice(0,252),slice(0,252)),'x1_y0':(slice(0,252),slice(252,504)),
      'x0_y1':(slice(252,504),slice(0,252)),'x1_y1':(slice(252,504),slice(252,504))}
for name,a in layers.items():
    v=(np.clip(a,0,1)*255).astype(np.uint8)
    Image.fromarray(v).save(os.path.join(T,"weight_%s_full.png"%name))
    b=v[0:504,0:504]
    for nm,(rs,cs) in QUAD.items(): Image.fromarray(b[rs,cs]).save(os.path.join(T,"weight_%s_%s.png"%(name,nm)))

# color preview (Balanced=mossy green island, elements by cardinal)
COL={'Balanced':(70,150,60),'Air':(60,200,225),'Earth':(130,90,40),'Fire':(225,80,30),'Water':(30,80,225)}
rgb=np.zeros((N,N,3))
for name,a in layers.items():
    col=np.array(COL[name]); rgb+=a[...,None]*col[None,None,:]
Image.fromarray(np.clip(rgb,0,255).astype(np.uint8)).save(os.path.join(T,"weight_preview.png"))
print("wrote 5 weightmaps (full 505 + 4 tiles each) + weight_preview.png")
print("coverage %%: " + ", ".join("%s=%.0f"%(k, layers[k].mean()*100) for k in layers))
print("Balanced=central island; Air=E(+X) Fire=S(+Y) Water=W(-X) Earth=N(-Y) ; riverbed=height-blend")
