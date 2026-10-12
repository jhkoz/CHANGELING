"""Export every forward-named Kith body -- the ones assembly actually builds.

The reversed-name folders (MHC_Boggan_Male_Wilder) are legacy output from
2026-09-13/14 with no MetaHumanCharacter behind them. Fitting a garment
against one produces a garment the live character never wears.
"""
import os, unreal
PROJECT = os.path.normpath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT_DIR = os.path.join(PROJECT, "Tools", "Export", "MetaHumanBodies")
if not os.path.isdir(OUT_DIR): os.makedirs(OUT_DIR)
KITH = "/Game/_Custom/Characters/Kith"
made = missing = skipped = 0
for clan in ("Boggan", "Clurichaun", "Eshu", "Sidhe", "SidheAutumn"):
    for seeming in ("Childling", "Wilder", "Grump"):
        for sex in ("Male", "Female"):
            name = "MHC_%s_%s_%s" % (clan, seeming, sex)
            src = "%s/%s/%s/Body/SKM_%s_BodyMesh" % (KITH, clan, name, name)
            out = os.path.join(OUT_DIR, "SKM_%s_BodyMesh.fbx" % name)
            if os.path.isfile(out):
                skipped += 1
                continue
            m = unreal.load_asset(src)
            if m is None:
                unreal.log("MISSING  %s" % name); missing += 1; continue
            t = unreal.AssetExportTask()
            t.set_editor_property("object", m); t.set_editor_property("filename", out)
            t.set_editor_property("automated", True); t.set_editor_property("prompt", False)
            t.set_editor_property("exporter", unreal.SkeletalMeshExporterFBX())
            if unreal.Exporter.run_asset_export_task(t):
                unreal.log("exported %s" % name); made += 1
            else:
                unreal.log("FAILED   %s" % name); missing += 1
unreal.log("=== exported %d, already present %d, missing %d" % (made, skipped, missing))
