"""Re-derive the breeches region from the leg bones.

    blender -b --python Tools/retag_breeches.py -- [--reach 0.12] [--body <blend>]

The breeches slot was assigned with a rule written before the costume was
widened, stretched, solidified and welded -- "below the hip and within 0.17
of the body's axis". By the time those had all run it held 11516 faces, most
of the skirt's lower half and part of the torso, because the body's axis
cannot separate a trouser from a skirt: both are below the hip and both are
near the centre line.

The LEG's axis can. Measured on the current mesh, faces below the pelvis on
one side sit at 1821 within 50mm of the leg's own thigh-to-foot line, 1792
by 100mm, then 2608 at 150mm -- the second group being the skirt, which
hangs well clear. Cut at `reach`.

Boots, trim, lapels and cuffs keep whatever they have; only the breeches
slot and the coat body are re-sorted, so this can be re-run without
compounding.
"""
import bpy, os, sys

sys.path.insert(0, "D:/CHANGELING/Tools")
import blender_metahuman_garment as mhg

argv = sys.argv
args = argv[argv.index("--") + 1:] if "--" in argv else []


def arg(flag, default=None):
    return args[args.index(flag) + 1] if flag in args else default


body_blend = arg("--body", "Tools/Blender/Bodies/Boggan_Childling_Male.blend")
reach = float(arg("--reach", "0.12"))

bpy.ops.wm.open_mainfile(filepath=os.path.abspath(body_blend))
for o in bpy.data.objects:
    if o.type == "MESH" and o.mode != "OBJECT":
        bpy.context.view_layer.objects.active = o
        bpy.ops.object.mode_set(mode="OBJECT")

coat = bpy.data.objects.get("Costume")
body = bpy.data.objects.get("Body")
rig = mhg._armature_for(body) or next(
    o for o in bpy.data.objects if o.type == "ARMATURE" and "Body" in o.name)
amw = rig.matrix_world
mw = coat.matrix_world

names = [m.name if m else "" for m in coat.data.materials]
br = next((i for i, n in enumerate(names) if "Breeches" in n), None)
coat_slot = next((i for i, n in enumerate(names)
                  if n == "Material_0" or "Coat" in n), 0)
protect = {i for i, n in enumerate(names)
           if any(k in n for k in ("Boots", "Trim", "Lapel", "Cuff"))}
if br is None:
    sys.exit("no breeches slot")

hip = (amw @ rig.data.bones["pelvis"].head_local).z

chains = []
for side in ("l", "r"):
    seq = []
    for a, b in (("thigh_%s", "calf_%s"), ("calf_%s", "foot_%s")):
        ba, bb = rig.data.bones.get(a % side), rig.data.bones.get(b % side)
        if ba is not None and bb is not None:
            seq.append((amw @ ba.head_local, amw @ bb.head_local))
    if seq:
        chains.append(seq)


def to_leg(point):
    best = None
    for seq in chains:
        for start, end in seq:
            span = end - start
            length = span.length
            if length < 1e-6:
                continue
            t = max(0.0, min(1.0,
                             (point - start).dot(span) / (length * length)))
            gap = (point - (start + span * t)).length
            if best is None or gap < best:
                best = gap
    return best if best is not None else 1e9


was = sum(1 for f in coat.data.polygons if f.material_index == br)
for face in coat.data.polygons:
    if face.material_index in protect:
        continue
    if face.material_index == br:
        face.material_index = coat_slot

tagged = 0
for face in coat.data.polygons:
    if face.material_index in protect:
        continue
    centre = mw @ face.center
    if centre.z > hip:
        continue
    if to_leg(centre) <= reach:
        face.material_index = br
        tagged += 1

coat.data.update()
mhg.log("breeches re-derived: %d -> %d faces (within %.0fmm of a leg axis, "
        "below the hip at z %.3f)" % (was, tagged, reach * 1000, hip))
pts = [mw @ coat.data.vertices[v].co
       for f in coat.data.polygons if f.material_index == br
       for v in f.vertices]
if pts:
    mhg.log("   x %.3f..%.3f  z %.3f..%.3f"
            % (min(p.x for p in pts), max(p.x for p in pts),
               min(p.z for p in pts), max(p.z for p in pts)))

from collections import Counter
for i, n in sorted(Counter(f.material_index
                           for f in coat.data.polygons).items()):
    mhg.log("   slot %d %-38s %6d" % (i, names[i] if i < len(names) else "?", n))

bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(body_blend))
mhg.log("saved %s" % body_blend)
