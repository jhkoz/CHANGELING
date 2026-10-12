"""
import_all.py

One entry point for getting the whole costume pipeline into Unreal.

Runs three stages, each idempotent -- already-imported garments are skipped and
materials are rebuilt cheaply, so re-running costs seconds rather than a full
60-mesh reimport.

    1. garments   Tools/Export/Garments/*.fbx  -> /Game/_Custom/Garments
    2. materials  M_CourtFabric (Cloth) + M_CourtTrim, assigned to every garment
    3. masks      Tools/Export/BodyMasks/*.png -> /Game/_Custom/BodyMasks

STAGE 3 IS OPT-IN, and that is not fussiness. Texture import needs Slate, which
a commandlet does not have: it asserts CurrentApplication.IsValid() and kills
the process. That happened twice here, both times AFTER the meshes imported but
BEFORE the log was written -- so a run that had detected 60 bad assets reported
nothing at all, and the failure looked like success.

There is no way to detect a commandlet from Python. sys.argv carries only the
script path; the editor world, every editor subsystem and the project paths are
all present and identical in both. So masks are requested explicitly instead:

    HEADLESS (editor closed) -- stages 1 and 2:
        "D:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
          "D:/CHANGELING/CHANGELING.uproject" \
          -run=pythonscript -script="D:/CHANGELING/Tools/import_all.py" \
          -unattended -nosplash -AllowCommandletRendering \
          -EnablePlugins=PythonScriptPlugin

    IN THE EDITOR'S PYTHON CONSOLE -- all three:
        py "D:/CHANGELING/Tools/import_all.py" masks

Log: Tools/Export/import_all_log.txt
"""

import os
import sys

import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
TOOLS = os.path.join(PROJECT, "Tools")
LOG_PATH = os.path.join(TOOLS, "Export", "import_all_log.txt")

WANT_MASKS = any(a.lower() == "masks" for a in sys.argv[1:])

_lines = []


def log(msg):
    text = str(msg)
    _lines.append(text)
    unreal.log(text)


def run(script):
    """Execute one stage script in its own namespace."""
    path = os.path.join(TOOLS, script)
    if not os.path.isfile(path):
        log("   MISSING: %s" % path)
        return False
    try:
        namespace = {"__name__": "__main__", "__file__": path}
        exec(compile(open(path).read(), path, "exec"), namespace)
        return True
    except Exception as exc:
        log("   FAILED: %s" % exc)
        return False


def counted(folder):
    try:
        return len(unreal.EditorAssetLibrary.list_assets(
            folder, recursive=True, include_folder=False))
    except Exception:
        return 0


log("=" * 66)
log("CHANGELING costume import")
log("project: %s" % PROJECT)
log("masks requested: %s" % WANT_MASKS)
log("=" * 66)

log("")
log("--- 1/3 garments ---")
run("import_garments.py")
log("   /Game/_Custom/Garments now holds %d asset(s)"
    % counted("/Game/_Custom/Garments"))

log("")
log("--- 2/3 materials ---")
run("make_cloth_materials.py")
log("   /Game/_Custom/Materials/Garments now holds %d asset(s)"
    % counted("/Game/_Custom/Materials/Garments"))

log("")
log("--- 3/3 body masks ---")
if WANT_MASKS:
    run("import_masks.py")
    log("   /Game/_Custom/BodyMasks now holds %d asset(s)"
        % counted("/Game/_Custom/BodyMasks"))
else:
    log("   SKIPPED -- texture import needs Slate and would crash a commandlet.")
    log("   Run this from the editor's Python console to include them:")
    log('   py "%s" masks' % os.path.join(TOOLS, "import_all.py"))

log("")
log("=" * 66)
log("garments  %d" % counted("/Game/_Custom/Garments"))
log("materials %d" % counted("/Game/_Custom/Materials/Garments"))
log("masks     %d" % counted("/Game/_Custom/BodyMasks"))
log("=" * 66)
log("Next, per character (Blueprint work):")
log("  body material -> Blend Mode Masked, mask -> Opacity Mask, clip ~0.33")
log("  AChangelingNPC's Outfit slot -> choose a dress from /Game/_Custom/Garments")

out_dir = os.path.dirname(LOG_PATH)
if not os.path.isdir(out_dir):
    os.makedirs(out_dir)
with open(LOG_PATH, "w") as handle:
    handle.write("\n".join(_lines))
