"""
repoint_boggan_blueprints.py -- point the male Boggan Blueprints at the live
character folders instead of the stale ones.

    python Tools/ue_remote.py Tools/repoint_boggan_blueprints.py

THE SPLIT

Boggan assets exist under two spellings, and the Blueprints use the wrong one:

    BP_Boggan_Wilder_Male  ->  SKM_MHC_Boggan_Male_Wilder_BodyMesh   (reversed)
    assembly builds into       MHC_Boggan_Wilder_Male/               (forward)

The reversed folders are legacy output from 2026-09-13/14 with no
MetaHumanCharacter asset behind them -- nothing can rebuild them, and nothing
has. Every outfit assembled today went into the forward folders, which is why
the level kept showing undressed characters however many times the assets were
verified correct.

That split has caused three separate problems today: an outfit fitted to one
body and worn by another, a "coincidence" where Childling measured the same
size before and after a build because nothing was changing, and garments
conformed against a body the Blueprint does not use.

WHAT THIS DOES

Repoints FaceMesh, the body Mesh and every groom at the forward-named folder,
and fills the Outfit slot from the same place, so a character's parts all come
from the build that actually produced them. Verifies by reading back from
disk. The reversed folders are left on disk untouched -- nothing here deletes
anything.
"""

import os

import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "repoint_blueprints.txt")

KITH = "/Game/_Custom/Characters/Kith"
BP_DIR = "%s/Blueprints" % KITH

# (blueprint, forward-named character folder)
PAIRS = [
    ("BP_Boggan_Childling_Male", "Boggan/MHC_Boggan_Childling_Male"),
    ("BP_Boggan_Wilder_Male",    "Boggan/MHC_Boggan_Wilder_Male"),
    ("BP_Boggan_Grump_Male",     "Boggan/MHC_Boggan_Grump_Male"),
]

GROOM_SLOTS = ["Hair", "Eyebrows", "Eyelashes", "Beard", "Mustache",
               "Goatee", "Peachfuzz", "Fuzz"]

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


def grooms_in(folder):
    """{slot: (groom, binding)} from a character's own Grooms folder."""
    out = {}
    groom_dir = "%s/%s/Grooms" % (KITH, folder)
    if not unreal.EditorAssetLibrary.does_directory_exist(groom_dir):
        return out
    paths = sorted(p.split(".")[0] for p in
                   unreal.EditorAssetLibrary.list_assets(
                       groom_dir, recursive=False, include_folder=False))
    bindings = {}
    grooms = []
    for path in paths:
        name = path.rsplit("/", 1)[-1]
        if "_CardsMesh" in name or "_Helmet" in name or name.startswith("MI_"):
            continue
        asset = unreal.load_asset(path)
        if isinstance(asset, unreal.GroomBindingAsset):
            bindings[name] = asset
        elif isinstance(asset, unreal.GroomAsset):
            grooms.append((name, asset))

    def slot_for(text):
        for candidate in GROOM_SLOTS:
            if text.startswith(candidate + "_"):
                return candidate
        return None

    claimed = set()
    for name, asset in grooms:
        binding_name = "%s_Binding" % name if "%s_Binding" % name in bindings else None
        slot = slot_for(name)
        if binding_name is None:
            for candidate in sorted(bindings):
                if candidate in claimed:
                    continue
                if slot is None and slot_for(candidate) is None:
                    continue
                binding_name = candidate
                break
        if binding_name:
            claimed.add(binding_name)
            if slot is None:
                slot = slot_for(binding_name)
        if slot:
            out[slot] = (asset, bindings.get(binding_name))
    return out


def main():
    log("=" * 70)
    log("repoint_boggan_blueprints")
    log("=" * 70)

    done, failed = 0, 0
    for bp_name, folder in PAIRS:
        log("")
        log("=== %s  ->  %s" % (bp_name, folder))
        bp_path = "%s/%s" % (BP_DIR, bp_name)
        bp = unreal.load_asset(bp_path)
        if bp is None:
            log("   FAILED: could not load")
            failed += 1
            continue

        name = folder.rsplit("/", 1)[-1]
        body = unreal.load_asset("%s/%s/Body/SKM_%s_BodyMesh" % (KITH, folder, name))
        face = unreal.load_asset("%s/%s/Face/SKM_%s_FaceMesh" % (KITH, folder, name))
        outfit = unreal.load_asset("%s/%s/Clothing/%s_Outfits" % (KITH, folder, name))
        if body is None or face is None:
            log("   FAILED: body or face missing in the forward folder")
            failed += 1
            continue

        found = components_of(bp)
        if "Mesh" in found:
            found["Mesh"].set_editor_property("skeletal_mesh_asset", body)
            log("   Mesh     <- %s" % body.get_name())
        if "FaceMesh" in found:
            found["FaceMesh"].set_editor_property("skeletal_mesh_asset", face)
            log("   FaceMesh <- %s" % face.get_name())
        if "Outfit" in found and outfit is not None:
            found["Outfit"].set_editor_property("skeletal_mesh_asset", outfit)
            log("   Outfit   <- %s" % outfit.get_name())

        for slot, (groom, binding) in grooms_in(folder).items():
            if slot not in found:
                continue
            found[slot].set_editor_property("groom_asset", groom)
            if binding is not None:
                found[slot].set_editor_property("binding_asset", binding)
            log("   %-8s <- %s" % (slot, groom.get_name()))

        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        if not unreal.EditorAssetLibrary.save_asset(bp_path):
            log("   FAILED to save")
            failed += 1
            continue
        done += 1

    log("")
    log("=" * 70)
    log("VERIFY -- read back from disk")
    log("=" * 70)
    for bp_name, _ in PAIRS:
        bp = unreal.load_asset("%s/%s" % (BP_DIR, bp_name))
        if bp is None:
            log("   %-28s NOT ON DISK" % bp_name)
            continue
        found = components_of(bp)
        bits = []
        for var in ("Mesh", "FaceMesh", "Outfit"):
            comp = found.get(var)
            if comp is None:
                bits.append("%s=absent" % var)
                continue
            mesh = comp.get_editor_property("skeletal_mesh_asset")
            bits.append("%s=%s" % (var, mesh.get_name() if mesh else "EMPTY"))
        log("   %s" % bp_name)
        for bit in bits:
            log("      %s" % bit)

    log("")
    log("repointed %d, failed %d" % (done, failed))
    log("")
    log("The placed actors in the level keep their own serialised component")
    log("values, so dress_level_actors still has to run after this.")
    log("=" * 70)


try:
    main()
finally:
    flush()
    unreal.log("report: %s" % OUT)
