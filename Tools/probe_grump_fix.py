"""
probe_grump_fix.py -- can MHC_Boggan_Grump_Male's collection be made to accept?

Writes nothing to the real character unless the slot add succeeds, and never
builds. Safe to run repeatedly.

MHC_Boggan_Grump_Male refuses EVERY slot -- Outfits, SkeletalMesh, Character,
Hair -- while its two siblings, on the same pipeline class and the same
collection class, accept Outfits. So the refusal is not about the slot or the
item; that character's collection is not in a state where anything can be
added.

Three candidates, cheapest first:
  A. it is not registered for editing, and the collection needs the subsystem
     to have it open before it will mutate;
  B. its package is read-only on disk, so the collection declines to change;
  C. the collection's pipeline was never initialised for this character, which
     would leave it with no slots to add into at all.
"""

import os
import stat

import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "grump_fix.txt")

CHARACTER = "/Game/_Custom/Characters/Kith/Boggan/MHC_Boggan_Grump_Male"
OUTFIT = "/Game/_Custom/Outfits/OA_Boggan_Grump_Male"
SCRATCH = "/Game/_Scratch/GrumpProbe"
SLOT = "Outfits"

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


def make_item():
    outfit = unreal.load_asset(OUTFIT)
    if outfit is None:
        return None
    path = "%s/WI_GrumpFix" % SCRATCH
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    item = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "WI_GrumpFix", SCRATCH, unreal.MetaHumanWardrobeItem,
        unreal.MetaHumanWardrobeItemFactory())
    if item is None:
        return None
    item.set_editor_property("principal_asset", outfit)
    cls = unreal.load_object(
        None,
        "/MetaHumanCharacter/BuildPipeline/DefaultWardobePipeline/"
        "DefaultOutfitPipeline.DefaultOutfitPipeline_C")
    if cls is not None:
        item.set_editor_property("pipeline", unreal.new_object(cls, outer=item))
    return item


def accepts(collection, item):
    try:
        key = collection.try_add_item_from_wardrobe_item(SLOT, item)
        if isinstance(key, tuple):
            key = key[1] if len(key) > 1 else None
        return key is not None and key.references_external_wardrobe_item, key
    except Exception as exc:
        return False, str(exc).splitlines()[0][:70]


def main():
    log("=" * 70)
    log("probe_grump_fix")
    log("=" * 70)

    character = unreal.load_asset(CHARACTER)
    if character is None:
        log("could not load %s" % CHARACTER)
        return
    collection = character.get_editor_property("internal_collection")
    item = make_item()
    if collection is None or item is None:
        log("missing collection or item")
        return

    # -------------------------------------------------- B: file permissions
    log("")
    log("B. IS THE PACKAGE WRITABLE?")
    disk = os.path.join(
        PROJECT, CHARACTER.replace("/Game/", "Content/").replace("/", os.sep)
        + ".uasset")
    try:
        mode = os.stat(disk).st_mode
        writable = bool(mode & stat.S_IWRITE)
        log("   %s" % disk)
        log("   writable: %s" % writable)
        if not writable:
            log("   -> read-only. Clearing the flag.")
            os.chmod(disk, mode | stat.S_IWRITE)
    except OSError as exc:
        log("   could not stat: %s" % exc)

    log("")
    log("   baseline (no changes): %s" % (accepts(collection, item)[0],))

    # ------------------------------------------------------ A: add to edit
    log("")
    log("A. REGISTER FOR EDITING FIRST")
    sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
    if sub is None:
        log("   no subsystem")
    else:
        registered = sub.try_add_object_to_edit(character)
        log("   try_add_object_to_edit: %s" % registered)
        ok, key = accepts(collection, item)
        log("   slot now: %s (key=%r)" % ("ACCEPTED" if ok else "refused", key))
        if ok:
            log("")
            log("   ANSWER: the character must be registered for editing")
            log("   BEFORE the wardrobe item is offered. wear_boggan_outfits")
            log("   does it the other way round, which is why the two that")
            log("   worked were already open from an earlier pass.")
            instance = collection.get_editor_property("default_instance")
            instance.set_single_slot_selection(SLOT, key)
            unreal.EditorLoadingAndSavingUtils.save_dirty_packages(False, True)
            log("   worn and saved -- re-run wear_boggan_outfits to build.")
            return
        try:
            sub.remove_object_to_edit(character)
        except Exception:
            pass

    # ------------------------------------------------------- C: the pipeline
    log("")
    log("C. IS THE PIPELINE INITIALISED?")
    try:
        pipeline = collection.get_editor_property("pipeline")
        log("   pipeline: %r" % pipeline)
        log("   can_build: %s"
            % (sub.can_build_meta_human(character, True) if sub else "?"))
    except Exception as exc:
        log("   unreadable: %s" % exc)

    log("")
    log("=" * 70)
    log("Still refusing. The remaining difference from its siblings is in the")
    log("character asset itself, and the cheapest route back is to rebuild")
    log("its collection the way the working two have it.")
    log("=" * 70)


try:
    main()
finally:
    flush()
    unreal.log("report: %s" % OUT)
