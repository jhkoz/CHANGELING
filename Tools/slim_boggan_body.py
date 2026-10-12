"""
slim_boggan_body.py -- bring a Boggan's girth down so tailored clothing fits.

    python Tools/ue_remote.py Tools/slim_boggan_body.py

WHY THESE MEASUREMENTS

The Childling male's parametric body reads:

    Height     135.0   (min 135 -- already as short as it goes)
    Chest       75.0   (min  75 -- already as slim as it goes)
    Underbust   85.0   Waist 80.0   High Hip 87.0   Hip 88.0
    Inseam      48.0   (min  48)

He is not uniformly bulky, he is pot-bellied: measured against the modular
suit fitted to his height, his belly is 34.1 cm deep against a 27.1 cm chest,
and the bands needing the most are the hip and waist, not the shoulders.

    z 0.62-0.75   34.1 cm vs 16.8 cm   -51%   hip/belly
    z 0.75-0.88   29.3 cm vs 16.7 cm   -43%   waist
    z 0.88-1.01   27.1 cm vs 15.8 cm   -42%   chest
    z 1.01-1.14   24.4 cm vs 15.6 cm   -36%   shoulders

So Height, Chest and Inseam are left alone -- they are at their minimums and
they are what make him a Boggan. Only the circumferences that build the belly
come down, plus the lower leg, which the trousers need.

The target is not "fits exactly": it is close enough that a x1.35 inflation on
the suit covers the rest, which is modest enough to keep the tailoring
readable. The number that decides is penetration, measured after rebuilding
the body. Under about 10% the conform only tidies edges; at 60% it flattens
the garment into a decal, which is what the frock coat did.
"""

import os
import shutil
import time

import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "slim_body.txt")
STAMP = time.strftime("%Y%m%d_%H%M")
BACKUP = os.path.join(PROJECT, "Tools", "Backup", "slim_%s" % STAMP)

CHARACTER = "/Game/_Custom/Characters/Kith/Boggan/MHC_Boggan_Childling_Male"

# measurement name -> target in cm. Anything not listed is left untouched.
TARGETS = {
    "Hip": 78.0,
    "High Hip": 72.0,
    "Waist": 66.0,
    "Underbust": 70.0,
    "Calf": 28.0,
    "Knee": 29.0,
    "Thigh": 40.0,
}

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


def back_up():
    """Copy the character asset out before changing its body."""
    rel = CHARACTER.replace("/Game/", "Content/") + ".uasset"
    src = os.path.join(PROJECT, rel.replace("/", os.sep))
    if not os.path.isfile(src):
        log("   nothing to back up at %s" % src)
        return
    if not os.path.isdir(BACKUP):
        os.makedirs(BACKUP)
    dst = os.path.join(BACKUP, os.path.basename(src))
    try:
        shutil.copy2(src, dst)
        log("   backed up -> %s (%.1f MB)"
            % (dst, os.path.getsize(dst) / 1048576.0))
    except Exception as exc:
        log("   BACKUP FAILED: %s" % exc)


def main():
    log("=" * 70)
    log("slim_boggan_body   %s" % STAMP)
    log("=" * 70)

    character = unreal.load_asset(CHARACTER)
    if character is None:
        log("could not load %s" % CHARACTER)
        return
    sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
    if sub is None:
        log("no MetaHumanCharacterEditorSubsystem")
        return

    back_up()

    # Registering for editing first: the body state is edited live and
    # committed, and an unregistered character has nothing to commit into.
    try:
        sub.remove_object_to_edit(character)
    except Exception:
        pass
    if not sub.try_add_object_to_edit(character):
        log("could not register the character for editing")
        return

    try:
        constraints = sub.get_body_constraints(character)
        log("")
        log("%-24s %8s %8s   %s" % ("measurement", "before", "after", "range"))

        changed = 0
        for constraint in constraints:
            name = str(constraint.get_editor_property("name"))
            if name not in TARGETS:
                continue
            before = constraint.get_editor_property("target_measurement")
            low = constraint.get_editor_property("min_measurement")
            high = constraint.get_editor_property("max_measurement")
            # Clamp rather than let the engine reject the whole set: Hip's own
            # minimum is 76, so a target below that is not reachable.
            target = max(low, min(high, TARGETS[name]))
            constraint.set_editor_property("target_measurement", target)
            constraint.set_editor_property("is_active", True)
            log("%-24s %8.1f %8.1f   %.0f..%.0f%s"
                % (name, before, target, low, high,
                   "  (clamped)" if abs(target - TARGETS[name]) > 0.01 else ""))
            changed += 1

        if not changed:
            log("no matching measurements found -- nothing changed")
            return

        sub.set_body_constraints(character, constraints)
        sub.commit_body_state(character)
        unreal.EditorAssetLibrary.save_asset(CHARACTER)
        log("")
        log("applied %d measurement(s) and saved" % changed)

        log("")
        log("READ BACK")
        for constraint in sub.get_body_constraints(character):
            name = str(constraint.get_editor_property("name"))
            if name in TARGETS:
                log("   %-22s %.1f"
                    % (name, constraint.get_editor_property("target_measurement")))
    finally:
        try:
            sub.remove_object_to_edit(character)
        except Exception:
            pass

    log("")
    log("=" * 70)
    log("Next: rebuild the character so the body mesh reflects this, then")
    log("re-export and re-fit to get the penetration number.")
    log("backup: %s" % BACKUP)
    log("=" * 70)


try:
    main()
finally:
    flush()
    unreal.log("report: %s" % OUT)
