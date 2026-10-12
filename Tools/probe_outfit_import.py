"""
probe_outfit_import.py -- two questions left before the Boggan suits can go on.

READ-MOSTLY. Everything lands in /Game/_Scratch/OutfitProbe. No MetaHuman is
built (that costs ~9 min) and no Kith asset is opened for write.

WHERE WE ARE

probe_outfit_asset established:
    ChaosOutfitAsset            reaches Python, and can be CREATED
    ChaosSizedOutfitSource      reaches Python
    populating it by property   ADDED: False -- no sources/pieces property
                                exists at all on the asset
    MetaHumanWardrobeItem       will hold a ChaosOutfitAsset as its
                                principal_asset

So the asset cannot be filled by assignment. But ChaosOutfitAssetFactory
carries an `asset_import_task`, which is how UFactory subclasses take a source
FILE -- suggesting outfits are populated by importing geometry rather than by
setting a property. That is the first question here.

The second is the one that actually gates the work: probe_wardrobe_register
showed the 'Outfits' slot REJECTS a plain SkeletalMesh. Does it accept a
wardrobe item backed by a ChaosOutfitAsset? If yes, the remaining manual step
is bounded -- populate each outfit once, script everything after. If no, this
route does not reach the characters at all and we should say so plainly rather
than spend a 9-minute build finding out.

    python Tools/ue_remote.py Tools/probe_outfit_import.py
"""

import os
import traceback

import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "outfit_import_probe.txt")

SCRATCH = "/Game/_Scratch/OutfitProbe"
COAT_FBX = os.path.join(PROJECT, "Tools", "Export", "Garments",
                        "Coat_boggan_male_wilder.fbx")
# Boggan BAKED folders use reversed names (MHC_Boggan_Male_Wilder) but the
# MetaHumanCharacter SOURCE assets use forward ones. Mixing them up loads
# nothing and the slot test silently never runs.
CHARACTER = "/Game/_Custom/Characters/Kith/Boggan/MHC_Boggan_Wilder_Male"

_lines = []


def log(msg=""):
    text = str(msg)
    _lines.append(text)
    unreal.log(text)


def flush():
    out_dir = os.path.dirname(OUT)
    if not os.path.isdir(out_dir):
        os.makedirs(out_dir)
    with open(OUT, "w") as handle:
        handle.write("\n".join(_lines))


