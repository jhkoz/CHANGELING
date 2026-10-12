"""Which object do we actually write to for an INHERITED component?

CharacterMesh0 comes from AChangelingNPC. If Python hands back the parent
class's template, writing to it changes the C++ default for every Kith BP at
once. get_object_for_blueprint() is supposed to give the per-Blueprint
override archetype instead. Verify before touching 30 assets. READ-ONLY.
"""
import os, traceback
import unreal

# One BP that already overrides (z=-88) and one that does not (z=0).
CASES = ["/Game/_Custom/Characters/Kith/Blueprints/BP_Boggan_Grump_Female",
         "/Game/_Custom/Characters/Kith/Blueprints/BP_Boggan_Childling_Female"]
REPORT = os.path.join(unreal.Paths.project_dir(), "Tools", "Export", "probe_inherited.txt")
LINES = []
def log(m=""):
    LINES.append(str(m)); unreal.log(str(m))

try:
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    log("get_object_for_blueprint available: %s"
        % hasattr(lib, "get_object_for_blueprint"))
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    for path in CASES:
        bp = unreal.load_asset(path)
        log("")
        log("=== %s" % path.rsplit("/", 1)[-1])
        for h in sub.k2_gather_subobject_data_for_blueprint(bp):
            d = sub.k2_find_subobject_data_from_handle(h)
            o = lib.get_object(d)
            if o is None or o.get_name() != "CharacterMesh0":
                continue
            log("  get_object():")
            log("      path  : %s" % o.get_path_name())
            log("      outer : %s" % o.get_outer().get_name())
            log("      z=%s yaw=%s" % (o.get_editor_property("relative_location").z,
                                       o.get_editor_property("relative_rotation").yaw))
            try:
                ob = lib.get_object_for_blueprint(d, bp)
                log("  get_object_for_blueprint():")
                if ob is None:
                    log("      returned None")
                else:
                    log("      path  : %s" % ob.get_path_name())
                    log("      outer : %s" % ob.get_outer().get_name())
                    log("      same object as get_object: %s" % (ob == o))
            except Exception as exc:
                log("  get_object_for_blueprint FAILED: %s" % exc)
            try:
                log("  is_inherited_component: %s"
                    % lib.is_inherited_component(d))
            except Exception as exc:
                log("  is_inherited_component unavailable (%s)" % exc)
except Exception:
    log(traceback.format_exc())
with open(REPORT, "w", encoding="utf-8") as fh:
    fh.write("\n".join(LINES) + "\n")
