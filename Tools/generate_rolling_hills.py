"""
generate_rolling_hills.py - bake a 16-bit heightmap of gently rolling hills
for a UE landscape.

Output values are centered on 32768, which Unreal treats as zero:
  - imported into a landscape EDIT LAYER, 32768 = "no change", so the hills
    stack on top of the existing sculpt (the riverbed survives);
  - imported as the BASE heightmap, 32768 = the landscape actor's Z origin.

Height conversion assumes the default landscape Z scale of 100
(1 height value = 1/128 m). Pass --z-scale if the landscape uses another.

The file size must match the landscape's Overall Resolution (Landscape mode
-> Manage shows it, e.g. 2017x2017); pass --size / --width / --height.

Examples:
  py -3 generate_rolling_hills.py --size 2017 --out hills.png
  py -3 generate_rolling_hills.py --size 4033 --wavelength 600 --amplitude 8 --seed 12 --out hills.png
"""

import argparse
import sys

import numpy as np


def fade(t):
    """Quintic smoothstep - C2-continuous, so hill flanks stay silky."""
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0)


def value_noise(height, width, cells_y, cells_x, rng):
    """One octave of lattice value noise in [-1, 1], (height x width) pixels."""
    lattice = rng.uniform(-1.0, 1.0, size=(cells_y + 1, cells_x + 1))

    y = np.linspace(0.0, cells_y, height)
    x = np.linspace(0.0, cells_x, width)
    yi = np.minimum(y.astype(np.int64), cells_y - 1)
    xi = np.minimum(x.astype(np.int64), cells_x - 1)
    ty = fade((y - yi)[:, None])
    tx = fade((x - xi)[None, :])

    c00 = lattice[np.ix_(yi, xi)]
    c01 = lattice[np.ix_(yi, xi + 1)]
    c10 = lattice[np.ix_(yi + 1, xi)]
    c11 = lattice[np.ix_(yi + 1, xi + 1)]

    top = c00 + (c01 - c00) * tx
    bottom = c10 + (c11 - c10) * tx
    return top + (bottom - top) * ty


def rolling_hills(height, width, extent_m, wavelength_m, octaves, persistence,
                  lacunarity, rng):
    """Fractal sum of value noise, normalized to [-1, 1]."""
    base_cells = max(1, round(extent_m / wavelength_m))
    total = np.zeros((height, width))
    amplitude = 1.0
    for octave in range(octaves):
        cells = min(int(base_cells * lacunarity ** octave), min(height, width) // 2)
        cells = max(cells, 1)
        total += amplitude * value_noise(height, width, cells, cells, rng)
        amplitude *= persistence
    return total / np.abs(total).max()


def edge_falloff_mask(height, width, quad_m, falloff_m):
    """1 in the interior, easing to 0 at the borders over falloff_m meters."""
    y_m = np.minimum(np.arange(height), np.arange(height)[::-1])[:, None] * quad_m
    x_m = np.minimum(np.arange(width), np.arange(width)[::-1])[None, :] * quad_m
    d = np.minimum(y_m, x_m) / falloff_m
    return fade(np.clip(d, 0.0, 1.0))


def main():
    p = argparse.ArgumentParser(description="Generate a rolling-hills heightmap for UE landscape import.")
    p.add_argument("--size", type=int, default=2017, help="square resolution in vertices (default 2017)")
    p.add_argument("--width", type=int, help="override width in vertices")
    p.add_argument("--height", type=int, help="override height in vertices")
    p.add_argument("--quad-size", type=float, default=1.0, help="meters per landscape quad (default 1.0 = XY scale 100)")
    p.add_argument("--wavelength", type=float, default=400.0, help="hill spacing in meters (default 400)")
    p.add_argument("--amplitude", type=float, default=5.0, help="hill height in meters, peak deviation (default 5)")
    p.add_argument("--octaves", type=int, default=3, help="noise octaves; more = more small detail (default 3)")
    p.add_argument("--persistence", type=float, default=0.45, help="octave amplitude falloff (default 0.45)")
    p.add_argument("--lacunarity", type=float, default=2.0, help="octave frequency step (default 2.0)")
    p.add_argument("--edge-falloff", type=float, default=0.0, help="fade hills to zero within N meters of the border (default off)")
    p.add_argument("--z-scale", type=float, default=100.0, help="landscape Z scale (default 100)")
    p.add_argument("--seed", type=int, default=0, help="random seed")
    p.add_argument("--out", default="RollingHills.png", help="output file (.png 16-bit, or .r16/.raw)")
    args = p.parse_args()

    w = args.width or args.size
    h = args.height or args.size
    extent_m = (max(w, h) - 1) * args.quad_size
    rng = np.random.default_rng(args.seed)

    hills = rolling_hills(h, w, extent_m, args.wavelength, args.octaves,
                          args.persistence, args.lacunarity, rng)
    meters = hills * args.amplitude
    if args.edge_falloff > 0.0:
        meters *= edge_falloff_mask(h, w, args.quad_size, args.edge_falloff)

    # UE height encoding: value = 32768 + meters * 128 * (100 / ZScale)
    values = 32768.0 + meters * 128.0 * (100.0 / args.z_scale)
    clipped = (values < 0).sum() + (values > 65535).sum()
    data = np.clip(np.rint(values), 0, 65535).astype(np.uint16)

    if args.out.lower().endswith((".r16", ".raw")):
        data.astype("<u2").tofile(args.out)
    else:
        try:
            from PIL import Image
        except ImportError:
            sys.exit("Pillow is required for PNG output; use a .r16 extension instead.")
        Image.fromarray(data).save(args.out)

    print(f"wrote {args.out}  ({w}x{h}, 16-bit)")
    print(f"  hills: {meters.min():+.2f} m .. {meters.max():+.2f} m, "
          f"~{args.wavelength:.0f} m apart, seed {args.seed}")
    print(f"  values: {data.min()} .. {data.max()} (32768 = zero)")
    if clipped:
        print(f"  WARNING: {clipped} samples clipped - lower --amplitude or check --z-scale")
    print("  import: Landscape mode -> Sculpt -> Import, pick this file,")
    print("  target a new edit layer ('Hills') to keep the existing sculpt.")


if __name__ == "__main__":
    main()
