"""
reimport_garments_rootfix.py -- force a re-import of all 60 garments after the
root-bone fix.

import_garments.py skips anything that already exists and is bound to the right
skeleton, which is every garment -- and the root-bone bug does not change either
of those things, so the normal importer would do nothing. This forces the
re-import, then verifies bone 0 is now `root` rather than `Avatar_Armature`.

Options are inlined rather than imported because import_garments.py runs its
whole sweep at module import time (no __main__ guard).

    python Tools/ue_remote.py Tools/reimport_garments_rootfix.py
"""

import os
import time
import traceback

import unreal

PROJECT = unreal.Paths.project_dir()
FBX_DIR = os.path.join(PROJECT, "Tools", "Export", "Garments")
DEST = "/Game/_Custom/Garments"
SKELETON = "/MetaHumanCharacter/Female/Medium/NormalWeight/Body/metahuman_base_skel"
LOD_SETTINGS = "/MetaHumanCharacter/Body/IdentityTemplate/Body_LODSettings"
REPORT = os.path.join(PROJECT, "Tools", "Export", "reimport_rootfix.txt")

LINES = []


def log(m=""):
    LINES.append(str(m))
    unreal.log(str(m))


def flush():
    with open(REPORT, "w", encoding="utf-8") as fh:
        fh.write("\n".join(LINES) + "\n")


def options(skeleton):
    opts = unreal.FbxImportUI()
    opts.set_editor_property("import_mesh", True)
    opts.set_editor_property("import_as_skeletal", True)
    opts.set_editor_property("import_animations", False)
    opts.set_editor_property("create_physics_asset", False)
    opts.set_editor_property("skeleton", skeleton)
    opts.set_editor_property("mesh_type_to_import",
                             unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    opts.set_editor_property("import_materials", False)
    opts.set_editor_property("import_textures", False)
    data = opts.get_editor_property("skeletal_mesh_import_data")
    for k, v in (("import_morph_targets", False),
                 ("update_skeleton_reference_pose", False),
                 ("use_t0_as_ref_pose", False),
                 ("preserve_smoothing_groups", True),
                 ("import_meshes_in_bone_hierarchy", False),
                 ("convert_scene", True)):
        try:
            data.set_editor_property(k, v)
        except Exception:
            pass
    return opts


def root_bone(mesh):
    comp = unreal.new_object(unreal.SkeletalMeshComponent)
    comp.set_skeletal_mesh_asset(mesh)
    if comp.get_num_bones() == 0:
        return "<no bones>", 0
    return str(comp.get_bone_name(0)), comp.get_num_bones()


def main():
    t0 = time.time()
    try:
        unreal.SystemLibrary.execute_console_command(
            None, "Interchange.FeatureFlags.Import.FBX 0")
        log("Interchange FBX path disabled")
    except Exception as exc:
        log("could not disable Interchange (%s)" % exc)

    skeleton = unreal.load_asset(SKELETON)
    lods = unreal.load_asset(LOD_SETTINGS)
    fbxs = sorted(f for f in os.listdir(FBX_DIR) if f.lower().endswith(".fbx"))
    log("re-importing %d garments from %s" % (len(fbxs), FBX_DIR))

    # Record what the meshes look like BEFORE, so the fix is measurable.
    before = {}
    for fname in fbxs:
        name = os.path.splitext(fname)[0]
        existing = unreal.load_asset("%s/%s" % (DEST, name))
        if isinstance(existing, unreal.SkeletalMesh):
            before[name] = root_bone(existing)[0]

    ok, bad = [], []
    for i, fname in enumerate(fbxs, 1):
        name = os.path.splitext(fname)[0]
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", os.path.join(FBX_DIR, fname))
        task.set_editor_property("destination_path", DEST)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", True)
        task.set_editor_property("options", options(skeleton))
        try:
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
            mesh = unreal.load_asset("%s/%s" % (DEST, name))
            if not isinstance(mesh, unreal.SkeletalMesh):
                bad.append((name, "not a SkeletalMesh"))
                continue
            bound = mesh.get_editor_property("skeleton")
            if not bound or bound.get_path_name().split(".")[0] != SKELETON:
                bad.append((name, "wrong skeleton: %s" % bound))
                continue
            if lods:
                mesh.set_editor_property("lod_settings", lods)
            rb, nb = root_bone(mesh)
            if rb != "root":
                bad.append((name, "root bone is still '%s'" % rb))
                continue
            ok.append((name, rb, nb))
            log("  [%2d/%d] %-56s %s -> %s  (%d bones)"
                % (i, len(fbxs), name, before.get(name, "?"), rb, nb))
        except Exception as exc:
            bad.append((name, str(exc)))
            log("  [%2d/%d] %-56s FAILED %s" % (i, len(fbxs), name, exc))

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(False, True)

    log("")
    log("=" * 70)
    log("re-imported OK: %d   problems: %d   (%.1f min)"
        % (len(ok), len(bad), (time.time() - t0) / 60.0))
    for name, why in bad:
        log("   PROBLEM %-52s %s" % (name, why))

    # Materials are assigned in UE, not carried in the FBX -- a re-import can
    # drop them. Report rather than guess.
    log("")
    log("MATERIAL SLOT CHECK (re-import can clear these):")
    empty = []
    for name, _, _ in ok:
        mesh = unreal.load_asset("%s/%s" % (DEST, name))
        mats = mesh.get_editor_property("materials")
        assigned = [m for m in mats if m.material_interface]
        if len(assigned) < len(mats) or not mats:
            empty.append((name, len(assigned), len(mats)))
    if empty:
        log("   %d garments have unassigned slots -- re-run "
            "Tools/make_cloth_materials.py" % len(empty))
        for name, a, t in empty[:5]:
            log("      %-56s %d/%d assigned" % (name, a, t))
    else:
        log("   all %d garments still have every material slot assigned" % len(ok))
    flush()


try:
    main()
except Exception:
    log(traceback.format_exc())
    flush()
