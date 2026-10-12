"""Scale a placed costume about its own feet, girth apart from length.

    blender -b --python Tools/scale_costume.py -- [--xy 1.10] [--z 1.0]
        [--body <blend>] [--object Costume]

GIRTH, NOT LENGTH, unless asked. Measured on the Boggan childling, the
costume is already wider than the body nearly everywhere -- the ratio of
body radius to coat radius runs 0.53 to 0.97 -- and pinches at only two
heights: 1.08 at the waist and 1.10 at the upper chest. Scaling uniformly to
fix those would carry the collar 112mm past the neck, because the costume is
not too short; it is too narrow in two places.

Scaled about the soles and the costume's own axis, so the feet stay on the
floor and it stays centred on the figure.
"""
import bpy, os, sys
from mathutils import Vector

sys.path.insert(0, "D:/CHANGELING/Tools")
import blender_metahuman_garment as mhg

argv = sys.argv
args = argv[argv.index("--") + 1:] if "--" in argv else []


def arg(flag, default=None):
    return args[args.index(flag) + 1] if flag in args else default


body_blend = arg("--body", "Tools/Blender/Bodies/Boggan_Childling_Male.blend")
obj_name = arg("--object", "Costume")
xy = float(arg("--xy", "1.10"))
zf = float(arg("--z", "1.0"))


def _leave_editmode():
    """Drop out of Edit Mode before touching geometry.

    A mesh saved in Edit Mode keeps a separate edit-mesh, and on save that
    overwrites the mesh datablock -- so every vertex edit is silently thrown
    away on write, while object-level changes survive. That cost a sleeve
    rotation and two scalings before bmesh refused outright with
    "Mesh 'Mesh_0' is in editmode".
    """
    for obj in bpy.data.objects:
        if obj.type == "MESH" and obj.mode != "OBJECT":
            bpy.context.view_layer.objects.active = obj
            bpy.ops.object.mode_set(mode="OBJECT")
    if bpy.context.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")


bpy.ops.wm.open_mainfile(filepath=os.path.abspath(body_blend))
_leave_editmode()
obj = bpy.data.objects.get(obj_name)
body = bpy.data.objects.get("Body")
if obj is None:
    sys.exit("no object %r" % obj_name)

mw = obj.matrix_world
inv = mw.inverted()
pts = [mw @ v.co for v in obj.data.vertices]
floor = min(p.z for p in pts)
cx = sum(p.x for p in pts) / len(pts)
cy = sum(p.y for p in pts) / len(pts)
mhg.log("%s before: x %.3f..%.3f  z %.3f..%.3f  axis (%.3f, %.3f)"
        % (obj_name, min(p.x for p in pts), max(p.x for p in pts),
           floor, max(p.z for p in pts), cx, cy))

# A BAND OF EXTRA GIRTH, not girth everywhere.
#
# Measured, this coat is tight in one place only: 0.142 against the body's
# 0.164 at z 0.62 and 0.123 against 0.155 at z 0.69. By z 0.76 it carries
# 0.224 against 0.151 and has room to spare. Widening the whole garment to
# fix a 70mm band would bloat everything that already fits.
band_at = float(arg("--band-at", "0.655"))
band_extra = float(arg("--band-extra", "0.0"))
band_reach = float(arg("--band-reach", "0.10"))

import math as _m


# The band must not reach the sleeves. They hang beside the waist, so a
# band defined by height alone catches them: +55% at z 0.795 took the
# costume from x 0.360 to 0.551 and inflated both sleeves with the torso.
band_max = float(arg("--band-max", "0.26"))


def girth_at(z, radius):
    """The xy factor here: the global one, plus the band if this is torso."""
    here = xy
    if band_extra and band_reach > 0.0 and radius <= band_max:
        away = abs(z - band_at) / band_reach
        if away < 1.0:
            # raised cosine, so the band blends rather than stepping
            here += band_extra * 0.5 * (1.0 + _m.cos(_m.pi * away))
    return here


# A global xy factor can also be held to the torso. Applied to everything,
# it pushes the sleeves OUTWARD off the arms rather than widening them --
# they sit far from the body's axis, so a percentage there is a large
# translation.
xy_max = float(arg("--xy-max", "0"))

for v in obj.data.vertices:
    w = mw @ v.co
    r = _m.hypot(w.x - cx, w.y - cy)
    k = girth_at(w.z, r)
    if xy_max and r > xy_max:
        k = 1.0 + (k - 1.0) * 0.0 if False else min(k, 1.0)
    v.co = inv @ Vector((cx + (w.x - cx) * k,
                         cy + (w.y - cy) * k,
                         floor + (w.z - floor) * zf))
obj.data.update()
if band_extra:
    mhg.log("band: +%.0f%% girth at z %.3f, fading over %.0fmm"
            % (band_extra * 100, band_at, band_reach * 1000))

