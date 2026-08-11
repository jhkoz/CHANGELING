"""Prep heightmap_island_hill.png for World Partition: crop 505->504 and split
into the project's 2x2 quadrant tiles, matching the existing basewp_* convention."""
import os, numpy as np
from PIL import Image
T = r"G:\personal_projects\CHANGELING\Tools"
def load(n): return np.array(Image.open(os.path.join(T, n)))
def save(arr, n): Image.fromarray(arr.astype(np.uint16)).save(os.path.join(T, n))
rep = []

QUAD = {'x0_y0': (slice(0,252),  slice(0,252)),
        'x1_y0': (slice(0,252),  slice(252,504)),
        'x0_y1': (slice(252,504),slice(0,252)),
        'x1_y1': (slice(252,504),slice(252,504))}

# 1) confirm tile<->quadrant mapping using the sloped set (full vs its 4 tiles)
full_s = load("basewp_sloped_full.png")           # 504x504
mapping, all_ok = {}, True
for name, sl in QUAD.items():
    t = load("basewp_sloped_%s.png" % name)
    if t.shape == full_s[sl].shape and np.array_equal(t, full_s[sl]):
        mapping[name] = sl
    else:
        all_ok = False
if all_ok:
    rep.append("tile<->quadrant mapping CONFIRMED standard (x{col}_y{row} = array[row,col] quadrant)")
else:
    rep.append("standard mapping mismatch; re-deriving by exact match...")
    mapping = {}
    for name in QUAD:
        t = load("basewp_sloped_%s.png" % name)
        for q, sl in QUAD.items():
            if t.shape == full_s[sl].shape and np.array_equal(t, full_s[sl]):
                mapping[name] = sl; rep.append("  %s = %s" % (name, q)); break

# 2) confirm 505->504 crop direction from the real base pipeline (island_505 -> current tiles)
isl = load("island_505.png")
for tag, crop in (("drop-last [0:504,0:504]", isl[0:504,0:504]),
                  ("drop-first [1:505,1:505]", isl[1:505,1:505])):
    hits = sum(1 for name, sl in QUAD.items()
               if np.array_equal(load("basewp_current_%s.png" % name), crop[sl]))
    rep.append("island_505 %s -> matches %d/4 basewp_current tiles" % (tag, hits))

# 3) prep the hill: crop 505->504 (drop-last), full + 4 tiles
hill = load("heightmap_island_hill.png")          # 505x505
hill504 = hill[0:504, 0:504].copy()
save(hill504, "basewp_hill_full.png")
for name, sl in mapping.items():
    save(hill504[sl].copy(), "basewp_hill_%s.png" % name)

# 4) verify outputs
rep.append("WRITTEN (16-bit):")
for n in ["basewp_hill_full.png","basewp_hill_x0_y0.png","basewp_hill_x1_y0.png",
          "basewp_hill_x0_y1.png","basewp_hill_x1_y1.png"]:
    im = Image.open(os.path.join(T, n))
    rep.append("  %-26s %s %s" % (n, im.size, im.mode))
# seam sanity: tiles reassemble to the full
re_full = np.zeros((504,504), np.uint16)
for name, sl in mapping.items(): re_full[sl] = load("basewp_hill_%s.png" % name)
rep.append("tiles reassemble == full: %s" % np.array_equal(re_full, hill504))
print("\n".join(rep))
