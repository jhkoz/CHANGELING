"""Import the root-bone-fixed garment and check it against the body skeleton.
Imports to /Game/_Scratch/RootFixTest only."""
import os, sys, traceback
import unreal

# NB: import_garments runs its whole import sweep at module import time
# (no __main__ guard), so the options are inlined here instead.

FBX = os.path.join(unreal.Paths.project_dir(), "Tools", "Export",
                   "RootFixTest", "Dress_sidhe_grump_female_rootfix.fbx")
DEST = "/Game/_Scratch/RootFixTest"
BODY = ("/Game/_Scratch/WardrobeProbe/Built/Probe_222432/Body/"
        "SKM_MHC_Probe_222432_BodyMesh")
SKEL = "/MetaHumanCharacter/Female/Medium/NormalWeight/Body/metahuman_base_skel"
LODS = "/MetaHumanCharacter/Body/IdentityTemplate/Body_LODSettings"

REPORT = os.path.join(unreal.Paths.project_dir(), "Tools", "Export", "probe_rootfix.txt")
LINES = []
def log(m=""):
    LINES.append(str(m)); unreal.log(str(m))
def flush():
    with open(REPORT, "w", encoding="utf-8") as fh: fh.write("\n".join(LINES) + "\n")

def bones_of(mesh):
    comp = unreal.new_object(unreal.SkeletalMeshComponent)
    comp.set_skeletal_mesh_asset(mesh)
    n = comp.get_num_bones()
    names = [str(comp.get_bone_name(i)) for i in range(n)]
    parents = {nm: str(comp.get_parent_bone(nm)) for nm in names}
    return names, parents

try:
    log("ROOT-BONE FIX VERIFICATION")
    log("  fbx: %s (%d bytes)" % (FBX, os.path.getsize(FBX)))
    try:
        unreal.SystemLibrary.execute_console_command(
            None, "Interchange.FeatureFlags.Import.FBX 0")
    except Exception as exc:
        log("  could not disable Interchange (%s)" % exc)
    skel = unreal.load_asset(SKEL)
    lods = unreal.load_asset(LODS)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", FBX)
    task.set_editor_property("destination_path", DEST)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    opts = unreal.FbxImportUI()
    opts.set_editor_property("import_mesh", True)
    opts.set_editor_property("import_as_skeletal", True)
    opts.set_editor_property("import_animations", False)
    opts.set_editor_property("create_physics_asset", False)
    opts.set_editor_property("skeleton", skel)
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
    task.set_editor_property("options", opts)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported = list(task.get_editor_property("imported_object_paths") or [])
    log("  imported: %s" % imported)
    if not imported:
        log("  IMPORT PRODUCED NOTHING"); flush(); raise SystemExit

    mesh = unreal.load_asset(imported[0].split(".")[0])
    log("  class: %s" % mesh.get_class().get_name())
    bound = mesh.get_editor_property("skeleton")
    log("  bound skeleton: %s" % (bound.get_path_name() if bound else "<none>"))
    mesh.set_editor_property("lod_settings", lods)

    gn, gp = bones_of(mesh)
    bn, bp = bones_of(unreal.load_asset(BODY))
    log()
    log("  garment bones: %d   body bones: %d" % (len(gn), len(bn)))
    log("  garment bone[0]: %s      body bone[0]: %s" % (gn[0], bn[0]))
    log("  name set identical : %s" % (set(gn) == set(bn)))
    mism = [b for b in gn if b in bp and gp[b] != bp[b]]
    log("  parent mismatches  : %d %s" % (len(mism), mism[:10]))
    order = [i for i in range(min(len(gn), len(bn))) if gn[i] != bn[i]]
    log("  order divergence   : %s" % (order[0] if order else "none"))

    log()
    if set(gn) == set(bn) and not mism and not order:
        log("  RESULT: hierarchy now matches the body exactly.")
    else:
        log("  RESULT: STILL MISMATCHED -- see above.")
except Exception:
    log(traceback.format_exc())
flush()
