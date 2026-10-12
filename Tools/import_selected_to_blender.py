"""Import the exported selection into one .blend, body and face together.

    blender -b --python Tools/import_selected_to_blender.py

A MetaHuman body is headless -- the face is a separate asset -- so importing
the body on its own gives a decapitated character. This brings in every mesh
the selected actor was using.

Body and face keep SEPARATE armatures, because that is what they are: the
face rig has 874 bones and the body's 341 include arms and legs the face rig
has never heard of. They are two skeletons in Unreal as well.

Keeps LOD0 of each mesh and drops the rest. LOD0 is chosen by vertex count,
because the FBX names every LOD identically.
"""

import json
import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, ".."))
SRC_DIR = os.path.join(PROJECT, "Tools", "Export", "SelectedMeshes")
OUT_DIR = os.path.join(PROJECT, "Tools", "Blender", "Bodies")


def log(msg):
    print("[import] %s" % msg)


def keep_lod0(before):
    """Of the meshes this import added, keep the densest and bin the rest."""
    added = [o for o in bpy.context.scene.objects
             if o.type == "MESH" and o not in before]
    if not added:
        return None
    added.sort(key=lambda o: len(o.data.vertices), reverse=True)
    for extra in added[1:]:
        bpy.data.objects.remove(extra, do_unlink=True)
    return added[0]


def main():
    path = os.path.join(SRC_DIR, "manifest.json")
    if not os.path.isfile(path):
        log("no manifest at %s" % path)
        log("export first: python Tools/ue_remote.py Tools/export_selected_meshes.py")
        return
    manifest = json.load(open(path))
    if not manifest:
        log("manifest is empty")
        return

    bpy.ops.wm.read_factory_settings(use_empty=True)

    label, pieces = sorted(manifest.items())[0]
    log("%s: %d mesh(es)" % (label, len(pieces)))

    parts = []
    for piece in pieces:
        fbx = os.path.join(SRC_DIR, piece["file"])
        if not os.path.isfile(fbx):
            log("   missing %s" % piece["file"])
            continue
        before = set(bpy.context.scene.objects)
        bpy.ops.import_scene.fbx(filepath=fbx)
        mesh = keep_lod0(before)
        if mesh is None:
            log("   %s imported nothing" % piece["mesh"])
            continue
        mesh.name = "Face" if piece.get("is_face") else "Body"
        parts.append(mesh)
        log("   %-6s %-40s %6d verts, %2d material(s)"
            % (mesh.name, piece["mesh"], len(mesh.data.vertices),
               len(mesh.data.materials)))

    # EACH MESH KEEPS ITS OWN ARMATURE.
    #
    # The obvious move is to merge onto one rig, and it is wrong here. A
    # MetaHuman face and body are on DIFFERENT skeletons, not a superset and
    # a subset: the face rig has 874 bones and the body's 341 include arms
    # and legs the face rig has never heard of. Merging onto the bigger one
    # left 262 of the body's 285 vertex groups pointing at bones that no
    # longer existed, which silently destroys the body's skinning.
    #
    # They are separate skeletons in Unreal too, so keeping them separate
    # here is not a compromise -- it is what the character actually is.
    for mesh in parts:
        own = None
        for mod in mesh.modifiers:
            if mod.type == "ARMATURE" and mod.object is not None:
                own = mod.object
                break
        if own is None:
            continue
        own.name = "%s_%s_Rig" % (label.replace("BP_", ""), mesh.name)
        groups = set(g.name for g in mesh.vertex_groups)
        bones = set(b.name for b in own.data.bones)
        orphan = len(groups - bones)
        log("   %-6s rig %-34s %4d bones, %d orphaned group(s)"
            % (mesh.name, own.name, len(bones), orphan))
        if orphan:
            log("      WARNING: %d vertex group(s) have no bone -- skinning "
                "is broken" % orphan)

    for obj in list(bpy.context.scene.objects):
        if obj.type == "EMPTY" and not obj.children:
            bpy.data.objects.remove(obj, do_unlink=True)
        else:
            obj.lock_location = (False, False, False)
            obj.lock_rotation = (False, False, False)
            obj.lock_scale = (False, False, False)
            obj.hide_viewport = False
            obj.hide_render = False

    if parts:
        bpy.ops.object.select_all(action="DESELECT")
        for mesh in parts:
            mesh.select_set(True)
        bpy.context.view_layer.objects.active = parts[0]
        pts = [m.matrix_world @ v.co for m in parts for v in m.data.vertices]
        zs = [p.z for p in pts]
        log("whole figure: %d verts, height %.3f m (z %.3f..%.3f)"
            % (sum(len(m.data.vertices) for m in parts),
               max(zs) - min(zs), min(zs), max(zs)))

    if not os.path.isdir(OUT_DIR):
        os.makedirs(OUT_DIR)
    out = os.path.join(OUT_DIR, "%s.blend" % label.replace("BP_", ""))
    bpy.ops.wm.save_as_mainfile(filepath=out)
    log("saved %s (%.1f MB)" % (out, os.path.getsize(out) / 1048576.0))


main()
