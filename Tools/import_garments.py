"""
import_garments.py

Unreal half of the CHANGELING MetaHuman clothing pipeline. Imports garment FBX
files exported from Blender (Tools/blender_metahuman_garment.py) as Skeletal
Meshes bound to the EXISTING MetaHuman skeleton, and assigns the shared body LOD
settings so garment LODs stay in lockstep with the body.

Every MetaHuman body in CHANGELING shares one skeleton, so a garment imported
once works on all of them.

This deliberately stops at the asset. It does not touch the character
Blueprints -- adding the SkeletalMeshComponent and setting its Leader Pose
Component is Blueprint work.

Run headless with the editor CLOSED:
    "D:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
      "D:/CHANGELING/CHANGELING.uproject" \
      -run=pythonscript \
      -script="D:/CHANGELING/Tools/import_garments.py" \
      -unattended -nosplash -EnablePlugins=PythonScriptPlugin

Or from the running editor's Python console:
    py "D:/CHANGELING/Tools/import_garments.py"

Imports every FBX in Tools/Export/Garments/ into /Game/_Custom/Garments/.
Log: Tools/Export/Garments/_import_log.txt
"""

import os
import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
FBX_DIR = os.path.join(PROJECT, "Tools", "Export", "Garments")
LOG_PATH = os.path.join(FBX_DIR, "_import_log.txt")

DEST = "/Game/_Custom/Garments"

# Re-import even when the asset is already present and correctly bound.
#
# The skip below makes re-running cheap, but it also means a garment whose FBX
# has CHANGED is silently left at the old geometry. Set this when the source
# has been rebuilt. It replaces in place rather than deleting first, because
# the cloth assets' Dataflow graphs hold references to these meshes and
# deleting a referenced asset is what crashed the editor earlier.
FORCE_REIMPORT = False

# The live Kith roster binds to the skeleton shipped by the MetaHumanCharacter
# PLUGIN (30 of 34 bodies as of 2026-09-21), not the legacy project-local copy
# under /Game/MetaHumans. Three old Boggans still use the /Game one and one
# (Boggan_Female_Grump) is wrongly bound to SK_Mannequin -- garments for those
# will report a skeleton mismatch until those bodies are rebuilt.
SKELETON = "/MetaHumanCharacter/Female/Medium/NormalWeight/Body/metahuman_base_skel"
LEGACY_SKELETON = "/Game/MetaHumans/Common/Female/Medium/NormalWeight/Body/metahuman_base_skel"
LOD_SETTINGS = "/MetaHumanCharacter/Body/IdentityTemplate/Body_LODSettings"

_lines = []


def log(msg):
    # print() does not reach stdout under -run=pythonscript.
    text = str(msg)
    _lines.append(text)
    unreal.log(text)


def _force_legacy_fbx():
    """Interchange ignores most of FbxImportUI, including the skeleton we pin.

    Turning its FBX path off makes FbxImportUI authoritative again, which is the
    only way to guarantee the garment binds to metahuman_base_skel instead of
    silently generating a fresh skeleton.
    """
    try:
        unreal.SystemLibrary.execute_console_command(
            None, "Interchange.FeatureFlags.Import.FBX 0")
        log("Interchange FBX path disabled -- using legacy importer")
    except Exception as exc:
        log("could not disable Interchange (%s) -- continuing" % exc)


