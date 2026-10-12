"""Read back what material each garment actually has assigned."""

import os
import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "garment_materials.txt")
GARMENT_DIR = "/Game/_Custom/Garments"

lines = []


def log(m):
    lines.append(str(m))
    unreal.log(str(m))


paths = sorted(p.split(".")[0] for p in
               unreal.EditorAssetLibrary.list_assets(GARMENT_DIR, recursive=True,
                                                     include_folder=False))
shown = 0
counts = {}
for path in paths:
    mesh = unreal.load_asset(path)
    if not isinstance(mesh, unreal.SkeletalMesh):
        continue
    mats = mesh.get_editor_property("materials")
    names = []
    for slot in mats:
        mi = slot.material_interface
        names.append(mi.get_name() if mi else "<none>")
    key = tuple(names)
    counts[key] = counts.get(key, 0) + 1
    if shown < 3:
        log("%s -> %s" % (path.rsplit("/", 1)[-1], names))
        shown += 1

log("")
log("distinct slot configurations across %d garments:" % len(paths))
for key, n in sorted(counts.items(), key=lambda kv: -kv[1]):
    log("   %-44s %d" % (str(list(key)), n))

with open(OUT, "w") as h:
    h.write("\n".join(lines))
