"""Probe: is there ANY scripted way to reassign a SkeletalMesh's Skeleton in 5.7?

set_editor_property('skeleton', ...) is refused -- the property is read-only.
This enumerates the plausible alternatives so the answer is measured, not guessed.
"""

import os
import unreal

OUT = os.path.join(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()),
    "Tools", "Export", "skeleton_api_probe.txt")

lines = []


def log(msg):
    lines.append(str(msg))
    unreal.log(str(msg))


MESH = ("/Game/_Custom/Characters/Kith/Boggan/MHC_Boggan_Female_Grump/Body/"
        "SKM_MHC_Boggan_Female_Grump_BodyMesh")

mesh = unreal.load_asset(MESH)
log("mesh loaded: %s" % (mesh is not None))

log("")
log("--- SkeletalMesh methods mentioning skeleton ---")
for name in sorted(dir(mesh)):
    if "skel" in name.lower():
        log("   %s" % name)

log("")
log("--- candidate subsystems / libraries ---")
for name in ("SkeletalMeshEditorSubsystem", "EditorSkeletalMeshLibrary",
             "SkeletalMeshEditorLibrary", "AssetToolsHelpers",
             "SkeletalMeshLODSettings", "InterchangeAssetImportData",
             "FbxSkeletalMeshImportData"):
    log("   unreal.%-32s %s" % (name, hasattr(unreal, name)))

for sub in ("SkeletalMeshEditorSubsystem",):
    if hasattr(unreal, sub):
        cls = getattr(unreal, sub)
        log("")
        log("--- %s members ---" % sub)
        for name in sorted(dir(cls)):
            if not name.startswith("_"):
                log("   %s" % name)

log("")
log("--- is the skeleton property writable at all? ---")
try:
    prop = mesh.get_editor_property("skeleton")
    log("   current: %s" % (prop.get_path_name() if prop else None))
except Exception as exc:
    log("   read failed: %s" % exc)

log("")
log("--- reimport path: does the mesh carry import data? ---")
try:
    data = mesh.get_editor_property("asset_import_data")
    log("   asset_import_data: %s" % type(data).__name__)
    for attr in sorted(dir(data)):
        if "skel" in attr.lower() or "file" in attr.lower():
            log("      %s" % attr)
except Exception as exc:
    log("   none: %s" % exc)

with open(OUT, "w") as handle:
    handle.write("\n".join(lines))
log("")
log("written: %s" % OUT)
