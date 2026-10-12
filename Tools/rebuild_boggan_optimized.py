"""
rebuild_boggan_optimized.py -- rebuild the dressed Boggans without the bloat.

    python Tools/ue_remote.py Tools/rebuild_boggan_optimized.py

WHY

Today's builds produced 167-193 MB face meshes against 11.8 MB originals, even
though pipeline_quality was set to HIGH. There are TWO parameters on
MetaHumanCharacterEditorBuildParameters and only one of them was set:

    pipeline_quality = MetaHumanQualityLevel.CINEMATIC        <- set to HIGH
    pipeline_type    = MetaHumanDefaultPipelineType.CINEMATIC <- missed

pipeline_type is the one that selects the output pipeline, and it defaults to
CINEMATIC. MetaHumanDefaultPipelineType also offers OPTIMIZED, which is the
game-ready one. Both are set here.

ONE AT A TIME BY DEFAULT

TARGETS holds a single character. A build takes 4-12 minutes, and the whole
point is to see whether OPTIMIZED actually changes the face size before
spending that time three more times -- the last assumption about a quality
parameter cost 36 minutes and three inflated characters. Add the rest once the
first one reports a sane number.

The wardrobe is left exactly as it is: no slot is touched, so a character that
is already dressed stays dressed and one that is not stays undressed. This
only re-bakes.
"""

import os
import time

import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "rebuild_optimized.txt")

KITH = "/Game/_Custom/Characters/Kith/Boggan"

# Start with one. The face size it reports decides whether the others follow.
# Wilder verified OPTIMIZED works: 186.4 MB -> 11.8 MB, outfit intact.
TARGETS = [
    # One at a time now. Batching cost two outfits: they were deleted to
    # defeat assembly's dirty tracking and the editor crashed before
    # regenerating either.
    "MHC_Boggan_Childling_Male",
]

ALL = [
    "MHC_Boggan_Wilder_Male",
    "MHC_Boggan_Childling_Male",
    "MHC_Boggan_Grump_Male",
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


def mb(package_path):
    rel = package_path.replace("/Game/", "Content/") + ".uasset"
    try:
        return os.path.getsize(
            os.path.join(PROJECT, rel.replace("/", os.sep))) / 1048576.0
    except OSError:
        return -1.0


def face_mb(name):
    return mb("%s/%s/Face/SKM_%s_FaceMesh" % (KITH, name, name))


def outfit_mb(name):
    return mb("%s/%s/Clothing/%s_Outfits" % (KITH, name, name))


def main():
    log("=" * 70)
    log("rebuild_boggan_optimized")
    log("=" * 70)
    log("pipeline_type = OPTIMIZED, pipeline_quality = HIGH")
    log("(the defaults are CINEMATIC for both)")

    sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
    if sub is None:
        log("FATAL: no MetaHumanCharacterEditorSubsystem")
        return

    for name in TARGETS:
        path = "%s/%s" % (KITH, name)
        log("")
        log("=" * 70)
        log(name)
        log("   face before  : %.1f MB" % face_mb(name))
        log("   outfit before: %.1f MB" % outfit_mb(name))

        character = unreal.load_asset(path)
        if character is None:
            log("   FAILED: could not load")
            continue

        # Earlier probes left characters registered, which makes
        # try_add_object_to_edit return False on the next run.
        try:
            sub.remove_object_to_edit(character)
        except Exception:
            pass
        if not sub.try_add_object_to_edit(character):
            log("   FAILED: could not register for editing")
            continue

        try:
            params = unreal.MetaHumanCharacterEditorBuildParameters()
            params.set_editor_property(
                "pipeline_type", unreal.MetaHumanDefaultPipelineType.OPTIMIZED)
            params.set_editor_property(
                "pipeline_quality", unreal.MetaHumanQualityLevel.HIGH)
            start = time.time()
            log("   building ...")
            sub.build_meta_human(character, params)
            unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
            log("   done in %.1f min" % ((time.time() - start) / 60.0))
        except Exception as exc:
            log("   FAILED: %s" % exc)
            continue
        finally:
            try:
                sub.remove_object_to_edit(character)
            except Exception:
                pass

        face = face_mb(name)
        log("   face after   : %.1f MB" % face)
        log("   outfit after : %.1f MB" % outfit_mb(name))
        if face > 30.0:
            log("   STILL INFLATED -- OPTIMIZED did not fix it. Stop here and")
            log("   do not rebuild the others; the cause is elsewhere.")
        else:
            log("   looks sane (originals are 11.8 MB). Add the rest to")
            log("   TARGETS and re-run.")

    log("")
    log("=" * 70)
    log("%-32s %10s %10s" % ("character", "face MB", "outfit MB"))
    for name in ALL:
        log("%-32s %10.1f %10.1f" % (name, face_mb(name), outfit_mb(name)))
    log("=" * 70)


try:
    main()
finally:
    flush()
    unreal.log("report: %s" % OUT)
