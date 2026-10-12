"""Export the forward-named male Boggan bodies -- the ones assembly builds."""
import os, unreal
PROJECT = os.path.normpath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT_DIR = os.path.join(PROJECT, "Tools", "Export", "MetaHumanBodies")
if not os.path.isdir(OUT_DIR): os.makedirs(OUT_DIR)
for n in ("MHC_Boggan_Childling_Male", "MHC_Boggan_Wilder_Male", "MHC_Boggan_Grump_Male"):
    src = "/Game/_Custom/Characters/Kith/Boggan/%s/Body/SKM_%s_BodyMesh" % (n, n)
    m = unreal.load_asset(src)
    if m is None:
        unreal.log("MISSING %s" % src); continue
    out = os.path.join(OUT_DIR, "SKM_%s_BodyMesh.fbx" % n)
    t = unreal.AssetExportTask()
    t.set_editor_property("object", m); t.set_editor_property("filename", out)
    t.set_editor_property("automated", True); t.set_editor_property("prompt", False)
    t.set_editor_property("exporter", unreal.SkeletalMeshExporterFBX())
    ok = unreal.Exporter.run_asset_export_task(t)
    unreal.log("export %-30s %s" % (n, "OK" if ok else "FAILED"))
