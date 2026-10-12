"""Dump the slot selections on a clothed vs unclothed character."""

import os
import unreal

OUT = os.path.join(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()),
    "Tools", "Export", "slot_data_probe.txt")

lines = []


def log(msg):
    lines.append(str(msg))
    unreal.log(str(msg))


for fn in ("get_slot_selection_data", "set_single_slot_selection",
           "try_add_slot_selection", "assemble", "clear_assembly_output"):
    log("--- %s ---" % fn)
    try:
        log((getattr(unreal.MetaHumanCharacterInstance, fn).__doc__ or "<no doc>").strip())
    except Exception as exc:
        log("   %s" % exc)
    log("")

PAIRS = [
    ("CLOTHED  Eshu_Wilder_Female",
     "/Game/_Custom/Characters/Kith/Eshu/MHC_Eshu_Wilder_Female"),
    ("UNCLOTHED Eshu_Grump_Female",
     "/Game/_Custom/Characters/Kith/Eshu/MHC_Eshu_Grump_Female"),
]

for tag, path in PAIRS:
    log("=" * 60)
    log(tag)
    ch = unreal.load_asset(path)
    if ch is None:
        log("   could not load")
        continue
    coll = ch.get_editor_property("internal_collection")
    inst = coll.get_editor_property("default_instance")
    try:
        data = inst.get_slot_selection_data()
        log("   slot selections: %d" % len(data))
        for i, sel in enumerate(data):
            log("   [%d] %r" % (i, sel))
            for attr in sorted(x for x in dir(sel) if not x.startswith("_")):
                if attr in ("cast", "static_struct", "call_method",
                            "get_editor_property", "set_editor_property",
                            "set_editor_properties", "reset_editor_property",
                            "is_editor_property_overridden", "copy",
                            "assign", "to_tuple"):
                    continue
                try:
                    log("        %-24s = %r" % (attr, getattr(sel, attr)))
                except Exception:
                    pass
    except Exception as exc:
        log("   get_slot_selection_data FAILED: %s" % exc)
    log("")

with open(OUT, "w") as handle:
    handle.write("\n".join(lines))
log("written: %s" % OUT)
