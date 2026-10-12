"""What shape are the coat and breeches actually in?

The render shows a sleeveless bodice with panels flaring sideways at the hip,
and no breeches legs. The build log claimed sleeves were added
("sleeves: fitted, 12 extrusions across 2 armhole(s)"), so either they did not
survive add_thickness, or they are the sideways panels.

Reports per garment: world bounds, and how much geometry sits near the hand,
elbow and knee bones -- which is what tells sleeves and leg tubes apart from a
skirt.
"""
import bpy, os, sys
from mathutils import Vector

BLEND = "D:/CHANGELING/Tools/Blender/Work/boggan_male_wilder_suit.blend"
bpy.ops.wm.open_mainfile(filepath=BLEND)

arm = None
for o in bpy.data.objects:
    if o.type == "ARMATURE":
        arm = o
        break

def bone_pos(name):
    if arm is None: return None
    b = arm.data.bones.get(name)
    return (arm.matrix_world @ b.head_local) if b else None

targets = {n: bone_pos(n) for n in
           ("hand_l", "lowerarm_l", "upperarm_l", "calf_l", "foot_l", "pelvis")}
print("=== bone positions ===")
for n, p in targets.items():
    print("   %-12s %s" % (n, "%.3f,%.3f,%.3f" % (p.x,p.y,p.z) if p else "absent"))

for name in ("Coat", "Breeches"):
    o = bpy.data.objects.get(name)
    print("=== %s ===" % name)
    if o is None:
        print("   MISSING")
        continue
    mw = o.matrix_world
    pts = [mw @ v.co for v in o.data.vertices]
    xs=[p.x for p in pts]; ys=[p.y for p in pts]; zs=[p.z for p in pts]
    print("   verts %d  faces %d" % (len(pts), len(o.data.polygons)))
    print("   x %.3f..%.3f   y %.3f..%.3f   z %.3f..%.3f"
          % (min(xs),max(xs),min(ys),max(ys),min(zs),max(zs)))
    print("   width %.3f  depth %.3f  height %.3f"
          % (max(xs)-min(xs), max(ys)-min(ys), max(zs)-min(zs)))
    for bn in ("hand_l","lowerarm_l","calf_l","foot_l"):
        bp = targets.get(bn)
        if bp is None: continue
        near = sum(1 for p in pts if (p-bp).length < 0.10)
        print("   verts within 10cm of %-11s %d" % (bn, near))