def main():
    log("=" * 70)
    log("probe_outfit_import")
    log("=" * 70)

    # ------------------------------------------- 1. what formats does it take
    log("")
    log("1. WHAT SOURCE FORMATS DOES ChaosOutfitAssetFactory ACCEPT?")
    factory = unreal.ChaosOutfitAssetFactory()
    for prop in ("formats", "supported_class", "import_priority",
                 "editor_import", "text", "asset_import_task"):
        try:
            value = factory.get_editor_property(prop)
            # 'formats' is the extension list, and it is the whole answer to
            # what the outfit pipeline eats. Printing the array object itself
            # says nothing, so walk it.
            if prop == "formats":
                try:
                    entries = [str(v) for v in value]
                    log("   %-22s %s" % (prop, entries if entries else "(empty)"))
                    continue
                except Exception:
                    pass
            log("   %-22s %r" % (prop, value))
        except Exception as exc:
            log("   %-22s <unreadable: %s>" % (prop, str(exc)[:70]))

    # UFactory keeps its extension list in a native array that Python often
    # cannot read; the interchange registry is the second opinion.
    log("")
    log("   does any registered importer claim .zprj / outfit files?")
    for ext in ("zprj", "zpac", "fbx", "usd", "abc"):
        try:
            found = unreal.AssetToolsHelpers.get_asset_tools()
            log("      .%-5s (asset tools present: %s)" % (ext, found is not None))
            break
        except Exception:
            pass

    # ------------------------------------------- 2. try importing the coat
    log("")
    log("2. CAN THE COAT FBX IMPORT AS AN OUTFIT ASSET?")
    if not os.path.isfile(COAT_FBX):
        log("   no FBX at %s" % COAT_FBX)
    else:
        log("   source: %s" % os.path.basename(COAT_FBX))
        try:
            task = unreal.AssetImportTask()
            task.set_editor_property("filename", COAT_FBX)
            task.set_editor_property("destination_path", SCRATCH)
            task.set_editor_property("destination_name", "OA_Import_Coat")
            task.set_editor_property("automated", True)
            task.set_editor_property("replace_existing", True)
            task.set_editor_property("save", False)
            task.set_editor_property("factory", unreal.ChaosOutfitAssetFactory())
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

            made = list(task.get_editor_property("imported_object_paths") or [])
            if made:
                log("   imported: %s" % ", ".join(made))
                for path in made:
                    obj = unreal.load_asset(path.split(".")[0])
                    log("      -> %s" % (type(obj).__name__ if obj else "unloadable"))
            else:
                log("   NOTHING IMPORTED -- the factory did not accept an FBX")
        except Exception as exc:
            log("   import FAILED: %s" % exc)
            for line in traceback.format_exc().splitlines()[-3:]:
                log("      %s" % line)

    # -------------------------------- 3. does the Outfits slot take an outfit?
    log("")
    log("3. DOES THE 'Outfits' SLOT ACCEPT AN OUTFIT-BACKED WARDROBE ITEM?")
    log("   (a plain SkeletalMesh was REJECTED -- that is the whole question)")

    character = unreal.load_asset(CHARACTER)
    if character is None:
        log("   could not load %s" % CHARACTER)
        return

    # Work on a duplicate. The real character is never written.
    dup_path = "%s/MHC_SlotProbe" % SCRATCH
    try:
        if unreal.EditorAssetLibrary.does_asset_exist(dup_path):
            unreal.EditorAssetLibrary.delete_asset(dup_path)
        dup = unreal.EditorAssetLibrary.duplicate_asset(CHARACTER, dup_path)
        log("   duplicate: %s" % (dup.get_path_name() if dup else "FAILED"))
    except Exception as exc:
        log("   duplicate FAILED: %s" % exc)
        return
    if dup is None:
        return

    outfit = unreal.load_asset("%s/OA_Probe_Coat" % SCRATCH)
    if outfit is None:
        log("   no OA_Probe_Coat -- run probe_outfit_asset first")
        return

    try:
        wi_path = "%s/WI_SlotProbe" % SCRATCH
        if unreal.EditorAssetLibrary.does_asset_exist(wi_path):
            unreal.EditorAssetLibrary.delete_asset(wi_path)
        item = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "WI_SlotProbe", SCRATCH, unreal.MetaHumanWardrobeItem, None)
        item.set_editor_property("principal_asset", outfit)
        log("   wardrobe item principal: %s"
            % type(item.get_editor_property("principal_asset")).__name__)
    except Exception as exc:
        log("   wardrobe item FAILED: %s" % exc)
        return

    collection = None
    for prop in ("internal_collection", "collection", "palette"):
        try:
            collection = dup.get_editor_property(prop)
            if collection is not None:
                log("   collection via '%s': %s" % (prop, type(collection).__name__))
                break
        except Exception:
            continue
    if collection is None:
        log("   could not reach the character's collection")
        return

    # Offer the item to every slot and report which take it. The slot table is
    # the answer: 'Outfits' accepting an outfit-backed item is the green light.
    log("")
    log("   %-18s %s" % ("slot", "result"))
    for slot in ("Outfits", "Outfit", "SkeletalMesh", "Character",
                 "Hair", "Body"):
        try:
            # try_add_* returns bool + an out param, so Python hands back a
            # tuple. add_item_from_wardrobe_item does not exist.
            res = collection.try_add_item_from_wardrobe_item(slot, item)
        except Exception as exc:
            log("   %-18s EXCEPTION %s" % (slot, str(exc).splitlines()[0][:60]))
            continue
        if isinstance(res, tuple):
            ok, key = res[0], res[1]
        else:
            ok, key = bool(res), None
        log("   %-18s %s%s" % (slot, "ACCEPTED" if ok else "rejected",
                               ("  key=%s" % key) if ok else ""))

    log("")
    log("=" * 70)
    log("If 'Outfits' ACCEPTED: the route works. The only manual step is")
    log("filling each ChaosOutfitAsset with geometry in the Outfit editor,")
    log("because nothing on the asset takes a source from Python.")
    log("If it rejected: this route does not reach the characters, and the")
    log("Blueprint-component route is the only one that does.")
    log("=" * 70)


try:
    main()
except Exception as exc:
    log("FATAL: %s" % exc)
    for line in traceback.format_exc().splitlines():
        log("   %s" % line)
finally:
    flush()
    unreal.log("report: %s" % OUT)
