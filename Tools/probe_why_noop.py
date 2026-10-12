"""Why does assembly no-op on Childling but build Wilder?

Captures what the editor logs during can_build and build for each, instead of
reading only the return values -- which say nothing.
"""
import os, time, unreal
PROJECT = os.path.normpath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
LOG = os.path.join(PROJECT, "Saved", "Logs", "CHANGELING.log")
K = "/Game/_Custom/Characters/Kith/Boggan"
sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)

def size():
    try: return os.path.getsize(LOG)
    except OSError: return 0

def tail(since):
    try:
        with open(LOG, "r", errors="replace") as h:
            h.seek(since); return h.read()
    except OSError:
        return ""

for name in ("MHC_Boggan_Childling_Male", "MHC_Boggan_Wilder_Male"):
    c = unreal.load_asset("%s/%s" % (K, name))
    unreal.log("===== %s" % name)
    if c is None:
        unreal.log("   could not load"); continue
    try: sub.remove_object_to_edit(c)
    except Exception: pass
    reg = sub.try_add_object_to_edit(c)
    mark = size()
    can = sub.can_build_meta_human(c, True)
    unreal.log("   registered=%s  can_build=%s" % (reg, can))
    fresh = tail(mark)
    for line in fresh.splitlines():
        if any(w in line for w in ("MetaHuman", "Build", "Warning", "Error", "Outfit", "Wardrobe")):
            unreal.log("      %s" % line.strip()[:165])
    try: sub.remove_object_to_edit(c)
    except Exception: pass
