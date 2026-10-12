"""Does the generated Blueprint carry a component for the garment?"""
import os, traceback
import unreal
BP = "/Game/_Scratch/WardrobeProbe/Built/Probe_223234/BP_Probe_223234"
REPORT = os.path.join(unreal.Paths.project_dir(), "Tools", "Export", "probe_built_bp.txt")
LINES = []
def log(m=""):
    LINES.append(str(m)); unreal.log(str(m))
try:
    bp = unreal.load_asset(BP)
    log("blueprint: %s" % bp.get_path_name())
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    handles = sub.k2_gather_subobject_data_for_blueprint(bp)
    log("subobjects: %d" % len(handles))
    for h in handles:
        d = sub.k2_find_subobject_data_from_handle(h)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(d)
        if obj is None:
            continue
        cls = obj.get_class().get_name()
        line = "  %-34s %s" % (obj.get_name(), cls)
        extra = ""
        for prop in ("skeletal_mesh_asset", "skeletal_mesh", "static_mesh", "groom_asset"):
            try:
                v = obj.get_editor_property(prop)
                if v:
                    extra = "\n        %s = %s" % (prop, v.get_path_name())
                    break
            except Exception:
                pass
        try:
            lp = obj.get_editor_property("leader_pose_component")
            if lp:
                extra += "\n        leader_pose = %s" % lp
        except Exception:
            pass
        log(line + extra)
except Exception:
    log(traceback.format_exc())
with open(REPORT, "w", encoding="utf-8") as fh:
    fh.write("\n".join(LINES) + "\n")
