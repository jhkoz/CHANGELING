"""
fix_body_skeletons.py

Repoint outlier Kith body meshes at the standard MetaHumanCharacter plugin
skeleton, so the whole roster agrees.

As of 2026-09-21, 30 of 34 Kith bodies bind to the plugin skeleton. Four do not:
three legacy Boggans still reference the project-local /Game/MetaHumans copy,
and Boggan_Female_Grump references SK_Mannequin. In every case the MESH itself
is a normal MetaHuman body (341 bones, 32,334 verts, no mannequin-only bones) --
only the Skeleton asset POINTER is wrong, so this reassigns that pointer rather
than re-skinning anything.

The plugin and project-local skeletons were verified to have identical bone
sets, so the swap is structural no-op for the 3 legacy bodies.

Back up the .uasset files before running. Verify afterwards by re-exporting the
bodies and confirming bone count, vert count and height are unchanged.

Run from the running editor's Python console, or via remote execution.
"""

import os
import unreal

TARGET_SKELETON = "/MetaHumanCharacter/Female/Medium/NormalWeight/Body/metahuman_base_skel"

KITH = "/Game/_Custom/Characters/Kith/Boggan"
BODIES = [
    "MHC_Boggan_Female_Grump",
    "MHC_Boggan_Female_Wilder",
    "MHC_Boggan_Male_Grump",
    "MHC_Boggan_Male_Wilder",
]

OUT = os.path.join(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()),
    "Tools", "Export", "skeleton_fix_log.txt")

lines = []


def log(msg):
    lines.append(str(msg))
    unreal.log(str(msg))


def skel_path(mesh):
    skel = mesh.get_editor_property("skeleton")
    return skel.get_path_name().split(".")[0] if skel else "<none>"


def main():
    target = unreal.load_asset(TARGET_SKELETON)
    if target is None:
        log("FATAL: could not load %s" % TARGET_SKELETON)
        return

    log("target skeleton: %s" % TARGET_SKELETON)
    fixed, skipped, failed = 0, 0, 0

    for name in BODIES:
        path = "%s/%s/Body/SKM_%s_BodyMesh" % (KITH, name, name)
        log("")
        log(name)
        mesh = unreal.load_asset(path)
        if mesh is None:
            log("   FAILED: could not load %s" % path)
            failed += 1
            continue

        before = skel_path(mesh)
        log("   before: %s" % before)
        if before == TARGET_SKELETON:
            log("   already correct -- skipped")
            skipped += 1
            continue

        try:
            mesh.set_editor_property("skeleton", target)
            unreal.EditorAssetLibrary.save_asset(path)
        except Exception as exc:
            log("   FAILED: %s" % exc)
            failed += 1
            continue

        after = skel_path(unreal.load_asset(path))
        log("   after : %s" % after)
        if after == TARGET_SKELETON:
            fixed += 1
        else:
            log("   PROBLEM: reassignment did not stick")
            failed += 1

    log("")
    log("=" * 60)
    log("fixed %d, already-correct %d, failed %d" % (fixed, skipped, failed))
    log("Now re-export these bodies and confirm 341 bones / 32,334 verts /")
    log("unchanged height before trusting the result.")
    log("=" * 60)

    with open(OUT, "w") as handle:
        handle.write("\n".join(lines))


main()
