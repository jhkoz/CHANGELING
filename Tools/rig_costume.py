"""Rig a sculpted costume to the MetaHuman skeleton, across a pose mismatch.

    blender -b --python Tools/rig_costume.py -- --mesh <costume.obj> \
        [--body <body.blend>] [--out <out.blend>] [--scale N] [--drop 37]

THE PROBLEM. The costume is sculpted with its arms hanging at its sides; the
MetaHuman body is in A-pose, arms out 37 degrees from vertical. Bound as it
stands, every sleeve vertex finds a SPINE bone as its nearest, not an arm
bone, and the sleeves would never follow the arms once animated. Measured on
the Boggan childling: at 0.6 of its height the body needs 434mm of half-width
and the costume offers 190mm.

THE FIX, in the order it has to happen:

  1. copy the armature and drop its arms to match the costume
  2. make that the copy's REST pose, so a bind against it is a bind in the
     costume's own pose
  3. bind the costume to the copy -- now nearest-bone is the right bone
  4. pose the copy back to the real A-pose and apply the deformation, so the
     costume's geometry moves into A-pose carrying its weights
  5. bind the A-posed costume to the REAL armature, which is the one Unreal
     knows

Step 4 is the one that cannot be skipped. A mesh whose geometry is in one
pose and whose bind pose is another deforms wrongly the moment anything
moves it, and looks perfectly fine standing still.
"""
import bpy, os, sys, math
from mathutils import Matrix, Vector

sys.path.insert(0, "D:/CHANGELING/Tools")
import blender_metahuman_garment as mhg

argv = sys.argv
args = argv[argv.index("--") + 1:] if "--" in argv else []


def arg(flag, default=None):
    return args[args.index(flag) + 1] if flag in args else default


mesh_path = arg("--mesh")
body_blend = arg("--body", "Tools/Blender/Bodies/Boggan_Childling_Male.blend")
out_blend = arg("--out", "Tools/Blender/Outfits/costume_rigged.blend")
drop = math.radians(float(arg("--drop", "37")))
if not mesh_path:
    sys.exit(__doc__)

bpy.ops.wm.open_mainfile(filepath=os.path.abspath(body_blend))
body = bpy.data.objects.get("Body")
rig = mhg._armature_for(body)
if rig is None:
    sys.exit("no armature on the body")

rig_name = rig.name
bp = [body.matrix_world @ v.co for v in body.data.vertices]
bz0, bz1 = min(p.z for p in bp), max(p.z for p in bp)

known = set(o.name for o in bpy.data.objects)
bpy.ops.wm.obj_import(filepath=os.path.abspath(mesh_path))
coat = [o for o in bpy.data.objects
        if o.name not in known and o.type == "MESH"][0]
coat.name = "Costume"

cp = [coat.matrix_world @ v.co for v in coat.data.vertices]
cz0, cz1 = min(q.z for q in cp), max(q.z for q in cp)
manual = arg("--scale")
scale = float(manual) if manual else (bz1 - bz0) / (cz1 - cz0)
coat.scale = (scale, scale, scale)
bpy.context.view_layer.update()
cp = [coat.matrix_world @ v.co for v in coat.data.vertices]
coat.location.z += bz0 - min(q.z for q in cp)
bpy.context.view_layer.update()
# APPLY THE TRANSFORM BEFORE ANYTHING BINDS TO IT.
#
# An Armature modifier resolves against the mesh object's own matrix. Left
# carrying scale 1.571 and an offset while the armature sits at identity,
# the costume simply did not deform: the rig swung to A-pose, hand_l moved
# to x 0.377, and the evaluated mesh stayed at x +-0.211 exactly.
#
# export_garment already carries this warning for the same reason -- "the
# shells come out carrying scale 0.01 while the armature sits at 1.0, and
# the exporter writes bind-pose matrices from that mismatch".
# MATCH THE BODY'S OBJECT SPACE -- DO NOT FLATTEN TO IDENTITY.
#
# An Armature modifier resolves in the armature's space, and this armature
# carries the FBX's 0.01 scale: the body's evaluated mesh reads +-19.18 in
# local units for +-0.192 of world. A costume sitting at identity is in a
# space a hundred times larger, so the deform computes to nothing -- the rig
# swung to A-pose, hand_l moved to x 0.377, and the costume did not shift by
# a millimetre, through bind(), through data_transfer, through everything.
#
# So the mesh is baked INTO the body's local space and the object given the
# body's own matrix, rather than having its transform applied away.
target = body.matrix_world.copy()
coat.data.transform(target.inverted() @ coat.matrix_world)
# No parenting: an Armature modifier does not need it, and setting a parent
# AFTER matrix_world recomputes the matrix from it -- which put the costume
# back at a hundredth of its size, x +-0.002.
coat.matrix_world = target
bpy.context.view_layer.update()
mhg.log("costume scaled x%.3f and moved into the body's object space" % scale)

