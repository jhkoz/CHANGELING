"""
rebuild_without_outfit.py

Clears the Outfits slot on a MetaHumanCharacter and re-bakes it, restoring the
full body geometry that gets culled under a fitted garment.

Why: four Kith females (Clurichaun/Eshu Childling+Wilder) wear Fab casuals, and
their baked bodies have the covered region DELETED -- 21,599 verts against
32,334. No dress can be cut from them; the shell build fails outright with "no
faces in that band". The MetaHumanCharacter sources are intact, so re-baking
without the outfit restores a full body.

Mechanism (measured, not guessed): a clothed character's instance carries 6 slot
selections including "Outfits"; an unclothed one carries 4 with no such slot.
MetaHumanCharacterInstance.set_single_slot_selection(slot, key) documents that
passing a null item key selects nothing for that slot.

THIS MODIFIES CHARACTER ASSETS. Back up first. Run with TEST_ONLY = True to
build into a scratch folder and inspect the result before doing it in place.

Run from the editor's Python console, or via remote execution.
"""

import os
import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
LOG_PATH = os.path.join(PROJECT, "Tools", "Export", "rebuild_log.txt")

# Verified on Eshu_Wilder_Female: the scratch rebuild restored 32,334 verts /
# 341 bones / 285 vertex groups, matching a healthy unclothed body exactly.
TARGETS = [
    "/Game/_Custom/Characters/Kith/Eshu/MHC_Eshu_Childling_Female",
    "/Game/_Custom/Characters/Kith/Clurichaun/MHC_Clurichaun_Childling_Female",
    "/Game/_Custom/Characters/Kith/Clurichaun/MHC_Clurichaun_Wilder_Female",
]

TEST_ONLY = True
SCRATCH_PATH = "/Game/_Scratch/RebuildTest"

# Each build takes several minutes. Doing one per invocation keeps every run
# inside a sane timeout, and already-built characters are skipped, so this is
# safe to call repeatedly until it reports nothing left -- an interrupted run
# costs nothing.
MAX_PER_RUN = 1

lines = []


def log(msg):
    lines.append(str(msg))
    unreal.log(str(msg))


def slot_names(inst):
    try:
        return [str(d.selection.slot_name) for d in inst.get_slot_selection_data()]
    except Exception as exc:
        return ["<unreadable: %s>" % exc]


def main():
    sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
    if sub is None:
        log("FATAL: no MetaHumanCharacterEditorSubsystem")
        return

    done_this_run = 0
    remaining = []

    for path in TARGETS:
        name = path.rsplit("/", 1)[-1]
        built = "%s/%s_NoOutfit" % (SCRATCH_PATH, name)
        if TEST_ONLY and unreal.EditorAssetLibrary.does_directory_exist(built):
            log("SKIP %s -- already built at %s" % (name, built))
            continue
        if done_this_run >= MAX_PER_RUN:
            remaining.append(name)
            continue

        log("=" * 64)
        log(path)
        ch = unreal.load_asset(path)
        if ch is None:
            log("   FAILED: could not load")
            continue
        done_this_run += 1

        if not sub.try_add_object_to_edit(ch):
            log("   FAILED: could not register for editing")
            continue

        try:
            coll = ch.get_editor_property("internal_collection")
            inst = coll.get_editor_property("default_instance")

            log("   slots before : %s" % slot_names(inst))

            if not sub.can_build_meta_human(ch, True):
                log("   WARNING: can_build_meta_human said no -- continuing to see why")

            # Null item key == nothing selected for this slot.
            empty = unreal.MetaHumanPaletteItemKey()
            inst.set_single_slot_selection("Outfits", empty)
            log("   slots after  : %s" % slot_names(inst))

            params = unreal.MetaHumanCharacterEditorBuildParameters()
            if TEST_ONLY:
                params.set_editor_property("absolute_build_path", SCRATCH_PATH)
                params.set_editor_property("name_override",
                                           ch.get_name() + "_NoOutfit")
                log("   TEST build -> %s" % SCRATCH_PATH)
            else:
                log("   IN-PLACE build")

            sub.build_meta_human(ch, params)
            log("   build_meta_human returned")

            # build_meta_human leaves its output as dirty in-memory packages.
            # Without this the folder looks empty and the build appears to have
            # silently done nothing.
            try:
                unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
                log("   saved dirty packages to disk")
            except Exception as exc:
                log("   WARNING: could not save packages: %s" % exc)

        except Exception as exc:
            log("   FAILED: %s" % exc)
        finally:
            try:
                sub.remove_object_to_edit(ch)
            except Exception:
                pass

    log("")
    if remaining:
        log("STILL TO BUILD (%d): %s" % (len(remaining), ", ".join(remaining)))
        log("Run this script again to continue.")
    else:
        log("ALL TARGETS BUILT.")
    log("")
    log("Check the resulting body mesh vert count. 32,334 = full body (good),")
    log("~21,600 = still culled (the outfit was not actually dropped).")

    with open(LOG_PATH, "w") as handle:
        handle.write("\n".join(lines))
    log("log: %s" % LOG_PATH)


main()
