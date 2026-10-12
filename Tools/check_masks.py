"""Read back each mask's sRGB and compression settings.

sRGB ON is the failure that matters: the gamma curve shifts the values so the
Opacity Mask Clip threshold lands in the wrong place and every garment edge
frays. The import reported success once while configuring nothing, because it
was looking for an asset name UE had already stripped the UDIM tag from.
"""

import os
import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "mask_check.txt")
DEST = "/Game/_Custom/BodyMasks"

lines = []


def log(m):
    lines.append(str(m))
    unreal.log(str(m))


paths = sorted(p.split(".")[0] for p in
               unreal.EditorAssetLibrary.list_assets(DEST, recursive=True,
                                                     include_folder=False))
bad = 0
log("%-44s %6s %-22s %s" % ("mask", "sRGB", "compression", "size"))
for path in paths:
    tex = unreal.load_asset(path)
    if tex is None:
        log("%-44s  COULD NOT LOAD" % path.rsplit("/", 1)[-1])
        bad += 1
        continue
    srgb = tex.get_editor_property("srgb")
    comp = str(tex.get_editor_property("compression_settings")).split(".")[-1]
    try:
        w = tex.blueprint_get_size_x()
        h = tex.blueprint_get_size_y()
        size = "%dx%d" % (w, h)
    except Exception:
        size = "?"
    ok = (srgb is False) and "MASKS" in comp.upper()
    if not ok:
        bad += 1
    log("%-44s %6s %-22s %s%s"
        % (path.rsplit("/", 1)[-1], srgb, comp, size, "" if ok else "   <-- CHECK"))

log("")
log("masks: %d, problems: %d" % (len(paths), bad))

with open(OUT, "w") as h:
    h.write("\n".join(lines))
