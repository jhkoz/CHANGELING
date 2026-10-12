"""Where is the equipped outfit recorded, and can it be cleared?"""

import os
import unreal

OUT = os.path.join(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()),
    "Tools", "Export", "wardrobe_probe.txt")

lines = []


def log(msg):
    lines.append(str(msg))
    unreal.log(str(msg))


def members(obj, tag):
    log("--- %s (%s) ---" % (tag, type(obj).__name__))
    for n in sorted(x for x in dir(obj) if not x.startswith("_")):
        log("   %s" % n)
    log("")


CLOTHED = "/Game/_Custom/Characters/Kith/Eshu/MHC_Eshu_Wilder_Female"
ch = unreal.load_asset(CLOTHED)
coll = ch.get_editor_property("internal_collection")

inst = None
try:
    inst = coll.get_editor_property("default_instance")
    members(inst, "default_instance")
except Exception as exc:
    log("default_instance unreadable: %s" % exc)

log("--- MetaHumanCharacterInstanceBlueprintLibrary ---")
lib = unreal.MetaHumanCharacterInstanceBlueprintLibrary
for n in sorted(x for x in dir(lib) if not x.startswith("_")):
    log("   %s" % n)
log("")

log("--- MetaHumanPipelineSlotSelectionBlueprintLibrary ---")
try:
    lib2 = unreal.MetaHumanPipelineSlotSelectionBlueprintLibrary
    for n in sorted(x for x in dir(lib2) if not x.startswith("_")):
        log("   %s" % n)
except Exception as exc:
    log("   %s" % exc)
log("")

if inst is not None:
    log("--- current slot selections on the instance ---")
    for prop in ("slot_selection", "slot_selections", "overrides",
                 "parameter_overrides", "palette", "character_palette"):
        try:
            val = inst.get_editor_property(prop)
            log("   %-24s = %r" % (prop, val))
        except Exception:
            pass

log("--- MetaHumanCharacterPalette members ---")
try:
    pal = unreal.MetaHumanCharacterPalette
    for n in sorted(x for x in dir(pal) if not x.startswith("_")):
        log("   %s" % n)
except Exception as exc:
    log("   %s" % exc)

with open(OUT, "w") as handle:
    handle.write("\n".join(lines))
log("written: %s" % OUT)
