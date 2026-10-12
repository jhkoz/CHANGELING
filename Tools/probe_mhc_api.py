"""Probe: can a MetaHumanCharacter be re-assembled/baked from Python in 5.7?

The four blocked characters (Clurichaun/Eshu Childling+Wilder Female) have body
meshes with the clothed region culled, so no dress can be cut from them. Their
MetaHumanCharacter source assets are intact, so re-baking WITHOUT the outfit
would restore a full body. This finds out whether that is scriptable.
"""

import os
import unreal

OUT = os.path.join(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()),
    "Tools", "Export", "mhc_api_probe.txt")

lines = []


def log(msg):
    lines.append(str(msg))
    unreal.log(str(msg))


ASSET = "/Game/_Custom/Characters/Kith/Eshu/MHC_Eshu_Wilder_Female"

log("--- unreal.* names mentioning MetaHumanCharacter ---")
hits = [n for n in dir(unreal) if "metahumancharacter" in n.lower()]
for n in sorted(hits):
    log("   %s" % n)
log("   (%d found)" % len(hits))

log("")
log("--- broader: unreal.* mentioning MetaHuman ---")
mh = [n for n in dir(unreal) if "metahuman" in n.lower()]
for n in sorted(mh):
    log("   %s" % n)
log("   (%d found)" % len(mh))

log("")
log("--- load the character asset ---")
asset = unreal.load_asset(ASSET)
log("   loaded: %s" % (asset is not None))
if asset is not None:
    log("   class: %s" % type(asset).__name__)
    log("")
    log("--- its methods (non-dunder) ---")
    for n in sorted(dir(asset)):
        if not n.startswith("_"):
            log("      %s" % n)

log("")
log("--- editor subsystems mentioning MetaHuman ---")
for n in sorted(dir(unreal)):
    if "subsystem" in n.lower() and "metahuman" in n.lower():
        cls = getattr(unreal, n)
        log("   %s" % n)
        for m in sorted(dir(cls)):
            if not m.startswith("_"):
                log("      %s" % m)

with open(OUT, "w") as handle:
    handle.write("\n".join(lines))
log("")
log("written: %s" % OUT)
