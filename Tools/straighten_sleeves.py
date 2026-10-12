"""Run the sleeves as clean tapered tubes down the arm.

    blender -b --python Tools/straighten_sleeves.py -- \
        [--straight 0.9] [--round 0.8] [--bins 20] [--rounds 4] [--ease 0.18]
        [--flare <mm>] [--no-crest] [--match]
        [--flat] [--width <mm>] [--flat-until <mm>] [--axis]

The sleeves came off a sculpt, so the arm is a tube whose centre and whose
girth both wander from one band to the next -- which is what reads as the
outer line wobbling in and out instead of running from shoulder to cuff.

Neither a smooth modifier nor a shrink fixes that: smoothing works on
neighbours, so it takes the wobble down a little everywhere and the wrinkles
with it, and the low-frequency wander that actually shows is exactly what it
preserves.

So measure the tube instead. Bin the sleeve along the arm's own bone chain;
in each band take the centre and the mean radius; smooth those two short
sequences -- twenty numbers, not eleven thousand vertices -- and put every
vertex back on the tube they describe. `straight` is how far the centreline
moves to the smoothed line, `round` how far each vertex's radius moves to the
band's. Both ease off at the shoulder and the cuff so the seams stay put.
"""
import bpy, math, os, sys
from mathutils import Vector

sys.path.insert(0, "D:/CHANGELING/Tools")
import blender_metahuman_garment as mhg

argv = sys.argv
args = argv[argv.index("--") + 1:] if "--" in argv else []


def arg(flag, default=None):
    return args[args.index(flag) + 1] if flag in args else default


body_blend = arg("--body", "Tools/Blender/Bodies/Boggan_Childling_Male.blend")
straight = float(arg("--straight", "0.9"))
roundness = float(arg("--round", "0.8"))
bins = int(arg("--bins", "20"))
rounds = int(arg("--rounds", "4"))
ease = float(arg("--ease", "0.18"))
crest = "--no-crest" not in args
crest_slot = arg("--crest-slot", "Cuff")
flare = arg("--flare")
flare = None if flare is None else float(flare) / 1000.0
match = "--match" in args
flat = "--flat" in args
axis = "--axis" in args
flat_until = float(arg("--flat-until", "157")) / 1000.0
width = arg("--width")
width = None if width is None else float(width) / 1000.0

bpy.ops.wm.open_mainfile(filepath=os.path.abspath(body_blend))
for o in bpy.data.objects:
    if o.type == "MESH" and o.mode != "OBJECT":
        bpy.context.view_layer.objects.active = o
        bpy.ops.object.mode_set(mode="OBJECT")

coat = bpy.data.objects["Costume"]
body = bpy.data.objects.get("Body")
rig = mhg._armature_for(body) or next(
    o for o in bpy.data.objects if o.type == "ARMATURE" and "Body" in o.name)
amw = rig.matrix_world
mw = coat.matrix_world
inv = mw.inverted()


def bone(name):
    b = rig.data.bones.get(name)
    return None if b is None else amw @ b.head_local


def smooth(seq, times):
    """Moving average with the ends held, on a short sequence."""
    out = list(seq)
    for _ in range(times):
        prev = list(out)
        for i in range(1, len(out) - 1):
            out[i] = (prev[i - 1] + prev[i] * 2.0 + prev[i + 1]) * 0.25
    return out


