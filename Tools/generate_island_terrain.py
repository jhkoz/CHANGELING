"""
generate_island_terrain.py - compose a full 16-bit landscape heightmap:

  * a central FLAT island at world Z 0 (your structure sits here at the origin),
  * a MOAT ring carved around it,
  * four RIVERS carved along the diagonals out to the map corners (quartering
    the land), each flowing downhill from the high corners to the moat,
  * outlying land with gently ROLLING HILLS that slope downward toward the
    island (radial macro grade; inward = downhill).

Value 32768 = world Z 0 (landscape actor at Z=0, Z scale 100). The island top
is exactly 32768, so a structure based at (0,0,0) sits flush on flat ground.

Resolution MUST match the landscape's Overall Resolution (Landscape mode ->
Manage), e.g. 2017 / 4033 / 8129; pass --size. The island is at the image
CENTRE, so place the landscape so its centre lands on world (0,0) - see the
printout at the end for the exact actor offset.

Example:
  py -3 generate_island_terrain.py --size 2017 --out island.png
  py -3 generate_island_terrain.py --size 4033 --edge-height 55 --moat-depth 8 --seed 3 --out island.png
"""

import argparse
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from generate_rolling_hills import rolling_hills  # reuse the fractal value noise


def smooth01(t):
    """Cubic smoothstep clamped to [0,1]."""
    t = np.clip(t, 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def main():
    p = argparse.ArgumentParser(description="Compose island + moat + rivers + rolling hills heightmap.")
    p.add_argument("--size", type=int, default=2017, help="square resolution in vertices (default 2017)")
    p.add_argument("--width", type=int)
    p.add_argument("--height", type=int)
    p.add_argument("--quad-size", type=float, default=1.0, help="meters per landscape quad (default 1.0)")
    p.add_argument("--z-scale", type=float, default=100.0, help="landscape Z scale (default 100)")
    # shape
    p.add_argument("--island-radius", type=float, default=60.0, help="flat island radius, m (default 60)")
    p.add_argument("--moat-width", type=float, default=30.0, help="moat ring width, m (default 30)")
    p.add_argument("--moat-depth", type=float, default=6.0, help="moat depth below island, m (default 6)")
    p.add_argument("--river-width", type=float, default=24.0, help="river channel width, m (default 24)")
    p.add_argument("--river-depth", type=float, default=4.0, help="river depth below local ground, m (default 4)")
    p.add_argument("--edge-height", type=float, default=25.0, help="height difference between island and map corners, m (default 25)")
    p.add_argument("--island-peak", action="store_true", help="island is the HIGH point and land rolls gently DOWN to the corners (lakes become low sinks); default is the reverse")
    p.add_argument("--bank", type=float, default=8.0, help="smoothing width of channel walls / island lip, m (default 8)")
    # corner lakes (rivers terminate in pools instead of running off the map edge)
    p.add_argument("--corner-lakes", action="store_true", help="carve a pool at each corner where its river ends")
    p.add_argument("--lake-radius", type=float, default=70.0, help="corner lake radius, m (default 70)")
    p.add_argument("--lake-depth", type=float, default=5.0, help="corner lake depth below local ground, m (default 5)")
    p.add_argument("--lake-inset", type=float, default=160.0, help="how far the lake centre sits in from the corner, m (default 160)")
    p.add_argument("--smooth", action="store_true", help="omit channel carving (island pad + slope + hills only) so the Water plugin can dig the moat/rivers/lakes itself")
    # hills
    p.add_argument("--hill-amplitude", type=float, default=5.0, help="rolling hill height, m (default 5)")
    p.add_argument("--hill-wavelength", type=float, default=300.0, help="hill spacing, m (default 300)")
    p.add_argument("--octaves", type=int, default=4)
    p.add_argument("--persistence", type=float, default=0.5)
    p.add_argument("--lacunarity", type=float, default=2.0)
    p.add_argument("--seed", type=int, default=0)
    p.add_argument("--out", default="IslandTerrain.png")
    args = p.parse_args()

    w = args.width or args.size
    h = args.height or args.size
    quad = args.quad_size
    rng = np.random.default_rng(args.seed)

    # centred coordinates in meters (island at image centre)
    cx = (w - 1) / 2.0
    cy = (h - 1) / 2.0
    X = (np.arange(w) - cx)[None, :] * quad
    Y = (np.arange(h) - cy)[:, None] * quad
    d = np.sqrt(X * X + Y * Y)
    d_corner = np.sqrt((cx * quad) ** 2 + (cy * quad) ** 2)

    R_isl = args.island_radius
    R_mo = args.island_radius + args.moat_width
    bank = max(1.0, min(args.bank, args.moat_width * 0.45, args.river_width * 0.45))
    r_half = args.river_width * 0.5

    # macro radial grade: 0 at the moat's outer edge, +/- edge_height at the corners
    # (island-peak -> land descends outward so the island is the high point)
    t = (d - R_mo) / max(d_corner - R_mo, 1.0)
    base = (-1.0 if args.island_peak else 1.0) * args.edge_height * smooth01(t)

    # rolling hills, normalised [-1,1], faded in beyond the moat so the island/moat stay clean
    hills = rolling_hills(h, w, d_corner * 2.0, args.hill_wavelength,
                          args.octaves, args.persistence, args.lacunarity, rng)
    hill_mask = smooth01((d - R_mo) / max(4.0 * bank, 1.0))
    land = base + hills * args.hill_amplitude * hill_mask

    # moat ring: full depth between island lip and outer edge, smooth walls
    moat = args.moat_depth * smooth01((d - R_isl) / bank) * smooth01((R_mo - d) / bank)

    # rivers: the X of diagonals, active only outside the island so they join the moat
    rdist = np.minimum(np.abs(X - Y), np.abs(X + Y)) / np.sqrt(2.0)
    river = (args.river_depth
             * (1.0 - smooth01((rdist - r_half) / bank))
             * smooth01((d - R_isl) / bank))

    # corner lakes: a pool on each diagonal where its river terminates
    lake = np.zeros_like(d)
    if args.corner_lakes:
        d_lake = max(d_corner - args.lake_inset, R_mo + args.lake_radius + bank)
        inv = 1.0 / np.sqrt(2.0)
        lake_dist = np.full_like(d, 1e9)
        for sx, sy in ((1, 1), (1, -1), (-1, 1), (-1, -1)):
            lake_dist = np.minimum(lake_dist, np.sqrt((X - d_lake * sx * inv) ** 2 + (Y - d_lake * sy * inv) ** 2))
        lake = args.lake_depth * (1.0 - smooth01((lake_dist - args.lake_radius) / bank))
        river = river * smooth01((d_lake - d) / bank)  # stop rivers at the lakes, not the map edge

    if args.smooth:
        moat = river = lake = np.zeros_like(d)
    carve = np.maximum(np.maximum(moat, river), lake)
    surf = land - carve

    # force a flat island top at exactly 0, smooth lip into the moat
    isl_w = 1.0 - smooth01((d - (R_isl - bank)) / bank)
    meters = surf * (1.0 - isl_w)

    # encode: value = 32768 + meters * 128 * (100 / zscale)
    values = 32768.0 + meters * 128.0 * (100.0 / args.z_scale)
    clipped = int((values < 0).sum() + (values > 65535).sum())
    data = np.clip(np.rint(values), 0, 65535).astype(np.uint16)

    if args.out.lower().endswith((".r16", ".raw")):
        data.astype("<u2").tofile(args.out)
    else:
        from PIL import Image
        Image.fromarray(data).save(args.out)

    # diagnostics
    def z(px, py):
        iy = min(max(int(round(cy + py / quad)), 0), h - 1)
        ix = min(max(int(round(cx + px / quad)), 0), w - 1)
        return meters[iy, ix]
    print(f"wrote {args.out}  ({w}x{h}, 16-bit)")
    print(f"  island centre Z = {z(0,0):+.2f} m   (structure base sits here)")
    print(f"  moat bottom    Z = {meters[d < R_mo][ (d[d<R_mo] > R_isl) ].min():+.2f} m")
    print(f"  corner land    Z = {meters[0,0]:+.2f} m")
    halfm = (min(w, h) - 1) / 2.0 * quad
    print(f"  along +X: " + "  ".join(f"{int(r)}m:{z(r,0):+.1f}" for r in (0, halfm*0.25, halfm*0.5, halfm*0.75, halfm*0.97)))
    if args.corner_lakes and (lake > args.lake_depth * 0.5).any():
        print(f"  corner lake bottom Z = {meters[lake > args.lake_depth*0.5].min():+.2f} m (high source pools draining to the moat)")
    print(f"  value range {data.min()}..{data.max()} (32768 = Z 0)")
    if clipped:
        print(f"  WARNING: {clipped} samples clipped - lower --edge-height/--moat-depth or --z-scale")
    off = -(w - 1) / 2.0 * quad * 100.0  # uu offset to put image centre at world origin
    print(f"  PLACEMENT: set Landscape actor X = Y = {off:.0f} uu (Z = 0) so the island sits on world (0,0,0)")
    print(f"  import: Landscape mode -> Manage -> Import (base heightmap), or File -> Import Into Level.")


if __name__ == "__main__":
    main()
