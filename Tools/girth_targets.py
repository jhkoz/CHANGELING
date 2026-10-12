"""Where, and by how much, a Boggan male has to slim for the suit to fit.

Compares the body against the modular suit fitted to its height, band by band,
and prints the reduction each band needs. The single number that matters is
penetration: under about 10% the conform only tidies edges and the garment
keeps its shape, which is how the gown behaved on the females at 1.2%.
"""
import bpy, os, sys
sys.path.insert(0, "D:/CHANGELING/Tools")

AVATAR = "SKM_MHC_Boggan_Male_Childling_BodyMesh"
FIT = "D:/CHANGELING/Tools/Blender/Work/%s_frockfit.blend" % AVATAR
bpy.ops.wm.open_mainfile(filepath=FIT)

body = garment = None
for o in bpy.context.scene.objects:
    if o.type != "MESH":
        continue
    if o.name == "SourceGarment":
        garment = o
    elif o.name.startswith("Avatar_"):
        body = o
if not body or not garment:
    print("[girth] missing body or garment"); raise SystemExit

def pts(o):
    mw = o.matrix_world
    return [mw @ v.co for v in o.data.vertices]

bp, gp = pts(body), pts(garment)
zs = [p.z for p in gp]
lo, hi = min(zs), max(zs)

print("[girth] band            body depth   suit depth   body must lose")
worst = 0.0
for i in range(8):
    a = lo + (hi - lo) * i / 8.0
    b = lo + (hi - lo) * (i + 1) / 8.0
    bb = [p for p in bp if a <= p.z < b]
    gb = [p for p in gp if a <= p.z < b]
    if not bb or not gb:
        continue
    bd = max(p.y for p in bb) - min(p.y for p in bb)
    gd = max(p.y for p in gb) - min(p.y for p in gb)
    bw = max(p.x for p in bb) - min(p.x for p in bb)
    gw = max(p.x for p in gb) - min(p.x for p in gb)
    need = (1.0 - gd / bd) * 100.0 if bd else 0
    worst = max(worst, need)
    print("[girth] z %.2f-%.2f   %5.1f cm     %5.1f cm     %s"
          % (a, b, bd * 100, gd * 100,
             ("-%.0f%%" % need) if need > 0 else "fits"))

print("[girth]")
print("[girth] deepest band needs about -%.0f%% off the body" % worst)
print("[girth] body overall  x %.1f cm  y %.1f cm"
      % ((max(p.x for p in bp) - min(p.x for p in bp)) * 100,
         (max(p.y for p in bp) - min(p.y for p in bp)) * 100))
print("[girth] suit overall  x %.1f cm  y %.1f cm"
      % ((max(p.x for p in gp) - min(p.x for p in gp)) * 100,
         (max(p.y for p in gp) - min(p.y for p in gp)) * 100))
