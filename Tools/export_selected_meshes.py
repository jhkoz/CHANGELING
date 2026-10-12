"""Export every skeletal mesh on the SELECTED actor, for Blender.

    python Tools/ue_remote.py Tools/export_selected_meshes.py

Driven by the editor selection rather than a hard-coded list, so "export
this one" means whatever is actually highlighted. A MetaHuman is more than
one mesh -- the body is headless and the face is a separate asset -- so
exporting a body alone gives a decapitated character.

Writes a manifest beside the FBXs recording which component each mesh came
from, so the Blender side can rebuild the character without guessing.
"""

import json
import os

import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT_DIR = os.path.join(PROJECT, "Tools", "Export", "SelectedMeshes")


def log(msg):
    unreal.log("[sel] %s" % msg)


def main():
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    selected = sub.get_selected_level_actors() or []
    if not selected:
        log("nothing selected in the level")
        return

    if not os.path.isdir(OUT_DIR):
        os.makedirs(OUT_DIR)

    manifest = {}
    for actor in selected:
        label = actor.get_actor_label()
        log("%s (%s)" % (label, actor.get_class().get_name()))
        pieces = []
        for comp in actor.get_components_by_class(unreal.SkeletalMeshComponent):
            mesh = comp.get_editor_property("skeletal_mesh_asset")
            if mesh is None:
                continue
            name = mesh.get_name()
            out = os.path.join(OUT_DIR, "%s.fbx" % name)

            task = unreal.AssetExportTask()
            task.set_editor_property("object", mesh)
            task.set_editor_property("filename", out)
            task.set_editor_property("automated", True)
            task.set_editor_property("prompt", False)
            task.set_editor_property("exporter",
                                     unreal.SkeletalMeshExporterFBX())
            ok = unreal.Exporter.run_asset_export_task(task)

            size = (os.path.getsize(out) / 1048576.0
                    if os.path.exists(out) else 0.0)
            log("   %-16s %-40s %s (%.1f MB)"
                % (comp.get_name(), name, "OK" if ok else "FAILED", size))
            if ok:
                pieces.append({
                    "component": comp.get_name(),
                    "mesh": name,
                    "asset": mesh.get_path_name(),
                    "file": "%s.fbx" % name,
                    "is_face": "face" in comp.get_name().lower()
                               or "Face" in name,
                })
        manifest[label] = pieces

    path = os.path.join(OUT_DIR, "manifest.json")
    with open(path, "w") as handle:
        handle.write(json.dumps(manifest, indent=2))
    log("manifest -> %s" % path)
    log("next: blender -b --python Tools/import_selected_to_blender.py")


main()
