"""Face mesh LOD count and vert count per character -- the actual symptom."""

import os
import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "face_check.txt")
KITH = "/Game/_Custom/Characters/Kith"

CHECK = [
    ("REBUILT", "Eshu/MHC_Eshu_Wilder_Female"),
    ("broken ", "Boggan/MHC_Boggan_Childling_Female"),
    ("broken ", "Boggan/MHC_Boggan_Wilder_Male"),
    ("healthy", "Boggan/MHC_Boggan_Female_Wilder"),
    ("healthy", "Sidhe/MHC_Sidhe_Grump_Female"),
    ("healthy", "Eshu/MHC_Eshu_Grump_Female"),
]

lines = []


def log(m):
    lines.append(str(m))
    unreal.log(str(m))


sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)

log("%-8s %-40s %5s %9s %s" % ("state", "character", "LODs", "verts", "materials"))
for state, rel in CHECK:
    name = rel.rsplit("/", 1)[-1]
    path = "%s/%s/Face/SKM_%s_FaceMesh" % (KITH, rel, name)
    mesh = unreal.load_asset(path)
    if mesh is None:
        log("%-8s %-40s  NO ASSET at %s" % (state, name, path))
        continue
    try:
        lods = mesh.get_lod_num()
    except Exception:
        lods = -1
    try:
        verts = sub.get_num_verts(mesh, 0)
    except Exception:
        verts = -1
    mats = len([m for m in mesh.get_editor_property("materials") if m])
    log("%-8s %-40s %5d %9d %d" % (state, name, lods, verts, mats))

with open(OUT, "w") as h:
    h.write("\n".join(lines))
