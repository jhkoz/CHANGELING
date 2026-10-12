"""Does a fresh DUPLICATE of Grump accept the outfit?

If the duplicate accepts and the original refuses, the original's collection is
wedged and can be replaced. If both refuse, the cause is in the character data
itself and duplicating will not help.

Read-only on the real character; the duplicate lives in /Game/_Scratch.
"""
import unreal

SRC = "/Game/_Custom/Characters/Kith/Boggan/MHC_Boggan_Grump_Male"
DUP = "/Game/_Scratch/GrumpProbe/MHC_Grump_Dup"
OUTFIT = "/Game/_Custom/Outfits/OA_Boggan_Grump_Male"
SLOT = "Outfits"


def item_for(tag):
    path = "/Game/_Scratch/GrumpProbe/WI_%s" % tag
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    it = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "WI_%s" % tag, "/Game/_Scratch/GrumpProbe",
        unreal.MetaHumanWardrobeItem, unreal.MetaHumanWardrobeItemFactory())
    it.set_editor_property("principal_asset", unreal.load_asset(OUTFIT))
    cls = unreal.load_object(None,
        "/MetaHumanCharacter/BuildPipeline/DefaultWardobePipeline/"
        "DefaultOutfitPipeline.DefaultOutfitPipeline_C")
    if cls:
        it.set_editor_property("pipeline", unreal.new_object(cls, outer=it))
    return it


def test(label, character):
    coll = character.get_editor_property("internal_collection")
    it = item_for(label)
    try:
        key = coll.try_add_item_from_wardrobe_item(SLOT, it)
        if isinstance(key, tuple):
            key = key[1] if len(key) > 1 else None
        ok = key is not None and key.references_external_wardrobe_item
    except Exception as exc:
        ok, key = False, str(exc).splitlines()[0][:70]
    unreal.log("   %-12s %s  (key=%r)" % (label, "ACCEPTED" if ok else "refused", key))
    return ok


unreal.log("=== original ===")
orig = unreal.load_asset(SRC)
test("orig", orig)

unreal.log("=== duplicate ===")
if unreal.EditorAssetLibrary.does_asset_exist(DUP):
    unreal.EditorAssetLibrary.delete_asset(DUP)
dup = unreal.EditorAssetLibrary.duplicate_asset(SRC, DUP)
if dup is None:
    unreal.log("   duplicate FAILED")
else:
    test("dup", dup)
