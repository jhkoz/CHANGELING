"""Turn a flared cuff back over the end of each sleeve.

    blender -b --python Tools/add_cuffs.py -- \
        [--reach 0.86] [--height <mm>] [--flare 8] [--gap 2] \
        [--wall 5] [--roll 6] \
        [--segments 28] [--no-retag] [--no-join] [--no-match] [--no-tuck] [--tuck-reach 60]

A turned-up cuff is a band of cloth folded back up the outside of the
sleeve: the fold sits at the hem, the free edge points up toward the elbow,
and because the sleeve widens as it goes up, the band flares as it rises.

This builds it rather than extruding it. The obvious approach is to take the
hem edge loop and pull a band off it, but there is no loop to take: the
sleeve end is a welded fold, closed, with 2706 non-manifold edges in the
shell it belongs to, and surgery on that produces a cuff with the same
faults as its parent. A revolved profile fitted to measurements of the
sleeve gives clean geometry, and it can be fitted exactly because the sleeve
was just trued -- its radius at any height is now a number worth reading.

So: measure the sleeve at each height the cuff spans, and revolve a closed
profile around the arm at that radius plus a gap. The profile carries the
fold at the bottom and a rounded rim at the top, so the cuff reads as cloth
turned back on itself rather than a collar dropped over the wrist.
"""
import bpy, bmesh, math, os, sys
from mathutils import Vector

sys.path.insert(0, "D:/CHANGELING/Tools")
import blender_metahuman_garment as mhg

argv = sys.argv
args = argv[argv.index("--") + 1:] if "--" in argv else []


def arg(flag, default=None):
    return args[args.index(flag) + 1] if flag in args else default


body_blend = arg("--body", "Tools/Blender/Bodies/Boggan_Childling_Male.blend")
reach = float(arg("--reach", "0.86"))
flare = float(arg("--flare", "8")) / 1000.0
gap = float(arg("--gap", "2")) / 1000.0
wall = float(arg("--wall", "5")) / 1000.0
roll = float(arg("--roll", "6")) / 1000.0
segments = int(arg("--segments", "28"))
retag = "--no-retag" not in args
match = "--no-match" not in args
tuck = "--no-tuck" not in args
tuck_reach = float(arg("--tuck-reach", "60")) / 1000.0
join = "--no-join" not in args

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

slots = [m.name if m else "" for m in coat.data.materials]
cuff_slot = next((i for i, n in enumerate(slots) if "Cuff" in n), None)
coat_slot = next((i for i, n in enumerate(slots)
                  if n == "Material_0" or "Coat" in n), 0)
if cuff_slot is None:
    sys.exit("no cuff material slot on the Costume")
cuff_mat = coat.data.materials[cuff_slot]


def bone(name):
    b = rig.data.bones.get(name)
    return None if b is None else amw @ b.head_local


# The profile, as (height above the hem, how far proud of the sleeve). It
# runs up the inside, over the rim, back down the outside, and around the
# fold -- a closed loop, so the revolved surface is a closed shell.
def profile():
    g, w, f, h = gap, wall, flare, height
    return [
        (h, g),
        (h * 0.66, g),
        (h * 0.33, g),
        (0.0, g),
        (-roll * 0.45, g + w * 0.30),          # under the fold
        (-roll, g + w * 0.60),
        (-roll * 0.45, g + w * 0.92),
        (0.0, g + w),
        (h * 0.33, g + w + f * 0.33),          # flaring as it rises
        (h * 0.66, g + w + f * 0.66),
        (h, g + w + f),
        (h + roll * 0.55, g + w * 0.5 + f * 0.5),   # rounded top rim
    ]


SAMPLES = 18


def fit_line(xs, ys, envelope=False):
    """Least squares y = a + b*x, optionally raised to clear every sample.

    The cuff has to be straight AND it has to clear the sleeve. A plain
    least-squares line does the first and fails the second: it runs through
    the middle of the measurements, so wherever the sleeve bulges above it
    the sleeve comes through the cuff. Raising the whole line until it sits
    above every sample keeps it straight and puts it outside the cloth.
    """
    n = len(xs)
    mx = sum(xs) / n
    my = sum(ys) / n
    den = sum((x - mx) ** 2 for x in xs)
    b = (sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / den
         if den > 1e-12 else 0.0)
    a = my - b * mx
    if envelope:
        a += max(0.0, max(y - (a + b * x) for x, y in zip(xs, ys)))
    return a, b


