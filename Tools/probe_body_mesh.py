"""Can the UNCUT body mesh be read from a MetaHumanCharacter without rebuilding?

get_mesh_for_body_conforming() looks like it returns the character's body mesh
straight from the source asset. If it yields the full 32,334-vert body for a
clothed character, the four blocked dresses can be built with nothing modified.
"""

import os
import unreal

OUT = os.path.join(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()),
    "Tools", "Export", "body_mesh_probe.txt")

lines = []


def log(msg):
    lines.append(str(msg))
    unreal.log(str(msg))


sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)

for fn in ("get_mesh_for_body_conforming", "get_joints_for_body_conforming",
           "assemble_for_preview", "can_build_meta_human",
           "try_add_object_to_edit", "is_object_added_for_editing"):
    log("--- %s ---" % fn)
    try:
        log((getattr(type(sub), fn).__doc__ or "<no doc>").strip())
    except Exception as exc:
        log("   %s" % exc)
    log("")

CLOTHED = "/Game/_Custom/Characters/Kith/Eshu/MHC_Eshu_Wilder_Female"
UNCLOTHED = "/Game/_Custom/Characters/Kith/Eshu/MHC_Eshu_Grump_Female"

for label, path in (("CLOTHED  ", CLOTHED), ("UNCLOTHED", UNCLOTHED)):
    log("=== %s  %s ===" % (label, path))
    ch = unreal.load_asset(path)
    if ch is None:
        log("   could not load")
        continue
    try:
        log("   added for editing: %s" % sub.is_object_added_for_editing(ch))
    except Exception as exc:
        log("   is_object_added_for_editing failed: %s" % exc)
    try:
        mesh = sub.get_mesh_for_body_conforming(ch)
        log("   get_mesh_for_body_conforming -> %r" % (mesh,))
        if mesh is not None:
            try:
                log("   type: %s" % type(mesh).__name__)
                for attr in ("get_num_vertices", "get_number_of_vertices"):
                    if hasattr(mesh, attr):
                        log("   %s: %s" % (attr, getattr(mesh, attr)()))
            except Exception as exc:
                log("   inspect failed: %s" % exc)
    except Exception as exc:
        log("   get_mesh_for_body_conforming FAILED: %s" % exc)
    log("")

with open(OUT, "w") as handle:
    handle.write("\n".join(lines))
log("written: %s" % OUT)
