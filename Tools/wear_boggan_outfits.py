"""
wear_boggan_outfits.py -- put the filled outfits on the male Boggans and build.

Run AFTER make_boggan_outfits.py and after filling each outfit asset by hand
in the Outfit editor.

    python Tools/ue_remote.py Tools/wear_boggan_outfits.py

WHAT IT DOES, PER CHARACTER
  1. Backs up the character's folder to Tools/Backup/mhc_outfit_<stamp>.
  2. Builds a MetaHumanWardrobeItem over the outfit asset, with the shipped
     DefaultOutfitPipeline instanced onto it.
  3. Adds it to the 'Outfits' slot and selects it on the default instance.
  4. Assembles the character IN PLACE at HIGH quality.

QUALITY, EXPLICITLY
MetaHumanCharacterEditorBuildParameters defaults pipeline_quality to
CINEMATIC. That default is what took a face from 12 MB to 147 MB on an earlier
rebuild in this project. It is set to HIGH here deliberately; change it only on
purpose.

SAFETY
  * An outfit asset still at its empty size is SKIPPED, not built. Building an
    empty outfit spends nine minutes producing a character with no clothes and
    overwrites the good one on the way.
  * The build writes over the character's own Body/Face/Grooms, so the folder
    is copied out first. An earlier in-place rebuild in this project ran with
    no backup and the day was spent recovering from it.
  * A null key back from the slot means the item was refused; that character is
    skipped rather than built.
"""

import os
import shutil
import time

import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "wear_boggan_outfits.txt")
STAMP = time.strftime("%Y%m%d_%H%M")
BACKUP = os.path.join(PROJECT, "Tools", "Backup", "mhc_outfit_%s" % STAMP)

KITH = "/Game/_Custom/Characters/Kith"
OUTFIT_DIR = "/Game/_Custom/Outfits"
SLOT = "Outfits"

# The shipped wardrobe pipeline for outfits. Note Epic's own typo in the path
# ("Wardobe"); it is correct as written.
PIPELINE_BP = ("/MetaHumanCharacter/BuildPipeline/DefaultWardobePipeline/"
               "DefaultOutfitPipeline")

# An outfit asset that has never been filled sits at ~4.7 KB. Anything below
# this is empty, whatever the Content Browser shows.
EMPTY_KB = 32.0

# Re-running costs ~4 minutes of assembly per character, so one that already
# carries a built Outfits mesh is left alone unless FORCE is set.
# Forced: Childling's slot selection still pointed at the Fab casual outfit
# (16 slots, MI_M_Casual_Shirt...) rather than his frock coat, and every recent
# run skipped him as "already built" so the selection was never revisited.
# Grump needs a rebuild too -- his face came out at 193 MB because this script
# was setting pipeline_quality without pipeline_type.
FORCE = True

# (character asset, outfit asset)
PAIRS = [
    # Childling alone. A plain re-bake reports "done in 0.0 min" on him --
    # assembly sees nothing dirty even though his outfit asset was rebuilt --
    # so he goes back through the wardrobe path, which re-selects the slot and
    # makes the character dirty enough to actually build.
    ("%s/Boggan/MHC_Boggan_Childling_Male" % KITH, "OA_Boggan_Childling_Male"),
]

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


def disk_path(package_path):
    rel = package_path.replace("/Game/", "Content/") + ".uasset"
    return os.path.join(PROJECT, rel.replace("/", os.sep))


def package_kb(package_path):
    try:
        return os.path.getsize(disk_path(package_path)) / 1024.0
    except OSError:
        return -1.0


def back_up(character_path):
    """Copy the character's folder out before assembly overwrites it."""
    src = os.path.join(PROJECT,
                       character_path.replace("/Game/", "Content/")
                       .replace("/", os.sep))
    if not os.path.isdir(src):
        return None
    dst = os.path.join(BACKUP, os.path.basename(src))
    try:
        shutil.copytree(src, dst)
        count = sum(len(f) for _, _, f in os.walk(dst))
        log("   backed up %d file(s) -> %s" % (count, dst))
        return dst
    except Exception as exc:
        log("   BACKUP FAILED: %s" % exc)
        return None