def arm_bands(pts, chain, bins=20, bound=0.100):
    """Bin a sleeve along its own bone chain and measure each band.

    This is the measurement straighten_sleeves uses, and it is here because
    two attempts to invent a cheaper one for the cuff both failed. Slicing by
    distance along a single infinite forearm axis also cuts the coat's skirt,
    because the forearm points steeply downward. Estimating a ring centre
    from the mean of one slice collapses onto whichever lobe survives outlier
    rejection when the slice is patchy -- it reported sleeve radii of 2 to
    9mm at some heights, and a cuff fitted to clear that came out 104mm
    across at a 70mm wrist.

    Binning along the chain fixes both. A vertex is placed on the nearest
    point of the arm, so the bend at the elbow is handled and nothing lands
    on the wrong limb; and each band averages some ninety vertices, so one
    stray cannot move it.

    Returns (lo, hi, step, centres, dirs, radii) in world space, where hi is
    the hem end of the sleeve.
    """
    segs, run = [], 0.0
    for a, b in zip(chain, chain[1:]):
        span = b - a
        segs.append((a, span, span.length, run))
        run += span.length

    def place(point):
        best = None
        for start, span, length, base in segs:
            if length < 1e-6:
                continue
            t = max(0.0, min(1.0, (point - start).dot(span) / (length * length)))
            gap = (point - (start + span * t)).length
            if best is None or gap < best[0]:
                best = (gap, base + length * t, span / length)
        return best

    seated = [(q,) + place(q)[1:] for q in pts]
    lo = min(t for _q, t, _d in seated)
    hi = max(t for _q, t, _d in seated)
    step = (hi - lo) / bins

    sums = [Vector((0.0, 0.0, 0.0)) for _ in range(bins)]
    dirs = [Vector((0.0, 0.0, 0.0)) for _ in range(bins)]
    count = [0] * bins
    for q, t, d in seated:
        j = min(bins - 1, int((t - lo) / step))
        sums[j] += q
        dirs[j] += d
        count[j] += 1
    for j in range(bins):
        if count[j]:
            sums[j] /= count[j]
            dirs[j].normalize()
    for j in range(bins):
        if not count[j]:
            near = min((i for i in range(bins) if count[i]),
                       key=lambda i: abs(i - j), default=None)
            if near is not None:
                sums[j], dirs[j] = sums[near].copy(), dirs[near].copy()

    # Re-seat the sleeve on its own straight axis before measuring anything.
    #
    # The bands above are laid out by arc length along the bone chain, which
    # bends at the elbow. The sleeve no longer does -- straighten_sleeves
    # --axis put it on one line -- and the two do not correspond. A hem that
    # is a flat ring about the straight axis maps to a RANGE of arc lengths
    # along the bent chain, so `hi` is not the hem, it is the furthest corner
    # of it; the cuff fold gets placed past the hem on one side of the arm
    # and short of it on the other, and the sleeve breaks through the fold.
    # That is what put a wedge of sleeve through the bottom of both cuffs.
    #
    # It also fixes the girth: slicing a straight tube on bent planes cuts it
    # obliquely, and an oblique cut through a cylinder is an ellipse, so the
    # radius came back too big and varying.
    head = sum(sums[:3], Vector((0.0, 0.0, 0.0))) / 3.0
    tail = sum(sums[-3:], Vector((0.0, 0.0, 0.0))) / 3.0
    if (tail - head).length < 1e-6:
        run = Vector((0.0, 0.0, -1.0))
    else:
        run = (tail - head).normalized()

    # Only what is actually on the arm. Seating by a plain dot product is
    # unbounded, unlike the nearest-point-on-chain it replaces, so a skirt
    # vertex that strayed into the Sleeve group projects clean past the hem
    # and takes `hi` with it -- which puts the cuff fold beyond the end of
    # the sleeve again and made the band radii read 24 39 39 45 40 34 40 31
    # across a sleeve that tapers smoothly.
    seated = []
    for q in pts:
        off = q - head
        along = off.dot(run)
        if (off - run * along).length <= bound:
            seated.append((q, along))
    if len(seated) < 24:
        seated = [(q, (q - head).dot(run)) for q in pts]
    lo = min(t for _q, t in seated)
    hi = max(t for _q, t in seated)
    step = (hi - lo) / bins

    sums = [Vector((0.0, 0.0, 0.0)) for _ in range(bins)]
    count = [0] * bins
    for q, t in seated:
        j = min(bins - 1, int((t - lo) / step))
        sums[j] += q
        count[j] += 1
    for j in range(bins):
        if count[j]:
            sums[j] /= count[j]
    for j in range(bins):
        if not count[j]:
            near = min((i for i in range(bins) if count[i]),
                       key=lambda i: abs(i - j), default=None)
            if near is not None:
                sums[j] = sums[near].copy()
    dirs = [run.copy() for _ in range(bins)]

    # The mean radius, deliberately, and the sleeve is tucked inside the cuff
    # afterwards rather than the cuff being widened to clear it. Fitting to a
    # high percentile instead does clear the sleeve, but this shell carries
    # vertices some 20mm proud of its own mean surface -- it has 2706
    # non-manifold edges and 3008 folds past 107 degrees -- so a cuff that
    # cleared every spike came out 123mm across a 66mm wrist, a barrel with
    # no taper left in it. The garment should not be shaped by the worst
    # vertex in a bad mesh.
    heap = [[] for _ in range(bins)]
    for q, t in seated:
        j = min(bins - 1, int((t - lo) / step))
        off = q - sums[j]
        heap[j].append((off - run * off.dot(run)).length)
    radii = [0.0] * bins
    for j in range(bins):
        if heap[j]:
            radii[j] = sum(heap[j]) / len(heap[j])
    for j in range(bins):
        if not heap[j]:
            near = min((i for i in range(bins) if heap[i]),
                       key=lambda i: abs(i - j), default=None)
            if near is not None:
                radii[j] = radii[near]
    return lo, hi, step, sums, dirs, radii, head, run



