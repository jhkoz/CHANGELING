"""Re-assign the cuff band to the actual sleeve ends.

    blender -b --python Tools/retag_cuffs.py -- [--band 0.035] [--body <blend>]

The cuff slot scatters. Assigned when the sleeve ends were an open edge, it
then survived a solidify (which doubled every face and added rims) and a
weld (which merged 266 islands into 9), and ended up spanning x -0.425..0.432
and z 0.280..0.786 -- the full width of the costume, from the skirt upward.

Re-derived here from the Sleeve_L / Sleeve_R groups, which did survive both:
the cuff is the lowest `band` metres of each sleeve. Faces already in the
cuff slot elsewhere go back to the coat body.
"""
import bpy, os, sys
from collections import Counter

sys.path.insert(0, "D:/CHANGELING/Tools")
import blender_metahuman_garment as mhg

argv = sys.argv
args = argv[argv.index("--") + 1:] if "--" in argv else []


def arg(flag, default=None):
    return args[args.index(flag) + 1] if flag in args else default


body_blend = arg("--body", "Tools/Blender/Bodies/Boggan_Childling_Male.blend")
band = float(arg("--band", "0.035"))

bpy.ops.wm.open_mainfile(filepath=os.path.abspath(body_blend))
for o in bpy.data.objects:
    if o.type == "MESH" and o.mode != "OBJECT":
        bpy.context.view_layer.objects.active = o
        bpy.ops.object.mode_set(mode="OBJECT")

coat = bpy.data.objects.get("Costume")
mw = coat.matrix_world
names = [m.name if m else "" for m in coat.data.materials]
cuff = next((i for i, n in enumerate(names) if "Cuff" in n), None)
if cuff is None:
    sys.exit("no cuff slot on Costume")
coat_slot = next((i for i, n in enumerate(names)
                  if n == "Material_0" or "Coat" in n), 0)

# Everything currently marked cuff goes back to the coat; the band is then
# re-derived, so a stale assignment cannot survive a re-run.
reset = 0
for face in coat.data.polygons:
    if face.material_index == cuff:
        face.material_index = coat_slot
        reset += 1
mhg.log("cleared %d stale cuff face(s) back to slot %d" % (reset, coat_slot))

tagged = 0
for side in ("L", "R"):
    group = coat.vertex_groups.get("Sleeve_%s" % side)
    if group is None:
        mhg.log("   no Sleeve_%s" % side)
        continue
    inside = {v.index for v in coat.data.vertices
              if any(g.group == group.index and g.weight > 0 for g in v.groups)}
    if not inside:
        mhg.log("   Sleeve_%s is empty" % side)
        continue
    floor = min((mw @ coat.data.vertices[i].co).z for i in inside)
    hits = 0
    for face in coat.data.polygons:
        if not all(v in inside for v in face.vertices):
            continue
        if (mw @ face.center).z <= floor + band:
            face.material_index = cuff
            hits += 1
    mhg.log("   Sleeve_%s: floor z %.3f, %d face(s) within %.0fmm"
            % (side, floor, hits, band * 1000))
    tagged += hits

coat.data.update()
pts = [mw @ coat.data.vertices[v].co
       for f in coat.data.polygons if f.material_index == cuff
       for v in f.vertices]
if pts:
    mhg.log("cuff now %d face(s), x %.3f..%.3f  z %.3f..%.3f"
            % (tagged, min(p.x for p in pts), max(p.x for p in pts),
               min(p.z for p in pts), max(p.z for p in pts)))
else:
    mhg.log("cuff now EMPTY -- widen --band")

bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(body_blend))
mhg.log("saved %s" % body_blend)
