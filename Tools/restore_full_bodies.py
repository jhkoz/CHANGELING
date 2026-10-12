"""
restore_full_bodies.py -- give the 6 Kith Blueprints whose body geometry was
culled under a Fab casual outfit a FULL body again.

A MetaHuman baked while wearing a fitted garment has the covered geometry
DELETED (~21,600 verts vs 32,334, ~12.4 MB vs ~16.3 MB on disk). Clearing the
"Outfits" slot and re-baking restores it -- the MetaHumanCharacter source is
intact, only the baked mesh was lossy.

Non-destructive by design: the rebuilt body is saved BESIDE the culled one as
`SKM_..._BodyMesh_Full` and the Blueprint is repointed at it. Nothing that
referenced the old asset changes, and reverting is one property edit.

ONE BUILD PER INVOCATION. A MetaHuman build needs a fresh, fully-started editor
(see the CHANGELING editor-stability notes: looping them exhausts VRAM, and
running the script at editor launch crashes in TryBakeMaterials). Harvesting is
cheap, so every run harvests everything outstanding first, then builds at most
one. Drive it with Tools/dress_build_sweep.sh-style relaunching; it is
idempotent and safe to re-run until it reports ALL DONE.
"""

import os
import time
import traceback

import unreal

PROJECT = unreal.Paths.project_dir()
REPORT = os.path.join(PROJECT, "Tools", "Export", "restore_full_bodies.txt")
SCRATCH = "/Game/_Scratch/RestoreFullBodies"
BP_DIR = "/Game/_Custom/Characters/Kith/Blueprints"
KITH_ROOT = "/Game/_Custom/Characters/Kith"

# Measured 2026-09-22: these 6 Blueprints reference a culled body.
TARGETS = [
    ("BP_Clurichaun_Childling_Female", "Clurichaun/MHC_Clurichaun_Childling_Female"),
    ("BP_Clurichaun_Wilder_Female", "Clurichaun/MHC_Clurichaun_Wilder_Female"),
    ("BP_Clurichaun_Wilder_Male", "Clurichaun/MHC_Clurichaun_Wilder_Male"),
    ("BP_Eshu_Childling_Female", "Eshu/MHC_Eshu_Childling_Female"),
    ("BP_Eshu_Childling_Male", "Eshu/MHC_Eshu_Childling_Male"),
    ("BP_Eshu_Wilder_Female", "Eshu/MHC_Eshu_Wilder_Female"),
]

LINES = []


def log(m=""):
    LINES.append(str(m))
    unreal.log(str(m))
    try:
        d = os.path.dirname(REPORT)
        if not os.path.isdir(d):
            os.makedirs(d)
        with open(REPORT, "a", encoding="utf-8") as fh:
            print(m, file=fh)
    except Exception:
        pass


def body_component(bp):
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    for h in sub.k2_gather_subobject_data_for_blueprint(bp):
        d = sub.k2_find_subobject_data_from_handle(h)
        o = lib.get_object_for_blueprint(d, bp)
        if o is not None and o.get_name() == "CharacterMesh0":
            return o
    return None


def scratch_dir(char_name):
    return "%s/%s_NoOutfit" % (SCRATCH, char_name.replace("MHC_", ""))


def rebuilt_body(char_name):
    """The SKM_*_BodyMesh inside a finished scratch build, if there is one."""
    d = "%s/Body" % scratch_dir(char_name)
    for a in unreal.EditorAssetLibrary.list_assets(d, recursive=False) or []:
        if a.rsplit("/", 1)[-1].split(".")[0].endswith("_BodyMesh"):
            return a.split(".")[0]
    return None