def crest_at(rads, at, flare=None):
    """Re-lay the lower profile so its widest point is the band `at`.

    Truing the sleeve leaves a shape that is right in kind and wrong in
    placement: it tapers from the shoulder, narrows over the forearm, then
    swells again before it closes. That swell is the cuff flaring -- but it
    crests a band or two BELOW the seam, so the widest part of the sleeve
    sits inside the cuff and the seam itself falls on a slope, which is what
    makes the cuff read as a bulge with a band drawn across it rather than as
    a cuff the sleeve widens into.

    So find the narrowest band above the seam and the widest below it, and
    run a cosine from one to the other with the peak landing on the seam,
    then a second cosine down to the hem. Cosine because it arrives flat:
    zero slope at the trough, at the crest and at the hem, so the crest is a
    true ridge on the seam and neither join shows a corner.
    """
    n = len(rads)
    at = max(1.0, min(n - 2.0, at))
    above = range(max(1, n // 2), max(2, int(at)))
    trough_i = min(above, key=lambda i: rads[i])
    trough = rads[trough_i]
    # Left alone the crest is however much the sculpt happened to leave;
    # `flare` names it outright, as metres of radius above the forearm.
    peak = trough + flare if flare is not None else max(rads[trough_i:])
    hem = rads[-1]
    out = list(rads)
    for i in range(trough_i, n):
        if i <= at:
            t = (i - trough_i) / max(at - trough_i, 1e-6)
            out[i] = trough + (peak - trough) * (0.5 - 0.5 * math.cos(math.pi * t))
        else:
            t = (i - at) / max((n - 1) - at, 1e-6)
            out[i] = peak + (hem - peak) * (0.5 - 0.5 * math.cos(math.pi * t))
    return out, trough_i, trough, peak


slots = [m.name if m else "" for m in coat.data.materials]
band_slot = next((i for i, n in enumerate(slots) if crest_slot in n), None)
band_verts = set()
if band_slot is not None:
    for f in coat.data.polygons:
        if f.material_index == band_slot:
            band_verts.update(f.vertices)

# --- measure both sleeves before moving either ----------------------------
#
# Matching the two needs both profiles in hand at once, so measuring and
# moving became separate passes. Nothing in this first loop writes.
state = {}
for side in ("L", "R"):
    group = coat.vertex_groups.get("Sleeve_%s" % side)
    if group is None:
        mhg.log("no Sleeve_%s group -- skipped" % side)
        continue
    gi = group.index
    picked = []
    for v in coat.data.vertices:
        w = next((g.weight for g in v.groups if g.group == gi), 0.0)
        if w > 0.0:
            picked.append((v, w))
    if len(picked) < 40:
        mhg.log("Sleeve_%s has only %d verts -- skipped" % (side, len(picked)))
        continue

    low = side.lower()
    chain = [p for p in (bone("upperarm_%s" % low), bone("lowerarm_%s" % low),
                         bone("hand_%s" % low)) if p is not None]
    if len(chain) < 2:
        mhg.log("no arm bones for %s -- skipped" % side)
        continue

    # Arc length along the bone chain, so a band is a band of ARM, not of
    # world height -- the arm hangs at an angle and the elbow bends.
    segs, run = [], 0.0
    for a, b in zip(chain, chain[1:]):
        span = b - a
        segs.append((a, span, span.length, run))
        run += span.length

    def place(point, segs=segs):
        best = None
        for start, span, length, base in segs:
            if length < 1e-6:
                continue
            t = max(0.0, min(1.0, (point - start).dot(span) / (length * length)))
            foot = start + span * t
            gap = (point - foot).length
            if best is None or gap < best[0]:
                best = (gap, base + length * t, foot, span / length)
        return best

    seated = [(v, w) + place(mw @ v.co)[1:] for v, w in picked]
    lo = min(s for _v, _w, s, _f, _d in seated)
    hi = max(s for _v, _w, s, _f, _d in seated)
    if hi - lo < 1e-4:
        continue
    step = (hi - lo) / bins

    # --- measure each band: where its centre is, and how wide it runs ------
    sums = [Vector((0.0, 0.0, 0.0)) for _ in range(bins)]
    dirs = [Vector((0.0, 0.0, 0.0)) for _ in range(bins)]
    count = [0] * bins
    for v, _w, s, _foot, d in seated:
        b = min(bins - 1, int((s - lo) / step))
        sums[b] += mw @ v.co
        dirs[b] += d
        count[b] += 1
    for b in range(bins):
        if count[b]:
            sums[b] /= count[b]
            dirs[b].normalize()
    # Bands nobody landed in borrow from the nearest that has someone.
    for b in range(bins):
        if not count[b]:
            near = min((i for i in range(bins) if count[i]),
                       key=lambda i: abs(i - b), default=None)
            if near is not None:
                sums[b], dirs[b] = sums[near].copy(), dirs[near].copy()

    rads = [0.0] * bins
    tally = [0] * bins
    for v, _w, s, _foot, _d in seated:
        b = min(bins - 1, int((s - lo) / step))
        off = (mw @ v.co) - sums[b]
        off -= dirs[b] * off.dot(dirs[b])
        rads[b] += off.length
        tally[b] += 1
    for b in range(bins):
        if tally[b]:
            rads[b] /= tally[b]
    for b in range(bins):
        if not tally[b]:
            near = min((i for i in range(bins) if tally[i]),
                       key=lambda i: abs(i - b), default=None)
            if near is not None:
                rads[b] = rads[near]

    cx = smooth([p.x for p in sums], rounds)
    cy = smooth([p.y for p in sums], rounds)
    cz = smooth([p.z for p in sums], rounds)
    centres = [Vector((cx[i], cy[i], cz[i])) for i in range(bins)]

    # --- run the whole sleeve on one straight line, if asked -------------
    #
    # Smoothing the centreline takes the wobble out of it but keeps its
    # shape, and its shape is the arm: these bands are measured along the
    # bone chain, which bends at the elbow, so the sleeve bends with it. A
    # sleeve that runs straight from shoulder to wrist needs the centreline
    # replaced, not smoothed.
    #
    # Anchored at the two ends rather than least-squares fitted. A fitted
    # line is better behaved in the middle but moves both ends, and the ends
    # are the two places that must not move -- the shoulder is sewn to the
    # coat and the wrist is where the cuff goes. Averaging the outer three
    # bands at each end keeps the anchors off any single noisy band.
    if axis:
        head = sum(centres[:3], Vector((0.0, 0.0, 0.0))) / 3.0
        tail = sum(centres[-3:], Vector((0.0, 0.0, 0.0))) / 3.0
        bend = max((centres[i] - head.lerp(tail, i / (bins - 1.0))).length
                   for i in range(bins))
        centres = [head.lerp(tail, i / (bins - 1.0)) for i in range(bins)]
        run = (tail - head).normalized()
        dirs = [run.copy() for _ in range(bins)]
        mhg.log("   %s straightened: the centreline bent %.0fmm off the "
                "shoulder-to-wrist line" % (side, bend * 1000))

    seam = [s for v, _w, s, _f, _d in seated if v.index in band_verts]
    state[side] = {
        "seated": seated, "lo": lo, "hi": hi, "step": step,
        "centres": centres,
        "dirs": dirs, "before": list(rads), "rads": smooth(rads, rounds),
        "seam": (min(seam) - lo) / step if seam else None,
    }

# --- one profile for both arms, if asked ---------------------------------
#
# The two sleeves came off the sculpt at different girths, the right forearm
# running about 5mm thicker than the left the whole way down. Averaging the
# two band sequences and handing the mean to both is what makes them the same
# WIDTH rather than merely the same taper: matching the cuff alone would push
# the difference up into the forearm, where it would still show.
if match and len(state) == 2:
    a, b = state["L"]["rads"], state["R"]["rads"]
    gap = max(abs(x - y) for x, y in zip(a, b))
    mean = [(x + y) * 0.5 for x, y in zip(a, b)]
    state["L"]["rads"] = list(mean)
    state["R"]["rads"] = list(mean)
    seams = [st["seam"] for st in state.values() if st["seam"] is not None]
    if seams:
        at = sum(seams) / len(seams)
        for st in state.values():
            st["seam"] = at
    mhg.log("matched: the arms differed by up to %.0fmm of radius; both now "
            "run the mean, cuff seam at band %.1f"
            % (gap * 1000, seams and at or -1))

# --- crest, then move ----------------------------------------------------
moved_total = 0
for side, st in state.items():
    seated, lo, hi, step = st["seated"], st["lo"], st["hi"], st["step"]
    centres, dirs, rads = st["centres"], st["dirs"], st["rads"]

    # --- one width the whole way down, if asked --------------------------
    #
    # The sculpt gave the sleeve a shape it should not have: 61mm of radius
    # at the upper arm narrowing to 40mm over the forearm, so the arm reads
    # as tapering to a point rather than as a coat sleeve. Replacing the
    # profile with a single number is the whole fix -- everything else in
    # this script already puts the vertices wherever the profile says.
    #
    # Only down to `flat_until`, which defaults to the reach of the cuff:
    # below that the sleeve has to close, and since the cuff covers it,
    # where it closes is nobody's business. The join runs on a cosine so
    # there is no step at the changeover.
    if flat:
        edge = max(2, min(bins - 3, int(round(bins - 1 - flat_until / step))))
        w = width if width is not None else sum(rads[1:6]) / 5.0
        hem = rads[-1]
        rads = [w] * (edge + 1)
        for i in range(edge + 1, bins):
            t = (i - edge) / float(bins - 1 - edge)
            rads.append(w + (hem - w) * (0.5 - 0.5 * math.cos(math.pi * t)))
        mhg.log("   %s flat at %.0fmm radius down to band %d, then closing "
                "to %.0fmm under the cuff" % (side, w * 1000, edge, hem * 1000))

    # Put the flare crest on the cuff seam rather than a band below it.
    if crest and not flat and st["seam"] is not None:
        rads, ti, tr, pk = crest_at(rads, st["seam"], flare)
        mhg.log("   %s crest on the cuff seam at band %.1f "
                "(trough %.0fmm at band %d, crest %.0fmm)"
                % (side, st["seam"], tr * 1000, ti, pk * 1000))

    def sample(seq, s, lo=lo, step=step):
        """Read the band sequence at s, straight-lining between centres."""
        u = (s - lo) / step - 0.5
        i = max(0, min(bins - 1, int(u) if u >= 0 else 0))
        j = min(bins - 1, i + 1)
        f = max(0.0, min(1.0, u - i))
        a, b = seq[i], seq[j]
        return a.lerp(b, f) if isinstance(a, Vector) else a + (b - a) * f

    moved = 0
    for v, w, s, _foot, _d in seated:
        # Ease at both ends: the shoulder seam and the cuff hem stay where
        # they are, or the sleeve pulls away from what it joins.
        u = (s - lo) / (hi - lo)
        edge = min(1.0, min(u, 1.0 - u) / max(ease, 1e-6))
        k = w * edge * edge * (3.0 - 2.0 * edge)
        if k <= 1e-4:
            continue
        p = mw @ v.co
        centre = sample(centres, s)
        d = sample(dirs, s).normalized()
        want_r = sample(rads, s)

        along = (p - centre).dot(d)
        radial = p - centre - d * along
        have_r = radial.length
        if have_r < 1e-6:
            continue
        # centreline first, then girth -- both only as far as asked
        target = (centre + d * along
                  + radial * ((have_r + (want_r - have_r) * roundness) / have_r))
        new = p.lerp(target, k * straight)
        if (new - p).length > 1e-6:
            v.co = inv @ new
            moved += 1

    coat.data.update()
    mhg.log("Sleeve_%s: %d of %d verts trued over %d bands, %.0fmm of arm"
            % (side, moved, len(seated), bins, (hi - lo) * 1000))
    mhg.log("   radius before %s"
            % " ".join("%.0f" % (r * 1000) for r in st["before"]))
    mhg.log("   radius after  %s"
            % " ".join("%.0f" % (r * 1000) for r in rads))
    moved_total += moved

bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(body_blend))
mhg.log("saved %s (%d verts moved)" % (body_blend, moved_total))
