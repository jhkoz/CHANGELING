"""Fit a sculpted costume mesh onto a MetaHuman body.

    blender -b --python Tools/fit_costume.py -- \
        --body Tools/Blender/Bodies/Boggan_Childling_Male.blend \
        --mesh <costume.obj> [--out <out.blend>]

Aligns by the part of the figure that actually corresponds -- the legs and
hips -- rather than by overall height, because the costume's top is a collar
and the body's top is a pair of shoulders, and matching those stretches the
whole garment.

Reports what does not fit rather than hiding it.
"""
import bpy, os, sys

sys.path.insert(0, "D:/CHANGELING/Tools")
import blender_metahuman_garment as mhg

argv = sys.argv
args = argv[argv.index("--") + 1:] if "--" in argv else []


def arg(flag, default=None):
    return args[args.index(flag) + 1] if flag in args else default


body_blend = arg("--body", "Tools/Blender/Bodies/Boggan_Childling_Male.blend")
mesh_path = arg("--mesh")
out_blend = arg("--out", "Tools/Blender/Outfits/costume_fitted.blend")
if not mesh_path:
    sys.exit(__doc__)

bpy.ops.wm.open_mainfile(filepath=os.path.abspath(body_blend))
body = bpy.data.objects.get("Body")
mw = body.matrix_world
bp = [mw @ v.co for v in body.data.vertices]
bz0, bz1 = min(p.z for p in bp), max(p.z for p in bp)

known = set(o.name for o in bpy.data.objects)
bpy.ops.wm.obj_import(filepath=os.path.abspath(mesh_path))
fresh = [o for o in bpy.data.objects
         if o.name not in known and o.type == "MESH"]
if not fresh:
    sys.exit("no mesh imported from %s" % mesh_path)
coat = fresh[0]
coat.name = "Costume"

cp = [coat.matrix_world @ v.co for v in coat.data.vertices]
cz0, cz1 = min(q.z for q in cp), max(q.z for q in cp)

# SCALE BY HEIGHT, AND SAY SO.
#
# Finding the costume's hip by looking for the narrowest slice picked a
# fold in the skirt instead and scaled it x2.415, standing 1.726m against a
# 1.136m body. Overall height is cruder but it is a landmark both meshes
# genuinely share: the costume runs sole to collar, the body sole to
# shoulder, and those are within a neck of each other.
#
# --scale overrides it, because the right number here is a judgement about
# how the collar should sit, not something the bounding boxes can settle.
manual = arg("--scale")
scale = float(manual) if manual else (bz1 - bz0) / (cz1 - cz0)
coat.scale = (scale, scale, scale)
bpy.context.view_layer.update()
cp = [coat.matrix_world @ v.co for v in coat.data.vertices]
coat.location.z += bz0 - min(q.z for q in cp)
bpy.context.view_layer.update()

cp = [coat.matrix_world @ v.co for v in coat.data.vertices]
mhg.log("costume scaled x%.3f, soles at z %.3f (body %.3f)"
        % (scale, min(q.z for q in cp), bz0))
mhg.log("costume now z %.3f..%.3f, body z %.3f..%.3f"
        % (min(q.z for q in cp), max(q.z for q in cp), bz0, bz1))

# WHAT STICKS OUT. Every body vertex outside the costume, by height band.
inv = coat.matrix_world.inverted()
cmw = coat.matrix_world
bands = {}
for v in body.data.vertices:
    p = mw @ v.co
    ok, loc, nor, _ = coat.closest_point_on_mesh(inv @ p)
    if not ok:
        continue
    surface = cmw @ loc
    normal = (cmw.to_3x3() @ nor).normalized()
    outside = (p - surface).dot(normal) > 0.002
    key = round((p.z - bz0) / (bz1 - bz0) * 10) / 10.0
    rec = bands.setdefault(key, [0, 0])
    rec[1] += 1
    if outside:
        rec[0] += 1
mhg.log("body left OUTSIDE the costume, by height:")
for key in sorted(bands):
    out, tot = bands[key]
    if tot < 50:
        continue
    mhg.log("   %.1f  %5d/%5d  %5.1f%%" % (key, out, tot, 100.0 * out / tot))

bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(out_blend))
mhg.log("saved %s" % out_blend)
