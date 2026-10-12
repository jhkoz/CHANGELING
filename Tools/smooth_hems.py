"""Level the ragged edge at the end of each sleeve.

    blender -b --python Tools/smooth_hems.py -- \
        [--depth 30] [--rounds 14] [--level 0.9] [--round 0.7] [--cut 0.72]

The cuff hems zigzag: the lowest forty vertices of the left sleeve scatter
over 16mm of height, the right over 10mm, where they should sit on a ring.

They are ragged for a specific reason. straighten_sleeves eases its effect
off at both ends so the shoulder seam and the cuff stay put -- which means
the last fifth of each sleeve is exactly the part it never touched. The
wobble there is the sculpt's, untouched, next to a run of arm that has been
trued, so it reads worse than it did before.

Two things are wrong with the edge and they need different fixes, because
they are in perpendicular directions:

  level  how far along the arm each vertex sits. Measured as the reach of
         the hem at each angle around the sleeve, smoothed around the ring,
         and applied as a SHIFT. Relaxing these positions instead is the
         obvious thing and it is wrong: a Laplacian pulls the extremes toward
         the interior, so the hem retreats wherever it currently reaches
         furthest. That shortens the sleeve unevenly, which is a different
         fault, not a cure for this one.
  round  how far out from the axis it sits. Pulled toward the mean radius of
         the hem, so the ring is circular rather than lumpy.

Both are weighted to zero at the inner edge of the band, so this blends into
the trued sleeve above it instead of leaving a step.
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
depth = float(arg("--depth", "30")) / 1000.0
rounds = int(arg("--rounds", "14"))
level = float(arg("--level", "0.9"))
roundness = float(arg("--round", "0.7"))
cut = float(arg("--cut", "0.72"))

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

# Who touches whom. Built once; both sleeves read it.
nbr = [[] for _ in coat.data.vertices]
for e in coat.data.edges:
    a, b = e.vertices
    nbr[a].append(b)
    nbr[b].append(a)


def bone(name):
    b = rig.data.bones.get(name)
    return None if b is None else amw @ b.head_local


moved_total = 0
for side in ("L", "R"):
    group = coat.vertex_groups.get("Sleeve_%s" % side)
    if group is None:
        mhg.log("no Sleeve_%s group -- skipped" % side)
        continue
    gi = group.index
    mine = [v.index for v in coat.data.vertices
            if any(g.group == gi for g in v.groups)]
    low = side.lower()
    chain = [p for p in (bone("upperarm_%s" % low), bone("lowerarm_%s" % low),
                         bone("hand_%s" % low)) if p is not None]
    if len(chain) < 2 or not mine:
        continue

    # The hem is a small patch at the very end of the arm, so one straight
    # axis describes it -- the forearm's. No need for the whole chain here.
    axis = (chain[-1] - chain[-2]).normalized()
    origin = chain[-2]

    along = {i: (mw @ coat.data.vertices[i].co - origin).dot(axis)
             for i in mine}
    far = max(along.values())
    band = [i for i in mine if along[i] >= far - depth]
    if len(band) < 12:
        mhg.log("Sleeve_%s hem has only %d verts -- skipped" % (side, len(band)))
        continue
    inband = set(band)

    def weight(i):
        t = (along[i] - (far - depth)) / depth
        t = max(0.0, min(1.0, t))
        return t * t * (3.0 - 2.0 * t)

    centre = Vector((0.0, 0.0, 0.0))
    for i in band:
        centre += mw @ coat.data.vertices[i].co
    centre /= len(band)

    def radial_of(p):
        off = p - centre
        return off - axis * off.dot(axis)

    # An angle around the sleeve, so the hem can be described as a reach per
    # bearing and smoothed as the closed loop it is.
    ref = axis.orthogonal().normalized()
    ref2 = axis.cross(ref).normalized()
    slices = 24

    def bearing(i):
        rad = radial_of(mw @ coat.data.vertices[i].co)
        return (math.atan2(rad.dot(ref2), rad.dot(ref)) % (2.0 * math.pi)) \
            / (2.0 * math.pi) * slices

    # --- level: clamp what protrudes back to a smooth ring ---------------
    #
    # Shifting each angular slice by its own correction does not work: with
    # a hundred vertices over twenty-four bearings, most of the raggedness is
    # scatter BETWEEN neighbouring vertices, which a per-slice shift moves
    # bodily and leaves intact. It took the variation from 27.5mm to 24.4mm.
    #
    # What "ragged" actually names is vertices sticking out past where the
    # edge should be. So find where the edge should be -- a high percentile
    # of each slice, smoothed around the ring -- and pull back only what
    # overshoots it. One-sided on purpose: a spike is cloth that should not
    # be there, whereas a vertex sitting short of the ring is the hem turning
    # under, and dragging it out would open the fold.
    buckets = [[] for _ in range(slices)]
    for i in band:
        buckets[int(bearing(i)) % slices].append(along[i])
    reach = [None] * slices
    for a in range(slices):
        if buckets[a]:
            vals = sorted(buckets[a])
            reach[a] = vals[min(len(vals) - 1, int(len(vals) * cut))]
    known = [a for a in range(slices) if reach[a] is not None]
    for a in range(slices):
        if reach[a] is None:
            reach[a] = reach[min(known,
                                 key=lambda k: min(abs(k - a),
                                                   slices - abs(k - a)))]
    ring = list(reach)
    for _ in range(rounds):
        prev = list(ring)
        ring = [(prev[(a - 1) % slices] + prev[a] * 2.0
                 + prev[(a + 1) % slices]) * 0.25 for a in range(slices)]

    def ring_at(i):
        """The smooth edge, read at this vertex's bearing between bin centres."""
        u = bearing(i) - 0.5
        a = int(math.floor(u)) % slices
        b = (a + 1) % slices
        f = u - math.floor(u)
        return ring[a] + (ring[b] - ring[a]) * f

    val = {}
    for i in band:
        edge = ring_at(i)
        val[i] = (along[i] + (edge - along[i]) * level
                  if along[i] > edge else along[i])

    want = sum(radial_of(mw @ coat.data.vertices[i].co).length
               for i in band) / len(band)

    moved = 0
    for i in band:
        p = mw @ coat.data.vertices[i].co
        new = p + axis * (val[i] - along[i])
        rad = radial_of(new)
        have = rad.length
        if have > 1e-6:
            k = roundness * weight(i)
            new = new + rad * ((want - have) / have * k)
        if (new - p).length > 1e-6:
            coat.data.vertices[i].co = inv @ new
            moved += 1

    # Raggedness is how far a vertex stands PROUD of where the edge should
    # be, not how much the reach varies around the sleeve: a cuff sits
    # roughly level in the world while the arm slopes, so the edge is
    # genuinely further along the axis on one side than the other. Measure
    # against the smooth ring, which carries that slant, and only spikes
    # count.
    over = [along[i] - ring_at(i) for i in band if along[i] > ring_at(i)]
    now = [(mw @ coat.data.vertices[i].co - origin).dot(axis) - ring_at(i)
           for i in band]
    now = [d for d in now if d > 0.0]
    mhg.log("Sleeve_%s hem: %d of %d verts clamped to the ring over the last "
            "%.0fmm; %d spikes up to %.1fmm proud -> %d up to %.1fmm; "
            "ring radius %.0fmm"
            % (side, moved, len(band), depth * 1000,
               len(over), (max(over) if over else 0.0) * 1000,
               len(now), (max(now) if now else 0.0) * 1000, want * 1000))
    moved_total += moved

coat.data.update()
bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(body_blend))
mhg.log("saved %s (%d verts moved)" % (body_blend, moved_total))
