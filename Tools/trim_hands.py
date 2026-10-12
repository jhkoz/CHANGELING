"""Remove a costume's hand geometry, and drop the sleeves a few degrees.

    blender -b --python Tools/trim_hands.py -- [--drop 8] [--keep 0.02]
        [--body <blend>] [--no-hands] [--no-drop]

The costume carries gloves past the wrist -- 347 vertices per side reaching
x 0.510 against a wrist at 0.377. The MetaHuman body has its own hands, and
two sets in one place is worse than none.

Sleeves are dropped about the shoulder using the same Sleeve_L / Sleeve_R
groups, with the same easing at the selection edge, so the panels bend.
"""
import bpy, os, sys, math, bmesh
from mathutils import Matrix, Vector

sys.path.insert(0, "D:/CHANGELING/Tools")
import blender_metahuman_garment as mhg

argv = sys.argv
args = argv[argv.index("--") + 1:] if "--" in argv else []


def arg(flag, default=None):
    return args[args.index(flag) + 1] if flag in args else default


body_blend = arg("--body", "Tools/Blender/Bodies/Boggan_Childling_Male.blend")
drop = float(arg("--drop", "8"))
keep = float(arg("--keep", "0.02"))

bpy.ops.wm.open_mainfile(filepath=os.path.abspath(body_blend))
for o in bpy.data.objects:
    if o.type == "MESH" and o.mode != "OBJECT":
        bpy.context.view_layer.objects.active = o
        bpy.ops.object.mode_set(mode="OBJECT")

coat = bpy.data.objects.get("Costume")
body = bpy.data.objects.get("Body")
rig = mhg._armature_for(body) or next(
    o for o in bpy.data.objects if o.type == "ARMATURE" and "Body" in o.name)
amw = rig.matrix_world
mw = coat.matrix_world
inv = mw.inverted()

# --- the hands ----------------------------------------------------------
if "--no-hands" not in args:
    doomed = set()

    # BY ISLAND, AT THE SLEEVE'S END -- not "outboard of the wrist".
    #
    # The costume's arms hang at its sides, so its gloves sit at the LOWER
    # end of the sleeve, nowhere near where the MetaHuman's hand bone is.
    # Cutting on "beyond the wrist" removed 826 verts once the sleeves had
    # been rotated outward, and removes exactly 0 from the costume at rest.
    #
    # The gloves are their own islands: 45 of them at the sleeve ends, the
    # small ones clustered at z 0.60-0.73 and |x| about 0.31, while the
    # sleeve proper runs as two large islands from z 0.61 up to 0.98.
    glove_z = float(arg("--glove-z", "0.70"))
    glove_x = float(arg("--glove-x", "0.28"))
    glove_max = int(arg("--glove-max", "120"))

    bm0 = bmesh.new()
    bm0.from_mesh(coat.data)
    bm0.verts.ensure_lookup_table()
    seen = set()
    islands = []
    for vert in bm0.verts:
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
    bm0.free()

    gloves = 0
    for group in islands:
        if len(group) > glove_max:
            continue                      # that is sleeve, not glove
        centre = Vector((0.0, 0.0, 0.0))
        for i in group:
            centre += mw @ coat.data.vertices[i].co
        centre /= len(group)
        if abs(centre.x) >= glove_x and centre.z <= glove_z:
            doomed.update(group)
            gloves += 1
    mhg.log("gloves: %d island(s) under %d verts at the sleeve ends"
            % (gloves, glove_max))
    if doomed:
        bm = bmesh.new()
        bm.from_mesh(coat.data)
        bm.verts.ensure_lookup_table()
        bmesh.ops.delete(bm, geom=[bm.verts[i] for i in sorted(doomed)],
                         context="VERTS")
        bm.to_mesh(coat.data)
        bm.free()
        coat.data.update()
    mhg.log("hands: %d vert(s) removed" % len(doomed))

# --- drop the sleeves ----------------------------------------------------
if "--no-drop" not in args and drop:
    for side, group_name in (("l", "Sleeve_L"), ("r", "Sleeve_R")):
        group = coat.vertex_groups.get(group_name)
        if group is None:
            mhg.log("   no group %s -- skipped" % group_name)
            continue
        pivot = amw @ rig.data.bones["upperarm_%s" % side].head_local
        weight = {v.index: 0.0 for v in coat.data.vertices}
        for v in coat.data.vertices:
            for g in v.groups:
                if g.group == group.index:
                    weight[v.index] = max(weight[v.index], g.weight)
        near = {}
        for edge in coat.data.edges:
            a, b = edge.vertices
            near.setdefault(a, []).append(b)
            near.setdefault(b, []).append(a)
        for _ in range(6):
            nxt = dict(weight)
            for i, lst in near.items():
                if lst:
                    nxt[i] = weight[i] * 0.5 + \
                        sum(weight[j] for j in lst) / len(lst) * 0.5
            weight = nxt
        sign = 1.0 if pivot.x >= 0 else -1.0
        moved = 0
        for v in coat.data.vertices:
            w = weight.get(v.index, 0.0)
            if w <= 0.001:
                continue
            rot = Matrix.Rotation(math.radians(drop) * sign * w, 4, "Y")
            world = mw @ v.co
            v.co = inv @ (pivot + rot @ (world - pivot))
            moved += 1
        mhg.log("   %s dropped %.1f deg (%d vert(s))"
                % (group_name, drop, moved))

