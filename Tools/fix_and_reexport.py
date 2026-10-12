"""
fix_and_reexport.py

One pass: repoint the four outlier Boggan bodies at the standard
MetaHumanCharacter plugin skeleton, then re-export just those four to FBX so the
result can be verified against the pre-fix baseline.

Run this from the RUNNING editor's Python console:

    py "D:/CHANGELING/Tools/fix_and_reexport.py"

It is deliberately a single script so everything happens inside the one open
editor -- running a headless commandlet against a project that already has an
editor attached risks a lock, and two editor instances on this project have
caused save failures before.

Pre-fix baseline (measured 2026-09-21 from the exported FBX):

    Boggan_Female_Grump    341 bones  32334 verts  height 1.1751
    Boggan_Female_Wilder   341 bones  32334 verts  height 1.1664
    Boggan_Male_Grump      341 bones  32334 verts  height 1.3168
    Boggan_Male_Wilder     341 bones  32334 verts  height 1.3168

If any of those change after the reassignment, the geometry did not survive and
the backups in Tools/Backup/skeleton_fix_*/ should be restored.
"""

import os
import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT_DIR = os.path.join(PROJECT, "Tools", "Export", "MetaHumanBodies")
LOG_PATH = os.path.join(PROJECT, "Tools", "Export", "fix_and_reexport_log.txt")

TARGET_SKELETON = "/MetaHumanCharacter/Female/Medium/NormalWeight/Body/metahuman_base_skel"
KITH = "/Game/_Custom/Characters/Kith/Boggan"
BODIES = [
    "MHC_Boggan_Female_Grump",
    "MHC_Boggan_Female_Wilder",
    "MHC_Boggan_Male_Grump",
    "MHC_Boggan_Male_Wilder",
]

lines = []


def log(msg):
    lines.append(str(msg))
    unreal.log(str(msg))


def skel_path(mesh):
    skel = mesh.get_editor_property("skeleton")
    return skel.get_path_name().split(".")[0] if skel else "<none>"


def export_options():
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
    return opts


def main():
    if not os.path.isdir(OUT_DIR):
        os.makedirs(OUT_DIR)

    target = unreal.load_asset(TARGET_SKELETON)
    if target is None:
        log("FATAL: could not load %s" % TARGET_SKELETON)
        return

    log("target skeleton: %s" % TARGET_SKELETON)
    log("")
    log("--- pass 1: reassign skeletons ---")

    fixed, skipped, failed = 0, 0, 0
    for name in BODIES:
        path = "%s/%s/Body/SKM_%s_BodyMesh" % (KITH, name, name)
        mesh = unreal.load_asset(path)
        if mesh is None:
            log("%-26s FAILED: could not load" % name)
            failed += 1
            continue
        before = skel_path(mesh)
        if before == TARGET_SKELETON:
            log("%-26s already correct" % name)
            skipped += 1
            continue
        try:
            mesh.set_editor_property("skeleton", target)
            unreal.EditorAssetLibrary.save_asset(path)
        except Exception as exc:
            log("%-26s FAILED: %s" % (name, exc))
            failed += 1
            continue
        after = skel_path(unreal.load_asset(path))
        ok = "ok" if after == TARGET_SKELETON else "DID NOT STICK"
        log("%-26s %s" % (name, ok))
        log("     %s" % before)
        log("  -> %s" % after)
        if after == TARGET_SKELETON:
            fixed += 1
        else:
            failed += 1

    log("")
    log("reassigned %d, already-correct %d, failed %d" % (fixed, skipped, failed))
    log("")
    log("--- pass 2: re-export those bodies ---")

    opts = export_options()
    exported = 0
    for name in BODIES:
        path = "%s/%s/Body/SKM_%s_BodyMesh" % (KITH, name, name)
        mesh = unreal.load_asset(path)
        if mesh is None:
            continue
        out_file = os.path.join(OUT_DIR, mesh.get_name() + ".fbx")
        task = unreal.AssetExportTask()
        task.set_editor_property("object", mesh)
        task.set_editor_property("filename", out_file)
        task.set_editor_property("automated", True)
        task.set_editor_property("prompt", False)
        task.set_editor_property("replace_identical", True)
        task.set_editor_property("options", opts)
        try:
            ok = unreal.Exporter.run_asset_export_task(task)
        except Exception as exc:
            log("%-26s EXPORT FAILED: %s" % (name, exc))
            continue
        if ok and os.path.isfile(out_file):
            log("%-26s -> %s (%.1f MB)"
                % (name, os.path.basename(out_file),
                   os.path.getsize(out_file) / 1048576.0))
            exported += 1
        else:
            log("%-26s EXPORT FAILED (exporter returned %s)" % (name, ok))

    log("")
    log("=" * 62)
    log("skeletons fixed %d | re-exported %d of %d" % (fixed, exported, len(BODIES)))
    log("Now verify the FBX against the baseline in this file's docstring:")
    log("341 bones / 32334 verts / unchanged height for all four.")
    log("=" * 62)

    with open(LOG_PATH, "w") as handle:
        handle.write("\n".join(lines))
    log("log written: %s" % LOG_PATH)


main()
