"""Import one Kith body FBX into a clean, workable .blend.

    blender -b --python Tools/import_body_to_blender.py -- \
        --body SKM_MHC_Boggan_Childling_Male_BodyMesh

Not the same thing as prep_avatar. That one LOCKS the body and its armature
so garment work cannot nudge them, and it keeps every LOD the FBX carries --
sensible for fitting a garment, annoying for opening the file and modelling
in it. This keeps LOD0 and the armature, drops the rest, unlocks everything
and leaves the mesh selected.

Writes to Tools/Blender/Bodies so it cannot be confused with the prepped
avatars that the garment pipeline reads.
"""

import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, ".."))
FBX_DIR = os.path.join(PROJECT, "Tools", "Export", "MetaHumanBodies")
OUT_DIR = os.path.join(PROJECT, "Tools", "Blender", "Bodies")


def log(msg):
    print("[body] %s" % msg)


def opt(args, name, default=None):
    return args[args.index(name) + 1] if name in args else default


def main():
    argv = sys.argv
    args = argv[argv.index("--") + 1:] if "--" in argv else []
    stem = opt(args, "--body", "SKM_MHC_Boggan_Childling_Male_BodyMesh")

    src = os.path.join(FBX_DIR, "%s.fbx" % stem)
    if not os.path.isfile(src):
        log("no FBX at %s" % src)
        log("export it first: python Tools/ue_remote.py Tools/export_male_bodies.py")
        return

    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=src)

    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    armature = next((o for o in bpy.context.scene.objects
                     if o.type == "ARMATURE"), None)
    if not meshes:
        log("nothing imported")
        return

    # LOD0 is simply the densest: the FBX names every LOD the same, so the
    # name cannot be used to tell them apart.
    meshes.sort(key=lambda o: len(o.data.vertices), reverse=True)
    body = meshes[0]
    for extra in meshes[1:]:
        log("dropping LOD: %d verts" % len(extra.data.vertices))
        bpy.data.objects.remove(extra, do_unlink=True)

    for obj in list(bpy.context.scene.objects):
        if obj.type == "EMPTY" and not obj.children:
            bpy.data.objects.remove(obj, do_unlink=True)

    body.name = stem.replace("SKM_MHC_", "").replace("_BodyMesh", "")
    if armature is not None:
        armature.name = "%s_Armature" % body.name

    # Unlocked and visible. prep_avatar locks these on purpose; here the
    # point is to be able to move things.
    for obj in bpy.context.scene.objects:
        obj.lock_location = (False, False, False)
        obj.lock_rotation = (False, False, False)
        obj.lock_scale = (False, False, False)
        obj.hide_viewport = False
        obj.hide_render = False
        try:
            obj.hide_set(False)
        except RuntimeError:
            pass

    bpy.ops.object.select_all(action="DESELECT")
    body.select_set(True)
    bpy.context.view_layer.objects.active = body

    pts = [body.matrix_world @ v.co for v in body.data.vertices]
    z = [p.z for p in pts]
    x = [p.x for p in pts]
    log("%s: %d verts, %d faces, %d vertex groups"
        % (body.name, len(body.data.vertices), len(body.data.polygons),
           len(body.vertex_groups)))
    log("   height %.3f m (z %.3f..%.3f), width %.3f m"
        % (max(z) - min(z), min(z), max(z), max(x) - min(x)))
    if armature is not None:
        log("   armature %s, %d bones" % (armature.name,
                                          len(armature.data.bones)))

    if not os.path.isdir(OUT_DIR):
        os.makedirs(OUT_DIR)
    out = os.path.join(OUT_DIR, "%s.blend" % body.name)
    bpy.ops.wm.save_as_mainfile(filepath=out)
    log("saved %s (%.1f MB)" % (out, os.path.getsize(out) / 1048576.0))


main()
