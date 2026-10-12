"""Width and depth per height band: fitted frock coat against the Boggan body.

93.6% of the garment sits inside the body after a uniform height-matched
scale. This says WHERE, so the correction is measured rather than guessed --
a single global scale factor would distort a tailored mesh badly.
"""
import bpy
BLEND = "D:/CHANGELING/Tools/Blender/Work/SKM_MHC_Boggan_Male_Wilder_BodyMesh_frockfit.blend"
bpy.ops.wm.open_mainfile(filepath=BLEND)

def pts(o):
    mw = o.matrix_world
    return [mw @ v.co for v in o.data.vertices]

body = None; garment = None
for o in bpy.context.scene.objects:
    if o.type != "MESH": continue
    if o.name == "SourceGarment": garment = o
    elif o.name.startswith("Avatar_"): body = o
if not body or not garment:
    print("[cmp] missing body or garment"); raise SystemExit

bp, gp = pts(body), pts(garment)
zs = [p.z for p in gp]
lo, hi = min(zs), max(zs)
print("[cmp] band        garment w/d        body w/d         needed")
for i in range(10):
    a = lo + (hi-lo)*i/10.0; b = lo + (hi-lo)*(i+1)/10.0
    gb = [p for p in gp if a <= p.z < b]
    bb = [p for p in bp if a <= p.z < b]
    if not gb or not bb:
        print("[cmp] z %.2f-%.2f  (sparse)" % (a,b)); continue
    gw = max(p.x for p in gb)-min(p.x for p in gb)
    gd = max(p.y for p in gb)-min(p.y for p in gb)
    bw = max(p.x for p in bb)-min(p.x for p in bb)
    bd = max(p.y for p in bb)-min(p.y for p in bb)
    print("[cmp] z %.2f-%.2f  %.3f / %.3f    %.3f / %.3f    x%.2f / y%.2f"
          % (a, b, gw, gd, bw, bd,
             (bw/gw) if gw else 0, (bd/gd) if gd else 0))
