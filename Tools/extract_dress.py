"""Pull ONE dress out of the two-in-one Witch_dresses mesh.

The source holds two garments side by side sharing a single mesh: 197k verts,
x spanning -0.65..2.39, and 20 material slots in duplicate sets (Corset and
Corset_001, Bodice and Bodice_001, and so on). Splitting on x=0 does not work
because the pair is not centred on the origin.

The material suffix is the reliable discriminator, so parts are separated by
loose geometry and then sorted by whether the materials they use carry _001.
"""
import bpy, os

SRC = "D:/CHANGELING/Tools/Export/SourceGarments/Witch_dresses.fbx"
OUT = "D:/CHANGELING/Tools/Export/SourceGarments/Witch_dress_A.fbx"

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=SRC)

# UCX_ is a collision hull, not geometry.
for o in [x for x in bpy.data.objects if x.name.startswith("UCX_")]:
    bpy.data.objects.remove(o, do_unlink=True)

o = max((x for x in bpy.data.objects if x.type == "MESH"),
        key=lambda m: len(m.data.vertices))
bpy.ops.object.select_all(action="DESELECT")
o.select_set(True); bpy.context.view_layer.objects.active = o
bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.mesh.separate(type='LOOSE'); bpy.ops.object.mode_set(mode='OBJECT')

parts = [x for x in bpy.data.objects if x.type == "MESH"]
print("[dress] %d loose parts" % len(parts))

a_parts, b_parts = [], []
used_by = {}
for p in parts:
    used = set()
    for poly in p.data.polygons:
        if poly.material_index < len(p.material_slots):
            slot = p.material_slots[poly.material_index]
            if slot.material:
                used.add(slot.material.name)
    used_by[p.name] = used
    variant_b = any(n.endswith("_001") for n in used)
    (b_parts if variant_b else a_parts).append(p)
    mw = p.matrix_world
    xs = [(mw @ v.co).x for v in p.data.vertices]
    print("[dress]   %-26s %6d verts  x %.2f..%.2f  -> %s  (%s)"
          % (p.name, len(p.data.vertices), min(xs), max(xs),
             "B" if variant_b else "A", ", ".join(sorted(used))[:60]))

# Drop the headwear. Head_scarf and Head_cover would sit on top of a
# MetaHuman's own head and hair, and the scarf is what makes the raw mesh
# 1.21 m deep -- it trails well behind the figure.
#
# Judged on the materials the POLYGONS actually use, not on material_slots:
# a loose-parts split leaves every part carrying all 20 slots, so a slot-based
# test flags every part as headwear and removes the entire dress.
head = [p for p in a_parts
        if any(n.startswith("Head_") for n in used_by.get(p.name, ()))]
for p in head:
    print("[dress]   dropping headwear: %s (%d verts)" % (p.name, len(p.data.vertices)))
    a_parts.remove(p)
    bpy.data.objects.remove(p, do_unlink=True)

keep = a_parts
print("[dress] dress A: %d parts, %d verts"
      % (len(keep), sum(len(p.data.vertices) for p in keep)))
print("[dress] dress B: %d parts, %d verts"
      % (len(b_parts), sum(len(p.data.vertices) for p in b_parts)))

for p in b_parts:
    bpy.data.objects.remove(p, do_unlink=True)

bpy.ops.object.select_all(action="DESELECT")
for p in keep:
    p.select_set(True)
bpy.context.view_layer.objects.active = keep[0]
if len(keep) > 1:
    bpy.ops.object.join()
dress = bpy.context.view_layer.objects.active
dress.name = "WitchDress"

mw = dress.matrix_world
pts = [mw @ v.co for v in dress.data.vertices]
xs=[p.x for p in pts]; ys=[p.y for p in pts]; zs=[p.z for p in pts]
print("[dress] kept: %d verts  x %.2f..%.2f  y %.2f..%.2f  z %.2f..%.2f"
      % (len(pts), min(xs), max(xs), min(ys), max(ys), min(zs), max(zs)))
print("[dress] size x %.2f y %.2f z %.2f" % (max(xs)-min(xs), max(ys)-min(ys), max(zs)-min(zs)))

bpy.ops.export_scene.fbx(filepath=OUT, use_selection=True,
                         object_types={'MESH'}, apply_scale_options='FBX_SCALE_NONE',
                         global_scale=1.0, path_mode='AUTO')
print("[dress] exported %s (%.1f MB)" % (OUT, os.path.getsize(OUT)/1048576.0))
