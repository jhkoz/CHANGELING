"""Where is the coat's 0.9 m width coming from? Width per height band."""
import bpy
BLEND = "D:/CHANGELING/Tools/Blender/Work/boggan_male_wilder_suit.blend"
bpy.ops.wm.open_mainfile(filepath=BLEND)
o = bpy.data.objects.get("Coat")
mw = o.matrix_world
pts = [mw @ v.co for v in o.data.vertices]
zs = [p.z for p in pts]
lo, hi = min(zs), max(zs)
print("=== Coat width by height band (z %.3f..%.3f) ===" % (lo, hi))
bands = 12
for i in range(bands):
    a = lo + (hi - lo) * i / bands
    b = lo + (hi - lo) * (i + 1) / bands
    band = [p for p in pts if a <= p.z < b]
    if not band:
        print("   z %.3f..%.3f   (empty)" % (a, b)); continue
    xs = [p.x for p in band]; ys = [p.y for p in band]
    print("   z %.3f..%.3f  n=%5d  x %.3f..%.3f (w %.3f)  y %.3f..%.3f (d %.3f)"
          % (a, b, len(band), min(xs), max(xs), max(xs)-min(xs),
             min(ys), max(ys), max(ys)-min(ys)))
