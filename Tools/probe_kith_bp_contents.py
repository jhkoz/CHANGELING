"""What is actually IN the Kith Blueprints besides components, and who
references them? Decides whether regenerating from scratch is cheap or
destructive. READ-ONLY."""
import os, traceback
import unreal

BP_DIR = "/Game/_Custom/Characters/Kith/Blueprints"
REPORT = os.path.join(unreal.Paths.project_dir(), "Tools", "Export", "kith_bp_contents.txt")
LINES = []
def log(m=""):
    LINES.append(str(m)); unreal.log(str(m))

def count_prop(bp, name):
    try:
        v = bp.get_editor_property(name)
        return len(v) if v is not None else 0
    except Exception:
        return None

try:
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    log("%-34s %5s %5s %5s %6s  %s" % ("BLUEPRINT", "VARS", "FUNCS", "COMPS", "REFS", "PARENT"))
    log("-" * 104)
    tot_vars = tot_funcs = tot_refs = 0
    all_refs = set()
    for a in sorted(unreal.EditorAssetLibrary.list_assets(BP_DIR, recursive=False) or []):
        path = a.split(".")[0]
        name = path.rsplit("/", 1)[-1]
        bp = unreal.load_asset(path)
        nvars = count_prop(bp, "new_variables")
        nfuncs = count_prop(bp, "function_graphs")
        ncomp = len(sub.k2_gather_subobject_data_for_blueprint(bp))
        try:
            parent = bp.get_editor_property("parent_class").get_name()
        except Exception:
            parent = "?"
        refs = [str(r) for r in (ar.get_referencers(
            unreal.Name(path),
            unreal.AssetRegistryDependencyOptions()) or [])]
        refs = [r for r in refs if BP_DIR not in r]
        all_refs.update(refs)
        tot_vars += nvars or 0
        tot_funcs += nfuncs or 0
        tot_refs += len(refs)
        log("%-34s %5s %5s %5s %6d  %s"
            % (name, nvars, nfuncs, ncomp, len(refs), parent))
    log("")
    log("TOTAL user variables : %d" % tot_vars)
    log("TOTAL user functions : %d" % tot_funcs)
    log("TOTAL references in  : %d  (distinct: %d)" % (tot_refs, len(all_refs)))
    log("")
    log("WHO REFERENCES THEM:")
    for r in sorted(all_refs)[:40]:
        log("   %s" % r)
except Exception:
    log(traceback.format_exc())
with open(REPORT, "w", encoding="utf-8") as fh:
    fh.write("\n".join(LINES) + "\n")
