"""
make_boggan_cloth.py -- create the empty Chaos Cloth Assets the outfits need.

Run BEFORE filling anything. Creates six empty cloth assets, one per garment,
so the only manual work left is wiring each graph.

    python Tools/ue_remote.py Tools/make_boggan_cloth.py

WHY A CLOTH ASSET AT ALL

The outfit's Make Outfit node does not accept a skeletal mesh. Its input type
is declared in ClothAssetAnyType.h as a policy over exactly three things:

    TObjectPtr<UChaosClothAssetBase>
    TObjectPtr<UChaosClothAsset>
    TObjectPtr<UChaosOutfitAsset>

-- asset objects, not meshes and not cloth collections. So each garment has to
become a UChaosClothAsset before it can enter an outfit, and the chain is:

    SkeletalMesh -> Chaos Cloth Asset -> Chaos Outfit Asset -> Outfits slot

None of the fill is scriptable. UChaosOutfitAsset keeps Pieces and Outfit as
bare UPROPERTY() with no EditAnywhere, and its Build() is UE_API C++ rather
than a UFUNCTION; both asset types are declared PrioritizeCategories=
("Dataflow") and are driven by an editor-only graph. Creating the containers is
the part that can be automated, so that is what this does.
"""

import os

import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "boggan_cloth.txt")

DEST = "/Game/_Custom/Outfits/Cloth"
GARMENTS = "/Game/_Custom/Garments"

# (cloth asset name, source skeletal mesh)
# One per CHARACTER, not per garment: Make Outfit has a single ClothAssets pin
# and the rest come from AddPins(), which is C++ only, so both garments are
# merged inside the cloth graph instead.
PIECES = [
    ("CA_Suit_Boggan_Childling_Male", "Coat_boggan_male_childling"),
    ("CA_Suit_Boggan_Grump_Male",     "Coat_boggan_male_grump"),
    ("CA_Suit_Boggan_Wilder_Male",    "Coat_boggan_male_wilder"),
    # The females wear the Witch Dresses gown, extracted as a single garment
    # from a mesh that held two side by side.
    ("CA_Gown_Boggan_Wilder_Female",    "Gown_boggan_female_wilder"),
    ("CA_Gown_Boggan_Grump_Female",     "Gown_boggan_female_grump"),
    ("CA_Gown_Boggan_Childling_Female", "Gown_boggan_female_childling"),
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
    rel = package_path.replace("/Game/", "Content/") + ".uasset"
    try:
        return os.path.getsize(
            os.path.join(PROJECT, rel.replace("/", os.sep))) / 1024.0
    except OSError:
        return -1.0


def main():
    log("=" * 70)
    log("make_boggan_cloth")
    log("=" * 70)

    if not hasattr(unreal, "ChaosClothAsset"):
        log("FATAL: ChaosClothAsset is not exposed to Python.")
        return

    factory = None
    for name in ("ChaosClothAssetFactory", "ClothAssetFactory"):
        if hasattr(unreal, name):
            factory = getattr(unreal, name)
            log("factory: unreal.%s" % name)
            break
    if factory is None:
        log("FATAL: no cloth asset factory reaches Python.")
        log("Create the six assets by hand instead:")
        for cloth_name, mesh in PIECES:
            log("   %-36s from %s" % (cloth_name, mesh))
        return

    made, existing, failed = 0, 0, 0
    for cloth_name, mesh_name in PIECES:
        path = "%s/%s" % (DEST, cloth_name)
        mesh = unreal.load_asset("%s/%s" % (GARMENTS, mesh_name))
        log("")
        log("=== %s" % cloth_name)
        log("   source mesh: %-34s %s"
            % (mesh_name, "ready" if mesh else "*** NOT IMPORTED ***"))

        if unreal.EditorAssetLibrary.does_asset_exist(path):
            log("   already exists (%.1f KB) -- left alone" % package_kb(path))
            existing += 1
            continue

        try:
            asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                cloth_name, DEST, unreal.ChaosClothAsset, factory())
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
    log("BY HAND, per cloth asset (6x):")
    log("  open it -> 'This Cloth Asset currently has no Dataflow graph' ->")
    log("  Create new Dataflow -> add a 'Skeletal Mesh Import' node, point it")
    log("  at the source mesh above -> connect it to the cloth 'Terminal'")
    log("  node -> save.")
    log("")
    log("THEN, per outfit asset (3x, in /Game/_Custom/Outfits):")
    log("  open it -> Create new Dataflow -> add 'Cloth Asset Make Outfit',")
    log("  use its AddPins to take the coat and breeches cloth assets ->")
    log("  connect to 'Outfit Terminal' -> save.")
    log("")
    log("Then: python Tools/ue_remote.py Tools/wear_boggan_outfits.py")
    log("=" * 70)


try:
    main()
finally:
    flush()
    unreal.log("report: %s" % OUT)
