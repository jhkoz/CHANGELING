"""
probe_outfit_asset.py -- can a Chaos Outfit Asset be built from Python?

READ-MOSTLY. Everything it creates goes to /Game/_Scratch/OutfitProbe. No real
Kith asset is opened for write, and no MetaHuman is built (that costs ~9 min).

WHY THIS EXISTS

probe_wardrobe_build already settled the plain-SkeletalMesh route, and the
answer was no. It got further than it looks:

    slot 'SkeletalMesh' selected on DefaultInstance
    can_build: True
    build returned + packages saved  (8.8 min)
    78 assets ... No obviously garment-named asset in the output.

So registration and wearing both SUCCEEDED from Python, the build ran to
completion, and the garment still did not come out the other end. Read with
probe_wardrobe_register's slot table:

    SkeletalMesh     ACCEPTED
    Outfits          rejected

the picture is consistent: 'Outfits' is the clothing slot and it refuses a
plain SkeletalMesh, while 'SkeletalMesh' accepts one but is not the clothing
path, so assembly ignored it. The MHC_*_Outfits assets already in the project
are /Script/Engine.SkeletalMesh because they are assembly OUTPUT, not input.

That leaves one question before the Blender coat and breeches can go in as
MetaHuman outfits: can a UChaosOutfitAsset be created and populated from
Python, or does it require the Dataflow graph editor by hand? This project
has already established that Chaos Cloth Dataflow node types are C++ structs
unreachable by name from Python, so this is a real risk, not a formality.

    python Tools/ue_remote.py Tools/probe_outfit_asset.py

What the answer means:
  * factory creates AND a source mesh can be added  -> scriptable end to end
  * factory creates but nothing can be added        -> Dataflow by hand, once
                                                       per garment, then the
                                                       rest can be scripted
  * no factory / no create                          -> fully manual authoring
"""

import os
import traceback

import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "outfit_asset_probe.txt")

SCRATCH = "/Game/_Scratch/OutfitProbe"
GARMENT = "/Game/_Custom/Garments/Coat_boggan_male_wilder"
# Falls back to a dress if the coat has not been imported yet.
GARMENT_FALLBACK = "/Game/_Custom/Garments/Dress_boggan_wilder_female_fitted_thin"

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


def surface(thing, label, keep=None):
    """Print the callable/property surface of a class or instance."""
    log("--- %s" % label)
    names = [n for n in dir(thing) if not n.startswith("_")]
    if keep:
        names = [n for n in names
                 if any(k.lower() in n.lower() for k in keep)]
    if not names:
        log("      (nothing matching)")
        return
    for name in sorted(names):
        try:
            attr = getattr(thing, name)
            kind = "call" if callable(attr) else "attr"
        except Exception:
            kind = "?"
        log("      %-4s %s" % (kind, name))


