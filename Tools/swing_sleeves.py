"""Swing a placed costume's sleeves out to match the body's arm angle.

    blender -b --python Tools/swing_sleeves.py -- [--body <blend>] [--extra N]

The costume was sculpted on a figure whose arms hang closer to its sides
than the MetaHuman A-pose. Measured against the Boggan childling: at the
shoulder the two agree within 2mm, and they part going down --

    z 1.07   arm x 0.119   sleeve x 0.117
    z 0.90   arm x 0.261   sleeve x 0.143
    z 0.80   arm x 0.340   sleeve x 0.203

-- because the sleeve runs about 19 degrees from vertical and the body's
upper arm runs 37.4.

The sleeve is rotated about the shoulder in the coronal plane, by the
difference. Falls off to nothing at the shoulder itself, so the armhole
stays where it was joined and only the hanging part moves.
"""
import bpy, os, sys, math
from mathutils import Vector, Matrix

sys.path.insert(0, "D:/CHANGELING/Tools")
import blender_metahuman_garment as mhg

argv = sys.argv
args = argv[argv.index("--") + 1:] if "--" in argv else []


def arg(flag, default=None):
    return args[args.index(flag) + 1] if flag in args else default


body_blend = arg("--body", "Tools/Blender/Bodies/Boggan_Childling_Male.blend")
extra = float(arg("--extra", "0"))


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
body = bpy.data.objects.get("Body")
coat = bpy.data.objects.get("Costume")
rig = mhg._armature_for(body) or next(
    o for o in bpy.data.objects if o.type == "ARMATURE" and "Body" in o.name)
amw = rig.matrix_world


def bone(name):
    return amw @ rig.data.bones[name].head_local


mw = coat.matrix_world
inv = mw.inverted()


def sleeve_angle(points, shoulder):
    """Degrees from vertical, from the sleeve's own centre line."""
    lo = [p for p in points if 0.78 < p.z < 0.86]
    hi = [p for p in points if 1.04 < p.z < 1.10]
    if len(lo) < 5 or len(hi) < 5:
        return None
    lx = sum(abs(p.x) for p in lo) / len(lo)
    hx = sum(abs(p.x) for p in hi) / len(hi)
    lz = sum(p.z for p in lo) / len(lo)
    hz = sum(p.z for p in hi) / len(hi)
    return math.degrees(math.atan2(abs(lx - hx), abs(hz - lz)))


moved = 0
for side, sign in (("l", 1.0), ("r", -1.0)):
    sh = bone("upperarm_%s" % side)
    el = bone("lowerarm_%s" % side)
    wr = bone("hand_%s" % side)
    want = math.degrees(math.atan2(math.hypot(el.x - sh.x, el.y - sh.y),
                                   abs(sh.z - el.z)))

    here = [mw @ v.co for v in coat.data.vertices
            if (v.co.x * sign > 0 if False else
                (mw @ v.co).x * sign > sh.x * sign - 0.02)
            and 0.72 < (mw @ v.co).z < 1.14]
    have = sleeve_angle(here, sh)
    if have is None:
        mhg.log("   %s: could not read the sleeve angle" % side)
        continue
    turn = math.radians(want - have + extra)
    mhg.log("   %s sleeve %.1f deg, arm %.1f deg -> swinging %.1f deg"
            % (side, have, want, math.degrees(turn)))

    # Rotate about the shoulder, in the plane the arm opens in. Falloff by
    # how far out along the arm the vertex sits, so the armhole does not tear.
    # WHOLE PLATES, RIGIDLY. NO FALLOFF.
    #
    # This sleeve is not one shell: it is 57 separate plates near the left
    # arm alone, 80 to 225 verts each. Selecting vertices by distance to the
    # bone took some of a plate and left the rest, and scaling the rotation
    # by distance from the shoulder sheared what it did take -- between them
    # they tore the sleeve into shards.
    #
    # An arm swings rigidly about its shoulder, so each plate is rotated
    # whole, by the same angle, chosen by where its CENTRE sits.
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(coat.data)
    bm.verts.ensure_lookup_table()
    seen = set()
    islands = []
    for vert in bm.verts:
        if vert.index in seen:
            continue
        stack, group = [vert], []
        seen.add(vert.index)
        while stack:
            here = stack.pop()
            group.append(here.index)
            for edge in here.link_edges:
                other = edge.other_vert(here)
                if other.index not in seen:
                    seen.add(other.index)
                    stack.append(other)
        islands.append(group)
    bm.free()

    def near_arm(p, lim=0.16):
        for a2, b2 in ((sh, el), (el, wr)):
            d = b2 - a2
            length = d.length
            if length < 1e-6:
                continue
            t = max(0.0, min(1.0, (p - a2).dot(d) / (length * length)))
            if (p - (a2 + d * t)).length < lim:
                return True
        return False

    rot = Matrix.Rotation(-turn * sign, 4, "Y")
    plates = 0
    for group in islands:
        centre = Vector((0.0, 0.0, 0.0))
        for i in group:
            centre += mw @ coat.data.vertices[i].co
        centre /= len(group)
        if centre.x * sign <= 0.0 or not near_arm(centre):
            continue
        for i in group:
            world = mw @ coat.data.vertices[i].co
            coat.data.vertices[i].co = inv @ (sh + rot @ (world - sh))
            moved += 1
        plates += 1
    mhg.log("   %s: %d plate(s) swung whole" % (side, plates))

coat.data.update()
mhg.log("swing: %d sleeve vert(s) moved" % moved)

pts = [mw @ v.co for v in coat.data.vertices]
mhg.log("skirt width at z 0.45 (must not change): %.3f"
        % max([abs(p.x) for p in pts if abs(p.z - 0.45) < 0.02] or [0.0]))
mhg.log("sleeve against arm, after:")
for z in (0.80, 0.90, 1.00, 1.08):
    sh = bone("upperarm_l")
    wr = bone("hand_l")
    t = max(0.0, min(1.0, (sh.z - z) / max(sh.z - wr.z, 1e-6)))
    arm_x = sh.x + (wr.x - sh.x) * t
    band = [p for p in pts if abs(p.z - z) < 0.025 and p.x > 0.08]
    if not band:
        continue
    mhg.log("   z=%.2f  arm %.3f   sleeve %.3f"
            % (z, arm_x, sum(p.x for p in band) / len(band)))

bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(body_blend))
mhg.log("saved %s" % body_blend)
