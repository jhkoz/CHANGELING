"""
dress_kith.py -- produce a skinned garment per character, in one pass.

    python Tools/dress_kith.py --only Boggan
    python Tools/dress_kith.py --all
    python Tools/dress_kith.py --all --dry-run

Runs OUTSIDE Blender and drives it. The three steps it calls already exist and
are the ones that have worked reliably all along; this only decides who wears
what and reports the numbers that matter.

WHAT THIS REPLACES

Getting one garment onto one character had been running through:

    export body -> prep -> fit -> conform -> bind -> export        (Blender)
    import skeletal mesh                                           (UE)
    cloth asset -> Dataflow -> outfit asset -> Dataflow
      -> wardrobe item -> slot -> build_meta_human                 (UE)
    Blueprint slot -> level actor                                  (UE)

Every failure came from the middle block: five editor crashes in
FDataflowInstance::OnOwnerPostEditChangeProperty, assembly silently reusing a
stale mesh while reporting "done in 0.0 min", and one character left with
can_build=False after a scripted body edit.

That block exists only to turn a garment into <Character>_Outfits, and it is
not needed. AChangelingNPC carries an Outfit component tagged OutfitTag,
BindOutfits gives it the body's pose, and this pipeline already exports
garments skinned to metahuman_base_skel. So Blender makes the skinned garment,
UE imports it, and it goes straight onto the component.

Given up: MetaHuman's automatic body-hiding, covered instead by the union masks
in Tools/Export/BodyMasks, and Chaos Cloth simulation -- the garments follow
the body by leader pose rather than simulating, which is the usual choice for
crowd NPCs and much cheaper.

THE NUMBER THAT MATTERS

Penetration after conform. Under about 2% the garment kept its shape; at 60%
it has been flattened onto the body and renders as a decal. Every character is
reported so a bad one is visible rather than discovered in the viewport.
"""

import argparse
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, ".."))
BLEND_DIR = os.path.join(PROJECT, "Tools", "Blender")
SOURCE_DIR = os.path.join(PROJECT, "Tools", "Export", "SourceGarments")
OUT_DIR = os.path.join(PROJECT, "Tools", "Export", "Garments")

BLENDER = r"D:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64"  # placeholder
BLENDER = r"D:\Program Files\Blender Foundation\Blender 5.0\blender.exe"

# Which garment each sex wears. A table so adding a kith is a line, not a code
# change. The frock coat is cut for a tall slim figure and needs the radial
# conform to survive a Boggan; the gown is voluminous enough that barely 1% of
# it ever penetrates.
GARMENTS = {
    "male": ("Frock_coat.fbx", "Frock"),
    "female": ("Witch_dress_A.fbx", "Gown"),
}


def log(msg=""):
    print("[dress] %s" % msg)


def sex_of(stem):
    low = stem.lower()
    if "female" in low:
        return "female"
    if "male" in low:
        return "male"
    return None


def forward_named(stem):
    """True for MHC_Boggan_Wilder_Male, False for MHC_Boggan_Male_Wilder.

    The reversed-name folders are legacy output from 2026-09-13/14 with no
    MetaHumanCharacter behind them. Fitting against those produced a garment
    the live character never wears.
    """
    parts = stem.replace("SKM_MHC_", "").replace("_BodyMesh", "").split("_")
    return len(parts) >= 3 and parts[-1].lower() in ("male", "female")


KITH = ("Boggan", "Clurichaun", "Eshu", "Sidhe", "SidheAutumn")
SEEMINGS = ("Childling", "Wilder", "Grump")
SEXES = ("Male", "Female")


def avatars(only=None):
    """One prepped avatar per kith/seeming/sex.

    Forward-named blends are preferred, because they come from the character
    assets assembly builds. But MHC_Boggan_Grump_Female has never been
    assembled into a forward folder -- only the legacy reversed spelling
    exists -- so a reversed blend is accepted when there is no forward one.
    Skipping her would silently leave one character undressed.
    """
    found = []
    if not os.path.isdir(BLEND_DIR):
        return found
    have = {os.path.splitext(n)[0] for n in os.listdir(BLEND_DIR)
            if n.lower().endswith(".blend")}
    for clan in KITH:
        for seeming in SEEMINGS:
            for sex in SEXES:
                label = "%s_%s_%s" % (clan, seeming, sex)
                if only and not any(o.lower() in label.lower() for o in only):
                    continue
                forward = "SKM_MHC_%s_BodyMesh" % label
                reversed_ = "SKM_MHC_%s_%s_%s_BodyMesh" % (clan, sex, seeming)
                if forward in have:
                    found.append(forward)
                elif reversed_ in have:
                    found.append(reversed_)
                else:
                    log("   no prepped avatar for %s" % label)
    return found


