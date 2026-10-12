"""What does build_meta_human need, and what controls whether the outfit is baked?"""

import os
import unreal

OUT = os.path.join(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()),
    "Tools", "Export", "build_params_probe.txt")

lines = []


def log(msg):
    lines.append(str(msg))
    unreal.log(str(msg))


def dump_struct(name):
    if not hasattr(unreal, name):
        log("%s: NOT PRESENT" % name)
        return
    cls = getattr(unreal, name)
    log("--- %s ---" % name)
    try:
        inst = cls()
        for prop in sorted(cls.__dict__.keys()):
            if prop.startswith("_"):
                continue
            try:
                val = getattr(inst, prop)
                log("   %-38s = %r" % (prop, val))
            except Exception as exc:
                log("   %-38s   <%s>" % (prop, exc))
    except Exception as exc:
        log("   could not instantiate: %s" % exc)
        for prop in sorted(p for p in dir(cls) if not p.startswith("_")):
            log("   %s" % prop)
    log("")


for name in ("MetaHumanCharacterEditorBuildParameters",
             "MetaHumanCharacterAssemblySettings",
             "MetaHumanCharacterPaletteBuildQuality",
             "MetaHumanClothingVisibilityState",
             "MetaHumanCharacterWardrobeIndividualAssets",
             "MetaHumanCharacterIndividualAssets",
             "MetaHumanCharacterUnpackPathMode"):
    dump_struct(name)

log("--- build_meta_human signature help ---")
sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
log("subsystem: %s" % (sub is not None))
for fn in ("build_meta_human", "can_build_meta_human", "assemble_for_preview"):
    try:
        log("")
        log("%s:" % fn)
        log((getattr(type(sub), fn).__doc__ or "<no doc>").strip())
    except Exception as exc:
        log("   %s" % exc)

log("")
log("--- the character's collection / wardrobe ---")
ch = unreal.load_asset("/Game/_Custom/Characters/Kith/Eshu/MHC_Eshu_Wilder_Female")
try:
    coll = ch.get_editor_property("internal_collection")
    log("internal_collection: %s" % type(coll).__name__)
    for p in sorted(x for x in dir(coll) if not x.startswith("_")):
        log("   %s" % p)
except Exception as exc:
    log("collection unreadable: %s" % exc)

with open(OUT, "w") as handle:
    handle.write("\n".join(lines))
log("")
log("written: %s" % OUT)
