"""Export the Witch Dresses static mesh for splitting and fitting in Blender."""
import os, unreal
PROJECT = os.path.normpath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT_DIR = os.path.join(PROJECT, "Tools", "Export", "SourceGarments")
if not os.path.isdir(OUT_DIR): os.makedirs(OUT_DIR)
out = os.path.join(OUT_DIR, "Witch_dresses.fbx")
m = unreal.load_asset("/Game/Fab/Witch_Dresses/Witch_dresses/StaticMeshes/Witch_dresses")
if m is None:
    unreal.log("NOT FOUND")
else:
    t = unreal.AssetExportTask()
    t.set_editor_property("object", m); t.set_editor_property("filename", out)
    t.set_editor_property("automated", True); t.set_editor_property("prompt", False)
    t.set_editor_property("exporter", unreal.StaticMeshExporterFBX())
    ok = unreal.Exporter.run_asset_export_task(t)
    unreal.log("export %s -> %s (%.1f MB)" % ("OK" if ok else "FAILED", out,
        os.path.getsize(out)/1048576.0 if os.path.exists(out) else 0))
