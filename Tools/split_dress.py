"""How many dresses are in Witch_dresses, and where?

303.9 cm wide with 20 materials in duplicate sets (Corset/Corset_001 ...)
says two garments share one mesh. Separates by loose parts and clusters the
pieces by X so each dress can be extracted on its own.
"""
import bpy
from collections import defaultdict

SRC = "D:/CHANGELING/Tools/Export/SourceGarments/Witch_dresses.fbx"
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=SRC)
meshes = [o for o in bpy.data.objects if o.type == "MESH"]
print("[split] imported %d object(s)" % len(meshes))
for o in meshes:
    mw = o.matrix_world
    pts = [mw @ v.co for v in o.data.vertices]
    xs = [p.x for p in pts]; zs = [p.z for p in pts]
    print("[split] %-28s %6d verts  x %.2f..%.2f  z %.2f..%.2f  %d mats"
          % (o.name, len(pts), min(xs), max(xs), min(zs), max(zs),
             len(o.material_slots)))

o = max(meshes, key=lambda m: len(m.data.vertices))
bpy.ops.object.select_all(action="DESELECT")
o.select_set(True); bpy.context.view_layer.objects.active = o
bpy.ops.mesh.separate(type='LOOSE') if bpy.context.mode == 'EDIT_MESH' else None
bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.mesh.separate(type='LOOSE'); bpy.ops.object.mode_set(mode='OBJECT')
parts = [x for x in bpy.data.objects if x.type == "MESH"]
print("[split] loose parts: %d" % len(parts))
groups = defaultdict(list)
for p in parts:
    mw = p.matrix_world
    cx = sum((mw @ v.co).x for v in p.data.vertices) / max(1, len(p.data.vertices))
    groups["LEFT" if cx < 0 else "RIGHT"].append((p.name, cx, len(p.data.vertices)))
for side, items in groups.items():
    tot = sum(i[2] for i in items)
    print("[split] %-6s %2d parts, %6d verts" % (side, len(items), tot))