pts = [mw @ v.co for v in obj.data.vertices]
mhg.log("%s after:  x %.3f..%.3f  z %.3f..%.3f   (xy x%.3f, z x%.3f)"
        % (obj_name, min(p.x for p in pts), max(p.x for p in pts),
           min(p.z for p in pts), max(p.z for p in pts), xy, zf))

# Does it clear the body now?
if body is not None:
    bmw = body.matrix_world
    bp = [bmw @ v.co for v in body.data.vertices]
    lo, hi = min(p.z for p in bp), max(p.z for p in bp)
    import math
    worst = 0.0
    worst_z = None
    for i in range(4, 19):
        z = lo + (hi - lo) * 0.05 * i
        bb = [p for p in bp if abs(p.z - z) < 0.02 and abs(p.x) < 0.20]
        cc = [p for p in pts if abs(p.z - z) < 0.02 and abs(p.x) < 0.30]
        if len(bb) < 10 or len(cc) < 10:
            continue
        br = sorted(math.hypot(p.x - cx, p.y - cy)
                    for p in bb)[int(len(bb) * 0.9)]
        cr = sorted(math.hypot(p.x - cx, p.y - cy)
                    for p in cc)[int(len(cc) * 0.9)]
        if cr and br / cr > worst:
            worst, worst_z = br / cr, z
    mhg.log("worst body/coat ratio now %.2f at z %.3f (over 1.00 = body "
            "standing outside)" % (worst, worst_z or 0.0))

# --- sleeves widen about the ARM, not the body ---------------------------
sleeve_xy = float(arg("--sleeve-xy", "0"))
if sleeve_xy:
    rig = mhg._armature_for(body) or next(
        (o for o in bpy.data.objects
         if o.type == "ARMATURE" and "Body" in o.name), None)
    amw = rig.matrix_world
    for side, gname in (("l", "Sleeve_L"), ("r", "Sleeve_R")):
        grp = obj.vertex_groups.get(gname)
        if grp is None:
            mhg.log("   no %s -- skipped" % gname)
            continue
        sh = amw @ rig.data.bones["upperarm_%s" % side].head_local
        wr = amw @ rig.data.bones["hand_%s" % side].head_local
        axis = (wr - sh)
        length = axis.length
        if length < 1e-6:
            continue
        axis = axis / length
        moved = 0
        for v in obj.data.vertices:
            if not any(g.group == grp.index and g.weight > 0 for g in v.groups):
                continue
            world = mw @ v.co
            along = (world - sh).dot(axis)
            spine = sh + axis * along
            out = world - spine
            if out.length < 1e-6:
                continue
            obj.data.vertices[v.index].co = inv @ (spine + out * sleeve_xy)
            moved += 1
        mhg.log("   %s widened x%.2f about the arm axis (%d verts)"
                % (gname, sleeve_xy, moved))
    obj.data.update()

# --- legs widen about the LEG, for the same reason sleeves do ------------
#
# The trousers hang either side of the body's centre line, so a global xy
# factor translates them apart instead of widening them. Scaled about each
# leg's own thigh-to-foot line, the tube opens and stays on the leg.
leg_xy = float(arg("--leg-xy", "0"))
if leg_xy:
    rig = mhg._armature_for(body) or next(
        (o for o in bpy.data.objects
         if o.type == "ARMATURE" and "Body" in o.name), None)
    amw = rig.matrix_world
    reach = float(arg("--leg-reach", "0.16"))
    for side in ("l", "r"):
        chain = []
        for a, b in (("thigh_%s", "calf_%s"), ("calf_%s", "foot_%s")):
            ba = rig.data.bones.get(a % side)
            bb = rig.data.bones.get(b % side)
            if ba is not None and bb is not None:
                chain.append((amw @ ba.head_local, amw @ bb.head_local))
        if not chain:
            mhg.log("   no leg bones for %s" % side)
            continue
        moved = 0
        for v in obj.data.vertices:
            world = mw @ v.co
            best = None
            for start, end in chain:
                span = end - start
                length = span.length
                if length < 1e-6:
                    continue
                t = max(0.0, min(1.0,
                                 (world - start).dot(span) / (length * length)))
                spine = start + span * t
                gap = (world - spine).length
                if best is None or gap < best[0]:
                    best = (gap, spine)
            if best is None or best[0] > reach:
                continue
            spine = best[1]
            out = world - spine
            if out.length < 1e-6:
                continue
            obj.data.vertices[v.index].co = inv @ (spine + out * leg_xy)
            moved += 1
        mhg.log("   leg %s widened x%.2f about its own axis (%d verts)"
                % (side, leg_xy, moved))
    obj.data.update()

bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(body_blend))
mhg.log("saved %s" % body_blend)