def read_band(seq, lo, step, bins, at):
    """Read a band sequence at an arc length, between band centres."""
    u = (at - lo) / step - 0.5
    i = max(0, min(bins - 1, int(math.floor(u)) if u >= 0 else 0))
    j = min(bins - 1, i + 1)
    f = max(0.0, min(1.0, u - math.floor(u) if u >= 0 else 0.0))
    a, b = seq[i], seq[j]
    return a.lerp(b, f) if isinstance(a, Vector) else a + (b - a) * f


BINS = 20

# --- pass one: measure each sleeve along its own arm ----------------------
state = {}
for side in ("L", "R"):
    group = coat.vertex_groups.get("Sleeve_%s" % side)
    if group is None:
        mhg.log("no Sleeve_%s group -- skipped" % side)
        continue
    gi = group.index
    mine = [mw @ v.co for v in coat.data.vertices
            if any(g.group == gi for g in v.groups)]
    low = side.lower()
    chain = [q for q in (bone("upperarm_%s" % low), bone("lowerarm_%s" % low),
                         bone("hand_%s" % low)) if q is not None]
    if len(chain) < 2 or len(mine) < 40:
        mhg.log("Sleeve_%s: nothing to measure -- skipped" % side)
        continue

    lo, hi, step, centres, dirs, radii, head, run = arm_bands(
        mine, chain, BINS)
    # How far the cuff may reach: from the elbow to the hem, measured on the
    # SAME axis the bands now use, or the two numbers do not compare.
    elbow = chain[1] if len(chain) == 3 else chain[0]
    state[side] = {"lo": lo, "hi": hi, "step": step, "centres": centres,
                   "dirs": dirs, "radii": radii,
                   "forearm": hi - (elbow - head).dot(run)}
    mhg.log("Sleeve_%s: %d verts, %.0fmm of arm, forearm %.0fmm below the "
            "elbow; radius %s"
            % (side, len(mine), (hi - lo) * 1000,
               state[side]["forearm"] * 1000,
               " ".join("%.0f" % (r * 1000) for r in radii[-8:])))

if not state:
    sys.exit("no sleeves to cuff")

