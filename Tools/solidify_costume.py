"""Give a panelled costume thickness, so its panels read as cloth.

    blender -b --python Tools/solidify_costume.py -- [--thickness 0.008]
        [--object Costume] [--body <blend>]

This costume is 266 separate OPEN panels, not a closed garment: 242 boundary
loops, the largest running 272 edges across the whole figure. Panels tuned to
sit edge-to-edge at one scale separate when the torso, chest, sleeves and
legs are widened by different factors, and the gaps read as holes. They are
not holes -- hole filling closed 56 of them and changed nothing visible.

Thickness closes them, because each panel grows by half the thickness on
each side and neighbours meet again. It also stops the panels reading as
paper.

THICKNESS IS IN METRES HERE AND IN LOCAL UNITS IN THE MODIFIER. This costume
carries the FBX's 0.01 scale, so 8mm asked for raw would apply as 0.08mm --
the same conversion _clear_of_body needs, and the same mistake that made
42mm of coat clearance 0.42mm.
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
obj_name = arg("--object", "Costume")
thickness = float(arg("--thickness", "0.008"))

bpy.ops.wm.open_mainfile(filepath=os.path.abspath(body_blend))
for o in bpy.data.objects:
    if o.type == "MESH" and o.mode != "OBJECT":
        bpy.context.view_layer.objects.active = o
        bpy.ops.object.mode_set(mode="OBJECT")

obj = bpy.data.objects.get(obj_name)
if obj is None:
    sys.exit("no object %r" % obj_name)

step = ((obj.matrix_world @ Vector((1.0, 0.0, 0.0)))
        - (obj.matrix_world @ Vector((0.0, 0.0, 0.0))))
per_metre = 1.0 / step.length if step.length > 1e-9 else 1.0
local = thickness * per_metre

before_f = len(obj.data.polygons)
before_v = len(obj.data.vertices)

mod = obj.modifiers.new("Solidify", "SOLIDIFY")
mod.thickness = local
mod.offset = 0.0                 # centred, so a panel grows both ways
# NOT even offset. It scales each vertex's displacement by 1/cos of the
# angle to its neighbours, which is fine on smooth cloth and explodes on a
# panelled mesh full of sharp edges: at 8mm it threw the costume from
# x +-0.42 out to x +-1.75 and z -1.27..2.95.
mod.use_even_offset = False
mod.use_rim = True
mod.use_rim_only = False

bpy.ops.object.select_all(action="DESELECT")
obj.select_set(True)
bpy.context.view_layer.objects.active = obj
bpy.ops.object.modifier_apply(modifier=mod.name)

mhg.log("%s solidified %.0fmm (%.4f local, scale 1 unit = %.0fmm)"
        % (obj_name, thickness * 1000, local, 1000.0 / per_metre))
mhg.log("   faces %d -> %d, verts %d -> %d"
        % (before_f, len(obj.data.polygons), before_v,
           len(obj.data.vertices)))

import bmesh
bm = bmesh.new()
bm.from_mesh(obj.data)
mhg.log("   boundary edges now %d"
        % len([e for e in bm.edges if e.is_boundary]))
bm.free()

pts = [obj.matrix_world @ v.co for v in obj.data.vertices]
mhg.log("   bounds x %.3f..%.3f  z %.3f..%.3f"
        % (min(p.x for p in pts), max(p.x for p in pts),
           min(p.z for p in pts), max(p.z for p in pts)))

bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(body_blend))
mhg.log("saved %s" % body_blend)
