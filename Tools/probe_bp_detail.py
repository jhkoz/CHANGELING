"""
probe_bp_detail.py -- full detail of one healthy Kith Blueprint.

READ-ONLY.

Rebuilding the six deleted Blueprints means recreating FaceMesh and the groom
components from scratch (they are Blueprint-added, not native -- ChangelingNPC.h
declares only NavigationInvoker, FootstepAudio, LODSync and Outfit). So we need
to know exactly how a working one is wired: what each component attaches to,
which leader-pose component drives it, and which binding asset each groom uses.
Guessing any of those produces a character that imports clean and renders wrong.
"""

import os
import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "bp_detail.txt")

SAMPLES = [
    "/Game/_Custom/Characters/Kith/Blueprints/BP_Sidhe_Wilder_Female",
    "/Game/_Custom/Characters/Kith/Blueprints/BP_Boggan_Grump_Female",
]

_lines = []


def log(m):
    _lines.append(str(m))
    unreal.log(str(m))


def name_of(v):
    if v is None:
        return "None"
    try:
        return v.get_name()
    except Exception:
        return str(v)


INTERESTING = [
    "leader_pose_component", "skeletal_mesh_asset", "anim_class",
    "anim_class_data", "groom_asset", "binding_asset", "attachment_name",
    "use_attached_parent_bound", "visible", "cast_shadow",
    "bounds_scale", "component_tags", "num_lo_ds", "forced_lod_model",
]

lib = unreal.SubobjectDataBlueprintFunctionLibrary

for path in SAMPLES:
    log("")
    log("#" * 70)
    log("# %s" % path)
    log("#" * 70)
    bp = unreal.load_asset(path)
    if bp is None:
        log("could not load")
        continue

    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    handles = sub.k2_gather_subobject_data_for_blueprint(bp)
    log("%d handle(s)" % len(handles))

    seen = set()
    for handle in handles:
        data = lib.get_data(handle)
        obj = lib.get_object(data)
        if obj is None or not isinstance(obj, unreal.ActorComponent):
            continue
        key = obj.get_path_name()
        if key in seen:
            continue
        seen.add(key)

        try:
            var = str(lib.get_variable_name(data))
        except Exception:
            var = obj.get_name()

        # Native components come from ChangelingNPC; the rest we must recreate.
        try:
            native = lib.is_native_component(data)
        except Exception:
            native = "?"
        try:
            inherited = lib.is_inherited_component(data)
        except Exception:
            inherited = "?"

        log("")
        log("--- %s   (%s)  native=%s inherited=%s"
            % (var, type(obj).__name__, native, inherited))

        parent = None
        try:
            if isinstance(obj, unreal.SceneComponent):
                parent = obj.get_editor_property("attach_parent")
        except Exception:
            pass
        log("      attach_parent : %s" % name_of(parent))

        for prop in INTERESTING:
            try:
                val = obj.get_editor_property(prop)
            except Exception:
                continue
            if val is None or val == [] or val == "":
                continue
            if isinstance(val, unreal.Object):
                val = name_of(val)
            log("      %-22s %s" % (prop, val))

out_dir = os.path.dirname(OUT)
if not os.path.isdir(out_dir):
    os.makedirs(out_dir)
with open(OUT, "w") as h:
    h.write("\n".join(_lines))
unreal.log("written: %s" % OUT)
