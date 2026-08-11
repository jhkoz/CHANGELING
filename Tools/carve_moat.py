"""Carve a flat-floored moat ring into the hill, then re-tile for WP.
Centered on map center (hill peak). Flat constant-Z floor so a flat lake sits
flush all the way around; banks smoothly blend back to the hill."""
import os, numpy as np
from PIL import Image
T = r"G:\personal_projects\CHANGELING\Tools"
def load(n): return np.array(Image.open(os.path.join(T, n))).astype(np.float64)
def save16(a, n): Image.fromarray(np.clip(a,0,65535).astype(np.uint16)).save(os.path.join(T, n))

ZS = 24.0/128.0                      # 0.1875 ; WorldZ=(v-32768)*ZS
def v2z(v): return (v - 32768.0) * ZS
def z2v(z): return 32768.0 + z / ZS

# --- moat params (units) ---
UPP        = 200.0                   # world units per heightmap pixel
RING_R     = 14500.0                 # centerline radius (flat-floor outer edge -> ~33000 dia)
HALF_FLOOR = 2000.0                  # half flat-floor width (band = 4000)
BANK       = 2000.0                  # bank transition each side
DEPTH      = 600.0                   # floor cut below mean ring terrain

hill = load("heightmap_island_hill.png")          # 505x505
N = hill.shape[0]; c = (N-1)/2.0                   # 252
yy, xx = np.mgrid[0:N, 0:N].astype(np.float64)
D = np.sqrt((xx-c)**2 + (yy-c)**2) * UPP           # world dist from center
d = np.abs(D - RING_R)                             # dist from ring centerline
Zh = v2z(hill)

mean_ring_z = float(Zh[d < 150].mean())
floor_z = mean_ring_z - DEPTH

s = np.clip((d - HALF_FLOOR) / BANK, 0.0, 1.0)
s = 3*s**2 - 2*s**3                                # smoothstep
Znew = np.where(d <= HALF_FLOOR, floor_z,
        np.where(d <= HALF_FLOOR + BANK, floor_z*(1-s) + Zh*s, Zh))

build = z2v(Znew)
save16(build, "heightmap_build.png")               # 505 working master

# crop 505->504 + split into WP tiles (confirmed-standard quadrant mapping)
b = build[0:504, 0:504]
save16(b, "basewp_build_full.png")
QUAD = {'x0_y0':(slice(0,252),slice(0,252)), 'x1_y0':(slice(0,252),slice(252,504)),
        'x0_y1':(slice(252,504),slice(0,252)), 'x1_y1':(slice(252,504),slice(252,504))}
for nm,(rs,cs) in QUAD.items(): save16(b[rs,cs], "basewp_build_%s.png" % nm)

# preview
pv = ((build - build.min())/max(1,(build.max()-build.min()))*255).astype(np.uint8)
Image.fromarray(pv).save(os.path.join(T,"heightmap_build_preview.png"))

# --- report: radial Z profile + bank/water levels ---
def zat(world_r):
    px = int(round(c + world_r/UPP))
    return round(v2z(build[252, px]), 0)
prof = [(r, zat(r)) for r in (0,5000,10000,12500,14500,16500,18500,21000,30000,50000)]
# outer-bank-top min around the ring (limiting containment height)
outer_mask = (np.abs(D-(RING_R+HALF_FLOOR+BANK)) < 200)
outer_bank_min = float(v2z(build)[outer_mask].min())
water_rec = floor_z + 150.0

print("MOAT carved:")
print("  center=world(-50400,-50400)=px(252,252)  centerline R=%.0f (outer floor dia=%.0f)" % (RING_R, 2*(RING_R+HALF_FLOOR)))
print("  flat-floor band=%.0f  banks=%.0f each  depth=%.0f" % (2*HALF_FLOOR, BANK, DEPTH))
print("  mean ring terrain=%.0f -> FLOOR Z=%.0f (flat, constant)" % (mean_ring_z, floor_z))
print("  lowest outer-bank top around ring=%.0f" % outer_bank_min)
print("  >>> set moat WaterBodyLake Z ~= %.0f  (above floor %.0f, below bank %.0f)" % (water_rec, floor_z, outer_bank_min))
print("  radial Z profile [worldR:Z]: %s" % prof)
print("files: heightmap_build.png(505), basewp_build_full.png(504), basewp_build_x*_y*.png(252)")