def _options(skeleton):
    opts = unreal.FbxImportUI()
    opts.set_editor_property("import_mesh", True)
    opts.set_editor_property("import_as_skeletal", True)
    opts.set_editor_property("import_animations", False)
    opts.set_editor_property("create_physics_asset", False)
    # Bind to the existing skeleton. This is the whole point of the script.
    opts.set_editor_property("skeleton", skeleton)
    opts.set_editor_property("mesh_type_to_import",
                             unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    # Garment materials are authored in UE, not carried over from Blender.
    opts.set_editor_property("import_materials", False)
    opts.set_editor_property("import_textures", False)

    data = opts.get_editor_property("skeletal_mesh_import_data")
    for key, value in (
        ("import_morph_targets", False),
        # Never let a garment redefine the shared skeleton's reference pose.
        ("update_skeleton_reference_pose", False),
        ("use_t0_as_ref_pose", False),
        ("preserve_smoothing_groups", True),
        ("import_meshes_in_bone_hierarchy", False),
        ("convert_scene", True),
    ):
        try:
            data.set_editor_property(key, value)
        except Exception as exc:
            log("   note: import option '%s' unavailable (%s)" % (key, exc))
    return opts


def import_one(fbx_path, skeleton, lod_settings):
    name = os.path.splitext(os.path.basename(fbx_path))[0]
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", fbx_path)
    task.set_editor_property("destination_path", DEST)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", _options(skeleton))

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    asset_path = "%s/%s" % (DEST, name)
    mesh = unreal.load_asset(asset_path)
    if mesh is None:
        return None, "import produced no asset at %s" % asset_path
    if not isinstance(mesh, unreal.SkeletalMesh):
        return None, "imported as %s, not a SkeletalMesh" % type(mesh).__name__

    bound = mesh.get_editor_property("skeleton")
    if bound is None:
        return mesh, "imported but has NO skeleton"
    bound_path = bound.get_path_name().split(".")[0]
    if bound_path != SKELETON:
        return mesh, ("bound to the WRONG skeleton: %s (a new one was probably "
                      "generated -- the garment will not work on the MetaHumans)"
                      % bound_path)

    if lod_settings is not None:
        try:
            mesh.set_editor_property("lod_settings", lod_settings)
            unreal.EditorAssetLibrary.save_asset(asset_path)
        except Exception as exc:
            log("   note: could not assign LOD settings (%s)" % exc)

    return mesh, None


def main():
    if not os.path.isdir(FBX_DIR):
        os.makedirs(FBX_DIR)
        log("created %s -- drop garment FBX files there and re-run" % FBX_DIR)
        return

    fbxs = sorted(f for f in os.listdir(FBX_DIR) if f.lower().endswith(".fbx"))
    if not fbxs:
        log("no FBX files in %s" % FBX_DIR)
        return

    skeleton = unreal.load_asset(SKELETON)
    if skeleton is None:
        log("FATAL: could not load %s" % SKELETON)
        return
    lod_settings = unreal.load_asset(LOD_SETTINGS)
    if lod_settings is None:
        log("warning: %s missing -- LODs will not be synced" % LOD_SETTINGS)

    _force_legacy_fbx()
    log("project: %s" % PROJECT)
    log("skeleton: %s" % SKELETON)
    log("found %d garment FBX file(s)" % len(fbxs))

    ok, bad, skipped = 0, 0, 0
    for fname in fbxs:
        # Already imported and correctly bound? Leave it. Makes re-running the
        # orchestrator cheap rather than a full 60-mesh reimport.
        existing = unreal.load_asset("%s/%s"
                                     % (DEST, os.path.splitext(fname)[0]))
        if isinstance(existing, unreal.SkeletalMesh) and not FORCE_REIMPORT:
            bound = existing.get_editor_property("skeleton")
            if bound and bound.get_path_name().split(".")[0] == SKELETON:
                skipped += 1
                continue

        log("")
        log(fname)
        try:
            mesh, problem = import_one(os.path.join(FBX_DIR, fname),
                                       skeleton, lod_settings)
        except Exception as exc:
            log("   FAILED: %s" % exc)
            bad += 1
            continue
        if problem:
            log("   PROBLEM: %s" % problem)
            bad += 1
            continue
        log("   -> %s/%s  (%d bones, %d LODs)"
            % (DEST, mesh.get_name(),
               len(mesh.get_editor_property("skeleton").get_editor_property("bone_tree"))
               if hasattr(mesh.get_editor_property("skeleton"), "get_editor_property")
               else -1,
               mesh.get_lod_num() if hasattr(mesh, "get_lod_num") else -1))
        ok += 1

    log("")
    log("=" * 60)
    log("Imported %d, problems %d, already present %d" % (ok, bad, skipped))
    log("Next (Blueprint work): add a SkeletalMeshComponent to the character BP,")
    log("set its mesh to the garment, and set Leader Pose Component = Body.")
    log("=" * 60)

    with open(LOG_PATH, "w") as handle:
        handle.write("\n".join(_lines))


main()
