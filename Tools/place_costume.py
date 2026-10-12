"""Scale and position a costume mesh over the skeleton, in the body file.

    blender -b --python Tools/place_costume.py -- --mesh <costume.obj> \
        [--body <body.blend>] [--scale N] [--lift N] [--out <blend>]

Fits to the RIG'S OWN LANDMARKS, not to bounding boxes. The body's box runs
sole to shoulder and the costume's runs sole to collar, so matching them
stretches the garment by the height of a neck. foot and neck_01 are points
both meshes genuinely share.

Does not rig anything. The costume is placed and left unparented, in the
body's own object space so that a later bind has nothing to correct: that
armature carries the FBX's 0.01 scale, and a mesh sitting at identity beside
it is in a space a hundred times larger.
"""
import bpy, os, sys
from mathutils import Matrix

sys.path.insert(0, "D:/CHANGELING/Tools")
import blender_metahuman_garment as mhg

argv = sys.argv
args = argv[argv.index("--") + 1:] if "--" in argv else []


def arg(flag, default=None):
    return args[args.index(flag) + 1] if flag in args else default


mesh_path = arg("--mesh")
body_blend = arg("--body", "Tools/Blender/Bodies/Boggan_Childling_Male.blend")
out_blend = arg("--out", body_blend)
if not mesh_path:
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
body = bpy.data.objects.get("Body")
rig = mhg._armature_for(body)

# drop any earlier attempt so re-running is clean
for stale in [o for o in bpy.data.objects if o.name.startswith("Costume")]:
    bpy.data.objects.remove(stale, do_unlink=True)

bp = [body.matrix_world @ v.co for v in body.data.vertices]
sole = min(p.z for p in bp)
amw = rig.matrix_world
neck = (amw @ rig.data.bones["neck_01"].head_local).z
shoulder = (amw @ rig.data.bones["upperarm_l"].head_local)
mhg.log("rig: sole %.3f, neck_01 %.3f, shoulder (%.3f, %.3f, %.3f)"
        % (sole, neck, shoulder.x, shoulder.y, shoulder.z))

known = set(o.name for o in bpy.data.objects)
ext = os.path.splitext(mesh_path)[1].lower()
if ext in (".gltf", ".glb"):
    bpy.ops.import_scene.gltf(filepath=os.path.abspath(mesh_path))
elif ext == ".fbx":
    bpy.ops.import_scene.fbx(filepath=os.path.abspath(mesh_path))
elif ext == ".obj":
    bpy.ops.wm.obj_import(filepath=os.path.abspath(mesh_path))
else:
    sys.exit("unsupported mesh format: %s" % ext)
mhg.log("imported %s" % os.path.basename(mesh_path))
fresh = [o for o in bpy.data.objects
         if o.name not in known and o.type == "MESH"]
if not fresh:
    sys.exit("nothing imported from %s" % mesh_path)
coat = fresh[0]
coat.name = "Costume"
# BAKE THE IMPORT TRANSFORM FIRST.
#
# glTF arrives parented to a root empty carrying the Y-up to Z-up rotation.
# Removing that empty before measuring silently changed the mesh's world
# transform: the costume scaled to a height of 1.119 and then measured
# 1.598, because it lost the parent it had been measured through.
# CAPTURE, UNPARENT, ZERO, THEN BAKE -- in that order.
#
# Setting matrix_world while the object still has a parent makes Blender
# solve for the basis instead, so clearing the parent afterwards leaves the
# compensation behind: this read objscale 1.4286 where identity was asked
# for, and the costume came out a factor of 0.7 short.
mhg.log("import transform: scale %.3f" % coat.matrix_world.to_scale().x)
world = coat.matrix_world.copy()
coat.parent = None
coat.matrix_world = Matrix.Identity(4)
coat.data.transform(world)
bpy.context.view_layer.update()
mhg.log("   [after bake] world z %.3f..%.3f  objscale %.4f" % (
    min((coat.matrix_world @ v.co).z for v in coat.data.vertices),
    max((coat.matrix_world @ v.co).z for v in coat.data.vertices),
    coat.matrix_world.to_scale().x))

for junk in [o for o in bpy.data.objects
             if o.name not in known and o.type in ("EMPTY", "ARMATURE")
             and o is not coat]:
    bpy.data.objects.remove(junk, do_unlink=True)
if len(fresh) > 1:
    bpy.ops.object.select_all(action="DESELECT")
    for o in fresh:
        o.select_set(True)
    bpy.context.view_layer.objects.active = coat
    bpy.ops.object.join()

cp = [coat.matrix_world @ v.co for v in coat.data.vertices]
cz0, cz1 = min(q.z for q in cp), max(q.z for q in cp)

# Sole to collar on the costume, sole to neck on the rig.
manual = arg("--scale")
scale = float(manual) if manual else (neck - sole) / (cz1 - cz0)
coat.scale = (scale, scale, scale)
bpy.context.view_layer.update()
mhg.log("   [after scale] world z %.3f..%.3f  objscale %.4f" % (
    min((coat.matrix_world @ v.co).z for v in coat.data.vertices),
    max((coat.matrix_world @ v.co).z for v in coat.data.vertices),
    coat.matrix_world.to_scale().x))

cp = [coat.matrix_world @ v.co for v in coat.data.vertices]
coat.location.z += sole - min(q.z for q in cp)
lift = arg("--lift")
if lift:
    coat.location.z += float(lift)
bpy.context.view_layer.update()

# Into the body's object space, so a later bind has nothing to correct.
target = body.matrix_world.copy()
coat.data.transform(target.inverted() @ coat.matrix_world)
coat.matrix_world = target
bpy.context.view_layer.update()
mhg.log("   [after space] world z %.3f..%.3f  objscale %.4f" % (
    min((coat.matrix_world @ v.co).z for v in coat.data.vertices),
    max((coat.matrix_world @ v.co).z for v in coat.data.vertices),
    coat.matrix_world.to_scale().x))

cp = [coat.matrix_world @ v.co for v in coat.data.vertices]
mhg.log("costume scaled x%.3f" % scale)
mhg.log("   x %.3f..%.3f   y %.3f..%.3f   z %.3f..%.3f"
        % (min(q.x for q in cp), max(q.x for q in cp),
           min(q.y for q in cp), max(q.y for q in cp),
           min(q.z for q in cp), max(q.z for q in cp)))
mhg.log("   body   z %.3f..%.3f, neck at %.3f" % (sole, max(p.z for p in bp), neck))

# How it sits against the rig, by height, so the fit can be judged on numbers.
mhg.log("half-width, body against costume:")
top = max(p.z for p in bp)
for i in range(11):
    f = i / 10.0
    z = sole + (top - sole) * f
    bb = [p for p in bp if abs(p.z - z) < 0.02]
    cc = [q for q in cp if abs(q.z - z) < 0.02]
    if not bb or not cc:
        continue
    mhg.log("   %.1f  z=%.3f   body %.3f   costume %.3f"
            % (f, z, max(abs(p.x) for p in bb), max(abs(q.x) for q in cc)))

bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(out_blend))
mhg.log("saved %s" % out_blend)
