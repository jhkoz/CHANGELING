"""Render a turnaround of the generated frock coat, in Blender.

    blender -b --python Tools/preview_frock.py -- --suit boggan_childling_male_suit

Shows the coat on its own, in black wool with a gold facing, so the cut can
be judged against the Met references without Unreal in the way. The facing is
a SEPARATE material on purpose: that is the whole point of the reference
detail, and rendering it in one colour would hide whether it worked.
"""

import os
import sys
import math

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, ".."))
OUT_DIR = os.path.join(PROJECT, "Tools", "Reference", "FrockCoats")

VIEWS = (("front", 0.0), ("quarter", 40.0), ("side", 90.0), ("back", 180.0))


def log(msg):
    print("[preview] %s" % msg)


def opt(args, name, default=None):
    return args[args.index(name) + 1] if name in args else default


def shade(material, colour, rough, metal=0.0):
    material.use_nodes = True
    # By TYPE, not by name. Looking the node up as "Principled BSDF" returns
    # None in Blender 5, shade() then returned silently, and both materials
    # stayed default grey -- so the coat rendered in one colour and the
    # facing looked like it had never been separated at all.
    bsdf = None
    for node in material.node_tree.nodes:
        if node.type == "BSDF_PRINCIPLED":
            bsdf = node
            break
    if bsdf is None:
        return
    bsdf.inputs["Base Color"].default_value = colour
    bsdf.inputs["Roughness"].default_value = rough
    if "Metallic" in bsdf.inputs:
        bsdf.inputs["Metallic"].default_value = metal


def main():
    argv = sys.argv
    args = argv[argv.index("--") + 1:] if "--" in argv else []
    suit = opt(args, "--suit", "boggan_childling_male_suit")

    path = os.path.join(PROJECT, "Tools", "Blender", "Work", "%s.blend" % suit)
    if not os.path.isfile(path):
        log("no suit at %s" % path)
        return
    bpy.ops.wm.open_mainfile(filepath=path)

    coat = None
    for obj in bpy.context.scene.objects:
        if obj.type == "MESH" and obj.name.startswith("Coat"):
            coat = obj
    if coat is None:
        log("no Coat in %s" % suit)
        return

    # Everything except the coat goes: the body and the other pieces would
    # hide the cut that is being judged.
    for obj in list(bpy.context.scene.objects):
        if obj is not coat and obj.type in ("MESH", "ARMATURE"):
            bpy.data.objects.remove(obj, do_unlink=True)

    wool = bpy.data.materials.new("PreviewWool")
    shade(wool, (0.016, 0.015, 0.019, 1.0), 0.62)
    gold = bpy.data.materials.new("PreviewGold")
    # Low metallic on purpose. A metal shows only what it reflects, so at
    # 0.85 against a near-black world the gold facing rendered pure black
    # and the whole coat looked like one material.
    shade(gold, (0.72, 0.52, 0.16, 1.0), 0.38, metal=0.25)

    # Map by NAME, not by index. The coat's slots are body / Facing / trim,
    # and guessing that the facing is the last one puts the hem trim in the
    # same colour as the lapels.
    was = [m.name if m else "" for m in coat.data.materials]
    facing_slots = set(i for i, n in enumerate(was) if "Facing" in n)
    coat.data.materials.clear()
    coat.data.materials.append(wool)
    coat.data.materials.append(gold)
    for face in coat.data.polygons:
        face.material_index = 1 if face.material_index in facing_slots else 0

    pts = [coat.matrix_world @ v.co for v in coat.data.vertices]
    mid_z = (min(p.z for p in pts) + max(p.z for p in pts)) * 0.5
    reach = max(max(p.x for p in pts) - min(p.x for p in pts),
                max(p.z for p in pts) - min(p.z for p in pts))

    world = bpy.data.worlds.new("PreviewWorld")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (
        0.28, 0.30, 0.34, 1.0)
    world.node_tree.nodes["Background"].inputs[1].default_value = 1.0
    bpy.context.scene.world = world

    for angle, energy in ((35.0, 4.0), (-120.0, 2.0)):
        data = bpy.data.lights.new("Key%d" % int(angle), type="AREA")
        data.energy = energy * 60.0
        data.size = 3.0
        lamp = bpy.data.objects.new("Key%d" % int(angle), data)
        rad = math.radians(angle)
        lamp.location = (math.sin(rad) * 3.0, -math.cos(rad) * 3.0, mid_z + 1.2)
        lamp.rotation_euler = (math.radians(60.0), 0.0, rad)
        bpy.context.collection.objects.link(lamp)

    camera_data = bpy.data.cameras.new("PreviewCam")
    camera_data.type = "ORTHO"
    camera_data.ortho_scale = reach * 1.25
    camera = bpy.data.objects.new("PreviewCam", camera_data)
    bpy.context.collection.objects.link(camera)
    bpy.context.scene.camera = camera

    scene = bpy.context.scene
    scene.render.resolution_x = 700
    scene.render.resolution_y = 1000
    scene.render.image_settings.file_format = "PNG"
    engines = scene.render.bl_rna.properties["engine"].enum_items
    for wanted in ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE", "BLENDER_WORKBENCH"):
        if wanted in engines:
            scene.render.engine = wanted
            break

    if not os.path.isdir(OUT_DIR):
        os.makedirs(OUT_DIR)

    rest = coat.rotation_euler.z
    for name, yaw in VIEWS:
        coat.rotation_euler.z = rest + math.radians(yaw)
        camera.location = (0.0, -reach * 2.0, mid_z)
        camera.rotation_euler = (math.radians(90.0), 0.0, 0.0)
        out = os.path.join(OUT_DIR, "generated_frock_%s.png" % name)
        scene.render.filepath = out
        bpy.ops.render.render(write_still=True)
        log("rendered %s" % os.path.basename(out))
    coat.rotation_euler.z = rest

    log("%d verts, %d faces, %d material slot(s)"
        % (len(coat.data.vertices), len(coat.data.polygons),
           len(coat.data.materials)))


main()
