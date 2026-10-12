"""Build a Blender contact sheet from the downloaded frock-coat references.

    blender -b --python Tools/make_reference_blend.py

Lays every reference image out as a plane at its true aspect ratio, scaled
to a common height, with a camera framing the lot. Opening the .blend and
pressing 0 gives a browsable board; individual planes can be moved into an
orthographic view to model against.

Shaded with emission, not a lit material, so the references read at their
own values in the viewport instead of being darkened by scene lighting.
"""

import json
import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, ".."))
REF_DIR = os.path.join(PROJECT, "Tools", "Reference", "FrockCoats")
OUT = os.path.join(REF_DIR, "frock_coat_reference.blend")

ROW_HEIGHT = 2.0       # metres; every plane is scaled to this tall
GAP = 0.25


def log(msg):
    print("[ref] %s" % msg)


def plane(name, image, x, y, height):
    """One image as a plane, sized to the image's own aspect."""
    aspect = image.size[0] / float(image.size[1]) if image.size[1] else 1.0
    half_w = height * aspect * 0.5
    half_h = height * 0.5

    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(
        [(-half_w, 0.0, -half_h), (half_w, 0.0, -half_h),
         (half_w, 0.0, half_h), (-half_w, 0.0, half_h)],
        [], [(0, 1, 2, 3)])
    mesh.update()

    uv = mesh.uv_layers.new(name="UVMap")
    for i, coord in enumerate(((0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0))):
        uv.data[i].uv = coord

    material = bpy.data.materials.new("M_" + name)
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    nodes.clear()
    tex = nodes.new("ShaderNodeTexImage")
    tex.image = image
    tex.location = (-300, 0)
    emit = nodes.new("ShaderNodeEmission")
    emit.location = (0, 0)
    emit.inputs["Strength"].default_value = 1.0
    out_node = nodes.new("ShaderNodeOutputMaterial")
    out_node.location = (220, 0)
    links.new(tex.outputs["Color"], emit.inputs["Color"])
    links.new(emit.outputs["Emission"], out_node.inputs["Surface"])
    mesh.materials.append(material)

    obj = bpy.data.objects.new(name, mesh)
    obj.location = (x + half_w, 0.0, y - half_h)
    bpy.context.collection.objects.link(obj)
    return obj, half_w * 2.0


def main():
    if not os.path.isdir(REF_DIR):
        log("no reference folder at %s" % REF_DIR)
        return

    manifest = {}
    path = os.path.join(REF_DIR, "manifest.json")
    if os.path.isfile(path):
        manifest = json.load(open(path))

    # Skip this script's own output, or a second run puts the previous
    # contact sheet on the board as if it were a reference.
    made_here = ("contact_sheet", "frock_coat_reference")
    names = sorted(f for f in os.listdir(REF_DIR)
                   if f.lower().endswith((".jpg", ".jpeg", ".png"))
                   and not f.lower().startswith(made_here))
    if not names:
        log("no images in %s" % REF_DIR)
        return

    bpy.ops.wm.read_factory_settings(use_empty=True)

    # Group by object id, so each garment's views sit together on one row.
    rows = {}
    for name in names:
        rows.setdefault(name.split("_")[0], []).append(name)

    y = 0.0
    widest = 0.0
    made = 0
    for oid in sorted(rows, key=lambda k: int(k) if k.isdigit() else 0):
        x = 0.0
        record = manifest.get(oid, {})
        label = "%s %s" % (record.get("title") or "frock coat",
                           record.get("date") or "")
        log("row %-8s %-32s %d view(s)" % (oid, label.strip()[:32],
                                           len(rows[oid])))
        for name in rows[oid]:
            image = bpy.data.images.load(os.path.join(REF_DIR, name),
                                         check_existing=True)
            obj, width = plane(os.path.splitext(name)[0], image, x, y,
                               ROW_HEIGHT)
            obj["met_object"] = oid
            obj["title"] = record.get("title") or ""
            obj["date"] = record.get("date") or ""
            obj["credit"] = record.get("credit") or ""
            obj["source"] = record.get("url") or ""
            x += width + GAP
            made += 1
        widest = max(widest, x)
        y -= ROW_HEIGHT + GAP

    height = abs(y)

    res_x, res_y = 2000, 1500
    bpy.context.scene.render.resolution_x = res_x
    bpy.context.scene.render.resolution_y = res_y

    camera_data = bpy.data.cameras.new("RefCam")
    camera_data.type = "ORTHO"
    # ortho_scale is the HORIZONTAL extent; the vertical extent is that
    # times res_y/res_x. Sizing it from max(width, height) cropped the top
    # and bottom rows off every sheet taller than it was wide.
    camera_data.ortho_scale = max(widest,
                                  height * res_x / float(res_y)) * 1.06
    camera = bpy.data.objects.new("RefCam", camera_data)
    camera.location = (widest * 0.5, -10.0, -height * 0.5)
    camera.rotation_euler = (1.5707963, 0.0, 0.0)
    bpy.context.collection.objects.link(camera)
    bpy.context.scene.camera = camera

    # Blender 5.0 renamed this back to BLENDER_EEVEE; pick whatever the
    # build actually offers rather than assuming a name.
    engines = bpy.context.scene.render.bl_rna.properties["engine"].enum_items
    for wanted in ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE", "BLENDER_WORKBENCH"):
        if wanted in engines:
            bpy.context.scene.render.engine = wanted
            break
    bpy.context.scene.render.film_transparent = False

    # Pack the images into the .blend so it survives being moved or sent
    # on its own; linked paths would break the moment it left this folder.
    try:
        bpy.ops.file.pack_all()
        log("packed images into the .blend")
    except Exception as exc:
        log("could not pack images: %s" % exc)

    bpy.ops.wm.save_as_mainfile(filepath=OUT)
    log("%d plane(s) across %d garment(s)" % (made, len(rows)))
    log("saved %s (%.1f MB)" % (OUT, os.path.getsize(OUT) / 1048576.0))


main()