# How far up the forearm the cuff runs, taken from the shorter arm so
# neither cuff overshoots its own elbow, and shared so the pair match.
span = (float(arg("--height")) / 1000.0 if arg("--height")
        else reach * min(st["forearm"] for st in state.values()))
height = span
prof = profile()

# --- pass two: fit a straight cone to each sleeve -------------------------
#
# Over 70mm the cuff could follow the sleeve's own radius at every height and
# look fine. Running almost to the elbow it cannot: the sleeve has a trough
# over the forearm and a crest above it, and a cuff tracing that is a wavy
# tube, not a cuff. Fit one line through the band radii instead -- raised to
# clear them -- and the band is straight-sided by construction.
SAMPLES = 18
ups = [span * i / (SAMPLES - 1) for i in range(SAMPLES)]
for side, st in state.items():
    lo, hi, step = st["lo"], st["hi"], st["step"]
    obs = [read_band(st["radii"], lo, step, BINS, hi - u) for u in ups]
    st["fit_r"] = fit_line(ups, obs, envelope=True)
    cen = [read_band(st["centres"], lo, step, BINS, hi - u) for u in ups]
    st["fit_c"] = [fit_line(ups, [c[k] for c in cen]) for k in range(3)]
    st["axis"] = read_band(st["dirs"], lo, step, BINS, hi - span * 0.5)
    st["ref"] = st["axis"].orthogonal().normalized()
    st["ref2"] = st["axis"].cross(st["ref"]).normalized()
    a, b = st["fit_r"]
    st["wave"] = max(abs(r - (a + b * u)) for u, r in zip(ups, obs))

if match and len(state) == 2:
    la, lb = state["L"]["fit_r"]
    ra, rb = state["R"]["fit_r"]
    mhg.log("matched: the fitted cones sat %.0fmm apart at the wrist and "
            "%.0fmm at the top; both cuffs built on the mean"
            % (abs(la - ra) * 1000,
               abs((la + lb * span) - (ra + rb * span)) * 1000))
    mean = ((la + ra) * 0.5, (lb + rb) * 0.5)
    for st in state.values():
        st["fit_r"] = mean

# --- pass three: revolve it ----------------------------------------------
built = []
for side, st in state.items():
    a, b = st["fit_r"]
    fc = st["fit_c"]

    # Take the direction from the centreline the rings actually sit on.
    #
    # st["axis"] is fitted to the WHOLE sleeve, shoulder to wrist, but
    # straighten_sleeves eases its correction off at both ends so the seams
    # stay put -- which means the shoulder end never fully straightened and
    # drags that fit off true. fit_c is fitted only over the stretch the cuff
    # covers. Centring the rings on one line and orienting them by another
    # left the cuff 6.9 degrees off its own sleeve, about 18mm of drift over
    # its length. They have to be the same line.
    lean = Vector([fc[k][1] for k in range(3)])
    axis = (-lean.normalized() if lean.length > 1e-6 else st["axis"])
    ref = axis.orthogonal().normalized()
    ref2 = axis.cross(ref).normalized()

    verts, faces = [], []
    for up, out in prof:
        u = max(0.0, min(span, up))
        # The centre line is fitted too, so the cuff follows any lean the arm
        # has -- but as a straight line, for the same reason the radius is.
        centre = Vector([fc[k][0] + fc[k][1] * u for k in range(3)])
        base = centre + axis * (u - up)
        r = (a + b * u) + out
        for k in range(segments):
            th = 2.0 * math.pi * k / segments
            verts.append(base + (ref * math.cos(th) + ref2 * math.sin(th)) * r)

    rows = len(prof)
    for j in range(rows):
        j2 = (j + 1) % rows
        for k in range(segments):
            k2 = (k + 1) % segments
            faces.append((j * segments + k, j * segments + k2,
                          j2 * segments + k2, j2 * segments + k))

    me = bpy.data.meshes.new("Cuff_%s_mesh" % side)
    inv = mw.inverted()
    me.from_pydata([inv @ q for q in verts], [], faces)
    me.update()
    obj = bpy.data.objects.new("Cuff_%s" % side, me)
    obj.matrix_world = mw
    bpy.context.collection.objects.link(obj)
    me.materials.append(cuff_mat)

    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()
    for poly in me.polygons:
        poly.use_smooth = True

    # Keep it in the sleeve group, so anything that works on the sleeve later
    # -- truing, hemming, the eventual skinning -- picks the cuff up too.
    vg = obj.vertex_groups.new(name="Sleeve_%s" % side)
    vg.add(list(range(len(me.vertices))), 1.0, "REPLACE")

    mhg.log("Cuff_%s: %d verts, %d faces; fold O%.0fmm -> rim O%.0fmm over "
            "%.0fmm of a %.0fmm forearm; sleeve waved %.1fmm off the line"
            % (side, len(me.vertices), len(me.polygons),
               (a + gap + wall) * 2000.0,
               (a + b * span + gap + wall + flare) * 2000.0,
               span * 1000.0, st["forearm"] * 1000.0, st["wave"] * 1000.0))
    built.append(obj)