# --- a cuff seam on the cut edge ----------------------------------------
#
# Removing the hands leaves a raw boundary at the wrist, and a raw boundary
# is what the clearance passes shred and Solidify rims. Rolled the same way
# as the collar -- out a little, then turned back -- so the sleeve ends in a
# cuff rather than a cut.
if "--no-cuff" not in args:
    bm = bmesh.new()
    bm.from_mesh(coat.data)
    bm.verts.ensure_lookup_table()

    # AT THE SLEEVE'S OWN END, not at the wrist bone.
    #
    # The costume's arms hang at its sides, so the edge left by removing the
    # gloves sits low and outboard -- around z 0.60 to 0.73 at |x| 0.31 --
    # while the MetaHuman's hand bone is at (0.377, -0.130, 0.754). Hunting
    # near the bone found nothing to hem.
    hem_below = float(arg("--hem-below", "0.78"))
    hem_out = float(arg("--hem-out", "0.20"))

    def at_cuff(vert):
        p = mw @ vert.co
        return abs(p.x) >= hem_out and p.z <= hem_below

    edges = [e for e in bm.edges
             if e.is_boundary and all(at_cuff(v) for v in e.verts)]
    made = 0

    # A DEEPER TURN-BACK, AND ITS OWN CLOTH.
    #
    # The first roll was 4mm out and 2mm up, then 3mm out and 4mm back --
    # enough to close the edge, not enough to read as a cuff: it showed as a
    # thin ragged lip. Doubled, and the new faces go into their own material
    # slot so the cuff reads as a band the way the coat's hem trim does.
    depth = float(arg("--cuff-depth", "2.0"))
    cuff_mat = arg("--cuff-material", "MI_Cuff_boggan_childling_male")
    cuff_slot = None
    if cuff_mat:
        mat = bpy.data.materials.get(cuff_mat) or bpy.data.materials.new(cuff_mat)
        names = [m.name if m else "" for m in coat.data.materials]
        if cuff_mat in names:
            cuff_slot = names.index(cuff_mat)
        else:
            coat.data.materials.append(mat)
            cuff_slot = len(coat.data.materials) - 1

    fresh_faces = []
    if edges:
        for grow, back in ((0.004 * depth, 0.002 * depth),
                           (0.003 * depth, -0.004 * depth),
                           (0.0025 * depth, -0.003 * depth)):
            ret = bmesh.ops.extrude_edge_only(bm, edges=edges)
            fresh = [g for g in ret["geom"]
                     if isinstance(g, bmesh.types.BMVert)]
            if not fresh:
                break
            hub = sum((mw @ v.co for v in fresh),
                      Vector((0.0, 0.0, 0.0))) / len(fresh)
            for v in fresh:
                world = mw @ v.co
                out = world - hub
                out.z = 0.0
                if out.length > 1e-6:
                    out = out.normalized() * grow
                v.co = inv @ (world + out + Vector((0.0, 0.0, back)))
            fresh_faces += [g for g in ret["geom"]
                            if isinstance(g, bmesh.types.BMFace)]
            edges = [e for e in ret["geom"]
                     if isinstance(e, bmesh.types.BMEdge) and e.is_boundary]
            made += 1
            if not edges:
                break
        if cuff_slot is not None:
            for face in fresh_faces:
                face.material_index = cuff_slot
        bm.to_mesh(coat.data)
        coat.data.update()
    bm.free()
    mhg.log("cuff: %d fold(s) at depth x%.1f, %d face(s) into slot %r"
            % (made, depth, len(fresh_faces), cuff_mat))

pts = [mw @ v.co for v in coat.data.vertices]
mhg.log("costume now x %.3f..%.3f  z %.3f..%.3f  (%d verts)"
        % (min(p.x for p in pts), max(p.x for p in pts),
           min(p.z for p in pts), max(p.z for p in pts),
           len(coat.data.vertices)))

# --- the torso's own side line, sleeves excluded -------------------------
mhg.log("coat side, armpit to waist (torso only, |x| < 0.26):")
for i in range(10):
    z = 0.62 + 0.04 * i
    band = [p for p in pts if abs(p.z - z) < 0.018
            and abs(p.y + 0.03) < 0.13 and abs(p.x) < 0.26]
    if len(band) < 5:
        continue
    mhg.log("   z=%.3f  n=%4d  max|x| %.3f"
            % (z, len(band), max(abs(p.x) for p in band)))

bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(body_blend))
mhg.log("saved %s" % body_blend)
