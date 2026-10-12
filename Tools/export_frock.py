"""Export the downloaded frock coat static mesh to FBX for fitting in Blender."""
import os
import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT_DIR = os.path.join(PROJECT, "Tools", "Export", "SourceGarments")
SRC = "/Game/Fab/Frock_Coat/Frock_coat/StaticMeshes/Frock_coat"

if not os.path.isdir(OUT_DIR):
    os.makedirs(OUT_DIR)
out = os.path.join(OUT_DIR, "Frock_coat.fbx")

mesh = unreal.load_asset(SRC)
if mesh is None:
    unreal.log("NOT FOUND: %s" % SRC)
else:
    task = unreal.AssetExportTask()
    task.set_editor_property("object", mesh)
    task.set_editor_property("filename", out)
    task.set_editor_property("automated", True)
    task.set_editor_property("prompt", False)
    task.set_editor_property("exporter", unreal.StaticMeshExporterFBX())
    ok = unreal.Exporter.run_asset_export_task(task)
    unreal.log("export %s -> %s" % ("OK" if ok else "FAILED", out))
    try:
        unreal.log("  size on disk: %.1f MB" % (os.path.getsize(out) / 1048576.0))
    except OSError:
        unreal.log("  nothing written")