# Tuck the sleeve inside the cuffs.
#
# A straight cone over a lumpy sleeve leaves some of the sleeve outside it,
# and the two ways to avoid that are both worse: widen the cuff until it
# clears every spike, which loses the taper, or cut the covered geometry
# away, which opens holes in a shell that is already barely manifold. Moving
# the offending vertices inward costs nothing -- they are under the cuff, so
# nobody sees where they end up -- and it changes no topology.
#
# Eased off over the last stretch below the rim, so the sleeve that emerges
# at the opening is not stepped where the clamp stops.
if tuck:
    for side, st in state.items():
        group = coat.vertex_groups.get("Sleeve_%s" % side)
        if group is None:
            continue
        gi = group.index
        axis, (a, b), fc = st["axis"], st["fit_r"], st["fit_c"]
        inv = mw.inverted()
        c0 = Vector([fc[k][0] for k in range(3)])
        moved, worst = 0, 0.0
        for v in coat.data.vertices:
            if not any(g.group == gi for g in v.groups):
                continue
            q = mw @ v.co
            up = (c0 - q).dot(axis)
            if up < -tuck_reach or up > span:
                continue
            u = max(0.0, min(span, up))
            # Below the fold the sleeve is not merely too wide, it is past
            # the end of the cuff, so raise it to the fold plane as well as
            # pulling it in. smooth_hems levels the hem perpendicular to the
            # BONE, while the fold is perpendicular to the straight sleeve
            # axis -- different planes, so some of the hem always lands
            # outside the roll, and a 6mm reach left a wedge of it showing.
            lift = axis * up if up < 0.0 else Vector((0.0, 0.0, 0.0))
            q = q + lift
            centre = Vector([fc[k][0] + fc[k][1] * u for k in range(3)])
            off = q - centre
            radial = off - axis * off.dot(axis)
            have = radial.length
            limit = (a + b * u) + gap - 0.0015
            if have <= limit or have < 1e-6:
                if lift.length > 1e-6:
                    v.co = inv @ q
                    moved += 1
                continue
            ease = min(1.0, (span - up) / 0.030)
            ease = ease * ease * (3.0 - 2.0 * ease)
            want = have + (limit - have) * ease
            worst = max(worst, have - limit)
            v.co = inv @ (q + radial * ((want - have) / have))
            moved += 1
        mhg.log("tucked %d sleeve verts inside Cuff_%s (worst was %.1fmm "
                "proud of the lining)" % (moved, side, worst * 1000))
    coat.data.update()

# The old cuff region was the sleeve's own hem roll, which the new cuff now
# covers. Left tagged it would colour cloth nobody can see, and show above
# the rim where the band does not reach.
if retag and cuff_slot is not None:
    n = 0
    for f in coat.data.polygons:
        if f.material_index == cuff_slot:
            f.material_index = coat_slot
            n += 1
    coat.data.update()
    mhg.log("retagged %d faces of the old hem roll back to the coat" % n)

if join and built:
    bpy.ops.object.select_all(action="DESELECT")
    for o in built:
        o.select_set(True)
    coat.select_set(True)
    bpy.context.view_layer.objects.active = coat
    bpy.ops.object.join()
    mhg.log("joined into the Costume: %d verts, %d faces"
            % (len(coat.data.vertices), len(coat.data.polygons)))

bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(body_blend))
mhg.log("saved %s" % body_blend)