def make_wardrobe_item(outfit, name):
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    path = "%s/WI_%s" % (OUTFIT_DIR, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    item = tools.create_asset("WI_%s" % name, OUTFIT_DIR,
                              unreal.MetaHumanWardrobeItem,
                              unreal.MetaHumanWardrobeItemFactory())
    if item is None:
        return None
    item.set_editor_property("principal_asset", outfit)

    # A wardrobe item with a null Pipeline fails build validation. The shipped
    # default is a Blueprint, so instantiate its generated class outered to the
    # item (the property is EditAnywhere, Instanced).
    cls = unreal.load_object(None, PIPELINE_BP + ".DefaultOutfitPipeline_C")
    if cls is None:
        log("   could not load DefaultOutfitPipeline_C")
        return None
    item.set_editor_property("pipeline", unreal.new_object(cls, outer=item))
    return item


def main():
    log("=" * 70)
    log("wear_boggan_outfits   %s" % STAMP)
    log("=" * 70)

    sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
    if sub is None:
        log("FATAL: no MetaHumanCharacterEditorSubsystem")
        return

    done, skipped, failed = 0, 0, 0
    for character_path, outfit_name in PAIRS:
        name = character_path.rsplit("/", 1)[-1]
        outfit_path = "%s/%s" % (OUTFIT_DIR, outfit_name)
        log("")
        log("=" * 70)
        log(name)

        size = package_kb(outfit_path)
        if size < 0:
            log("   SKIPPED: no outfit asset at %s" % outfit_path)
            skipped += 1
            continue
        if size < EMPTY_KB:
            log("   SKIPPED: %s is still empty (%.1f KB). Fill it in the "
                "Outfit editor first." % (outfit_name, size))
            skipped += 1
            continue
        log("   outfit: %s (%.0f KB)" % (outfit_name, size))

        built = package_kb("%s/Clothing/%s_Outfits" % (character_path, name))
        if built > 1024.0 and not FORCE:
            log("   already built: Clothing/%s_Outfits is %.0f KB -- skipping "
                "(set FORCE = True to rebuild)" % (name, built))
            skipped += 1
            continue

        character = unreal.load_asset(character_path)
        outfit = unreal.load_asset(outfit_path)
        if character is None or outfit is None:
            log("   FAILED: could not load character or outfit")
            failed += 1
            continue

        back_up(character_path)

        collection = character.get_editor_property("internal_collection")
        if collection is None:
            log("   FAILED: no internal collection")
            failed += 1
            continue

        item = make_wardrobe_item(outfit, outfit_name)
        if item is None:
            log("   FAILED: could not build a wardrobe item")
            failed += 1
            continue

        key = collection.try_add_item_from_wardrobe_item(SLOT, item)
        if isinstance(key, tuple):
            key = key[1] if len(key) > 1 else None
        # references_external_wardrobe_item is an ATTRIBUTE; calling it raises.
        if key is None or not key.references_external_wardrobe_item:
            log("   FAILED: the '%s' slot refused the item" % SLOT)
            # Say WHY, rather than leaving the next person to guess. The most
            # common cause is the slot already holding an item from a previous
            # pass, which try_add_* declines to duplicate.
            log("      key returned: %r" % (key,))
            try:
                principal = item.get_editor_property("principal_asset")
                pipeline = item.get_editor_property("pipeline")
                log("      item principal: %s (%s)"
                    % (getattr(principal, "get_name", lambda: "None")(),
                       type(principal).__name__))
                log("      item pipeline : %s" % type(pipeline).__name__)
            except Exception as exc:
                log("      could not read the item back: %s" % exc)
            try:
                instance = collection.get_editor_property("default_instance")
                current = instance.get_single_slot_selection(SLOT)
                log("      slot already holds: %r" % (current,))
            except Exception as exc:
                log("      slot selection unreadable: %s" % exc)
            failed += 1
            continue
        log("   '%s' slot accepted" % SLOT)

        instance = collection.get_editor_property("default_instance")
        instance.set_single_slot_selection(SLOT, key)
        unreal.EditorLoadingAndSavingUtils.save_dirty_packages(False, True)
        log("   worn and saved")

        if not sub.try_add_object_to_edit(character):
            log("   FAILED: could not register for editing")
            failed += 1
            continue

        try:
            if not sub.can_build_meta_human(character, True):
                log("   WARNING: can_build_meta_human said no")

            params = unreal.MetaHumanCharacterEditorBuildParameters()
            # BOTH parameters, not just quality. They default to CINEMATIC
            # independently, and pipeline_type is the one that decides the
            # output pipeline. Setting only pipeline_quality here left Grump
            # with a 193 MB face against an 11.8 MB original -- the same
            # 16x inflation this script's header warns about, reintroduced
            # because the fix was applied to rebuild_boggan_optimized and not
            # carried across to here.
            params.set_editor_property(
                "pipeline_type", unreal.MetaHumanDefaultPipelineType.OPTIMIZED)
            params.set_editor_property(
                "pipeline_quality", unreal.MetaHumanQualityLevel.HIGH)
            log("   building at HIGH (default would be CINEMATIC) ...")
            start = time.time()
            sub.build_meta_human(character, params)

            # Output is dirty in-memory packages until this runs; without it
            # the folder looks untouched and the build appears to do nothing.
            unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
            log("   built in %.1f min" % ((time.time() - start) / 60.0))
            done += 1
        except Exception as exc:
            log("   FAILED: %s" % exc)
            failed += 1
        finally:
            try:
                sub.remove_object_to_edit(character)
            except Exception:
                pass

    log("")
    log("=" * 70)
    log("VERIFY -- did an Outfits mesh come out?")
    log("=" * 70)
    for character_path, _ in PAIRS:
        name = character_path.rsplit("/", 1)[-1]
        found = None
        # Assembly writes Clothing/ directly under the character folder, NOT
        # under a second folder named after the character. Getting that wrong
        # reported "no Outfits mesh" for three characters that had all built
        # correctly.
        for candidate in ("%s/Clothing/%s_Outfits" % (character_path, name),
                          "%s/Clothing/%s_Outfit" % (character_path, name)):
            asset = unreal.load_asset(candidate)
            if asset is not None:
                found = "%s (%s)" % (candidate.rsplit("/", 1)[-1],
                                     type(asset).__name__)
                break
        log("   %-34s %s" % (name, found or "no Outfits mesh"))

    log("")
    log("built %d, skipped %d, failed %d" % (done, skipped, failed))
    log("backups: %s" % BACKUP)
    log("=" * 70)


try:
    main()
finally:
    flush()
    unreal.log("report: %s" % OUT)
