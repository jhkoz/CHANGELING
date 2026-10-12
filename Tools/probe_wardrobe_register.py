"""
probe_wardrobe_register.py -- can we register one of our 60 SkeletalMesh
garments as a MetaHuman Wardrobe Item and wear it, entirely from Python?

Everything destructive happens on a DUPLICATE of the character, written to
/Game/_Scratch/WardrobeProbe. The real Kith asset is opened read-only and is
never saved. Does NOT run build_meta_human (~6 min); this probes registration
and slot selection only, which is the part the C++ headers could not settle.

    python Tools/ue_remote.py Tools/probe_wardrobe_register.py
"""

import os
import traceback

import unreal

SOURCE_CHARACTER = "/Game/_Custom/Characters/Kith/Sidhe/MHC_Sidhe_Grump_Female"
GARMENT = "/Game/_Custom/Garments/Dress_sidhe_grump_female_fitted_thin"
SCRATCH = "/Game/_Scratch/WardrobeProbe"

REPORT = os.path.join(
    unreal.Paths.project_dir(), "Tools", "Export", "probe_wardrobe_register.txt")

LINES = []


def log(msg=""):
    LINES.append(str(msg))


def head(title):
    log()
    log("=" * 70)
    log(title)
    log("=" * 70)


def flush():
    d = os.path.dirname(REPORT)
    if not os.path.isdir(d):
        os.makedirs(d)
    with open(REPORT, "w", encoding="utf-8") as fh:
        fh.write("\n".join(LINES) + "\n")
    for line in LINES:
        unreal.log(line)


def dump_collection(coll, label):
    """Read out whatever the palette will show us."""
    log("  %s: %s" % (label, coll.get_name()))
    slots_seen = []
    try:
        items = coll.get_editor_property("items")
        log("    items: %d" % len(items))
        for it in items:
            slot = it.get_editor_property("slot_name")
            slots_seen.append(str(slot))
            wi = it.get_editor_property("wardrobe_item")
            principal = ""
            if wi:
                try:
                    principal = str(wi.get_editor_property("principal_asset"))
                except Exception:
                    principal = "<unreadable>"
            log("      slot=%-14s wi=%-30s" % (slot, wi.get_name() if wi else "-"))
            if principal:
                log("            principal: %s" % principal)
    except Exception as exc:
        log("    items unreadable: %s" % exc)

    try:
        pipe = coll.get_editor_property("pipeline")
        if pipe:
            log("    pipeline: %s (%s)"
                % (pipe.get_name(), pipe.get_class().get_name()))
            for prop in ("specification", "editor_pipeline"):
                try:
                    val = pipe.get_editor_property(prop)
                    log("      %s: %s" % (prop, val))
                    if prop == "specification" and val:
                        slots = val.get_editor_property("slots")
                        log("      SLOTS (%d):" % len(slots))
                        for sname, sdef in slots.items():
                            slots_seen.append(str(sname))
                            try:
                                types = sdef.get_editor_property(
                                    "supported_principal_asset_types")
                            except Exception:
                                types = "<unreadable>"
                            log("        %-18s %s" % (sname, types))
                except Exception as exc:
                    log("      %s unreadable: %s" % (prop, exc))
    except Exception as exc:
        log("    pipeline unreadable: %s" % exc)

    return slots_seen


def set_principal(wi, garment):
    """principal_asset is a TSoftObjectPtr<UObject>; try the usable forms."""
    attempts = [
        ("object", garment),
        ("SoftObjectPath", unreal.SoftObjectPath(GARMENT)),
        ("string", GARMENT),
    ]
    for label, value in attempts:
        try:
            wi.set_editor_property("principal_asset", value)
            got = wi.get_editor_property("principal_asset")
            log("    principal_asset set via %s -> %s" % (label, got))
            return True
        except Exception as exc:
            log("    principal_asset via %s failed: %s" % (label, exc))
    return False


