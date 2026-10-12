"""Export the modular suit's clothing parts for a fit test."""
import os, unreal
PROJECT = os.path.normpath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT_DIR = os.path.join(PROJECT, "Tools", "Export", "SourceGarments")
if not os.path.isdir(OUT_DIR): os.makedirs(OUT_DIR)
BASE = "/Game/CR/LQ/CharacterModular/Modular_005_Suit01/Mesh"
for part in ("Chest_1", "Leg_1", "Foot_1"):
    src = "%s/SKM_Suit01_%s" % (BASE, part)
    m = unreal.load_asset(src)
    if m is None:
        unreal.log("missing %s" % src); continue
    out = os.path.join(OUT_DIR, "Suit01_%s.fbx" % part)
    t = unreal.AssetExportTask()
    t.set_editor_property("object", m); t.set_editor_property("filename", out)
    t.set_editor_property("automated", True); t.set_editor_property("prompt", False)
    t.set_editor_property("exporter", unreal.SkeletalMeshExporterFBX())
    ok = unreal.Exporter.run_asset_export_task(t)
    unreal.log("export %-10s %s (%.2f MB)" % (part, "OK" if ok else "FAILED",
        os.path.getsize(out)/1048576.0 if os.path.exists(out) else 0))
