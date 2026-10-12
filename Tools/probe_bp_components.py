"""READ-ONLY: list every component on each Kith BP, flag clothing leftovers,
and report the delete API available in 5.7."""
import os, traceback
import unreal

BP_DIR = "/Game/_Custom/Characters/Kith/Blueprints"
REPORT = os.path.join(unreal.Paths.project_dir(), "Tools", "Export", "bp_components.txt")
LINES = []
def log(m=""):
    LINES.append(str(m)); unreal.log(str(m))

try:
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    log("delete API on SubobjectDataSubsystem:")
    for m in sorted(m for m in dir(sub) if "delete" in m.lower() or "remove" in m.lower()):
        log("   %s" % m)
    log("")
    total_bad = 0
    for a in sorted(unreal.EditorAssetLibrary.list_assets(BP_DIR, recursive=False) or []):
        path = a.split(".")[0]
        name = path.rsplit("/", 1)[-1]
        bp = unreal.load_asset(path)
        comps, bad = [], []
        for h in sub.k2_gather_subobject_data_for_blueprint(bp):
            d = sub.k2_find_subobject_data_from_handle(h)
            o = lib.get_object_for_blueprint(d, bp)
            if o is None:
                continue
            cn = o.get_name()
            try:
                inherited = lib.is_inherited_component(d)
            except Exception:
                inherited = "?"
            comps.append((cn, o.get_class().get_name(), inherited))
            if "outfit" in cn.lower() or "cloth" in cn.lower() or "garment" in cn.lower():
                bad.append((cn, o.get_class().get_name(), inherited))
        total_bad += len(bad)
        log("%-34s %2d components   clothing leftovers: %d"
            % (name, len(comps), len(bad)))
        for cn, cls, inh in bad:
            log("      %-28s %-24s inherited=%s" % (cn, cls, inh))
    log("")
    log("TOTAL clothing components still on Blueprints: %d" % total_bad)
except Exception:
    log(traceback.format_exc())
with open(REPORT, "w", encoding="utf-8") as fh:
    fh.write("\n".join(LINES) + "\n")