def main():
    log("probe_wardrobe_register")
    log("source character : %s" % SOURCE_CHARACTER)
    log("garment          : %s" % GARMENT)
    log("scratch          : %s" % SCRATCH)

    head("1. LOAD SOURCE (read-only)")
    char = unreal.load_asset(SOURCE_CHARACTER)
    garment = unreal.load_asset(GARMENT)
    log("  character: %s" % (char.get_class().get_name() if char else "FAILED"))
    log("  garment  : %s" % (garment.get_class().get_name() if garment else "FAILED"))
    if not char or not garment:
        log("  cannot continue")
        flush()
        return

    src_coll = char.get_editor_property("internal_collection")
    slots_seen = dump_collection(src_coll, "source collection")

    head("2. DUPLICATE CHARACTER INTO SCRATCH")
    eal = unreal.EditorAssetLibrary
    dup_path = "%s/MHC_Probe_Sidhe_Grump_Female" % SCRATCH
    if eal.does_asset_exist(dup_path):
        log("  deleting stale %s" % dup_path)
        eal.delete_asset(dup_path)
    dup = eal.duplicate_asset(SOURCE_CHARACTER, dup_path)
    log("  duplicate: %s" % (dup.get_path_name() if dup else "FAILED"))
    if not dup:
        flush()
        return
    dup_coll = dup.get_editor_property("internal_collection")
    log("  duplicate collection: %s" % dup_coll.get_name())

    head("3. CREATE WARDROBE ITEM ASSET")
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.MetaHumanWardrobeItemFactory()
    wi_name = "WI_%s" % garment.get_name()
    wi_path = "%s/%s" % (SCRATCH, wi_name)
    if eal.does_asset_exist(wi_path):
        log("  deleting stale %s" % wi_path)
        eal.delete_asset(wi_path)
    wi = tools.create_asset(wi_name, SCRATCH,
                            unreal.MetaHumanWardrobeItem, factory)
    log("  wardrobe item: %s" % (wi.get_path_name() if wi else "FAILED"))
    if not wi:
        flush()
        return
    set_principal(wi, garment)
    try:
        log("    is_external: %s" % wi.is_external())
    except Exception as exc:
        log("    is_external not callable from Python (%s)" % exc)

    head("4. TRY EVERY PLAUSIBLE SLOT")
    # Slot names come from the plugin's own Config/Editor.ini
    # (UMetaHumanCharacterEditorWardrobeSettings.WardrobeSections):
    #   Outfits      -> ChaosOutfitAsset only
    #   SkeletalMesh -> Engine.SkeletalMesh, bPureVirtual=true  <-- ours
    candidates = []
    for s in slots_seen + ["SkeletalMesh", "Outfits", "Hair", "Beard",
                           "Eyebrows", "Eyelashes", "Mustache", "Peachfuzz",
                           "Character"]:
        if s not in candidates:
            candidates.append(s)
    log("  candidates: %s" % ", ".join(candidates))
    log()

    accepted = None
    accepted_key = None
    for slot in candidates:
        try:
            res = dup_coll.try_add_item_from_wardrobe_item(slot, wi)
        except Exception as exc:
            log("  %-16s EXCEPTION %s" % (slot, exc))
            continue
        # bool return + out param -> tuple; guard both shapes.
        if isinstance(res, tuple):
            ok, key = res[0], res[1]
        else:
            ok, key = bool(res), None
        log("  %-16s %s%s" % (slot, "ACCEPTED" if ok else "rejected",
                              ("  key=%s" % key) if ok else ""))
        if ok and accepted is None:
            accepted, accepted_key = slot, key

    if accepted is None:
        head("RESULT: no slot accepted a SkeletalMesh wardrobe item")
        log("  The outfit slot likely demands a ChaosOutfitAsset, not a")
        log("  SkeletalMesh. See the per-slot rejections above.")
        flush()
        return

    head("5. WEAR IT (set slot selection on the instance)")
    log("  using slot %s" % accepted)
    inst = dup_coll.get_editor_property("default_instance")
    log("  instance: %s" % inst.get_name())
    try:
        inst.set_single_slot_selection(accepted, accepted_key)
        log("  set_single_slot_selection: OK")
    except Exception as exc:
        log("  set_single_slot_selection FAILED: %s" % exc)
        flush()
        return

    head("6. READ BACK (did it stick?)")
    dump_collection(dup_coll, "duplicate collection")

    head("7. SAVE SCRATCH ONLY")
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(False, True)
    log("  saved dirty content packages")
    log()
    log("  NOTE: build_meta_human deliberately NOT run (~6 min).")
    log("  Scratch assets left at %s for inspection." % SCRATCH)

    flush()


try:
    main()
except Exception:
    head("FATAL")
    log(traceback.format_exc())
    flush()
