"""
rebuild_faces.py

Re-assembles characters whose baked face and grooms came out empty.

Seven characters report `face (0 LODs), 0 groom(s)` at runtime. Their Blueprints
still carry FaceMesh, Hair and Eyebrows components, their face assets exist on
disk at 12 MB each, and every reference resolves -- the components simply come
up empty. Every one of them was saved on 2026-09-20 between 07:28 and 09:23, the
window in which those characters were re-assembled with Fab casual outfits, and
every healthy character was saved before that window. So the assembly pass that
morning wrote them without working face data.

Nothing under Kith/ was modified today, and none of this is in git (last commit
2026-08-20, zero Kith assets tracked), so re-assembly from the intact
MetaHumanCharacter sources is the only route back.

Unlike rebuild_without_outfit.py this leaves the Outfits slot ALONE and builds
IN PLACE -- the point is to restore these characters as they were meant to be,
not to produce a copy.

MUST run in the editor (build_meta_human needs a registered character):
    py "D:/CHANGELING/Tools/rebuild_faces.py"

Set TARGETS to one name first and look at the result before doing the rest.
"""

import os
import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
LOG_PATH = os.path.join(PROJECT, "Tools", "Export", "rebuild_faces_log.txt")

KITH = "/Game/_Custom/Characters/Kith"

# Trial run: one character, and deliberately not one of the Boggan duplicate
# pairs, so the result is unambiguous.
TARGETS = [
    "%s/Eshu/MHC_Eshu_Wilder_Female" % KITH,
]

# The full set, for when the trial checks out.
ALL_BROKEN = [
    "%s/Eshu/MHC_Eshu_Wilder_Female" % KITH,
    "%s/Boggan/MHC_Boggan_Childling_Female" % KITH,
    "%s/Boggan/MHC_Boggan_Childling_Male" % KITH,
    "%s/Boggan/MHC_Boggan_Wilder_Male" % KITH,
    "%s/Boggan/MHC_Boggan_Female_Childling" % KITH,
    "%s/Boggan/MHC_Boggan_Male_Childling" % KITH,
    "%s/Boggan/MHC_Boggan_Male_Wilder" % KITH,
]

_lines = []


def log(msg):
    text = str(msg)
    _lines.append(text)
    unreal.log(text)


def face_state(character_path):
    """Report the baked face mesh's LOD count, which is the symptom."""
    name = character_path.rsplit("/", 1)[-1]
    face = "%s/%s/Face/SKM_%s_FaceMesh" % (character_path, name, name)
    mesh = unreal.load_asset(face)
    if mesh is None:
        return "no face asset at %s" % face
    try:
        return "%d LOD(s), %d verts" % (
            mesh.get_lod_num(),
            unreal.get_editor_subsystem(
                unreal.SkeletalMeshEditorSubsystem).get_num_verts(mesh, 0))
    except Exception:
        return "face asset present"


def main():
    sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
    if sub is None:
        log("FATAL: no MetaHumanCharacterEditorSubsystem")
        return

    done, failed = 0, 0
    for path in TARGETS:
        name = path.rsplit("/", 1)[-1]
        log("=" * 64)
        log(name)

        character = unreal.load_asset(path)
        if character is None:
            log("   FAILED: could not load %s" % path)
            failed += 1
            continue

        log("   face before: %s" % face_state(path))

        if not sub.try_add_object_to_edit(character):
            log("   FAILED: could not register for editing")
            failed += 1
            continue

        try:
            if not sub.can_build_meta_human(character, True):
                log("   WARNING: can_build_meta_human said no -- see the log above")

            # In place, and the wardrobe untouched: no absolute_build_path and
            # no name_override, so assembly writes back over the character's
            # own folder exactly as the original pass did.
            params = unreal.MetaHumanCharacterEditorBuildParameters()
            sub.build_meta_human(character, params)
            log("   build_meta_human returned")

            # Output is dirty in-memory packages until this runs. Without it the
            # folder looks untouched and the build appears to have done nothing.
            unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
            log("   saved dirty packages")

            log("   face after : %s" % face_state(path))
            done += 1
        except Exception as exc:
            log("   FAILED: %s" % exc)
            failed += 1
        finally:
            try:
                sub.remove_object_to_edit(character)
            except Exception:
                pass

    log("")
    log("=" * 64)
    log("rebuilt %d, failed %d" % (done, failed))
    log("Now LOOK at the character in the level. A face LOD count above 0 means")
    log("the mesh baked; only the viewport tells you the head is actually there.")
    log("=" * 64)

    out_dir = os.path.dirname(LOG_PATH)
    if not os.path.isdir(out_dir):
        os.makedirs(out_dir)
    with open(LOG_PATH, "w") as handle:
        handle.write("\n".join(_lines))


main()