def blender(script, extra):
    cmd = [BLENDER, "-b", "--python", os.path.join(HERE, script), "--"] + extra
    proc = subprocess.run(cmd, capture_output=True, text=True)
    return proc.stdout + proc.stderr


def number(text, pattern):
    hit = re.search(pattern, text)
    return hit.group(1) if hit else "?"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--only", default=None,
                        help="comma-separated name fragments, e.g. Boggan")
    parser.add_argument("--all", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    only = [o.strip() for o in args.only.split(",")] if args.only else None
    if not only and not args.all:
        parser.error("pass --only <fragment> or --all")

    picked = avatars(only)
    log("=" * 68)
    log("dress_kith   %d character(s)" % len(picked))
    log("=" * 68)
    if not picked:
        log("nothing matched in %s" % BLEND_DIR)
        log("prep first: blender -b --python Tools/blender_metahuman_garment.py"
            " -- prep --all")
        return 1

    plan = []
    for stem in picked:
        sex = sex_of(stem)
        garment, tag = GARMENTS.get(sex, (None, None))
        source = os.path.join(SOURCE_DIR, garment) if garment else None
        ok = bool(source and os.path.isfile(source))
        short = stem.replace("SKM_MHC_", "").replace("_BodyMesh", "")
        log("   %-38s %-6s %-20s%s"
            % (short, sex, garment or "NO GARMENT",
               "" if ok else "  *** SOURCE MISSING ***"))
        if ok:
            plan.append((stem, short, source, tag))

    if args.dry_run:
        log("")
        log("dry run -- nothing built")
        return 0

    log("")
    results = []
    for stem, short, source, tag in plan:
        log("-" * 68)
        log(short)
        out = blender("fit_source_garment.py",
                      ["--avatar", stem, "--source", source])
        before = number(out, r"INSIDE the body \(([\d.]+)%\)")
        log("   fit      penetration %s%%" % before)

        out = blender("conform_source_garment.py", ["--avatar", stem])
        after = number(out, r"after : \d+ of \d+ sampled inside \(([\d.]+)%\)")
        log("   conform  penetration %s%%" % after)

        env = dict(os.environ, GARMENT_NAME=tag)
        cmd = [BLENDER, "-b", "--python",
               os.path.join(HERE, "bind_export_frock.py"), "--",
               "--avatar", stem]
        proc = subprocess.run(cmd, capture_output=True, text=True, env=env)
        out = proc.stdout + proc.stderr
        groups = number(out, r"transferred (\d+) vertex groups")
        stray = number(out, r"base weights: (\d+) stray")
        exported = "exported" in out
        log("   bind     %s groups, %s strays" % (groups, stray))
        log("   export   %s" % ("ok" if exported else "FAILED"))

        results.append((short, before, after, groups, stray, exported))

    log("")
    log("=" * 68)
    log("%-34s %8s %8s %7s %6s" % ("character", "before", "after", "groups", "ok"))
    bad = 0
    for short, before, after, groups, stray, exported in results:
        try:
            flag = "" if float(after) < 2.0 and exported else "   <-- CHECK"
        except ValueError:
            flag = "   <-- CHECK"
        if flag:
            bad += 1
        log("%-34s %7s%% %7s%% %7s %6s%s"
            % (short, before, after, groups, "yes" if exported else "NO", flag))
    log("")
    log("%d of %d clean. Under 2%% after conform means the garment kept its"
        % (len(results) - bad, len(results)))
    log("shape; a high number means it was flattened onto the body.")
    log("Output: %s" % OUT_DIR)
    log("=" * 68)
    return 0


if __name__ == "__main__":
    sys.exit(main())
