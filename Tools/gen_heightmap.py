"""
Generate a base heightmap for CHANGELING: broad central hill, gently rolling,
sloping to the map edges. No moat/river channels yet (carved in a later pass).

Landscape mapping (from the live landscape):
  origin (-100800,-100800), XY scale 200, Z scale 24
  WorldZ = (value16 - 32768) / 128 * 24   ->   value16 = 32768 + WorldZ / 0.1875
  505x505 vertices (504 quads * 200 = 100800 units span); world center = (-50400,-50400)
"""
import numpy as np
from PIL import Image

N        = 505          # heightmap resolution (verts)
Z_CENTER = 1000.0       # world Z at the hill top (before rolling)
Z_CORNER = -1500.0      # world Z at the far corners (before rolling)
ROLL1_AMP, ROLL1_RES = 180.0, 12   # gentle large rolls
ROLL2_AMP, ROLL2_RES =  70.0, 26   # subtle secondary undulation
SEED     = 42
ZSCALE   = 24.0
OUT      = r"G:\personal_projects\CHANGELING\Tools\heightmap_island_hill.png"
PREVIEW  = r"G:\personal_projects\CHANGELING\Tools\heightmap_island_hill_preview.png"

def val16(world_z):
    return 32768.0 + world_z / (ZSCALE / 128.0)   # /0.1875

# --- radial dome: 1 at center, 0 at corner, smooth cosine falloff ---
c = (N - 1) / 2.0
yy, xx = np.mgrid[0:N, 0:N].astype(np.float32)
dist = np.sqrt((xx - c) ** 2 + (yy - c) ** 2)
corner = np.sqrt(2.0) * c
r = np.clip(dist / corner, 0.0, 1.0)
falloff = 0.5 * (1.0 + np.cos(np.pi * r))           # 1 -> 0
Z = Z_CORNER + (Z_CENTER - Z_CORNER) * falloff

# --- gentle rolling: smooth upsampled value noise, a couple of octaves ---
def smooth_noise(res, amp, seed):
    rng = np.random.default_rng(seed)
    low = rng.random((res, res)).astype(np.float32)
    img = Image.fromarray((low * 255).astype(np.uint8)).resize((N, N), Image.BICUBIC)
    a = np.asarray(img, dtype=np.float32) / 255.0          # 0..1
    return (a * 2.0 - 1.0) * amp                            # -amp..amp

Z = Z + smooth_noise(ROLL1_RES, ROLL1_AMP, SEED) + smooth_noise(ROLL2_RES, ROLL2_AMP, SEED + 1)

# keep the very edge clean for a tidy import border (taper outer 4%)
edge = np.clip((1.0 - r) / 0.04, 0.0, 1.0)
Z = Z_CORNER + (Z - Z_CORNER) * (0.6 + 0.4 * edge)          # ease toward corner height at the rim

# --- to 16-bit ---
v = np.clip(val16(Z), 0, 65535).astype(np.uint16)
Image.fromarray(v).save(OUT)

# 8-bit preview (normalized) so it's easy to eyeball the shape
pv = ((v.astype(np.float32) - v.min()) / max(1, (v.max() - v.min())) * 255).astype(np.uint8)
Image.fromarray(pv).save(PREVIEW)

print("wrote %s  (%dx%d, 16-bit)" % (OUT, N, N))
print("world Z range: %.0f .. %.0f   (center~%.0f, corners~%.0f)"
      % (Z.min(), Z.max(), Z[int(c), int(c)], Z[0, 0]))
print("value16 range: %d .. %d" % (int(v.min()), int(v.max())))
print("mapping: WorldZ = (value-32768)/128*24 ; world center (-50400,-50400) = pixel (%d,%d)" % (int(c), int(c)))
print("preview: %s" % PREVIEW)
