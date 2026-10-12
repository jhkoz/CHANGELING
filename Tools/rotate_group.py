"""Rotate a named vertex group about a bone, by a measured angle.

    blender -b --python Tools/rotate_group.py -- --group Sleeve_L \
        [--bone upperarm_l] [--angle auto] [--axis Y] [--body <blend>]

WHY THIS EXISTS. Picking the sleeve out of this costume automatically does
not work: it is 266 unlabelled islands, and of the 54 sitting near the left
arm most are 3-62 vertex collar ornaments, not sleeve. Three attempts to
select it by geometry damaged the mesh three different ways.

A person can see which pieces are the sleeve in a second. What a person
should not have to do is get the pivot and the angle right by eye -- a
rotation about the wrong point swings the whole garment.

So: select the sleeve in Blender, assign it to a vertex group, and this
rotates exactly that group about the bone's head, by the angle that makes
the sleeve match the arm.

DO NOT PRESS Ctrl+L. The coat's panels run from the skirt hem to the
shoulder, so Select Linked from a sleeve vertex takes the whole skirt with
it. Box-select with X-ray on (Alt+Z) and stop there; this script eases the
rotation off at the selection's edge, so a panel cut part way bends rather
than creasing.
"""
import bpy, os, sys, math
from mathutils import Matrix, Vector

sys.path.insert(0, "D:/CHANGELING/Tools")
import blender_metahuman_garment as mhg

argv = sys.argv
args = argv[argv.index("--") + 1:] if "--" in argv else []


def arg(flag, default=None):
    return args[args.index(flag) + 1] if flag in args else default


body_blend = arg("--body", "Tools/Blender/Bodies/Boggan_Childling_Male.blend")
group_name = arg("--group")
bone_name = arg("--bone", "upperarm_l")
axis = arg("--axis", "Y")
want_angle = arg("--angle", "auto")
obj_name = arg("--object", "Costume")
if not group_name:
    sys.exit(__doc__)


def _leave_editmode():
    """Drop out of Edit Mode before touching geometry.

    A mesh saved in Edit Mode keeps a separate edit-mesh, and on save that
    overwrites the mesh datablock -- so every vertex edit is silently thrown
    away on write, while object-level changes survive. That cost a sleeve
    rotation and two scalings before bmesh refused outright with
    "Mesh 'Mesh_0' is in editmode".
    """
    for obj in bpy.data.objects:
        if obj.type == "MESH" and obj.mode != "OBJECT":
            bpy.context.view_layer.objects.active = obj
            bpy.ops.object.mode_set(mode="OBJECT")
    if bpy.context.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")


bpy.ops.wm.open_mainfile(filepath=os.path.abspath(body_blend))
_leave_editmode()
obj = bpy.data.objects.get(obj_name)
body = bpy.data.objects.get("Body")
rig = mhg._armature_for(body) or next(
    o for o in bpy.data.objects if o.type == "ARMATURE" and "Body" in o.name)

group = obj.vertex_groups.get(group_name)
if group is None:
    sys.exit("no vertex group %r on %s. Groups: %s"
             % (group_name, obj_name,
                [g.name for g in obj.vertex_groups]))

amw = rig.matrix_world
pivot = amw @ rig.data.bones[bone_name].head_local
mhg.log("pivot: %s head at (%.3f, %.3f, %.3f)"
        % (bone_name, pivot.x, pivot.y, pivot.z))

picked = [v for v in obj.data.vertices
          if any(g.group == group.index and g.weight > 0.0 for g in v.groups)]
if not picked:
    sys.exit("vertex group %r is empty" % group_name)
# GROW THE GROUP FIRST.
#
# Geometry added after the group was made -- the cuff hem, here -- carries
# no weight in it, so rotating the group alone would swing the sleeve and
# leave its hem standing where it was. Growing through connectivity pulls
# those attached rings in at full weight.
grow = int(arg("--grow", "3"))
if grow:
    near = {}
    for edge in obj.data.edges:
        a, b = edge.vertices
        near.setdefault(a, set()).add(b)
        near.setdefault(b, set()).add(a)
    inside = {v.index for v in picked}
    for _ in range(grow):
        ring = set()
        for i in inside:
            ring |= near.get(i, set())
        inside |= ring
    added = len(inside) - len(picked)
    picked = [obj.data.vertices[i] for i in sorted(inside)]
    for i in sorted(inside):
        group.add([i], 1.0, "REPLACE")
    mhg.log("grown %d ring(s): %d vert(s) added, %d total"
            % (grow, added, len(picked)))

