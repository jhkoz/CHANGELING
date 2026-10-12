"""Join the modular suit's clothing parts into one garment for fitting.

Chest, Leg and Foot are separate skeletal meshes on SK_Mannequin. The fit and
conform steps work on a single mesh, and the outfit pipeline wants one garment
per character, so they are joined here before anything else touches them.
"""
import bpy, os
SRC = "D:/CHANGELING/Tools/Export/SourceGarments"
OUT = os.path.join(SRC, "Suit01_joined.fbx")

bpy.ops.wm.read_factory_settings(use_empty=True)
for part in ("Chest_1", "Leg_1", "Foot_1"):
    bpy.ops.import_scene.fbx(filepath=os.path.join(SRC, "Suit01_%s.fbx" % part))

# LOD0 ONLY. A skeletal FBX exports every LOD as its own object, so joining
# everything doubles the geometry and lays LOD1 inside LOD0 -- 8,080 verts
# where the garment has 6,462.
meshes = [o for o in bpy.data.objects
          if o.type == "MESH" and not o.name.endswith("_LOD1")]
dropped = [o for o in bpy.data.objects
           if o.type == "MESH" and o.name.endswith("_LOD1")]
for o in dropped:
    print("[join]   dropping %s (%d verts)" % (o.name, len(o.data.vertices)))
    bpy.data.objects.remove(o, do_unlink=True)
print("[join] %d mesh object(s)" % len(meshes))
for o in meshes:
    mw = o.matrix_world
    pts = [mw @ v.co for v in o.data.vertices]
    zs = [p.z for p in pts]; ys = [p.y for p in pts]
    print("[join]   %-24s %5d verts  z %.2f..%.2f  depth %.2f"
          % (o.name, len(pts), min(zs), max(zs), max(ys)-min(ys)))

bpy.ops.object.select_all(action="DESELECT")
for o in meshes:
    o.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]
if len(meshes) > 1:
    bpy.ops.object.join()
g = bpy.context.view_layer.objects.active
g.name = "Suit"

# Drop the armature: the fit rebinds to the MetaHuman body's own skeleton, and
# SK_Mannequin's bones would only confuse the weight transfer.
for a in [o for o in bpy.data.objects if o.type == "ARMATURE"]:
    bpy.data.objects.remove(a, do_unlink=True)
for m in list(g.modifiers):
    g.modifiers.remove(m)

mw = g.matrix_world
pts = [mw @ v.co for v in g.data.vertices]
xs=[p.x for p in pts]; ys=[p.y for p in pts]; zs=[p.z for p in pts]
print("[join] joined: %d verts  x %.2f..%.2f y %.2f..%.2f z %.2f..%.2f"
      % (len(pts), min(xs),max(xs), min(ys),max(ys), min(zs),max(zs)))
print("[join] size x %.2f y %.2f z %.2f" % (max(xs)-min(xs), max(ys)-min(ys), max(zs)-min(zs)))

bpy.ops.object.select_all(action="DESELECT"); g.select_set(True)
bpy.ops.export_scene.fbx(filepath=OUT, use_selection=True, object_types={'MESH'},
                         apply_scale_options='FBX_SCALE_NONE', global_scale=1.0)
print("[join] exported %s" % OUT)
