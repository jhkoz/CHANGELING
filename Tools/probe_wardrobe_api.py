"""
probe_wardrobe_api.py -- READ-ONLY probe of the MetaHuman wardrobe/costume
scripting surface in UE 5.7.

Answers two questions the C++ headers cannot:
  1. Which of the wardrobe/palette/Dataflow classes and methods actually reach
     Python (a UFUNCTION in a header is necessary, not sufficient).
  2. What the REAL slot names are on this project's collections, and which
     principal asset types each slot accepts -- specifically whether a plain
     SkeletalMesh (our 60 garments) is allowed in the outfit slot.

Mutates nothing. Run with:
    python Tools/ue_remote.py Tools/probe_wardrobe_api.py
"""

import os
import traceback

import unreal

REPORT = os.path.join(
    unreal.Paths.project_dir(), "Tools", "Export", "probe_wardrobe_api.txt")

LINES = []


def log(msg=""):
    LINES.append(str(msg))


def flush():
    d = os.path.dirname(REPORT)
    if not os.path.isdir(d):
        os.makedirs(d)
    with open(REPORT, "w", encoding="utf-8") as fh:
        fh.write("\n".join(LINES) + "\n")
    for line in LINES:
        unreal.log(line)


def head(title):
    log()
    log("=" * 70)
    log(title)
    log("=" * 70)


# ---------------------------------------------------------------- phase A
def probe_classes():
    head("A1. CLASSES REACHABLE FROM PYTHON")
    wanted = [
        "MetaHumanCharacter",
        "MetaHumanCharacterPalette",
        "MetaHumanCollection",
        "MetaHumanCharacterInstance",
        "MetaHumanWardrobeItem",
        "MetaHumanWardrobeItemFactory",
        "MetaHumanPaletteItemKey",
        "MetaHumanCharacterPaletteItem",
        "MetaHumanCharacterPipelineSpecification",
        "MetaHumanCharacterEditorPipelineSpecification",
        "MetaHumanItemPipeline",
        "MetaHumanSkeletalMeshPipeline",
        "MetaHumanDefaultSkeletalMeshPipeline",
        "MetaHumanOutfitPipeline",
        "MetaHumanCollectionPipeline",
        "MetaHumanCharacterEditorSubsystem",
        "ChaosOutfitAsset",
        "ChaosClothAsset",
        "Dataflow",
        "DataflowBlueprintLibrary",
        "DataflowEditorBlueprintLibrary",
    ]
    for name in wanted:
        log("  %-46s %s" % (name, "YES" if hasattr(unreal, name) else "-- no"))


def probe_methods():
    head("A2. KEY METHODS")
    checks = [
        ("MetaHumanCharacterPalette", "try_add_item_from_wardrobe_item"),
        ("MetaHumanCharacterPalette", "try_add_item_from_principal_asset"),
        ("MetaHumanCharacterPalette", "try_remove_item"),
        ("MetaHumanCharacterInstance", "set_single_slot_selection"),
        ("MetaHumanCollection", "build"),
        ("MetaHumanCharacterEditorSubsystem", "build_meta_human"),
        ("MetaHumanSkeletalMeshPipeline",
         "apply_skeletal_mesh_assembly_output_to_skeletal_mesh_component"),
    ]
    for cls_name, meth in checks:
        cls = getattr(unreal, cls_name, None)
        if cls is None:
            log("  %-34s . %-52s CLASS MISSING" % (cls_name, meth))
            continue
        log("  %-34s . %-52s %s"
            % (cls_name, meth, "YES" if hasattr(cls, meth) else "-- no"))

    for lib in ("DataflowBlueprintLibrary", "DataflowEditorBlueprintLibrary"):
        cls = getattr(unreal, lib, None)
        log()
        if cls is None:
            log("  %s : NOT EXPOSED" % lib)
            continue
        meths = sorted(m for m in dir(cls) if not m.startswith("_"))
        log("  %s (%d members):" % (lib, len(meths)))
        for m in meths:
            log("      %s" % m)


def probe_wardrobe_item_props():
    head("A3. MetaHumanWardrobeItem WRITABLE PROPERTIES")
    cls = getattr(unreal, "MetaHumanWardrobeItem", None)
    if cls is None:
        log("  class not exposed")
        return
    try:
        tmp = unreal.new_object(cls)
    except Exception as exc:
        log("  could not instantiate: %s" % exc)
        return
    for prop in ("principal_asset", "pipeline", "thumbnail_image",
                 "thumbnail_name"):
        try:
            val = tmp.get_editor_property(prop)
            log("  %-20s readable  (%r)" % (prop, val))
        except Exception as exc:
            log("  %-20s NOT readable: %s" % (prop, exc))


# ---------------------------------------------------------------- phase B
def find_characters():
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    try:
        flt = unreal.ARFilter(
            class_paths=[unreal.TopLevelAssetPath(
                "/Script/MetaHumanCharacterPalette", "MetaHumanCharacter")],
            package_paths=["/Game/_Custom/Characters/Kith"],
            recursive_paths=True)
        found = ar.get_assets(flt)
    except Exception:
        flt = unreal.ARFilter(
            class_names=["MetaHumanCharacter"],
            package_paths=["/Game/_Custom/Characters/Kith"],
            recursive_paths=True)
        found = ar.get_assets(flt)
    return found