mw = obj.matrix_world
inv = mw.inverted()
pts = [mw @ v.co for v in picked]
mhg.log("group %r: %d vert(s), z %.3f..%.3f, x %.3f..%.3f"
        % (group_name, len(picked), min(p.z for p in pts),
           max(p.z for p in pts), min(p.x for p in pts), max(p.x for p in pts)))

# The angle the arm wants, read from the rig rather than guessed.
child = {"upperarm_l": "lowerarm_l", "upperarm_r": "lowerarm_r",
         "lowerarm_l": "hand_l", "lowerarm_r": "hand_r"}.get(bone_name)
if want_angle == "auto" and child:
    end = amw @ rig.data.bones[child].head_local
    arm_deg = math.degrees(math.atan2(
        math.hypot(end.x - pivot.x, end.y - pivot.y), abs(pivot.z - end.z)))
    hi = [p for p in pts if p.z > (min(p.z for p in pts)
                                   + max(p.z for p in pts)) * 0.5]
    lo = [p for p in pts if p not in hi]
    if len(hi) >= 3 and len(lo) >= 3:
        hx = sum(abs(p.x) for p in hi) / len(hi)
        lx = sum(abs(p.x) for p in lo) / len(lo)
        hz = sum(p.z for p in hi) / len(hi)
        lz = sum(p.z for p in lo) / len(lo)
        have = math.degrees(math.atan2(abs(lx - hx), abs(hz - lz)))
    else:
        have = 0.0
    turn = arm_deg - have
    mhg.log("arm %.1f deg, group %.1f deg -> rotating %.1f deg"
            % (arm_deg, have, turn))
else:
    turn = float(want_angle)
    mhg.log("rotating %.1f deg as asked" % turn)

# BLEND THE EDGE OF THE SELECTION.
#
# This coat is not made of small plates: its largest islands are PANELS that
# run the full height -- 766 verts from z 0.255 to 1.006, holding 361 skirt
# vertices and 7 sleeve ones. A sleeve vertex and a skirt vertex share a
# panel, so a selection of the sleeve always cuts panels part way, and
# rotating that selection rigidly would crease every one of them at the cut.
#
# The group's weights are smoothed over the mesh first, and each vertex
# turns by angle * weight. The selection's core rotates fully, its edge eases
# off, and the panels bend instead of breaking.
weight = {}
for v in obj.data.vertices:
    weight[v.index] = 0.0
for v in picked:
    for g in v.groups:
        if g.group == group.index:
            weight[v.index] = max(weight[v.index], g.weight)

neighbours = {}
for edge in obj.data.edges:
    a, b = edge.vertices
    neighbours.setdefault(a, []).append(b)
    neighbours.setdefault(b, []).append(a)

rounds = int(arg("--blend", "6"))
for _ in range(rounds):
    nxt = dict(weight)
    for index, near in neighbours.items():
        if not near:
            continue
        mean = sum(weight[n] for n in near) / float(len(near))
        nxt[index] = weight[index] * 0.5 + mean * 0.5
    weight = nxt

sign = 1.0 if pivot.x >= 0.0 else -1.0
touched = 0
for v in obj.data.vertices:
    w = weight.get(v.index, 0.0)
    if w <= 0.001:
        continue
    rot = Matrix.Rotation(math.radians(-turn) * sign * w, 4, axis)
    world = mw @ v.co
    v.co = inv @ (pivot + rot @ (world - pivot))
    touched += 1
obj.data.update()
mhg.log("rotated %d vert(s) (%d fully selected, rest eased over %d blend pass(es))"
        % (touched, len(picked), rounds))

pts = [mw @ v.co for v in picked]
mhg.log("after: z %.3f..%.3f, x %.3f..%.3f"
        % (min(p.z for p in pts), max(p.z for p in pts),
           min(p.x for p in pts), max(p.x for p in pts)))
bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(body_blend))
mhg.log("saved %s" % body_blend)
