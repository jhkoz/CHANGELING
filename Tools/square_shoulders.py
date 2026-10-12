"""Run the shoulder line level from the collar out to a corner.

    blender -b --python Tools/square_shoulders.py -- \
        [--inner 100] [--outer 215] [--edge 20] [--depth 60] [--height <mm>]

Measured across the front, the coat's top edge ramps from 1182mm at the
collar down to 1135mm by the time it is 120mm out -- a slope of about
forty-five degrees -- then runs flat at 1145mm across the shoulder, then
rounds off over the last 60mm into the sleeve. That reads as a sloped,
soft shoulder.

A square shoulder is the same line held level from where it leaves the
collar out to the shoulder point, and turned down sharply there. So this
lifts rather than flattens: the shoulder is padded UP to the height it
already has at the inner end, and nothing is lowered. Levelling it the
other way -- cutting the inner shoulder down to the plateau -- would take
37mm off the coat where it meets the collar and leave a trench there.

The lift is measured against the top of the cloth at each lateral station,
not applied as a fixed offset, so it follows whatever the coat is doing
front to back, and it fades out with depth so only the shoulder itself
moves and the chest below it stays where it is.
"""
import bpy, math, os, sys
from mathutils import Vector

sys.path.insert(0, "D:/CHANGELING/Tools")
import blender_metahuman_garment as mhg

argv = sys.argv
args = argv[argv.index("--") + 1:] if "--" in argv else []


def arg(flag, default=None):
    return args[args.index(flag) + 1] if flag in args else default


body_blend = arg("--body", "Tools/Blender/Bodies/Boggan_Childling_Male.blend")
inner = float(arg("--inner", "100")) / 1000.0
outer = float(arg("--outer", "215")) / 1000.0
edge = float(arg("--edge", "20")) / 1000.0
depth = float(arg("--depth", "60")) / 1000.0
forced = arg("--height")
forced = None if forced is None else float(forced) / 1000.0
which = arg("--objects", "Costume").split(",")

bpy.ops.wm.open_mainfile(filepath=os.path.abspath(body_blend))
for o in bpy.data.objects:
    if o.type == "MESH" and o.mode != "OBJECT":
        bpy.context.view_layer.objects.active = o
        bpy.ops.object.mode_set(mode="OBJECT")

STEP = 0.005


def smoothstep(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3.0 - 2.0 * t)


for name in which:
    coat = bpy.data.objects.get(name.strip())
    if coat is None:
        mhg.log("no object %r -- skipped" % name)
        continue
    mw = coat.matrix_world
    inv = mw.inverted()
    pts = [mw @ v.co for v in coat.data.vertices]

    # The top of the cloth at each lateral station -- the maximum, then the
    # SEQUENCE smoothed. A percentile of the column does not do this: every
    # station holds a few hundred vertices running from the shoulder down to
    # the skirt, so its 97th percentile sits 20 to 40mm below the top edge,
    # and a shoulder levelled to that lifts nothing and lowers the rest.
    # Spikes are better dealt with along the line than down it.
    lo = max(0.0, inner - 0.04)
    hi = outer + 0.05
    stations = int((hi - lo) / STEP) + 1
    tops = [None] * stations
    for p in pts:
        j = int(round((abs(p.x) - lo) / STEP))
        if 0 <= j < stations:
            if tops[j] is None:
                tops[j] = []
            tops[j].append(p.z)
    for j in range(stations):
        tops[j] = max(tops[j]) if tops[j] and len(tops[j]) >= 4 else None
    known = [j for j in range(stations) if tops[j] is not None]
    if not known:
        mhg.log("%s: no cloth across the shoulder -- skipped" % coat.name)
        continue
    for j in range(stations):
        if tops[j] is None:
            tops[j] = tops[min(known, key=lambda k: abs(k - j))]

    for _ in range(3):
        prev = list(tops)
        for j in range(1, stations - 1):
            tops[j] = (prev[j - 1] + prev[j] * 2.0 + prev[j + 1]) * 0.25

    def top_at(t):
        u = (t - lo) / STEP
        i = max(0, min(stations - 1, int(math.floor(u))))
        k = min(stations - 1, i + 1)
        f = max(0.0, min(1.0, u - math.floor(u)))
        return tops[i] + (tops[k] - tops[i]) * f

    level = forced if forced is not None else top_at(inner)

    def target_at(t):
        if t <= outer - edge:
            return level
        if t >= outer:
            return top_at(t)
        f = smoothstep((t - (outer - edge)) / edge)
        return level + (top_at(t) - level) * f

    moved = 0
    most = 0.0
    for v, p in zip(coat.data.vertices, pts):
        t = abs(p.x)
        if t < inner or t > outer:
            continue
        here = top_at(t)
        want = target_at(t)
        if want <= here:
            continue
        k = smoothstep((p.z - (here - depth)) / depth)
        if k <= 1e-3:
            continue
        rise = (want - here) * k
        v.co = inv @ (p + Vector((0.0, 0.0, rise)))
        most = max(most, rise)
        moved += 1

    coat.data.update()
    mhg.log("%s: squared to z %.0fmm from %.0f to %.0fmm out; %d verts "
            "lifted, most by %.0fmm"
            % (coat.name, level * 1000, inner * 1000, outer * 1000,
               moved, most * 1000))
    mhg.log("   line was %s"
            % " ".join("%.0f" % (top_at(inner + i * 0.02) * 1000)
                       for i in range(int((outer - inner) / 0.02) + 1)))

bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(body_blend))
mhg.log("saved %s" % body_blend)
