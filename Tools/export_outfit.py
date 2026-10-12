"""Export a generated outfit as Unreal-ready FBX, one file per garment.

The outfit blend already carries rigged garments -- make_outfit binds them
while the body is still whole, before the foot trim -- so there is nothing to
fit here. This just applies the house export rules (armature exported as
'root', scale applied, pose cleared) via export_garment and writes one FBX
per piece into Tools/Export/Garments/, where import_garments.py picks them up.

    blender -b --python Tools/export_outfit.py -- \
        --outfit boggan_childling_male_outfit
"""
import bpy, os, sys

sys.path.insert(0, "D:/CHANGELING/Tools")
import blender_metahuman_garment as mhg

argv = sys.argv
args = argv[argv.index("--") + 1:] if "--" in argv else []


def arg(flag, default):
    return args[args.index(flag) + 1] if flag in args else default


outfit = arg("--outfit", "boggan_childling_male_outfit")
out_dir = arg("--out", "D:/CHANGELING/Tools/Export/Garments")
blend = "D:/CHANGELING/Tools/Blender/Outfits/%s.blend" % outfit

bpy.ops.wm.open_mainfile(filepath=blend)
mhg.log("opened %s" % blend)

body = bpy.data.objects.get("Body")
armature = next((o for o in bpy.data.objects if o.type == "ARMATURE"), None)
if body is None or armature is None:
    raise SystemExit("outfit blend has no Body/armature")

# Everything that is not the figure itself is a garment.
skip = {"Body", "Face"}
pieces = [o for o in bpy.data.objects
          if o.type == "MESH" and o.name not in skip]
pieces.sort(key=lambda o: o.name)
mhg.log("garments: %s" % ", ".join(o.name for o in pieces))

tag = outfit.replace("_outfit", "")
written = []
for piece in pieces:
    if piece.find_armature() is None:
        # Not rigged -- an unrigged garment imports as a static mesh and is
        # silently useless in Unreal, so say so rather than writing it.
        mhg.log("SKIPPED %s: no armature, rebuild the outfit to rig it"
                % piece.name)
        continue
    stem = piece.name.split("_")[0]
    out = os.path.join(out_dir, "%s_%s.fbx" % (stem, tag))
    mhg.export_garment(out, garment=piece, armature=armature)
    written.append(out)

# --- and one joined mesh -----------------------------------------------
#
# AChangelingNPC carries a SINGLE Outfit component, and the native BindOutfits
# and ConfigureLODSync key off its OutfitTag, so a four-piece outfit cannot be
# worn as four assets without adding three more components to every Kith
# Blueprint. Joined, it is one skeletal mesh on the existing slot -- and the
# pieces survive as separate material slots, so coat, waistcoat, breeches and
# boots are still shaded independently.
#
# The individual FBX files above are still written, for anyone who does want
# to mix pieces later.
rigged = [p for p in pieces if p.find_armature() is not None]
if len(rigged) > 1:
    bpy.ops.object.select_all(action="DESELECT")
    copies = []
    for piece in rigged:
        clone = piece.copy()
        clone.data = piece.data.copy()
        bpy.context.scene.collection.objects.link(clone)
        copies.append(clone)
    for clone in copies:
        clone.select_set(True)
    bpy.context.view_layer.objects.active = copies[0]
    bpy.ops.object.join()
    joined = bpy.context.view_layer.objects.active
    joined.name = "Outfit"
    mhg.log("joined %d piece(s) -> %s (%d verts, %d material slot(s))"
            % (len(copies), joined.name, len(joined.data.vertices),
               len(joined.data.materials)))
    out = os.path.join(out_dir, "Outfit_%s.fbx" % tag)
    mhg.export_garment(out, garment=joined, armature=armature)
    written.append(out)

mhg.log("-" * 60)
for path in written:
    mhg.log("%9d bytes  %s" % (os.path.getsize(path), os.path.basename(path)))
mhg.log("%d garment(s) written to %s" % (len(written), out_dir))
