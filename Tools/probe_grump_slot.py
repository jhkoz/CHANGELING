"""
probe_grump_slot.py -- why does one character's Outfits slot refuse the item?

READ-ONLY. Nothing is written, nothing is built.

MHC_Boggan_Childling_Male and MHC_Boggan_Wilder_Male both accepted the same
kind of wardrobe item and assembled with clothing. MHC_Boggan_Grump_Male
returns a null key from try_add_item_from_wardrobe_item, with an item whose
principal is the right ChaosOutfitAsset and whose pipeline instanced correctly
-- so the item is fine and the collection is refusing it.

Compares the refusing character against a working one across the things that
could differ: the collection's pipeline, which slots it advertises, and what
the Outfits slot already holds.
"""

import os

import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "grump_slot.txt")

KITH = "/Game/_Custom/Characters/Kith/Boggan"
SUBJECTS = [
    ("REFUSES", "%s/MHC_Boggan_Grump_Male" % KITH),
    ("works  ", "%s/MHC_Boggan_Wilder_Male" % KITH),
    ("works  ", "%s/MHC_Boggan_Childling_Male" % KITH),
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


def describe(label, path):
    log("")
    log("=" * 70)
    log("%s  %s" % (label, path.rsplit("/", 1)[-1]))
    log("=" * 70)

    character = unreal.load_asset(path)
    if character is None:
        log("   could not load")
        return

    collection = character.get_editor_property("internal_collection")
    if collection is None:
        log("   no internal_collection")
        return
    log("   collection: %s" % type(collection).__name__)

    # The pipeline decides which slots exist at all. A character built through
    # a different pipeline can simply have no Outfits slot to add to.
    try:
        pipeline = collection.get_editor_property("pipeline")
        log("   pipeline: %s" % (pipeline.get_name() if pipeline else "None"))
        if pipeline is not None:
            log("      class: %s" % type(pipeline).__name__)
            try:
                editor_pipeline = pipeline.get_editor_property(
                    "editor_pipeline")
                log("      editor_pipeline: %s" % type(editor_pipeline).__name__)
            except Exception as exc:
                log("      editor_pipeline unreadable: %s" % str(exc)[:60])
    except Exception as exc:
        log("   pipeline unreadable: %s" % str(exc)[:70])

    # What the instance currently wears.
    try:
        instance = collection.get_editor_property("default_instance")
        log("   instance: %s" % (instance.get_name() if instance else "None"))
        for prop in ("slot_selections", "selections", "overrides"):
            try:
                value = instance.get_editor_property(prop)
                log("      %-18s %r" % (prop, value))
            except Exception:
                continue
    except Exception as exc:
        log("   instance unreadable: %s" % str(exc)[:70])

    # Offer a throwaway item to each slot and see which answer comes back.
    # Nothing is saved, so this is safe to run on the real asset.
    outfit = unreal.load_asset("/Game/_Custom/Outfits/OA_Boggan_Grump_Male")
    if outfit is None:
        log("   (no probe outfit available for the slot test)")
        return
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    scratch = "/Game/_Scratch/GrumpProbe"
    item_path = "%s/WI_GrumpProbe" % scratch
    if unreal.EditorAssetLibrary.does_asset_exist(item_path):
        unreal.EditorAssetLibrary.delete_asset(item_path)
    item = tools.create_asset("WI_GrumpProbe", scratch,
                              unreal.MetaHumanWardrobeItem,
                              unreal.MetaHumanWardrobeItemFactory())
    if item is None:
        log("   could not make a probe item")
        return
    item.set_editor_property("principal_asset", outfit)
    cls = unreal.load_object(
        None,
        "/MetaHumanCharacter/BuildPipeline/DefaultWardobePipeline/"
        "DefaultOutfitPipeline.DefaultOutfitPipeline_C")
    if cls is not None:
        item.set_editor_property("pipeline", unreal.new_object(cls, outer=item))

    log("   slot test:")
    for slot in ("Outfits", "SkeletalMesh", "Character", "Hair"):
        try:
            key = collection.try_add_item_from_wardrobe_item(slot, item)
            if isinstance(key, tuple):
                key = key[1] if len(key) > 1 else None
            ok = key is not None and key.references_external_wardrobe_item
            log("      %-14s %s" % (slot, "ACCEPTED" if ok else "refused"))
        except Exception as exc:
            log("      %-14s EXCEPTION %s" % (slot, str(exc).splitlines()[0][:60]))


for label, path in SUBJECTS:
    try:
        describe(label, path)
    except Exception as exc:
        import traceback
        log("   EXCEPTION: %s" % exc)
        for line in traceback.format_exc().splitlines()[-3:]:
            log("      %s" % line)

flush()
unreal.log("report: %s" % OUT)
