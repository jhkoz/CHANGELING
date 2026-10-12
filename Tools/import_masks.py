"""
import_masks.py

Imports the body-hiding masks as non-sRGB mask textures.

SEPARATE FROM import_garments.py ON PURPOSE. Texture import needs Slate, which a
commandlet does not have: it asserts CurrentApplication.IsValid() and takes the
whole process down. Previously that happened AFTER the garments imported but
BEFORE the log was written, so a run that had in fact detected 60 bad assets
reported nothing at all -- the crash ate the evidence.

-AllowCommandletRendering does not help. Nor does guarding on sys.argv: Python
inside a UE commandlet does not receive the engine's command line, so the guard
never fires.

So: RUN THIS FROM THE EDITOR'S PYTHON CONSOLE.

    py "D:/CHANGELING/Tools/import_masks.py"

sRGB is turned OFF on every texture. These are data, not colour -- left on, the
gamma curve shifts the values and the Opacity Mask Clip threshold lands in the
wrong place, fraying every garment edge.
"""

import os
import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
MASK_DIR = os.path.join(PROJECT, "Tools", "Export", "BodyMasks")
MASK_DEST = "/Game/_Custom/BodyMasks"
LOG_PATH = os.path.join(MASK_DIR, "_import_log.txt")

_lines = []


def log(msg):
    text = str(msg)
    _lines.append(text)
    unreal.log(text)


def import_masks():
    if not os.path.isdir(MASK_DIR):
        log("no mask folder at %s" % MASK_DIR)
        return 0

    pngs = sorted(f for f in os.listdir(MASK_DIR) if f.lower().endswith(".png"))
    if not pngs:
        log("no masks to import")
        return 0

    log("importing %d mask(s) into %s" % (len(pngs), MASK_DEST))

    tasks = []
    for fname in pngs:
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", os.path.join(MASK_DIR, fname))
        task.set_editor_property("destination_path", MASK_DEST)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", True)
        tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    import re

    fixed, missing = 0, 0
    for fname in pngs:
        stem = os.path.splitext(fname)[0]
        # UE CONSUMES the UDIM tag: "x_mask.1002.png" imports as asset "x_mask".
        # Looking for "x_mask.1002" finds nothing, so the texture imports fine
        # and then gets configured NOT AT ALL -- left with sRGB ON, which is the
        # one setting that ruins a mask.
        candidates = [re.sub(r"\.\d{4}$", "", stem), stem, fname.split(".")[0]]

        tex, path = None, None
        for candidate in candidates:
            path = "%s/%s" % (MASK_DEST, candidate)
            tex = unreal.load_asset(path)
            if tex is not None:
                break
        if tex is None:
            log("   MISSING after import: tried %s"
                % ", ".join(sorted(set(candidates))))
            missing += 1
            continue
        try:
            tex.set_editor_property("srgb", False)
            tex.set_editor_property(
                "compression_settings",
                unreal.TextureCompressionSettings.TC_MASKS)
            unreal.EditorAssetLibrary.save_asset(path)
            fixed += 1
        except Exception as exc:
            log("   could not configure %s: %s" % (name, exc))

    log("")
    log("=" * 60)
    log("masks imported %d, configured non-sRGB/TC_MASKS %d, missing %d"
        % (len(pngs) - missing, fixed, missing))
    log("Next: body material -> Blend Mode Masked, this texture -> Opacity Mask,")
    log("scalar for Opacity Mask Clip Value (~0.33), one instance per character.")
    log("=" * 60)
    return fixed


import_masks()

with open(LOG_PATH, "w") as handle:
    handle.write("\n".join(_lines))
log("log: %s" % LOG_PATH)
