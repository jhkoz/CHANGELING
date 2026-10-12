"""Export a Synty Sidekick OUTFIT's slots, for assembly alongside a body.

    (edit OUTFIT below, then)
    python Tools/ue_remote.py Tools/export_sidekick_outfit.py

An outfit in Sidekick is not a garment laid over a body. It is a replacement
set of the same numbered slots -- torso, hips, arms, legs, feet -- already
skinned to SKEL_Default_Sidekick and already cut to the base body's shape.

That is what makes it fit where the MetaHuman route could not. A frock coat
authored for a tall slim figure has to be DEFORMED onto a short stout one,
and pushing vertices around has no way to keep a coat skirt standing away
from the body: it ends up pressed flat against the breeches, z-fighting them.
An outfit slot needs no fitting at all, because it replaces the body part
rather than covering it -- and when the body is reshaped, the same reshape
applies to the outfit, so the fit survives any proportion.
"""
import os, re, unreal

PROJECT = os.path.normpath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUTFIT = "FANT_KNGT"
OUT_DIR = os.path.join(PROJECT, "Tools", "Export", "SidekickOutfits", OUTFIT)

reg = unreal.AssetRegistryHelpers.get_asset_registry()
found = {}
for a in reg.get_assets_by_path("/Game/Synty/SidekickCharacters", recursive=True):
    if str(a.asset_class_path.asset_name) != "SkeletalMesh":
        continue
    name = str(a.asset_name)
    m = re.match(r"SK_%s_(\d+)_(\w+?)_HU\d+$" % OUTFIT, name)
    if m:
        found.setdefault(m.group(2), str(a.package_name))

if not os.path.isdir(OUT_DIR):
    os.makedirs(OUT_DIR)

made = 0
for slot, path in sorted(found.items()):
    mesh = unreal.load_asset(path)
    if mesh is None:
        unreal.log("MISSING %s" % path); continue
    task = unreal.AssetExportTask()
    task.set_editor_property("object", mesh)
    task.set_editor_property("filename", os.path.join(OUT_DIR, "%s.fbx" % slot))
    task.set_editor_property("automated", True)
    task.set_editor_property("prompt", False)
    task.set_editor_property("exporter", unreal.SkeletalMeshExporterFBX())
    if unreal.Exporter.run_asset_export_task(task):
        made += 1
    else:
        unreal.log("FAILED %s" % slot)
unreal.log("=== %s: exported %d slot(s) -> %s" % (OUTFIT, made, OUT_DIR))
unreal.log("=== slots: %s" % ", ".join(sorted(found)))
