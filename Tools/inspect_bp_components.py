"""What components does each character Blueprint have, and what mesh is on each?

Reads the Blueprint's construction script nodes directly, so it reports what the
asset actually holds rather than what a string scan guesses.
"""

import os
import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "bp_components.txt")
BP_DIR = "/Game/_Custom/Characters/Kith/Blueprints"

BROKEN = {"BP_Boggan_Childling_Female", "BP_Boggan_Childling_Male",
          "BP_Boggan_Female_Childling", "BP_Boggan_Male_Childling",
          "BP_Boggan_Male_Wilder", "BP_Boggan_Wilder_Male",
          "BP_Eshu_Wilder_Female"}
CONTROLS = {"BP_Boggan_Grump_Female", "BP_Sidhe_Grump_Female"}

lines = []


def log(m):
    lines.append(str(m))
    unreal.log(str(m))


def describe(bp_path):
    bp = unreal.load_asset(bp_path)
    if bp is None:
        log("   could not load")
        return
    scs = bp.get_editor_property("simple_construction_script")
    if scs is None:
        log("   no construction script")
        return
    try:
        nodes = scs.get_editor_property("all_nodes")
    except Exception as exc:
        log("   could not read nodes: %s" % exc)
        return

    for node in nodes:
        try:
            template = node.get_editor_property("component_template")
            var = node.get_editor_property("internal_variable_name")
        except Exception:
            continue
        if template is None:
            log("   %-22s <no template>" % var)
            continue
        kind = type(template).__name__
        detail = ""
        if isinstance(template, unreal.SkeletalMeshComponent):
            mesh = template.get_editor_property("skeletal_mesh_asset")
            detail = mesh.get_name() if mesh else "*** NO MESH ***"
        elif kind == "GroomComponent":
            try:
                groom = template.get_editor_property("groom_asset")
                detail = groom.get_name() if groom else "*** NO GROOM ***"
            except Exception:
                detail = "?"
        log("   %-22s %-26s %s" % (var, kind, detail))


names = sorted(BROKEN | CONTROLS)
for name in names:
    state = "BROKEN " if name in BROKEN else "control"
    log("")
    log("=== %s  [%s] ===" % (name, state))
    try:
        describe("%s/%s" % (BP_DIR, name))
    except Exception as exc:
        # Never let one Blueprint take the whole report down -- the last time an
        # exception escaped here the log was never written and the run looked
        # silent rather than failed.
        import traceback
        log("   EXCEPTION: %s" % exc)
        for line in traceback.format_exc().splitlines()[-4:]:
            log("      %s" % line)

try:
    out_dir = os.path.dirname(OUT)
    if not os.path.isdir(out_dir):
        os.makedirs(out_dir)
    with open(OUT, "w") as h:
        h.write("\n".join(lines))
    unreal.log("written: %s" % OUT)
except Exception as exc:
    unreal.log("could not write report: %s" % exc)
