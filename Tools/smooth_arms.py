"""Take the crumple out of the sleeves without shrinking them.

    blender -b --python Tools/smooth_arms.py -- \
        [--rounds 12] [--lam 0.50] [--mu 0.53] [--grow 2] [--ease 3]
        [--spare Lapel,Trim,Cuff]

The sleeve surface is rough at about 7mm even where it has been trued, and
16mm over the shoulder cap with single vertices 81mm out of place. Those are
two different faults with two different causes.

The 7mm is arithmetic: straighten_sleeves ran at 0.9 straightness and 0.9
roundness, so it moves a vertex at most 81% of the way onto the tube and
leaves 19% of whatever it started with. Running it at 1.0 fixes that, and
costs nothing, because putting every vertex at the right radius is the whole
job there.

The shoulder cap is not arithmetic. It falls inside the ease that keeps the
truing away from the seam, and that ease has to stay -- force the cap onto a
cylinder and it tears off the coat it is sewn to. What it needs is the
crumple taken out while it keeps its shape, which is what smoothing is for.

Taubin smoothing rather than plain Laplacian: a Laplacian pulls every convex
surface inward, so twelve rounds of it would take millimetres off a sleeve
whose width was just set deliberately. Taubin follows each inward pass with
a smaller outward one, tuned so the high frequencies keep cancelling and the
low ones come back.
"""
import bpy, os, sys
from mathutils import Vector

sys.path.insert(0, "D:/CHANGELING/Tools")
import blender_metahuman_garment as mhg

argv = sys.argv
args = argv[argv.index("--") + 1:] if "--" in argv else []


def arg(flag, default=None):
    return args[args.index(flag) + 1] if flag in args else default


body_blend = arg("--body", "Tools/Blender/Bodies/Boggan_Childling_Male.blend")
rounds = int(arg("--rounds", "12"))
lam = float(arg("--lam", "0.50"))
mu = -float(arg("--mu", "0.53"))
grow = int(arg("--grow", "2"))
ease = int(arg("--ease", "3"))
groups = arg("--groups", "Sleeve_L,Sleeve_R").split(",")
spare = [t for t in arg("--spare", "Lapel,Trim,Cuff").split(",") if t]

bpy.ops.wm.open_mainfile(filepath=os.path.abspath(body_blend))
for o in bpy.data.objects:
    if o.type == "MESH" and o.mode != "OBJECT":
        bpy.context.view_layer.objects.active = o
        bpy.ops.object.mode_set(mode="OBJECT")

coat = bpy.data.objects["Costume"]
mw = coat.matrix_world
inv = mw.inverted()

nbr = [[] for _ in coat.data.vertices]
for e in coat.data.edges:
    a, b = e.vertices
    nbr[a].append(b)
    nbr[b].append(a)

want = set()
for name in groups:
    g = coat.vertex_groups.get(name.strip())
    if g is None:
        mhg.log("no group %r" % name)
        continue
    want.update(v.index for v in coat.data.vertices
                if any(x.group == g.index for x in v.groups))
if not want:
    sys.exit("no sleeve vertices")

# Leave the tailored parts out of it. Growing into the shoulder also reaches
# the collar and the lapels, and they are meant to be crisp -- smoothed with
# the sleeve they go soft and the coat loses the one edge it has.
slots = [m.name if m else "" for m in coat.data.materials]
keep = {i for i, n in enumerate(slots) if any(t in n for t in spare)}
if keep:
    sharp = set()
    for f in coat.data.polygons:
        if f.material_index in keep:
            sharp.update(f.vertices)
    want -= sharp
    mhg.log("sparing %d verts of %s"
            % (len(sharp), ", ".join(slots[i] for i in sorted(keep))))
else:
    sharp = set()

# Reach a little past the group, so the shoulder the sleeve is sewn into is
# smoothed with it rather than left as a rough collar around a clean tube.
region = set(want)
for _ in range(max(0, grow)):
    region |= {j for i in region for j in nbr[i] if j not in sharp}

# Hold the edge of the region still and let the strength come up over a few
# rings, or the smoothing leaves a visible ridge where it stops.
depth = {i: 0 for i in region if any(j not in region for j in nbr[i])}
front = list(depth)
while front:
    nxt = []
    for i in front:
        for j in nbr[i]:
            if j in region and j not in depth:
                depth[j] = depth[i] + 1
                nxt.append(j)
    front = nxt
strength = {}
for i in region:
    d = depth.get(i, ease)
    t = min(1.0, d / float(max(1, ease)))
    strength[i] = t * t * (3.0 - 2.0 * t)

pos = {i: mw @ coat.data.vertices[i].co for i in region}
start = dict(pos)


def pass_over(factor):
    moved = {}
    for i in region:
        near = [pos[j] for j in nbr[i] if j in pos]
        if not near:
            continue
        mean = Vector((0.0, 0.0, 0.0))
        for q in near:
            mean += q
        mean /= len(near)
        moved[i] = pos[i] + (mean - pos[i]) * (factor * strength[i])
    pos.update(moved)


for _ in range(rounds):
    pass_over(lam)
    pass_over(mu)

shift = [(pos[i] - start[i]).length for i in region]
shift.sort()
for i in region:
    coat.data.vertices[i].co = inv @ pos[i]
coat.data.update()

mhg.log("smoothed %d verts (%d in the sleeves, %d grown into the shoulder); "
        "moved median %.1fmm, most %.1fmm"
        % (len(region), len(want), len(region) - len(want),
           shift[len(shift) // 2] * 1000, shift[-1] * 1000))

bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(body_blend))
mhg.log("saved %s" % body_blend)
