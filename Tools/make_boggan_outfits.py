"""
make_boggan_outfits.py -- create the empty Chaos Outfit Assets to fill by hand.

Run this FIRST, then fill each asset in the Outfit editor, then run
wear_boggan_outfits.py.

    python Tools/ue_remote.py Tools/make_boggan_outfits.py

WHY THE FILL IS MANUAL

UChaosOutfitAsset holds its geometry in

    UPROPERTY()  TArray<FChaosOutfitPiece> Pieces;
    UPROPERTY()  TObjectPtr<UChaosOutfit> Outfit;

Bare UPROPERTY() -- no EditAnywhere, no BlueprintReadWrite -- so
set_editor_property cannot reach either, and Build() is UE_API C++ rather than
a UFUNCTION. The class is declared PrioritizeCategories=("Dataflow"): the asset
is driven by its Dataflow graph, and that graph is editor-only. Probing
confirmed it: every property route reported ADDED: False, and
ChaosOutfitAssetFactory's `formats` list is empty, so it is not a file importer
either (an FBX import through it produced nothing).

WHY IT HAS TO BE AN OUTFIT ASSET AT ALL

The wardrobe slots are typed, and the plugin's own Config/Editor.ini says:

    Outfits      -> ChaosOutfitAsset only
    SkeletalMesh -> Engine.SkeletalMesh, bPureVirtual=true

An earlier probe put a plain SkeletalMesh garment in the SkeletalMesh slot,
selected it, and ran a full 8.8-minute build -- and no garment came out. That
slot is pure-virtual; it accepts a mesh and emits nothing. Offering the same
wardrobe item to 'Outfits' was rejected. Re-tested with an item backed by a
ChaosOutfitAsset, 'Outfits' ACCEPTED. So the outfit asset is the only thing
that reaches an assembled character.

ONE OUTFIT PER CHARACTER, NOT PER GARMENT

A ChaosOutfitAsset holds several pieces, and assembly bakes the whole Outfits
slot into a single <Character>_Outfits skeletal mesh. So the coat and the
breeches belong in ONE asset per character, which is also why the slot takes a
single selection rather than a list.
"""

import os

import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "boggan_outfits.txt")

DEST = "/Game/_Custom/Outfits"
GARMENTS = "/Game/_Custom/Garments"

# (outfit asset name, [garment meshes that go into it])
#
# Mind the naming: the MetaHumanCharacter SOURCE assets are forward-ordered
# (MHC_Boggan_Wilder_Male) while the BAKED body folders the garments were cut
# from are reversed (MHC_Boggan_Male_Wilder), so the garment files carry the
# reversed form. These pairs are the mapping between them.
OUTFITS = [
    ("OA_Boggan_Childling_Male",
     ["Coat_boggan_male_childling", "Breeches_boggan_male_childling"]),
    ("OA_Boggan_Grump_Male",
     ["Coat_boggan_male_grump", "Breeches_boggan_male_grump"]),
    ("OA_Boggan_Wilder_Male",
     ["Coat_boggan_male_wilder", "Breeches_boggan_male_wilder"]),
    ("OA_Boggan_Wilder_Female",    ["Gown_boggan_female_wilder"]),
    ("OA_Boggan_Grump_Female",     ["Gown_boggan_female_grump"]),
    ("OA_Boggan_Childling_Female", ["Gown_boggan_female_childling"]),
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


def package_kb(package_path):
    """Size on disk. An unfilled outfit asset is a couple of KB; once it holds
    garment geometry it is orders of magnitude bigger. Pieces is not readable
    from Python, so this is how the fill gets verified."""
    rel = package_path.replace("/Game/", "Content/") + ".uasset"
    full = os.path.join(PROJECT, rel.replace("/", os.sep))
    try:
        return os.path.getsize(full) / 1024.0
    except OSError:
        return -1.0


def main():
    log("=" * 70)
    log("make_boggan_outfits")
    log("=" * 70)

    made, existing, failed = 0, 0, 0
    for name, garments in OUTFITS:
        path = "%s/%s" % (DEST, name)
        log("")
        log("=== %s" % name)

        for garment in garments:
            mesh = unreal.load_asset("%s/%s" % (GARMENTS, garment))
            log("   piece: %-34s %s"
                % (garment, "ready" if mesh else "*** NOT IMPORTED ***"))

        if unreal.EditorAssetLibrary.does_asset_exist(path):
            log("   already exists (%.1f KB) -- left alone" % package_kb(path))
            existing += 1
            continue

        try:
            asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                name, DEST, unreal.ChaosOutfitAsset,
                unreal.ChaosOutfitAssetFactory())
            if asset is None:
                log("   FAILED to create")
                failed += 1
                continue
            unreal.EditorAssetLibrary.save_asset(path)
            log("   created, empty (%.1f KB)" % package_kb(path))
            made += 1
        except Exception as exc:
            log("   FAILED: %s" % exc)
            failed += 1

    log("")
    log("=" * 70)
    log("created %d, already present %d, failed %d" % (made, existing, failed))
    log("")
    log("NOW, BY HAND, ONCE PER OUTFIT (this part has no Python route):")
    log("  1. Content Browser -> %s -> open the outfit asset." % DEST)
    log("  2. It opens on its Dataflow graph. Add the two garment meshes")
    log("     listed above as pieces, and set the body to that character's")
    log("     SKM_MHC_<name>_BodyMesh so the outfit fits the right shape.")
    log("  3. Save. The .uasset should grow from a couple of KB to MB --")
    log("     re-run this script and the size is printed back.")
    log("")
    log("Then: python Tools/ue_remote.py Tools/wear_boggan_outfits.py")
    log("=" * 70)


try:
    main()
finally:
    flush()
    unreal.log("report: %s" % OUT)
