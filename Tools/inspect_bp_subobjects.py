"""
inspect_bp_subobjects.py

Dump the component tree of every surviving Kith Blueprint.

READ-ONLY. Nothing is built, saved or modified.

Why not `simple_construction_script`: UE5 does not expose it to Python. The
previous attempt read it on every Blueprint and threw the same exception 24
times, producing a report that said nothing except that the property is absent.
UE5 keeps component templates behind SubobjectDataSubsystem instead, which is
what this uses.

Six Blueprints were deleted from disk this session:
    BP_Boggan_Childling_Female      BP_Clurichaun_Wilder_Female
    BP_Boggan_Childling_Male        BP_Clurichaun_Wilder_Male
    BP_Clurichaun_Childling_Female  BP_Eshu_Wilder_Female
Their MetaHumanCharacter sources and baked Face/Body meshes all survive, so the
plan is to rebuild each one by duplicating a healthy sibling and repointing its
mesh and groom references -- NOT by re-running build_meta_human, which is what
inflated the Eshu face from 12 MB to 147 MB.

This report is what tells us which references a duplicate would have to repoint.

    py "D:/CHANGELING/Tools/inspect_bp_subobjects.py"
"""

import os
import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "bp_subobjects.txt")
BP_DIR = "/Game/_Custom/Characters/Kith/Blueprints"

_lines = []


def log(msg):
    text = str(msg)
    _lines.append(text)
    unreal.log(text)


def asset_name(value):
    """Name of an asset-valued property, or a marker if it is empty."""
    if value is None:
        return "*** EMPTY ***"
    try:
        return value.get_name()
    except Exception:
        return str(value)


def describe_template(template):
    """One line of detail for the component types we care about."""
    kind = type(template).__name__
    detail = ""
    try:
        if isinstance(template, unreal.SkeletalMeshComponent):
            detail = asset_name(
                template.get_editor_property("skeletal_mesh_asset"))
            anim = template.get_editor_property("anim_class")
            if anim is not None:
                detail += "   anim=%s" % asset_name(anim)
        elif isinstance(template, unreal.StaticMeshComponent):
            detail = asset_name(template.get_editor_property("static_mesh"))
        elif "Groom" in kind:
            detail = asset_name(template.get_editor_property("groom_asset"))
        elif "LODSync" in kind:
            detail = "num_lods=%s" % template.get_editor_property("num_lo_ds")
    except Exception as exc:
        detail = "<could not read: %s>" % exc
    return kind, detail


def describe(bp_path):
    bp = unreal.load_asset(bp_path)
    if bp is None:
        log("   could not load")
        return

    try:
        parent = bp.get_editor_property("parent_class")
        log("   parent class: %s" % (parent.get_name() if parent else "?"))
    except Exception:
        pass

    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    if sub is None:
        log("   no SubobjectDataSubsystem")
        return

    try:
        handles = sub.k2_gather_subobject_data_for_blueprint(bp)
    except Exception as exc:
        log("   could not gather subobjects: %s" % exc)
        return

    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    for handle in handles:
        try:
            data = lib.get_data(handle)
            obj = lib.get_object(data)
        except Exception as exc:
            log("   <handle unreadable: %s>" % exc)
            continue
        if obj is None:
            continue
        try:
            var = str(lib.get_variable_name(data))
        except Exception:
            var = obj.get_name()
        # The root handle is the actor itself, not a component.
        if not isinstance(obj, unreal.ActorComponent):
            log("   [actor] %s" % type(obj).__name__)
            continue
        kind, detail = describe_template(obj)
        log("   %-24s %-26s %s" % (var, kind, detail))


paths = sorted(
    p.split(".")[0]
    for p in unreal.EditorAssetLibrary.list_assets(
        BP_DIR, recursive=False, include_folder=False))

log("found %d Blueprint(s) under %s" % (len(paths), BP_DIR))
for path in paths:
    log("")
    log("=== %s ===" % path.rsplit("/", 1)[-1])
    try:
        describe(path)
    except Exception as exc:
        import traceback
        log("   EXCEPTION: %s" % exc)
        for line in traceback.format_exc().splitlines()[-4:]:
            log("      %s" % line)

out_dir = os.path.dirname(OUT)
if not os.path.isdir(out_dir):
    os.makedirs(out_dir)
with open(OUT, "w") as handle:
    handle.write("\n".join(_lines))
unreal.log("written: %s" % OUT)
