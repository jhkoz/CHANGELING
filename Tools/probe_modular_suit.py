"""What is in the CR/LQ modular suits, and will the clothing parts fit a Boggan?

These are SKELETAL meshes rather than static ones, so they already carry
skinning -- though almost certainly to their own skeleton rather than
metahuman_base_skel, which means a re-bind in Blender either way.

What matters most is the SHAPE. The frock coat failed because it was cut for a
tall slim figure and is smaller than a Boggan nearly everywhere; no scaling
gave both fit and silhouette. A suit jacket may be cut loose enough to work,
and the size numbers say so before anything is built.
"""
import unreal
BASE = "/Game/CR/LQ/CharacterModular/Modular_005_Suit01/Mesh"
sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)

assets = unreal.EditorAssetLibrary.list_assets(BASE, recursive=False, include_folder=False) or []
unreal.log("=== %d assets in Suit01" % len(assets))
for a in sorted(assets):
    path = a.split(".")[0]
    m = unreal.load_asset(path)
    if m is None or not isinstance(m, unreal.SkeletalMesh):
        continue
    name = path.rsplit("/", 1)[-1]
    try:
        b = m.get_bounds(); e = b.box_extent
        size = "x %.1f y %.1f z %.1f" % (e.x*2, e.y*2, e.z*2)
    except Exception:
        size = "?"
    try:
        verts = sub.get_num_verts(m, 0)
    except Exception:
        verts = -1
    sk = m.get_editor_property("skeleton")
    mats = len(m.get_editor_property("materials"))
    unreal.log("   %-26s %6d verts  %-28s %2d mats  skel=%s"
               % (name, verts, size, mats, sk.get_name() if sk else "NONE"))
