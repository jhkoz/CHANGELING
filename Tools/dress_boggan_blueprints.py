"""
dress_boggan_blueprints.py -- put the built outfits into the Outfit slot.

    python Tools/ue_remote.py Tools/dress_boggan_blueprints.py

RESTART THE EDITOR FIRST. Outfit is a new native UPROPERTY component on
AChangelingNPC; hot reload does not reliably apply a changed class layout, so
without a restart the slot may not exist to write to. The script says so rather
than guessing if it cannot find it.

WHAT IT DOES

Sets Outfit's skeletal mesh to the character's assembled
Clothing/<name>_Outfits, then compiles and saves. BindOutfits gives it the
body's pose at construction, and ConfigureLODSync picks it up by its OutfitTag.

THE BODY MISMATCH, WHICH IS REAL AND NOT FIXED HERE

Boggan assets exist under two spellings. The Blueprints reference the reversed
ones (SKM_MHC_Boggan_Male_Wilder_BodyMesh) while assembly built the outfits
against the forward-named characters (MHC_Boggan_Wilder_Male). Comparing the
two bodies:

    Wilder     forward 16,296,201 B   reversed 16,296,849 B   same character
    Childling  forward 12,266,376 B   reversed 16,280,124 B   DIFFERENT

MetaHuman fits an outfit to the body it assembles against, so Wilder's coat
fits either body and Childling's is cut for one its Blueprint does not use. It
may well look wrong on him -- sleeves and hem in the wrong place, or clipping.

That is left visible rather than papered over. The fix, if it does look wrong,
is to repoint that Blueprint at the forward-named folder entirely (body, face,
grooms), which is the character that was actually built and dressed; the
reversed folders are legacy from 2026-09-13/14. That is a bigger change than
filling a slot and should be made after looking at him, not before.
"""

import os

import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "dress_blueprints.txt")

KITH = "/Game/_Custom/Characters/Kith"
BP_DIR = "%s/Blueprints" % KITH

# (blueprint, character folder holding the assembled outfit)
PAIRS = [
    ("BP_Boggan_Wilder_Male",    "Boggan/MHC_Boggan_Wilder_Male"),
    ("BP_Boggan_Childling_Male", "Boggan/MHC_Boggan_Childling_Male"),
    # Grump joined once his collection stopped refusing wardrobe items. That
    # refusal was editor SESSION state, not anything in the asset -- a restart
    # cleared it. The duplicate that also refused had been made in the same
    # wedged session, which is why it looked like asset damage.
    ("BP_Boggan_Grump_Male",     "Boggan/MHC_Boggan_Grump_Male"),
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


def main():
    log("=" * 70)
    log("dress_boggan_blueprints")
    log("=" * 70)

    done, failed = 0, 0
    for bp_name, folder in PAIRS:
        log("")
        log("=== %s" % bp_name)
        bp_path = "%s/%s" % (BP_DIR, bp_name)
        bp = unreal.load_asset(bp_path)
        if bp is None:
            log("   FAILED: could not load %s" % bp_path)
            failed += 1
            continue

        char_name = folder.rsplit("/", 1)[-1]
        outfit_path = "%s/%s/Clothing/%s_Outfits" % (KITH, folder, char_name)
        outfit = unreal.load_asset(outfit_path)
        if outfit is None:
            log("   FAILED: no assembled outfit at %s" % outfit_path)
            failed += 1
            continue
        log("   outfit mesh: %s" % outfit.get_name())

        found = components_of(bp)
        if "Outfit" not in found:
            log("   FAILED: no 'Outfit' component on this Blueprint.")
            log("      The component is native on AChangelingNPC and was added")
            log("      this session. If the editor was not restarted after the")
            log("      build, hot reload will not have applied the new class")
            log("      layout. Restart and re-run.")
            log("      components seen: %s" % ", ".join(sorted(found)))
            failed += 1
            continue

        slot = found["Outfit"]
        try:
            slot.set_editor_property("skeletal_mesh_asset", outfit)
        except Exception as exc:
            log("   FAILED to set the mesh: %s" % exc)
            failed += 1
            continue

        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        if not unreal.EditorAssetLibrary.save_asset(bp_path):
            log("   FAILED to save")
            failed += 1
            continue
        log("   set and saved")
        done += 1

    log("")
    log("=" * 70)
    log("VERIFY -- read back from disk")
    log("=" * 70)
    for bp_name, _ in PAIRS:
        bp = unreal.load_asset("%s/%s" % (BP_DIR, bp_name))
        if bp is None:
            log("   %-32s NOT ON DISK" % bp_name)
            continue
        found = components_of(bp)
        slot = found.get("Outfit")
        if slot is None:
            log("   %-32s no Outfit component" % bp_name)
            continue
        mesh = slot.get_editor_property("skeletal_mesh_asset")
        tagged = slot.component_has_tag("Outfit")
        log("   %-32s %s   tagged=%s"
            % (bp_name, mesh.get_name() if mesh else "EMPTY", tagged))

    log("")
    log("set %d, failed %d" % (done, failed))
    log("")
    log("Now LOOK at them in the level. A slot that reads correct here only")
    log("means the reference is set; whether the coat FITS is a question the")
    log("viewport answers -- especially for Childling, whose outfit was cut")
    log("for a different body than his Blueprint uses.")
    log("=" * 70)


try:
    main()
finally:
    flush()
    unreal.log("report: %s" % OUT)