# --- 1. a BODY in the costume's pose, to take weights from ---------------
#
# NOT bone heat. parent_set(ARMATURE_AUTO) was tried first and shredded the
# mesh: on a costume whose sleeves hang against the torso, bone heat cannot
# tell sleeve from body, so it splits those vertices across many bones and
# the 37-degree swing tears them into shards.
#
# The BODY is the map that knows which bone owns what. Posed to match the
# costume, its weights transfer by proximity and every sleeve vertex finds
# an arm bone because an arm is what is actually next to it.
def swing(rig_obj, radians):
    bpy.context.view_layer.objects.active = rig_obj
    bpy.ops.object.mode_set(mode="POSE")
    for side, sign in (("l", 1.0), ("r", -1.0)):
        pb = rig_obj.pose.bones.get("upperarm_%s" % side)
        if pb is None:
            continue
        turn = Matrix.Rotation(radians * sign, 4, "Y")
        pivot = rig_obj.matrix_world @ pb.bone.head_local
        here = rig_obj.matrix_world @ pb.matrix
        moved = (Matrix.Translation(pivot) @ turn
                 @ Matrix.Translation(-pivot) @ here)
        pb.matrix = rig_obj.matrix_world.inverted() @ moved
        bpy.context.view_layer.update()
    bpy.ops.object.mode_set(mode="OBJECT")


swing(rig, drop)
bpy.ops.object.select_all(action="DESELECT")
body.select_set(True)
bpy.context.view_layer.objects.active = body
bpy.ops.object.duplicate()
down = bpy.context.view_layer.objects.active
down.name = "Body_CostumePose"
for mod in [m for m in down.modifiers if m.type == "ARMATURE"]:
    bpy.ops.object.modifier_apply(modifier=mod.name)
down.parent = None
swing(rig, -drop)
mhg.log("body baked in the costume's pose as %s" % down.name)

# --- 2. weights, by proximity, from that body ----------------------------
bpy.ops.object.select_all(action="DESELECT")
down.select_set(True)
coat.select_set(True)
bpy.context.view_layer.objects.active = down
bpy.ops.object.data_transfer(
    data_type="VGROUP_WEIGHTS", use_create=True,
    vert_mapping="POLYINTERP_NEAREST", layers_select_src="ALL",
    layers_select_dst="NAME", mix_mode="REPLACE")
mhg.log("weights transferred: %d vertex group(s) on the costume"
        % len(coat.vertex_groups))
_named = {g.name: g.index for g in coat.vertex_groups}
_weighted = sum(1 for v in coat.data.vertices
                if any(g.weight > 0.0 for g in v.groups))
mhg.log("   verts carrying ANY weight: %d of %d"
        % (_weighted, len(coat.data.vertices)))
for _b in ("upperarm_l", "lowerarm_l", "hand_l", "spine_04"):
    _i = _named.get(_b)
    if _i is None:
        mhg.log("   %-11s NO GROUP" % _b); continue
    _n = sum(1 for v in coat.data.vertices
             for g in v.groups if g.group == _i and g.weight > 0.01)
    mhg.log("   %-11s %d vert(s) weighted" % (_b, _n))

# --- 3. a rig whose REST is the costume's pose ---------------------------
bpy.ops.object.select_all(action="DESELECT")
rig.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.object.duplicate()
posed = bpy.context.view_layer.objects.active
posed.name = "Rig_CostumePose"
swing(posed, drop)
bpy.context.view_layer.objects.active = posed
bpy.ops.object.mode_set(mode="POSE")
bpy.ops.pose.armature_apply()
bpy.ops.object.mode_set(mode="OBJECT")

# Deform only -- the weights are already right and must not be replaced.
mod = coat.modifiers.new("ToPose", "ARMATURE")
mod.object = posed
mhg.log("   [before swing] costume x %.3f..%.3f" % (
    min((coat.matrix_world @ v.co).x for v in coat.data.vertices),
    max((coat.matrix_world @ v.co).x for v in coat.data.vertices)))


# --- 4. swing that rig to the real A-pose and bake it in -----------------
swing(posed, -drop)
mhg.log("   posed rig hand_l now x %.3f"
        % (posed.matrix_world @ posed.pose.bones["hand_l"].matrix.translation).x)
_dg = bpy.context.evaluated_depsgraph_get()
_ev = coat.evaluated_get(_dg).to_mesh()
mhg.log("   [evaluated] costume x %.3f..%.3f"
        % (min((coat.matrix_world @ v.co).x for v in _ev.vertices),
           max((coat.matrix_world @ v.co).x for v in _ev.vertices)))
bpy.ops.object.select_all(action="DESELECT")
coat.select_set(True)
bpy.context.view_layer.objects.active = coat
bpy.ops.object.modifier_apply(modifier=mod.name)
coat.parent = None
mhg.log("costume baked into A-pose")

coat_name = coat.name
bpy.data.objects.remove(posed, do_unlink=True)
bpy.data.objects.remove(down, do_unlink=True)
coat = bpy.data.objects[coat_name]
body = bpy.data.objects["Body"]

cp = [coat.matrix_world @ v.co for v in coat.data.vertices]
mhg.log("costume now x %.3f..%.3f  z %.3f..%.3f"
        % (min(q.x for q in cp), max(q.x for q in cp),
           min(q.z for q in cp), max(q.z for q in cp)))

bpy.data.objects.remove(posed, do_unlink=True)

# --- 5. bind to the REAL rig, the one Unreal knows -----------------------
# Keep the transferred weights; just hang it on the real rig.
rig = mhg._armature_for(body) or bpy.data.objects.get(rig_name)
if not any(m.type == "ARMATURE" for m in coat.modifiers):
    final = coat.modifiers.new("Armature", "ARMATURE")
    final.object = rig
mhg.base_weights(dress=coat, bone="pelvis")

bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(out_blend))
mhg.log("saved %s" % out_blend)
