"""Which Kith characters accept an Outfits wardrobe item, and which refuse?

Distinguishes "MHC_Boggan_Grump_Male is individually broken" from "characters
that have never carried clothing refuse", which would matter for every other
kith before they can be dressed.

Read-only: nothing is selected, saved or built.
"""
import unreal

KITH = "/Game/_Custom/Characters/Kith"
SCRATCH = "/Game/_Scratch/GrumpProbe"
OUTFIT = "/Game/_Custom/Outfits/OA_Boggan_Wilder_Male"

SUBJECTS = [
    "Boggan/MHC_Boggan_Grump_Male",
    "Boggan/MHC_Boggan_Wilder_Male",
    "Boggan/MHC_Boggan_Childling_Male",
    "Boggan/MHC_Boggan_Grump_Female",
    "Boggan/MHC_Boggan_Wilder_Female",
    "Sidhe/MHC_Sidhe_Wilder_Male",
    "Eshu/MHC_Eshu_Wilder_Male",
    "Clurichaun/MHC_Clurichaun_Grump_Male",
]

cls = unreal.load_object(None,
    "/MetaHumanCharacter/BuildPipeline/DefaultWardobePipeline/"
    "DefaultOutfitPipeline.DefaultOutfitPipeline_C")
outfit = unreal.load_asset(OUTFIT)

unreal.log("%-44s %-10s %s" % ("character", "outfits", "already has Clothing/"))
for rel in SUBJECTS:
    c = unreal.load_asset("%s/%s" % (KITH, rel))
    name = rel.rsplit("/", 1)[-1]
    if c is None:
        unreal.log("%-44s could not load" % name)
        continue
    coll = c.get_editor_property("internal_collection")
    path = "%s/WI_W_%s" % (SCRATCH, name[-12:])
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    it = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "WI_W_%s" % name[-12:], SCRATCH, unreal.MetaHumanWardrobeItem,
        unreal.MetaHumanWardrobeItemFactory())
    it.set_editor_property("principal_asset", outfit)
    if cls:
        it.set_editor_property("pipeline", unreal.new_object(cls, outer=it))
    try:
        key = coll.try_add_item_from_wardrobe_item("Outfits", it)
        if isinstance(key, tuple):
            key = key[1] if len(key) > 1 else None
        ok = key is not None and key.references_external_wardrobe_item
    except Exception:
        ok = False
    dressed = unreal.EditorAssetLibrary.does_asset_exist(
        "%s/%s/Clothing/%s_Outfits" % (KITH, rel, name))
    unreal.log("%-44s %-10s %s" % (name, "ACCEPT" if ok else "refuse", dressed))
