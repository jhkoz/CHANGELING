"""probe_eshu_hair.py -- what class are GroomAsset_0 and Hair_M_TwistedBraids?

READ-ONLY.

Two Eshu characters have a 199 MB asset called GroomAsset_0 and a 7.7 KB asset
called Hair_M_TwistedBraids sitting beside it. The size split says the names got
swapped during assembly -- 7.7 KB is the size band of every other _Binding in
these folders, and 199 MB is strands groom data. BP_Eshu_Grump_Female already
wires Hair to GroomAsset_0 and survived, so whatever binding IT uses is the
answer for BP_Eshu_Childling_Male, which is being repaired.

Guessing here would wire a groom to a binding built against a different head,
which RebindGrooms refuses -- the hair then renders unbound at the character's
feet instead of on the head.
"""

import os
import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "eshu_hair.txt")

ASSETS = [
    "/Game/_Custom/Characters/Kith/Eshu/MHC_Eshu_Childling_Male/Grooms/GroomAsset_0",
    "/Game/_Custom/Characters/Kith/Eshu/MHC_Eshu_Childling_Male/Grooms/Hair_M_TwistedBraids",
    "/Game/_Custom/Characters/Kith/Eshu/MHC_Eshu_Grump_Female/Grooms/GroomAsset_0",
    "/Game/_Custom/Characters/Kith/Eshu/MHC_Eshu_Grump_Female/Grooms/Hair_M_TwistedBraids",
]

_lines = []


def log(m):
    _lines.append(str(m))
    unreal.log(str(m))


log("=== asset classes ===")
for path in ASSETS:
    asset = unreal.load_asset(path)
    short = "/".join(path.split("/")[-3:])
    if asset is None:
        log("   %-58s could not load" % short)
        continue
    kind = type(asset).__name__
    extra = ""
    if isinstance(asset, unreal.GroomBindingAsset):
        try:
            target = asset.get_target_skeletal_mesh()
            extra = "  target=%s" % (target.get_name() if target else "None")
        except Exception:
            try:
                target = asset.get_editor_property("target_skeletal_mesh")
                extra = "  target=%s" % (target.get_name() if target else "None")
            except Exception as exc:
                extra = "  <target unreadable: %s>" % exc
    log("   %-58s %s%s" % (short, kind, extra))

log("")
log("=== how BP_Eshu_Grump_Female (which works) wires its Hair ===")
bp = unreal.load_asset(
    "/Game/_Custom/Characters/Kith/Blueprints/BP_Eshu_Grump_Female")
if bp is None:
    log("   could not load the Blueprint")
else:
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    for handle in sub.k2_gather_subobject_data_for_blueprint(bp):
        data = lib.get_data(handle)
        obj = lib.get_object(data)
        if not isinstance(obj, unreal.GroomComponent):
            continue
        try:
            var = str(lib.get_variable_name(data))
        except Exception:
            var = obj.get_name()
        groom = obj.get_editor_property("groom_asset")
        bind = obj.get_editor_property("binding_asset")
        log("   %-12s groom=%-16s binding=%s"
            % (var,
               groom.get_name() if groom else "None",
               bind.get_name() if bind else "None"))

out_dir = os.path.dirname(OUT)
if not os.path.isdir(out_dir):
    os.makedirs(out_dir)
with open(OUT, "w") as h:
    h.write("\n".join(_lines))
unreal.log("written: %s" % OUT)
