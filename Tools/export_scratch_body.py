"""Export every rebuilt no-outfit body from the scratch folder."""

import os
import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT_DIR = os.path.join(PROJECT, "Tools", "Export", "MetaHumanBodies")
SCRATCH = "/Game/_Scratch/RebuildTest"

if not os.path.isdir(OUT_DIR):
    os.makedirs(OUT_DIR)

opts = unreal.FbxExportOption()
for key, value in (("ascii", False), ("collision", False),
                   ("level_of_detail", False), ("vertex_color", False),
                   ("export_morph_targets", False),
                   ("export_preview_mesh", False),
                   ("map_skeletal_motion_to_root", False),
                   ("force_front_x_axis", False)):
    try:
        opts.set_editor_property(key, value)
    except Exception:
        pass

count = 0
for path in unreal.EditorAssetLibrary.list_assets(SCRATCH, recursive=True,
                                                  include_folder=False):
    pkg = path.split(".")[0]
    if not pkg.rsplit("/", 1)[-1].endswith("BodyMesh"):
        continue
    mesh = unreal.load_asset(pkg)
    if not isinstance(mesh, unreal.SkeletalMesh):
        continue
    skel = mesh.get_editor_property("skeleton")
    # Name the FBX after the character so it slots into the normal pipeline.
    out_file = os.path.join(OUT_DIR, mesh.get_name() + ".fbx")
    task = unreal.AssetExportTask()
    task.set_editor_property("object", mesh)
    task.set_editor_property("filename", out_file)
    task.set_editor_property("automated", True)
    task.set_editor_property("prompt", False)
    task.set_editor_property("replace_identical", True)
    task.set_editor_property("options", opts)
    ok = unreal.Exporter.run_asset_export_task(task)
    unreal.log("%s  ok=%s  skel=%s"
               % (mesh.get_name(), ok,
                  skel.get_path_name().split(".")[0] if skel else None))
    count += 1

unreal.log("exported %d rebuilt bodies" % count)
