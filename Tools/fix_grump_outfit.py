"""
fix_grump_outfit.py -- re-assemble MHC_Boggan_Grump_Male, then dress it.

    python Tools/ue_remote.py Tools/fix_grump_outfit.py

THE DIAGNOSIS

MHC_Boggan_Grump_Male refuses every wardrobe item offered to its 'Outfits'
slot, and so does MHC_Boggan_Grump_Female. Everything else tried accepts,
including characters that have never worn anything (Sidhe, Eshu, Clurichaun),
so it is not about being undressed. A fresh DUPLICATE of Grump refuses too, so
it is not wedged in-memory state either -- it is in the asset.

Crossing characters against outfits isolated it to the character, not the
outfit:

    Grump_Male  + Grump outfit    refused
    Grump_Male  + Wilder outfit   refused
    Wilder_Male + Grump outfit    ACCEPTED
    Wilder_Male + Wilder outfit   ACCEPTED

And the file dates name the cause: the two refusing characters were last
written 2026-09-18 11:13, while every Boggan that accepts is from 09-20. The
Grumps were never re-assembled by that pass and are still on the older
collection state.

THE TEST

Re-assemble Grump with its wardrobe untouched, then offer the outfit again. If
it accepts afterwards, the stale collection was the cause and the same fix
applies to Grump_Female. If it still refuses, the rebuild cost four minutes and
ruled the theory out.

SAFETY
Backs the character folder up first, and builds at HIGH -- the parameter
defaults to CINEMATIC, which is what took a face to 147 MB earlier in this
project.
"""

import os
import shutil
import time

import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "fix_grump.txt")
STAMP = time.strftime("%Y%m%d_%H%M")
BACKUP = os.path.join(PROJECT, "Tools", "Backup", "grump_fix_%s" % STAMP)

CHARACTER = "/Game/_Custom/Characters/Kith/Boggan/MHC_Boggan_Grump_Male"
OUTFIT = "/Game/_Custom/Outfits/OA_Boggan_Grump_Male"
OUTFIT_DIR = "/Game/_Custom/Outfits"
SLOT = "Outfits"
PIPELINE_BP = ("/MetaHumanCharacter/BuildPipeline/DefaultWardobePipeline/"
               "DefaultOutfitPipeline")

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


def back_up():
    # Boggan BAKED folders use reversed names (MHC_Boggan_Male_Grump) while the
    # character SOURCE asset is forward (MHC_Boggan_Grump_Male). Looking only
    # for the forward one found nothing and reported "nothing to back up" on a
    # character whose output is sitting right there under the other spelling.
    base = os.path.join(PROJECT, "Content", "_Custom", "Characters", "Kith",
                        "Boggan")
    src = None
    for candidate in ("MHC_Boggan_Grump_Male", "MHC_Boggan_Male_Grump"):
        path = os.path.join(base, candidate)
        if os.path.isdir(path):
            src = path
            break
    if src is None:
        log("   no baked folder under either spelling -- nothing to back up")
        return
    dst = os.path.join(BACKUP, os.path.basename(src))
    try:
        shutil.copytree(src, dst)
        log("   backed up %d file(s) -> %s"
            % (sum(len(f) for _, _, f in os.walk(dst)), dst))
    except Exception as exc:
        log("   BACKUP FAILED: %s" % exc)


def make_item():
    path = "%s/WI_OA_Boggan_Grump_Male" % OUTFIT_DIR
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    item = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "WI_OA_Boggan_Grump_Male", OUTFIT_DIR, unreal.MetaHumanWardrobeItem,
        unreal.MetaHumanWardrobeItemFactory())
    if item is None:
        return None
    item.set_editor_property("principal_asset", unreal.load_asset(OUTFIT))
    cls = unreal.load_object(None, PIPELINE_BP + ".DefaultOutfitPipeline_C")
    if cls is not None:
        item.set_editor_property("pipeline", unreal.new_object(cls, outer=item))
    return item


def offer(collection, item):
    try:
        key = collection.try_add_item_from_wardrobe_item(SLOT, item)
        if isinstance(key, tuple):
            key = key[1] if len(key) > 1 else None
        return (key is not None and key.references_external_wardrobe_item), key
    except Exception as exc:
        log("      exception: %s" % str(exc).splitlines()[0][:70])
        return False, None


def assemble(sub, character, label):
    params = unreal.MetaHumanCharacterEditorBuildParameters()
    params.set_editor_property("pipeline_quality",
                               unreal.MetaHumanQualityLevel.HIGH)
    start = time.time()
    log("   %s: building at HIGH ..." % label)
    sub.build_meta_human(character, params)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log("   %s: done in %.1f min" % (label, (time.time() - start) / 60.0))


def main():
    log("=" * 70)
    log("fix_grump_outfit   %s" % STAMP)
    log("=" * 70)

    character = unreal.load_asset(CHARACTER)
    if character is None:
        log("could not load %s" % CHARACTER)
        return
    collection = character.get_editor_property("internal_collection")
    item = make_item()
    if collection is None or item is None:
        log("missing collection or wardrobe item")
        return

    ok, _ = offer(collection, item)
    log("")
    log("before rebuild: %s" % ("ACCEPTED" if ok else "refused"))
    if ok:
        log("   already accepting -- nothing to fix here.")
        return

    log("")
    back_up()

    sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
    if sub is None:
        log("no MetaHumanCharacterEditorSubsystem")
        return
    # try_add_object_to_edit returns False when the character is ALREADY
    # registered, which earlier probes left it as -- they registered and never
    # released. Drop any existing registration first so the result means what
    # it says.
    try:
        sub.remove_object_to_edit(character)
    except Exception:
        pass
    if not sub.try_add_object_to_edit(character):
        log("could not register the character for editing")
        return

    try:
        assemble(sub, character, "refresh")

        # Offer it again against a freshly loaded collection: the rebuild
        # replaces the collection, so the handle captured above is stale.
        character = unreal.load_asset(CHARACTER)
        collection = character.get_editor_property("internal_collection")
        item = make_item()
        ok, key = offer(collection, item)
        log("")
        log("after rebuild: %s" % ("ACCEPTED" if ok else "still refused"))

        if not ok:
            log("")
            log("The stale-collection theory is wrong. The difference is")
            log("something else inside the 09-18 character asset, and the")
            log("remaining route is to rebuild the character from its source")
            log("the way the 09-20 pass did to its siblings.")
            return

        instance = collection.get_editor_property("default_instance")
        instance.set_single_slot_selection(SLOT, key)
        unreal.EditorLoadingAndSavingUtils.save_dirty_packages(False, True)
        log("   worn and saved")

        if not sub.try_add_object_to_edit(character):
            log("   could not re-register for editing")
            return
        assemble(sub, character, "dressed")
    finally:
        try:
            sub.remove_object_to_edit(character)
        except Exception:
            pass

    name = CHARACTER.rsplit("/", 1)[-1]
    built = unreal.load_asset("%s/Clothing/%s_Outfits" % (CHARACTER, name))
    log("")
    log("=" * 70)
    log("VERIFY: %s" % ("%s (%s)" % (built.get_name(), type(built).__name__)
                        if built else "no Outfits mesh"))
    log("backup: %s" % BACKUP)
    log("=" * 70)


try:
    main()
finally:
    flush()
    unreal.log("report: %s" % OUT)
