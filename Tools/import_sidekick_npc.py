"""Import an assembled Sidekick NPC against Synty's own skeleton.

Not metahuman_base_skel: these parts are rigged to SKEL_Default_Sidekick, and
letting the importer create a new skeleton instead would give every NPC its
own, which defeats the point of one animation set driving all of them.
"""
import os, unreal

PROJECT = os.path.normpath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
SRC_DIR = os.path.join(PROJECT, "Tools", "Export", "Characters")
DEST = "/Game/_Custom/Characters/Sidekick"
SKELETON = "/Game/Synty/SidekickCharacters/Resources/Skeletons/SKEL_Default_Sidekick"
MATERIAL = "/Game/Synty/SidekickCharacters/Resources/Materials/M_Default_Sidekick"

skeleton = unreal.load_asset(SKELETON)
unreal.log("skeleton: %s" % ("found" if skeleton else "NOT FOUND at %s" % SKELETON))

fbxs = sorted(f for f in os.listdir(SRC_DIR) if f.lower().endswith(".fbx")) \
    if os.path.isdir(SRC_DIR) else []
if not fbxs:
    unreal.log("no FBX in %s" % SRC_DIR)

for fname in fbxs:
    name = os.path.splitext(fname)[0]
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(SRC_DIR, fname))
    task.set_editor_property("destination_path", DEST)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)

    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", True)
    # Synty's material is already in the project and drives the colour atlas.
    # Letting the importer bring materials in makes a BLANK duplicate beside
    # the mesh -- same name, no atlas, no colour -- and binds that instead, so
    # the character renders grey. Import none and bind the real one below.
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("import_animations", False)
    if skeleton:
        options.set_editor_property("skeleton", skeleton)
    sk = options.skeletal_mesh_import_data
    sk.set_editor_property("import_morph_targets", False)
    sk.set_editor_property("convert_scene", True)
    sk.set_editor_property("normal_import_method",
                           unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    task.set_editor_property("options", options)

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    mesh = unreal.load_asset("%s/%s" % (DEST, name))
    if mesh is None:
        unreal.log("%-28s IMPORT FAILED" % name)
        continue
    material = unreal.load_asset(MATERIAL)
    if material is not None:
        slots = mesh.get_editor_property("materials")
        for slot in slots:
            slot.set_editor_property("material_interface", material)
            slot.set_editor_property("material_slot_name", "Body")
        mesh.set_editor_property("materials", slots)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)

    bound = mesh.get_editor_property("skeleton")
    sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    try:
        verts = sub.get_num_verts(mesh, 0)
    except Exception:
        verts = -1
    slots = mesh.get_editor_property("materials")
    first = slots[0].get_editor_property("material_interface") if slots else None
    unreal.log("%-28s -> %d verts  skeleton=%s  %d slot(s)  material=%s"
               % (name, verts,
                  bound.get_name() if bound else "*** NONE ***",
                  len(slots),
                  first.get_path_name() if first else "*** NONE ***"))
