"""Shade the costume smooth, keeping the edges that are meant to be edges.

    blender -b --python Tools/shade_costume.py -- \
        [--angle 40] [--objects Costume,Shirt,Waistcoat]

The costume came off the sculpt flat shaded -- 672 of its 29516 faces were
marked smooth, and those 672 were the cuffs, because add_cuffs sets its own.
Everything else rendered as what it literally is, a few thousand flat
triangles with a hard edge at every join, which is the faceted, chewed look
the arms have had throughout. The body, face, shirt and waistcoat are all
100% smooth, so the coat was the odd one out.

Flat shading is not a geometry fault and cannot be smoothed away: the
surface underneath was already fairly clean by the time this was found.

Marking every face smooth on its own would go too far the other way and
round off the lapels, the collar and the cuff fold. So this does what auto
smooth used to: everything smooth, then any edge whose two faces meet at
more than `angle` marked sharp, which leaves the tailored creases crisp and
lets the cloth read as cloth.
"""
import bpy, math, os, sys

sys.path.insert(0, "D:/CHANGELING/Tools")
import blender_metahuman_garment as mhg

argv = sys.argv
args = argv[argv.index("--") + 1:] if "--" in argv else []


def arg(flag, default=None):
    return args[args.index(flag) + 1] if flag in args else default


body_blend = arg("--body", "Tools/Blender/Bodies/Boggan_Childling_Male.blend")
angle = math.radians(float(arg("--angle", "40")))
which = arg("--objects", "Costume,Shirt,Waistcoat").split(",")

bpy.ops.wm.open_mainfile(filepath=os.path.abspath(body_blend))
for o in bpy.data.objects:
    if o.type == "MESH" and o.mode != "OBJECT":
        bpy.context.view_layer.objects.active = o
        bpy.ops.object.mode_set(mode="OBJECT")

limit = math.cos(angle)

for name in which:
    obj = bpy.data.objects.get(name.strip())
    if obj is None or obj.type != "MESH":
        mhg.log("no mesh %r -- skipped" % name)
        continue
    me = obj.data
    was = sum(1 for f in me.polygons if f.use_smooth)

    for f in me.polygons:
        f.use_smooth = True

    # Which edges keep their crease. An edge shared by two faces whose
    # normals diverge by more than the threshold is a fold, not curvature.
    faces_of = {}
    for f in me.polygons:
        for key in f.edge_keys:
            faces_of.setdefault(key, []).append(f.index)

    sharp = 0
    for e in me.edges:
        key = tuple(sorted(e.vertices))
        near = faces_of.get(key) or faces_of.get(tuple(e.vertices)) or []
        if len(near) != 2:
            # A boundary keeps its edge; anything non-manifold is left sharp
            # rather than guessed at.
            e.use_edge_sharp = len(near) != 2 and len(near) != 0
            if e.use_edge_sharp:
                sharp += 1
            continue
        a = me.polygons[near[0]].normal
        b = me.polygons[near[1]].normal
        hard = a.dot(b) < limit
        e.use_edge_sharp = hard
        if hard:
            sharp += 1

    me.update()
    mhg.log("%s: %d of %d faces were smooth, now all of them; %d edges "
            "(%.1f%%) kept sharp past %.0f degrees"
            % (obj.name, was, len(me.polygons), sharp,
               100.0 * sharp / max(1, len(me.edges)), math.degrees(angle)))

bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(body_blend))
mhg.log("saved %s" % body_blend)
