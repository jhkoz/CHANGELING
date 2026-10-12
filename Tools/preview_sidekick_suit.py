"""Render the Sidekick character wearing its suit.

    blender -b --python Tools/preview_sidekick_suit.py -- \
        --character boggan_childling_male

Shows the body with coat, breeches and boots together, which is the only way
to tell whether the pieces sit on each other properly. The coat's facing gets
its own colour on purpose: separating collar and lapels into different cloth
is the whole point of the reference detail, and one colour would hide it.
"""

import os
import sys
import math

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, ".."))
SUIT_DIR = os.path.join(PROJECT, "Tools", "Blender", "Sidekick")
OUT_DIR = SUIT_DIR

VIEWS = (("front", 0.0), ("quarter", 35.0), ("side", 90.0), ("back", 180.0))


def log(msg):
    print("[preview] %s" % msg)


def opt(args, name, default=None):
    return args[args.index(name) + 1] if name in args else default


def shade(name, colour, rough, metal=0.0):
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    # By TYPE, not by the name "Principled BSDF", which returns None in
    # Blender 5 and leaves every material flat grey without complaining.
    bsdf = None
    for node in material.node_tree.nodes:
        if node.type == "BSDF_PRINCIPLED":
            bsdf = node
            break
    if bsdf is not None:
        bsdf.inputs["Base Color"].default_value = colour
        bsdf.inputs["Roughness"].default_value = rough
        if "Metallic" in bsdf.inputs:
            bsdf.inputs["Metallic"].default_value = metal
    return material


def main():
    argv = sys.argv
    args = argv[argv.index("--") + 1:] if "--" in argv else []
    name = opt(args, "--character", "boggan_childling_male")

    path = os.path.join(SUIT_DIR, "%s_suit.blend" % name)
    if not os.path.isfile(path):
        log("no suit at %s" % path)
        return
    bpy.ops.wm.open_mainfile(filepath=path)

    wool = shade("Wool", (0.020, 0.019, 0.024, 1.0), 0.62)
    gold = shade("Gold", (0.74, 0.55, 0.18, 1.0), 0.36, metal=0.30)
    cloth = shade("Cloth", (0.16, 0.14, 0.12, 1.0), 0.70)
    leather = shade("Leather", (0.030, 0.024, 0.020, 1.0), 0.45)
    skin = shade("Skin", (0.60, 0.44, 0.36, 1.0), 0.55)

    pieces = []
    for obj in bpy.context.scene.objects:
        if obj.type != "MESH":
            continue
        if obj.name.startswith("Coat"):
            # Slot order out of make_coat is body / Facing / trim. The
            # facing carries the collar, lapels and buttons.
            was = [m.name if m else "" for m in obj.data.materials]
            gilt = set(i for i, n in enumerate(was)
                       if "Facing" in n or "Trim" in n)
            obj.data.materials.clear()
            obj.data.materials.append(wool)
            obj.data.materials.append(gold)
            for face in obj.data.polygons:
                face.material_index = 1 if face.material_index in gilt else 0
            pieces.append(("coat", obj))
        elif obj.name.startswith("Breeches"):
            obj.data.materials.clear()
            obj.data.materials.append(cloth)
            for face in obj.data.polygons:
                face.material_index = 0
            pieces.append(("breeches", obj))
        elif obj.name.startswith("Boots"):
            obj.data.materials.clear()
            obj.data.materials.append(leather)
            for face in obj.data.polygons:
                face.material_index = 0
            pieces.append(("boots", obj))
        elif obj.name.startswith("Avatar_"):
            obj.data.materials.clear()
            obj.data.materials.append(skin)
            for face in obj.data.polygons:
                face.material_index = 0
            pieces.append(("body", obj))

    for label, obj in pieces:
        log("%-9s %6d verts" % (label, len(obj.data.vertices)))

    everything = [o for _, o in pieces]
    pts = [o.matrix_world @ v.co for o in everything for v in o.data.vertices]
    lo_z, hi_z = min(p.z for p in pts), max(p.z for p in pts)
    reach = max(max(p.x for p in pts) - min(p.x for p in pts), hi_z - lo_z)

    world = bpy.data.worlds.new("W")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (
        0.32, 0.35, 0.40, 1.0)
    bpy.context.scene.world = world

    for angle, energy in ((35.0, 340.0), (-130.0, 170.0)):
        data = bpy.data.lights.new("L", type="AREA")
        data.energy = energy
        data.size = 3.0
        lamp = bpy.data.objects.new("L", data)
        rad = math.radians(angle)
        lamp.location = (math.sin(rad) * 3.0, -math.cos(rad) * 3.0, hi_z + 0.6)
        lamp.rotation_euler = (math.radians(62.0), 0.0, rad)
        bpy.context.collection.objects.link(lamp)

    camera_data = bpy.data.cameras.new("C")
    camera_data.type = "ORTHO"
    camera_data.ortho_scale = reach * 1.12
    camera = bpy.data.objects.new("C", camera_data)
    bpy.context.collection.objects.link(camera)
    bpy.context.scene.camera = camera

    scene = bpy.context.scene
    scene.render.resolution_x = 620
    scene.render.resolution_y = 940
    scene.render.image_settings.file_format = "PNG"
    engines = scene.render.bl_rna.properties["engine"].enum_items
    for wanted in ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE", "BLENDER_WORKBENCH"):
        if wanted in engines:
            scene.render.engine = wanted
            break

    for view, yaw in VIEWS:
        for obj in everything:
            obj.rotation_euler.z = math.radians(yaw)
        camera.location = (0.0, -reach * 2.2, (lo_z + hi_z) * 0.5)
        camera.rotation_euler = (math.radians(90.0), 0.0, 0.0)
        scene.render.filepath = os.path.join(OUT_DIR, "suit_%s.png" % view)
        bpy.ops.render.render(write_still=True)
        log("rendered %s" % view)


main()