def harvest(bp_name, char_name):
    """Copy the rebuilt body next to the culled one and repoint the Blueprint."""
    src = rebuilt_body(char_name)
    if not src:
        return False, "no rebuilt body in scratch"

    bp = unreal.load_asset("%s/%s" % (BP_DIR, bp_name))
    comp = body_component(bp)
    if comp is None:
        return False, "no CharacterMesh0"
    current = comp.get_editor_property("skeletal_mesh_asset")
    if current is None:
        return False, "CharacterMesh0 has no mesh"

    if current.get_name().endswith("_Full"):
        return True, "already repointed (%s)" % current.get_name()

    # Put the full body beside the culled one it replaces.
    folder = current.get_path_name().rsplit("/", 1)[0].rsplit(".", 1)[0]
    folder = folder.rsplit("/", 1)[0] if "." in folder else folder
    folder = current.get_path_name().split(".")[0].rsplit("/", 1)[0]
    dst = "%s/%s_Full" % (folder, current.get_name())

    eal = unreal.EditorAssetLibrary
    if not eal.does_asset_exist(dst):
        if not eal.duplicate_asset(src, dst):
            return False, "duplicate_asset failed -> %s" % dst
    full = unreal.load_asset(dst)
    if full is None:
        return False, "could not load %s" % dst

    comp.set_editor_property("skeletal_mesh_asset", full)
    eal.save_asset(dst, only_if_is_dirty=False)
    eal.save_asset("%s/%s" % (BP_DIR, bp_name), only_if_is_dirty=False)
    return True, "%s -> %s" % (current.get_name(), full.get_name())


def build_one(char_path):
    sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
    ch = unreal.load_asset(char_path)
    if ch is None:
        return False, "could not load character"
    if not sub.try_add_object_to_edit(ch):
        return False, "try_add_object_to_edit failed"
    try:
        coll = ch.get_editor_property("internal_collection")
        inst = coll.get_editor_property("default_instance")
        # A null item key means "nothing selected for this slot".
        inst.set_single_slot_selection(
            "Outfits", unreal.MetaHumanPaletteItemKey())

        params = unreal.MetaHumanCharacterEditorBuildParameters()
        params.set_editor_property("absolute_build_path", SCRATCH)
        params.set_editor_property(
            "name_override", ch.get_name().replace("MHC_", "") + "_NoOutfit")
        t0 = time.time()
        sub.build_meta_human(ch, params)
        # Output is left as dirty in-memory packages; without this the folder
        # stays empty and the log still claims success.
        unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
        made = unreal.EditorAssetLibrary.list_assets(
            scratch_dir(ch.get_name()), recursive=True) or []
        return bool(made), "%d assets in %.1f min" % (
            len(made), (time.time() - t0) / 60.0)
    finally:
        try:
            sub.remove_object_to_edit(ch)
        except Exception:
            pass


def main():
    log("")
    log("=" * 70)
    log("restore_full_bodies  %s" % time.strftime("%Y-%m-%d %H:%M:%S"))
    log("=" * 70)

    pending_build = []
    for bp_name, rel in TARGETS:
        char_path = "%s/%s" % (KITH_ROOT, rel)
        char_name = rel.rsplit("/", 1)[-1]
        if rebuilt_body(char_name):
            ok, why = harvest(bp_name, char_name)
            log("  harvest %-34s %-4s %s" % (bp_name, "OK" if ok else "FAIL", why))
        else:
            pending_build.append((bp_name, char_path, char_name))

    log("")
    log("still to rebuild: %d" % len(pending_build))
    for bp_name, _, _ in pending_build:
        log("    %s" % bp_name)

    if not pending_build:
        log("")
        log("ALL DONE")
        return

    bp_name, char_path, char_name = pending_build[0]
    log("")
    log("--- rebuilding %s (no outfit)" % char_name)
    ok, why = build_one(char_path)
    log("    %s: %s" % ("OK" if ok else "FAILED", why))
    log("REMAINING AFTER THIS RUN: %d" % (len(pending_build) - 1))


try:
    main()
except Exception:
    log(traceback.format_exc())

try:
    unreal.SystemLibrary.quit_editor()
except Exception:
    pass
