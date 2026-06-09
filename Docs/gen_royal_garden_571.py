# 571x571 royal-garden heightmap for UE Landscape import.
# Quartered to the CORNERS by diagonal rivers (an "X"), a central island in a
# round pool, gentle rolling hills. Heights are tuned modest so a low Z-scale
# (your import uses Z=20) reads as a garden, not mountains.
# Tweak the constants and re-run: python Docs/gen_royal_garden_571.py

import math, struct, zlib
from array import array

W = H = 571           # matches the landscape import (38 components x 15 quads + 1)

WATER      = 0.40     # riverbed / pool floor
LAND_BASE  = 0.50     # average land level
HILL_AMP   = 0.06     # gentle rolling
ISLAND_TOP = 0.58     # flat island top (palace pad)
ISLAND_R   = 0.10     # island radius (fraction of terrain)
POOL_R     = 0.17     # central pool radius
RIVER_HALF = 0.045    # river channel half-width
RIVER_EDGE = 0.030    # bank softness

CX = CY = 0.5
SQRT2 = math.sqrt(2.0)

def smoothstep(e0, e1, x):
    if e1 <= e0:
        return 0.0 if x < e0 else 1.0
    t = (x - e0) / (e1 - e0)
    t = 0.0 if t < 0 else (1.0 if t > 1 else t)
    return t * t * (3 - 2 * t)

def rolling(u, v):
    n = (      math.sin(u * 8.0 + 0.3) * math.cos(v * 7.0 - 0.5)
         + 0.5 * math.sin(u * 17.0 - 1.0) * math.cos(v * 15.0 + 0.8)
         + 0.25 * math.sin(u * 29.0)       * math.cos(v * 25.0))
    return n / 1.75

def height(u, v):
    dx = u - CX
    dy = v - CY
    dist = math.hypot(dx, dy)

    h = LAND_BASE + HILL_AMP * rolling(u, v)

    # DIAGONAL rivers running corner-to-corner (the "X")
    distD1 = abs(dx - dy) / SQRT2   # top-left  -> bottom-right
    distD2 = abs(dx + dy) / SQRT2   # top-right -> bottom-left
    chan1 = 1.0 - smoothstep(RIVER_HALF - RIVER_EDGE, RIVER_HALF, distD1)
    chan2 = 1.0 - smoothstep(RIVER_HALF - RIVER_EDGE, RIVER_HALF, distD2)
    river = chan1 if chan1 > chan2 else chan2

    pool = 1.0 - smoothstep(POOL_R - 0.04, POOL_R, dist)
    water = river if river > pool else pool
    h = h * (1.0 - water) + WATER * water

    isle = 1.0 - smoothstep(ISLAND_R - 0.025, ISLAND_R, dist)
    h = h * (1.0 - isle) + ISLAND_TOP * isle

    return 0.0 if h < 0 else (1.0 if h > 1 else h)

def clampi(x):
    return 0 if x < 0 else (65535 if x > 65535 else x)

raw = bytearray()
for y in range(H):
    v = y / (H - 1)
    row = array('H', (clampi(int(height(x / (W - 1), v) * 65535)) for x in range(W)))
    row.byteswap()
    raw.append(0)
    raw += row.tobytes()

def chunk(typ, data):
    body = typ + data
    return struct.pack('>I', len(data)) + body + struct.pack('>I', zlib.crc32(body) & 0xffffffff)

out = r'G:\personal_projects\CHANGELING\Docs\RoyalGarden_571.png'
with open(out, 'wb') as f:
    f.write(b'\x89PNG\r\n\x1a\n')
    f.write(chunk(b'IHDR', struct.pack('>IIBBBBB', W, H, 16, 0, 0, 0, 0)))
    f.write(chunk(b'IDAT', zlib.compress(bytes(raw), 9)))
    f.write(chunk(b'IEND', b''))

print('wrote', out, W, 'x', H)
