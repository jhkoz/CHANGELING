"""
wear_garments.py -- put each Kith Blueprint's garment on its Outfit component.

    python Tools/ue_remote.py Tools/wear_garments.py

This is the whole UE side now. No cloth asset, no outfit asset, no Dataflow,
no wardrobe item, no build_meta_human -- the garment is already a skeletal
mesh skinned to metahuman_base_skel, so it goes straight onto the component
that AChangelingNPC already carries.

BindOutfits gives it the body's pose at construction and ConfigureLODSync
picks it up by its OutfitTag, both of which are native and already working.

NAMING

Blueprints are forward-ordered (BP_SidheAutumn_Grump_Female) and garments are
named after the avatar they were fitted to, lowercased:

    BP_Boggan_Childling_Male   ->  Frock_boggan_childling_male
    BP_SidheAutumn_Grump_Female -> Gown_sidheautumn_grump_female

with one exception. MHC_Boggan_Grump_Female has never been assembled into a
forward-named folder, so her garment was fitted against the legacy reversed
blend and carries that spelling:

    BP_Boggan_Grump_Female     ->  Gown_boggan_female_grump

Both spellings are tried rather than special-casing her, because the same
thing may be true of characters nobody has looked at yet.

The level's placed actors keep their own serialised component values, so
dress_level_actors still has to run after this.
"""

import os

import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "wear_garments.txt")

BP_DIR = "/Game/_Custom/Characters/Kith/Blueprints"
GARMENTS = "/Game/_Custom/Garments"

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


def components_of(bp):
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    found = {}
    for handle in sub.k2_gather_subobject_data_for_blueprint(bp):
        data = lib.get_data(handle)
        obj = lib.get_object(data)
        if obj is None or not isinstance(obj, unreal.ActorComponent):
            continue
        try:
            var = str(lib.get_variable_name(data))
        except Exception:
            var = obj.get_name()
        found[var] = obj
    return found


def garment_for(bp_name):
    """Find the garment fitted to this Blueprint's character."""
    label = bp_name[3:] if bp_name.startswith("BP_") else bp_name
    parts = label.split("_")
    if len(parts) < 3:
        return None, []
    clan, seeming, sex = parts[0], parts[1], parts[2]
    prefix = "Gown" if sex.lower() == "female" else "Frock"
    candidates = [
        # make_outfit/export_outfit produce a joined four-piece garment named
        # Outfit_<clan>_<seeming>_<sex>. Tried first: where one exists it is
        # the current build, and the Frock/Gown names below are the older
        # single-piece garments it supersedes.
        "Outfit_%s_%s_%s" % (clan.lower(), seeming.lower(), sex.lower()),
        "%s_%s_%s_%s" % (prefix, clan.lower(), seeming.lower(), sex.lower()),
        # legacy reversed spelling, for characters never assembled forward
        "%s_%s_%s_%s" % (prefix, clan.lower(), sex.lower(), seeming.lower()),
    ]
    for name in candidates:
        asset = unreal.load_asset("%s/%s" % (GARMENTS, name))
        if asset is not None:
            return asset, candidates
    return None, candidates


def main():
    log("=" * 74)
    log("wear_garments")
    log("=" * 74)

    paths = sorted(p.split(".")[0] for p in
                   unreal.EditorAssetLibrary.list_assets(
                       BP_DIR, recursive=False, include_folder=False))

    dressed, already, missing, no_slot = 0, 0, 0, 0
    for path in paths:
        name = path.rsplit("/", 1)[-1]
        bp = unreal.load_asset(path)
        if bp is None:
            continue

        garment, tried = garment_for(name)
        if garment is None:
            log("   %-32s NO GARMENT (tried %s)" % (name, ", ".join(tried)))
            missing += 1
            continue

        found = components_of(bp)
        slot = found.get("Outfit")
        if slot is None:
            log("   %-32s no Outfit component -- restart the editor after the"
                " C++ build" % name)
            no_slot += 1
            continue

        current = slot.get_editor_property("skeletal_mesh_asset")
        if current is not None and current.get_name() == garment.get_name():
            already += 1
            continue

        slot.set_editor_property("skeletal_mesh_asset", garment)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        if not unreal.EditorAssetLibrary.save_asset(path):
            log("   %-32s FAILED to save" % name)
            continue
        log("   %-32s <- %s" % (name, garment.get_name()))
        dressed += 1

    log("")
    log("=" * 74)
    log("VERIFY -- read back from disk")
    log("=" * 74)
    empty = 0
    for path in paths:
        name = path.rsplit("/", 1)[-1]
        bp = unreal.load_asset(path)
        if bp is None:
            continue
        slot = components_of(bp).get("Outfit")
        if slot is None:
            log("   %-32s no Outfit component" % name)
            empty += 1
            continue
        mesh = slot.get_editor_property("skeletal_mesh_asset")
        tagged = slot.component_has_tag("Outfit")
        if mesh is None:
            empty += 1
        log("   %-32s %-34s tag=%s"
            % (name, mesh.get_name() if mesh else "*** EMPTY ***", tagged))

    log("")
    log("dressed %d, already correct %d, no garment %d, no slot %d, empty %d"
        % (dressed, already, missing, no_slot, empty))
    log("")
    log("Next: dress_level_actors, because placed actors carry their own")
    log("serialised component values and do not inherit this.")
    log("=" * 74)


try:
    main()
finally:
    flush()
    unreal.log("report: %s" % OUT)