def main():
    log("=" * 70)
    log("probe_outfit_asset")
    log("=" * 70)

    # ---------------------------------------------------------- 1. classes
    log("")
    log("1. DO THE CLASSES REACH PYTHON?")
    for name in ("ChaosOutfitAsset", "ChaosOutfitAssetFactory",
                 "ChaosSizedOutfitSource", "ChaosOutfit",
                 "MetaHumanWardrobeItem", "MetaHumanCharacterPipelineSlot"):
        log("   %-34s %s"
            % (name, "yes" if hasattr(unreal, name) else "NOT EXPOSED"))

    if not hasattr(unreal, "ChaosOutfitAsset"):
        log("")
        log("VERDICT: ChaosOutfitAsset is not exposed to Python at all.")
        log("Outfit authoring is manual. Nothing further to test.")
        return

    # ------------------------------------------------------- 2. the surface
    log("")
    log("2. WHAT CAN BE SET")
    surface(unreal.ChaosOutfitAsset, "ChaosOutfitAsset",
            keep=["source", "outfit", "mesh", "add", "set", "build",
                  "body", "size", "piece", "dataflow"])
    if hasattr(unreal, "ChaosOutfitAssetFactory"):
        surface(unreal.ChaosOutfitAssetFactory, "ChaosOutfitAssetFactory",
                keep=["source", "outfit", "mesh", "set", "skeletal", "asset"])
    if hasattr(unreal, "ChaosSizedOutfitSource"):
        log("--- ChaosSizedOutfitSource (struct fields)")
        try:
            src = unreal.ChaosSizedOutfitSource()
            for name in sorted(n for n in dir(src) if not n.startswith("_")):
                try:
                    log("      %-28s = %r" % (name, getattr(src, name)))
                except Exception as exc:
                    log("      %-28s <unreadable: %s>" % (name, exc))
        except Exception as exc:
            log("      could not instantiate: %s" % exc)

    # ------------------------------------------------------- 3. create one
    log("")
    log("3. CREATE AN EMPTY OUTFIT ASSET IN SCRATCH")
    garment = unreal.load_asset(GARMENT) or unreal.load_asset(GARMENT_FALLBACK)
    if garment is None:
        log("   no garment mesh found -- tried:")
        log("     %s" % GARMENT)
        log("     %s" % GARMENT_FALLBACK)
    else:
        log("   garment: %s (%s)"
            % (garment.get_name(), type(garment).__name__))

    asset = None
    if hasattr(unreal, "ChaosOutfitAssetFactory"):
        name = "OA_Probe_Coat"
        path = "%s/%s" % (SCRATCH, name)
        try:
            if unreal.EditorAssetLibrary.does_asset_exist(path):
                unreal.EditorAssetLibrary.delete_asset(path)
            factory = unreal.ChaosOutfitAssetFactory()
            asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                name, SCRATCH, unreal.ChaosOutfitAsset, factory)
            log("   created: %s" % (asset.get_path_name() if asset else "FAILED"))
        except Exception as exc:
            log("   create FAILED: %s" % exc)
            for line in traceback.format_exc().splitlines()[-3:]:
                log("      %s" % line)

    # ------------------------------------------ 4. put the garment into it
    log("")
    log("4. CAN THE GARMENT MESH BE ADDED?")
    if asset is None or garment is None:
        log("   skipped -- need both an outfit asset and a garment")
    else:
        added = False
        # Try the property routes first; then any add/set method that takes
        # a mesh. Report each attempt, because which one works IS the answer.
        for prop in ("sources", "outfit_sources", "sized_outfit_sources",
                     "source_meshes", "outfit_pieces"):
            try:
                current = asset.get_editor_property(prop)
                log("   property '%s' readable: %s (len=%s)"
                    % (prop, type(current).__name__,
                       len(current) if hasattr(current, "__len__") else "n/a"))
            except Exception:
                continue
            if not hasattr(unreal, "ChaosSizedOutfitSource"):
                continue
            try:
                src = unreal.ChaosSizedOutfitSource()
                for field in ("source_asset", "outfit_asset", "skeletal_mesh",
                              "mesh", "source"):
                    try:
                        src.set_editor_property(field, garment)
                        log("      set ChaosSizedOutfitSource.%s" % field)
                        break
                    except Exception:
                        continue
                asset.set_editor_property(prop, [src])
                back = asset.get_editor_property(prop)
                log("      wrote 1 source -> reads back len=%s"
                    % (len(back) if hasattr(back, "__len__") else "n/a"))
                added = bool(back)
            except Exception as exc:
                log("      write FAILED: %s" % exc)

        for call in ("add_outfit_piece", "add_source", "add_sized_source",
                     "set_source_mesh", "add_piece"):
            if hasattr(asset, call):
                log("   method present: %s" % call)

        log("   ADDED: %s" % added)

    # --------------------------------- 5. will the Outfits slot take it?
    log("")
    log("5. DOES THE 'Outfits' SLOT ACCEPT AN OUTFIT-BACKED WARDROBE ITEM?")
    log("   (probe_wardrobe_register showed it REJECTS a plain SkeletalMesh)")
    if asset is None:
        log("   skipped -- no outfit asset to offer")
    else:
        try:
            wi_name = "WI_Probe_Coat"
            wi_path = "%s/%s" % (SCRATCH, wi_name)
            if unreal.EditorAssetLibrary.does_asset_exist(wi_path):
                unreal.EditorAssetLibrary.delete_asset(wi_path)
            item = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                wi_name, SCRATCH, unreal.MetaHumanWardrobeItem, None)
            if item is None:
                log("   could not create a wardrobe item")
            else:
                item.set_editor_property("principal_asset", asset)
                log("   wardrobe item holds: %s"
                    % type(item.get_editor_property("principal_asset")).__name__)
                log("   -> now offer this to the 'Outfits' slot with")
                log("      probe_wardrobe_register's step 4, which is the")
                log("      remaining unknown.")
        except Exception as exc:
            log("   FAILED: %s" % exc)
            for line in traceback.format_exc().splitlines()[-3:]:
                log("      %s" % line)

    log("")
    log("=" * 70)
    log("Read section 4. If a source went in and read back, the whole")
    log("conversion is scriptable. If not, each garment needs one pass")
    log("through the Outfit editor by hand -- and only that part is manual.")
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
