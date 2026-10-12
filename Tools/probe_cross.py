"""Cross each Boggan male character against each outfit.

Tells apart the two remaining explanations:
  * Grump's CHARACTER is the problem -> it refuses every outfit
  * Grump's OUTFIT asset is the problem -> every character refuses it
"""
import unreal

KITH = "/Game/_Custom/Characters/Kith/Boggan"
OUT = "/Game/_Custom/Outfits"
SCRATCH = "/Game/_Scratch/GrumpProbe"
CHARS = ["MHC_Boggan_Grump_Male", "MHC_Boggan_Wilder_Male"]
OUTFITS = ["OA_Boggan_Grump_Male", "OA_Boggan_Wilder_Male"]


def item_for(outfit_name, tag):
    path = "%s/WI_X_%s" % (SCRATCH, tag)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    it = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "WI_X_%s" % tag, SCRATCH, unreal.MetaHumanWardrobeItem,
        unreal.MetaHumanWardrobeItemFactory())
    outfit = unreal.load_asset("%s/%s" % (OUT, outfit_name))
    it.set_editor_property("principal_asset", outfit)
    cls = unreal.load_object(None,
        "/MetaHumanCharacter/BuildPipeline/DefaultWardobePipeline/"
        "DefaultOutfitPipeline.DefaultOutfitPipeline_C")
    if cls:
        it.set_editor_property("pipeline", unreal.new_object(cls, outer=it))
    return it, outfit


unreal.log("%-28s %-28s %s" % ("character", "outfit", "result"))
for ci, cname in enumerate(CHARS):
    c = unreal.load_asset("%s/%s" % (KITH, cname))
    coll = c.get_editor_property("internal_collection") if c else None
    for oi, oname in enumerate(OUTFITS):
        it, outfit = item_for(oname, "%d%d" % (ci, oi))
        if coll is None or outfit is None:
            unreal.log("%-28s %-28s missing" % (cname, oname))
            continue
        try:
            key = coll.try_add_item_from_wardrobe_item("Outfits", it)
            if isinstance(key, tuple):
                key = key[1] if len(key) > 1 else None
            ok = key is not None and key.references_external_wardrobe_item
        except Exception as exc:
            ok = False
        unreal.log("%-28s %-28s %s" % (cname, oname, "ACCEPTED" if ok else "refused"))
