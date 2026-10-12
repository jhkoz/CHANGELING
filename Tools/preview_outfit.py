"""Render the dressed character: body, face and all four garments.

    blender -b --python Tools/preview_outfit.py -- \
        --outfit boggan_childling_male_outfit

Each layer gets its own colour, because the point of the render is to tell
whether they sit on each other properly -- coat over waistcoat over breeches,
boots under the lot -- and one colour would hide exactly that.
"""

import os
import sys
import math

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, ".."))
OUT_DIR = os.path.join(PROJECT, "Tools", "Blender", "Outfits")

VIEWS = (("front", 0.0), ("quarter", 35.0), ("side", 90.0), ("back", 180.0))


def log(msg):
    print("[preview] %s" % msg)


def opt(args, name, default=None):
    return args[args.index(name) + 1] if name in args else default


def shade(name, colour, rough, metal=0.0):
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    # By TYPE. Looking the node up by the name "Principled BSDF" returns None
    # in Blender 5, and every material then stays flat grey without a word.
    bsdf = next((n for n in material.node_tree.nodes
                 if n.type == "BSDF_PRINCIPLED"), None)
    if bsdf is not None:
        bsdf.inputs["Base Color"].default_value = colour
        bsdf.inputs["Roughness"].default_value = rough
        if "Metallic" in bsdf.inputs:
            bsdf.inputs["Metallic"].default_value = metal
    return material


PALETTE = {
    "Coat": ((0.022, 0.020, 0.026, 1.0), 0.62, 0.0),
    "Waistcoat": ((0.30, 0.06, 0.09, 1.0), 0.58, 0.0),
    "Breeches": ((0.52, 0.45, 0.33, 1.0), 0.70, 0.0),
    "Boots": ((0.035, 0.028, 0.022, 1.0), 0.42, 0.0),
    "Body": ((0.62, 0.46, 0.38, 1.0), 0.55, 0.0),
    "Face": ((0.62, 0.46, 0.38, 1.0), 0.55, 0.0),
}
GILT = ((0.76, 0.57, 0.19, 1.0), 0.36, 0.30)


def main():
    argv = sys.argv
    args = argv[argv.index("--") + 1:] if "--" in argv else []
    name = opt(args, "--outfit", "boggan_childling_male_outfit")

    path = os.path.join(OUT_DIR, "%s.blend" % name)
    if not os.path.isfile(path):
        log("no outfit at %s" % path)
        return
    bpy.ops.wm.open_mainfile(filepath=path)

    gold = shade("Gilt", *GILT)
    shown = []
    for obj in bpy.context.scene.objects:
        if obj.type != "MESH":
            continue
        key = next((k for k in PALETTE if obj.name.startswith(k)), None)
        if key is None:
            continue
        colour, rough, metal = PALETTE[key]
        base = shade("P_%s" % obj.name, colour, rough, metal)

        # Facing and trim slots go gold; everything else takes the base.
        was = [m.name if m else "" for m in obj.data.materials]
        gilt = set(i for i, n in enumerate(was)
                   if "Facing" in n or "Trim" in n)
        obj.data.materials.clear()
        obj.data.materials.append(base)
        obj.data.materials.append(gold)
        for face in obj.data.polygons:
            face.material_index = 1 if face.material_index in gilt else 0
        shown.append((key, obj, len(gilt)))

    for key, obj, gilt_slots in sorted(shown):
        log("%-10s %6d verts%s" % (key, len(obj.data.vertices),
                                   "  (+gilt)" if gilt_slots else ""))

    objs = [o for _, o, _ in shown]
    pts = [o.matrix_world @ v.co for o in objs for v in o.data.vertices]
    lo_z, hi_z = min(p.z for p in pts), max(p.z for p in pts)
    reach = max(max(p.x for p in pts) - min(p.x for p in pts), hi_z - lo_z)

    world = bpy.data.worlds.new("W")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (
        0.33, 0.36, 0.41, 1.0)
    bpy.context.scene.world = world

    for angle, energy in ((35.0, 360.0), (-130.0, 180.0)):
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
    camera_data.ortho_scale = reach * 1.10
    camera = bpy.data.objects.new("C", camera_data)
    bpy.context.collection.objects.link(camera)
    bpy.context.scene.camera = camera

    scene = bpy.context.scene
    scene.render.resolution_x = 640
    scene.render.resolution_y = 980
    scene.render.image_settings.file_format = "PNG"
    engines = scene.render.bl_rna.properties["engine"].enum_items
    for wanted in ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE", "BLENDER_WORKBENCH"):
        if wanted in engines:
            scene.render.engine = wanted
            break

    for view, yaw in VIEWS:
        for obj in objs:
            obj.rotation_euler.z = math.radians(yaw)
        camera.location = (0.0, -reach * 2.2, (lo_z + hi_z) * 0.5)
        camera.rotation_euler = (math.radians(90.0), 0.0, 0.0)
        scene.render.filepath = os.path.join(OUT_DIR, "outfit_%s.png" % view)
        bpy.ops.render.render(write_still=True)
        log("rendered %s" % view)


main()