def probe_slots():
    head("B1. REAL SLOTS ON THIS PROJECT'S CHARACTERS")
    assets = find_characters()
    log("  MetaHumanCharacter assets under Kith: %d" % len(assets))
    if not assets:
        log("  none found -- cannot enumerate slots")
        return

    # Prefer a character we know carries an outfit, so the Outfits slot exists.
    clothed_hint = ("eshu", "sidhe", "boggan")
    ordered = sorted(
        assets,
        key=lambda a: 0 if any(h in str(a.package_name).lower()
                               for h in clothed_hint) else 1)

    for asset in ordered[:4]:
        pkg = str(asset.package_name)
        log()
        log("  --- %s" % pkg)
        try:
            char = asset.get_asset()
        except Exception as exc:
            log("      load failed: %s" % exc)
            continue

        coll = None
        for prop in ("internal_collection", "InternalCollection"):
            try:
                coll = char.get_editor_property(prop)
                break
            except Exception:
                continue
        if coll is None:
            log("      no internal_collection property")
            continue
        log("      collection: %s" % coll.get_name())

        # Existing palette items tell us the live slot names empirically.
        try:
            items = coll.get_editor_property("items")
            log("      items: %d" % len(items))
            for it in items:
                slot = it.get_editor_property("slot_name")
                wi = it.get_editor_property("wardrobe_item")
                principal = ""
                external = ""
                if wi:
                    try:
                        principal = str(wi.get_editor_property(
                            "principal_asset"))
                    except Exception:
                        principal = "<unreadable>"
                    try:
                        external = " external=%s" % wi.is_external()
                    except Exception:
                        external = ""
                log("        slot=%-16s wi=%-28s%s"
                    % (slot, wi.get_name() if wi else "<none>", external))
                if principal:
                    log("              principal: %s" % principal)
        except Exception as exc:
            log("      items unreadable: %s" % exc)

        # Slot selections currently applied.
        try:
            inst = coll.get_editor_property("default_instance")
            log("      default_instance: %s" % inst.get_name())
            for prop in ("slot_selection", "slot_selections",
                         "overridden_slot_selections"):
                try:
                    sel = inst.get_editor_property(prop)
                    log("        %s -> %r" % (prop, sel))
                except Exception:
                    pass
        except Exception as exc:
            log("      instance unreadable: %s" % exc)

        # The pipeline spec, if Python can see it at all.
        try:
            pipe = coll.get_editor_property("pipeline")
            log("      pipeline: %s (%s)"
                % (pipe.get_name() if pipe else "<none>",
                   pipe.get_class().get_name() if pipe else "-"))
            if pipe:
                for prop in ("specification", "Specification"):
                    try:
                        spec = pipe.get_editor_property(prop)
                        log("        specification: %s" % spec)
                        slots = spec.get_editor_property("slots")
                        log("        SLOTS (%d):" % len(slots))
                        for slot_name, slot in slots.items():
                            types = ""
                            try:
                                types = str(slot.get_editor_property(
                                    "supported_principal_asset_types"))
                            except Exception:
                                types = "<types unreadable>"
                            log("          %-20s %s" % (slot_name, types))
                        break
                    except Exception:
                        continue
                else:
                    log("        specification NOT reachable from Python "
                        "(bare UPROPERTY, no Edit/BlueprintReadOnly)")
        except Exception as exc:
            log("      pipeline unreadable: %s" % exc)


def probe_garments():
    head("B2. GARMENTS AVAILABLE")
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    try:
        flt = unreal.ARFilter(
            class_paths=[unreal.TopLevelAssetPath(
                "/Script/Engine", "SkeletalMesh")],
            package_paths=["/Game/_Custom/Garments"],
            recursive_paths=True)
        found = ar.get_assets(flt)
    except Exception as exc:
        log("  filter failed: %s" % exc)
        return
    log("  SkeletalMesh garments: %d" % len(found))
    for a in found[:3]:
        log("    %s" % a.package_name)


def probe_dataflow_templates():
    head("B3. SHIPPED OUTFIT DATAFLOW TEMPLATES")
    for path in ("/ChaosOutfitAsset/ResizeOutfitTemplate",
                 "/ChaosOutfitAsset/MakeResizableOutfitTemplate",
                 "/ChaosOutfitAsset/OutfitAssetTemplate",
                 "/ChaosOutfitAsset/EmptyOutfitAssetTemplate"):
        obj = unreal.load_asset(path)
        log("  %-52s %s"
            % (path, obj.get_class().get_name() if obj else "-- not loaded"))


def main():
    log("probe_wardrobe_api -- READ ONLY")
    log("project: %s" % unreal.Paths.project_dir())
    for fn in (probe_classes, probe_methods, probe_wardrobe_item_props,
               probe_slots, probe_garments, probe_dataflow_templates):
        try:
            fn()
        except Exception:
            head("EXCEPTION in %s" % fn.__name__)
            log(traceback.format_exc())
    log()
    log("report written to %s" % REPORT)
    flush()


main()
