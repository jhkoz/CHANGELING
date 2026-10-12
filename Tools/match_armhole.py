"""Put the coat's shoulder and armpit where the body's are.

    blender -b --python Tools/match_armhole.py -- \
        [--mesh Costume] [--target Body] \
        [--reach 130] [--ease 60] [--margin 70]

Measured at the shoulder joint, the coat sits low and hangs deep:

    shoulder z    armpit z          armhole depth
    Body       1.1340 / 1.1342   1.0810 / 1.0807      53mm
    Costume    1.1173 / 1.1216   1.0243 / 1.0095      93mm / 112mm

The shoulder is 17 to 21mm low, which is a lot, but the armpit is 57 to
70mm low, which is the fault you actually see: the armhole is nearly twice
as deep as the body's, so the sleeve hangs off a narrow yoke instead of
sitting on the shoulder. The two sides do not agree with each other either,
by 4mm at the shoulder and 15mm at the armpit.

Rather than lift the region bodily, which would keep the droop and just move
it, this remaps height across the armhole: whatever is at the coat's armpit
goes to the body's armpit, whatever is at the coat's shoulder goes to the
body's shoulder, and everything between is scaled to suit. Matching both
ends to the body also brings the left and right into line with each other,
since they are being sent to the same place rather than to each other.

The remap is bounded twice over. Radially it fades out with distance from
the joint, so the chest and the rest of the sleeve are left alone. And
vertically it fades out below the armpit -- without that, the scaling runs
away down the torso and a vertex 100mm below the armpit gets lifted 100mm.
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
which = arg("--mesh", "Costume")
target_name = arg("--target", "Body")
reach = float(arg("--reach", "130")) / 1000.0
ease = float(arg("--ease", "60")) / 1000.0
margin = float(arg("--margin", "70")) / 1000.0

bpy.ops.wm.open_mainfile(filepath=os.path.abspath(body_blend))
for o in bpy.data.objects:
    if o.type == "MESH" and o.mode != "OBJECT":
        bpy.context.view_layer.objects.active = o
        bpy.ops.object.mode_set(mode="OBJECT")

coat = bpy.data.objects[which]
body = bpy.data.objects[target_name]
rig = mhg._armature_for(body) or next(
    o for o in bpy.data.objects if o.type == "ARMATURE" and "Body" in o.name)
amw = rig.matrix_world


def bone(n):
    b = rig.data.bones.get(n)
    return None if b is None else amw @ b.head_local


def smoothstep(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3.0 - 2.0 * t)


def heights(obj, joint, run, lo=0.010, hi=0.060, bound=0.075):
    """Shoulder and armpit height, by ONE definition for every mesh.

    Both are taken from the collar of geometry just OUTBOARD of the joint --
    the top of the sleeve and its underside. The obvious alternative, the
    lowest vertex inboard of the joint, is what this first used and it is
    not comparable between meshes: on the body that finds a clean armpit,
    24 vertices of it, but on the coat it finds the side panel, 119 vertices
    hanging well below the armhole. Measured that way the coat's armhole
    looked 93mm deep against the body's 53mm and the fix looked like a
    remap. Measured the same way on both, the depths are 67mm and 68mm --
    the armhole is the right shape and is simply sitting too low.
    """
    mw = obj.matrix_world
    sel = []
    for v in obj.data.vertices:
        p = mw @ v.co
        off = p - joint
        a = off.dot(run)
        if not (lo <= a <= hi):
            continue
        if (off - run * a).length > bound:
            continue
        sel.append(p.z)
    if len(sel) < 12:
        return None
    sel.sort()
    return sel[-1], sel[int(len(sel) * 0.02)]


mw = coat.matrix_world
inv = mw.inverted()
lift = {}

for side in ("l", "r"):
    up = bone("upperarm_%s" % side)
    lo = bone("lowerarm_%s" % side)
    if up is None or lo is None:
        continue
    run = (lo - up).normalized()

    want = heights(body, up, run)
    have = heights(coat, up, run)
    if want is None or have is None:
        mhg.log("%s: cannot measure the %s joint -- skipped" % (which, side))
        continue
    shoulder_t, armpit_t = want
    shoulder_c, armpit_c = have
    span = shoulder_c - armpit_c
    if span < 0.005:
        mhg.log("%s: %s armhole is only %.0fmm deep -- skipped"
                % (which, side, span * 1000))
        continue
    scale = (shoulder_t - armpit_t) / span

    mhg.log("%s %s: shoulder %.4f -> %.4f (%+.0fmm), armpit %.4f -> %.4f "
            "(%+.0fmm); armhole %.0fmm -> %.0fmm"
            % (which, side.upper(), shoulder_c, shoulder_t,
               (shoulder_t - shoulder_c) * 1000, armpit_c, armpit_t,
               (armpit_t - armpit_c) * 1000, span * 1000,
               (shoulder_t - armpit_t) * 1000))

    for v in coat.data.vertices:
        p = mw @ v.co
        gap = (p - up).length
        if gap > reach + ease:
            continue
        near = 1.0 - smoothstep((gap - reach) / ease) if gap > reach else 1.0

        # Vertically: full across the armhole, fading out below the armpit
        # and above the shoulder, so the scaling cannot run away down the
        # torso.
        if p.z < armpit_c:
            deep = 1.0 - smoothstep((armpit_c - p.z) / margin)
        elif p.z > shoulder_c:
            deep = 1.0 - smoothstep((p.z - shoulder_c) / margin)
        else:
            deep = 1.0
        k = near * deep
        if k <= 1e-3:
            continue

        aim = armpit_t + (p.z - armpit_c) * scale
        delta = (aim - p.z) * k
        if abs(delta) > 1e-6:
            lift[v.index] = lift.get(v.index, 0.0) + delta

for i, dz in lift.items():
    v = coat.data.vertices[i]
    v.co = inv @ ((mw @ v.co) + Vector((0.0, 0.0, dz)))
coat.data.update()

moved = sorted(abs(d) for d in lift.values())
mhg.log("%s: %d verts moved, median %.1fmm, most %.1fmm"
        % (which, len(moved), moved[len(moved) // 2] * 1000,
           moved[-1] * 1000) if moved else "%s: nothing moved" % which)

bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(body_blend))
mhg.log("saved %s" % body_blend)
