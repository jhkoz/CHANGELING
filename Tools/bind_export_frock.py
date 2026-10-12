"""Bind the conformed frock coat to the MetaHuman skeleton and export it.

Reuses the existing pipeline: bind() transfers vertex weights from the body
(which is already skinned to metahuman_base_skel), base_weights catches any
vertex the transfer missed, and export_garment writes an Unreal-ready FBX with
the armature renamed to 'root'.
"""
import bpy, os, sys
sys.path.insert(0, "D:/CHANGELING/Tools")
exec(open("D:/CHANGELING/Tools/blender_metahuman_garment.py").read())

argv = sys.argv
args = argv[argv.index("--") + 1:] if "--" in argv else []
avatar = args[args.index("--avatar") + 1] if "--avatar" in args else \
    "SKM_MHC_Boggan_Male_Wilder_BodyMesh"
tag = avatar.replace("SKM_MHC_", "").replace("_BodyMesh", "").lower()

work = "D:/CHANGELING/Tools/Blender/Work"
blend = os.path.join(work, "%s_frock.blend" % avatar)
bpy.ops.wm.open_mainfile(filepath=blend)

garment = bpy.data.objects.get("SourceGarment")
body = _find_body(exclude=garment)
if garment is None or body is None:
    log("missing garment or body"); raise SystemExit

garment.name = "Frock"
log("binding %s to %s" % (garment.name, body.name))
bind(garment=garment, body=body)
base_weights(dress=garment, bone="pelvis")

out_dir = "D:/CHANGELING/Tools/Export/Garments"
out = os.path.join(out_dir, "%s_%s.fbx" % (os.environ.get("GARMENT_NAME", "Frock"), tag))
export_garment(out, garment=garment)

bpy.ops.wm.save_as_mainfile(filepath=os.path.join(work, "%s_frockbound.blend" % avatar))
log("done: %s" % out)
