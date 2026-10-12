"""
export_metahuman_bodies.py

Batch-exports every MetaHuman body SkeletalMesh in CHANGELING to FBX, for use as
fit avatars in Blender / Jinny / Marvelous Designer.

Meshes export in their reference (bind) pose. That is deliberate: fit a garment
against a posed body and the garment is baked to that pose forever after.

Paths are derived from the RUNNING project (unreal.Paths.project_dir()), never
hard-coded -- so this always writes into whichever copy of CHANGELING is
actually open, rather than a stale one somewhere else on disk.

Run from the running editor's Python console (Output Log -> cmd dropdown -> Python):
    py "D:/CHANGELING/Tools/export_metahuman_bodies.py"

Or headless, with the editor CLOSED. -AllowCommandletRendering is REQUIRED, not
optional: the FBX skeletal exporter calls GetCPUSkinnedVertices(), which asserts
on a null MeshObject without a renderer. Drop the flag and this hard-crashes
with "Assertion failed: MeshObject" at SkinnedMeshComponent.cpp:4677.

    "D:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
      "D:/CHANGELING/CHANGELING.uproject" \
      -run=pythonscript \
      -script="D:/CHANGELING/Tools/export_metahuman_bodies.py" \
      -unattended -nosplash -AllowCommandletRendering \
      -EnablePlugins=PythonScriptPlugin

Output: <project>/Tools/Export/MetaHumanBodies/*.fbx  (+ _export_log.txt)
"""

import os
import unreal

# The live character roster. /Game/MetaHumans is the OLD location -- it holds
# 9 bodies, 8 of them stale duplicates of Kith characters plus one test
# artifact, so exporting from there silently produces avatars for superseded
# bodies.
SEARCH_ROOT = "/Game/_Custom/Characters/Kith"
NAME_SUFFIX = "BodyMesh"

# Optional: export only bodies whose name contains one of these fragments.
# Set MH_EXPORT_ONLY to a comma-separated list to re-export a handful without
# waiting on the full 34-body pass.
ONLY = [s.strip() for s in os.environ.get("MH_EXPORT_ONLY", "").split(",") if s.strip()]

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT_DIR = os.path.join(PROJECT, "Tools", "Export", "MetaHumanBodies")
LOG_PATH = os.path.join(OUT_DIR, "_export_log.txt")

_lines = []


def log(msg):
    # print() does not reach stdout under -run=pythonscript, so mirror to the
    # UE log and to a file on disk.
    text = str(msg)
    _lines.append(text)
    unreal.log(text)


def find_body_meshes():
    """Every SkeletalMesh under SEARCH_ROOT whose name ends in BodyMesh."""
    found = []
    for path in unreal.EditorAssetLibrary.list_assets(SEARCH_ROOT, recursive=True,
                                                      include_folder=False):
        # list_assets returns "/Game/Foo/Bar.Bar" -- strip the object part.
        pkg = path.split(".")[0]
        if not pkg.rsplit("/", 1)[-1].endswith(NAME_SUFFIX):
            continue
        if ONLY and not any(o.lower() in pkg.lower() for o in ONLY):
            continue
        asset = unreal.load_asset(pkg)
        if isinstance(asset, unreal.SkeletalMesh):
            found.append((pkg, asset))
    found.sort(key=lambda p: p[0])
    return found


def make_options():
    """LOD0-only, no extras. This is a retopo/fit reference, not a game asset."""
    opts = unreal.FbxExportOption()
    wanted = {
        "ascii": False,
        "collision": False,
        "level_of_detail": False,
        "vertex_color": False,
        "export_morph_targets": False,
        "export_preview_mesh": False,
        "map_skeletal_motion_to_root": False,
        "force_front_x_axis": False,
    }
    for key, value in wanted.items():
        try:
            opts.set_editor_property(key, value)
        except Exception as exc:
            log("   note: FbxExportOption has no '%s' in this build (%s)" % (key, exc))
    return opts


def export_one(asset, opts):
    out_file = os.path.join(OUT_DIR, asset.get_name() + ".fbx")
    task = unreal.AssetExportTask()
    task.set_editor_property("object", asset)
    task.set_editor_property("filename", out_file)
    task.set_editor_property("automated", True)
    task.set_editor_property("prompt", False)
    task.set_editor_property("replace_identical", True)
    task.set_editor_property("options", opts)
    # Leaving 'exporter' unset lets UE pick the SkeletalMesh FBX exporter itself.
    ok = unreal.Exporter.run_asset_export_task(task)
    return ok, out_file


def skeleton_of(asset):
    try:
        skel = asset.get_editor_property("skeleton")
        return skel.get_path_name().split(".")[0] if skel else "<none>"
    except Exception:
        return "<unreadable>"


def main():
    if not os.path.isdir(OUT_DIR):
        os.makedirs(OUT_DIR)

    log("project: %s" % PROJECT)
    bodies = find_body_meshes()
    log("Found %d body mesh(es) under %s" % (len(bodies), SEARCH_ROOT))
    if not bodies:
        log("Nothing matched *%s -- check SEARCH_ROOT." % NAME_SUFFIX)
        return

    skeletons = set()
    opts = make_options()
    exported, failed = 0, 0

    for pkg, asset in bodies:
        skel = skeleton_of(asset)
        skeletons.add(skel)
        log("")
        log("%s" % pkg)
        log("   skeleton: %s" % skel)
        try:
            ok, out_file = export_one(asset, opts)
        except Exception as exc:
            log("   FAILED: %s" % exc)
            failed += 1
            continue
        if ok and os.path.isfile(out_file):
            size_mb = os.path.getsize(out_file) / (1024.0 * 1024.0)
            log("   -> %s (%.1f MB)" % (out_file, size_mb))
            exported += 1
        else:
            log("   FAILED: exporter returned %s" % ok)
            failed += 1

    log("")
    log("=" * 60)
    log("Exported %d, failed %d" % (exported, failed))
    log("Distinct skeletons across all bodies: %d" % len(skeletons))
    for skel in sorted(skeletons):
        log("   %s" % skel)
    if len(skeletons) == 1:
        log("One shared skeleton -- a garment skinned to it fits every character.")
    else:
        log("More than one skeleton -- garments must be built per skeleton.")
    log("=" * 60)

    with open(LOG_PATH, "w") as handle:
        handle.write("\n".join(_lines))


main()
