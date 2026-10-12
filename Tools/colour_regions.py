"""Give every material region a distinct colour, for looking at in Blender.

    blender -b --python Tools/colour_regions.py -- [--body <blend>]

Sets both the Principled base colour and the VIEWPORT display colour, so the
regions read in Solid shading as well as Material Preview -- solid view uses
material.diffuse_color and ignores the node tree entirely, which is what
most people have the viewport set to.

These are working colours for identifying regions, not the costume's real
palette. UE binds its own materials to these slots.
"""
import bpy, os, sys

sys.path.insert(0, "D:/CHANGELING/Tools")
import blender_metahuman_garment as mhg

argv = sys.argv
args = argv[argv.index("--") + 1:] if "--" in argv else []


def arg(flag, default=None):
    return args[args.index(flag) + 1] if flag in args else default


body_blend = arg("--body", "Tools/Blender/Bodies/Boggan_Childling_Male.blend")

# key fragment -> (r, g, b)
PALETTE = (
    ("Lapel",     (0.92, 0.72, 0.16)),   # gold
    ("Cuff",      (0.95, 0.45, 0.10)),   # orange
    ("CourtTrim", (0.85, 0.85, 0.22)),   # yellow
    ("Boots",     (0.10, 0.10, 0.12)),   # near black
    ("Breeches",  (0.30, 0.65, 0.35)),   # green
    ("Shirt",     (0.95, 0.95, 0.92)),   # white
    ("Waistcoat", (0.78, 0.13, 0.18)),   # red
    ("Material_0", (0.16, 0.24, 0.52)),  # navy -- the coat body
)

bpy.ops.wm.open_mainfile(filepath=os.path.abspath(body_blend))
for o in bpy.data.objects:
    if o.type == "MESH" and o.mode != "OBJECT":
        bpy.context.view_layer.objects.active = o
        bpy.ops.object.mode_set(mode="OBJECT")

done = []
for name in ("Costume", "Shirt", "Waistcoat"):
    obj = bpy.data.objects.get(name)
    if obj is None:
        continue
    for mat in obj.data.materials:
        if mat is None:
            continue
        hit = next((c for k, c in PALETTE if k in mat.name), None)
        if hit is None:
            mhg.log("   no colour for %r -- left alone" % mat.name)
            continue
        rgba = (hit[0], hit[1], hit[2], 1.0)
        mat.diffuse_color = rgba            # solid viewport
        mat.use_nodes = True
        bsdf = next((n for n in mat.node_tree.nodes
                     if n.type == "BSDF_PRINCIPLED"), None)
        if bsdf is not None:
            bsdf.inputs["Base Color"].default_value = rgba
            bsdf.inputs["Roughness"].default_value = 0.55
        if mat.name not in done:
            done.append(mat.name)
            mhg.log("   %-40s -> (%.2f, %.2f, %.2f)"
                    % (mat.name, hit[0], hit[1], hit[2]))

mhg.log("coloured %d material(s)" % len(done))
bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(body_blend))
mhg.log("saved %s" % body_blend)
