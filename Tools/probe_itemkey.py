"""What does try_add_item_from_wardrobe_item actually return, and can we get a
usable FMetaHumanPaletteItemKey out of it? Operates only on the scratch
duplicate created by probe_wardrobe_register.py."""

import os
import traceback

import unreal

DUP = "/Game/_Scratch/WardrobeProbe/MHC_Probe_Sidhe_Grump_Female"
WI = "/Game/_Scratch/WardrobeProbe/WI_Dress_sidhe_grump_female_fitted_thin"
SLOT = "SkeletalMesh"

REPORT = os.path.join(
    unreal.Paths.project_dir(), "Tools", "Export", "probe_itemkey.txt")
LINES = []


def log(m=""):
    LINES.append(str(m))


def flush():
    with open(REPORT, "w", encoding="utf-8") as fh:
        fh.write("\n".join(LINES) + "\n")
    for line in LINES:
        unreal.log(line)


def main():
    dup = unreal.load_asset(DUP)
    wi = unreal.load_asset(WI)
    log("character: %s" % dup)
    log("wardrobe item: %s" % wi)
    coll = dup.get_editor_property("internal_collection")

    log()
    log("--- MetaHumanPaletteItemKey members")
    key = unreal.MetaHumanPaletteItemKey()
    log("  dir: %s" % [m for m in dir(key) if not m.startswith("_")])
    for prop in ("variation", "principal_asset_or_external_wardrobe_item",
                 "b_references_external_wardrobe_item",
                 "references_external_wardrobe_item"):
        try:
            log("  read  %-44s -> %r" % (prop, key.get_editor_property(prop)))
        except Exception as exc:
            log("  read  %-44s -> BLOCKED (%s)" % (prop, exc))

    log()
    log("--- calling try_add_item_from_wardrobe_item('%s')" % SLOT)
    res = coll.try_add_item_from_wardrobe_item(SLOT, wi)
    log("  type   : %s" % type(res))
    log("  repr   : %r" % (res,))
    try:
        log("  len    : %s" % len(res))
        for i, part in enumerate(res):
            log("    [%d] %s = %r" % (i, type(part), part))
    except TypeError:
        log("  (not a sequence)")

    # If we got a key, try wearing it.
    got_key = None
    if isinstance(res, (tuple, list)):
        for part in res:
            if isinstance(part, unreal.MetaHumanPaletteItemKey):
                got_key = part
    elif isinstance(res, unreal.MetaHumanPaletteItemKey):
        got_key = res

    log()
    if got_key is None:
        log("--- no key returned; trying to build one by hand")
        try:
            k = unreal.MetaHumanPaletteItemKey()
            k.set_editor_property(
                "principal_asset_or_external_wardrobe_item",
                unreal.SoftObjectPath(WI + "." + WI.rsplit("/", 1)[-1]))
            log("  hand-built: %r" % k)
            got_key = k
        except Exception as exc:
            log("  hand-build BLOCKED: %s" % exc)
    else:
        log("--- key returned: %r" % got_key)
        for prop in ("variation", "b_references_external_wardrobe_item"):
            try:
                log("    %s = %r" % (prop, got_key.get_editor_property(prop)))
            except Exception as exc:
                log("    %s unreadable (%s)" % (prop, exc))

    if got_key is not None:
        log()
        log("--- wearing it")
        inst = coll.get_editor_property("default_instance")
        try:
            inst.set_single_slot_selection(SLOT, got_key)
            log("  set_single_slot_selection('%s', key): OK" % SLOT)
            unreal.EditorLoadingAndSavingUtils.save_dirty_packages(False, True)
            log("  saved scratch packages")
        except Exception as exc:
            log("  FAILED: %s" % exc)

    flush()


try:
    main()
except Exception:
    log(traceback.format_exc())
    flush()
