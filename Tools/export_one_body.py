"""Export one assembled body mesh so it can be re-fitted in Blender."""
import os, unreal
PROJECT = os.path.normpath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT_DIR = os.path.join(PROJECT, "Tools", "Export", "MetaHumanBodies")
if not os.path.isdir(OUT_DIR): os.makedirs(OUT_DIR)
NAME = "MHC_Boggan_Childling_Male"
src = "/Game/_Custom/Characters/Kith/Boggan/%s/Body/SKM_%s_BodyMesh" % (NAME, NAME)
m = unreal.load_asset(src)
if m is None:
    unreal.log("NOT FOUND %s" % src)
else:
    out = os.path.join(OUT_DIR, "SKM_%s_BodyMesh.fbx" % NAME)
    t = unreal.AssetExportTask()
    t.set_editor_property("object", m); t.set_editor_property("filename", out)
    t.set_editor_property("automated", True); t.set_editor_property("prompt", False)
    t.set_editor_property("exporter", unreal.SkeletalMeshExporterFBX())
    ok = unreal.Exporter.run_asset_export_task(t)
    unreal.log("export %s -> %s (%.1f MB)" % ("OK" if ok else "FAILED", out,
        os.path.getsize(out)/1048576.0 if os.path.exists(out) else 0))
