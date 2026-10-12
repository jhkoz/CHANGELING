"""Put a waistcoat behind a placed costume's open front.

    blender -b --python Tools/add_underlayer.py -- \
        [--body <body.blend>] [--top 0.95] [--bottom 0.60] [--offset 0.010]

The costume coat is open down the front and the Boggan is stout, so the
chest and belly stand through the opening. Scaling the coat up does not
close it -- measured, the coat is already WIDER than the body at the belly
(232mm against 165mm per side); the gap is the front being open by design.
What is missing is the layer the opening is meant to reveal.

Built against the body, not the coat, so it follows the figure rather than
the garment, and kept thin so it fills the opening without fighting the
coat for the same space.
"""
import bpy, os, sys

sys.path.insert(0, "D:/CHANGELING/Tools")
import blender_metahuman_garment as mhg

argv = sys.argv
args = argv[argv.index("--") + 1:] if "--" in argv else []


def arg(flag, default=None):
    return args[args.index(flag) + 1] if flag in args else default


body_blend = arg("--body", "Tools/Blender/Bodies/Boggan_Childling_Male.blend")
top = float(arg("--top", "0.88"))
bottom = float(arg("--bottom", "0.50"))
offset = float(arg("--offset", "0.010"))


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
if body is None:
    sys.exit("no Body in %s" % body_blend)

for stale in [o for o in bpy.data.objects
              if o.name.startswith("Waistcoat") or o.name.startswith("Shirt")
              or o.name.startswith("Breeches")]:
    bpy.data.objects.remove(stale, do_unlink=True)

# STOP BELOW THE SHOULDER JOINT.
#
# At 0.95 the waistcoat reached z 1.081 and the shoulder joint is at 1.091,
# so its kept deltoid cap sat exactly where the coat's sleeve is tightest
# and came through it -- the waistcoat read as worn OVER the coat at the
# shoulders. On the chest it has room; at the shoulder it has none.
#
# And it runs lower, because ending at the hip left a band of bare body
# between its hem and the top of the coat's skirt.
# DROP LOOSE ISLANDS FROM THE COSTUME.
#
# The source mesh carries a small detached cube that sits on the floor
# between the feet. Anything not joined to the main shell is debris.
# OFF BY DEFAULT. A sculpted costume's small islands are buttons and
# buckles, not debris: at "under 30 verts" this removed 1456 of them. The
# stray cube is one object a person can delete in a second; guessing which
# fragments are rubbish is not worth losing detail over.
if coat is not None and "--strip-debris" in args:
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
            group.append(here)
            for edge in here.link_edges:
                other = edge.other_vert(here)
                if other.index not in seen:
                    seen.add(other.index)
                    stack.append(other)
        islands.append(group)
    # ONLY THE TINY ONES.
    #
    # "Keep the largest island" is wrong for a sculpted costume: this one is
    # 267 separate shells -- coat, trousers, boots, buttons, all modelled
    # apart -- and that rule threw away 8666 of its 9432 vertices. The debris
    # is a cube of a handful of verts; anything with real geometry stays.
    islands.sort(key=len, reverse=True)
    doomed = [v for grp in islands if len(grp) < 30 for v in grp]
    if doomed:
        mhg.log("costume: %d island(s); dropping %d vert(s) in fragments "
                "under 30 verts" % (len(islands), len(doomed)))
        bmesh.ops.delete(bm, geom=doomed, context="VERTS")
        bm.to_mesh(coat.data)
        coat.data.update()
    bm.free()

# --- the shirt, closest to the skin ------------------------------------
#
# The waistcoat stops below the shoulder joint so its deltoid cap does not
# come through the coat's sleeve, which leaves the shoulders and upper chest
# bare. The shirt covers them, and has to sit UNDER the waistcoat to do it:
# 5mm against the waistcoat's 10mm, thinner again so the two do not fight
# for the same millimetre.
# 0.99, not 0.97: at 0.97 the shirt topped out at z 1.117 against a body
# reaching 1.136, leaving a 19mm strip of bare shoulder above the collar.
shirt_top = float(arg("--shirt-top", "0.99"))
# Above the waistcoat's hem, not below it. At 0.45 the shirt ran 45mm
# lower than the waistcoat and its hem showed at the hip, through the
# coat's open front.
shirt_bottom = float(arg("--shirt-bottom", "0.52"))
shirt_offset = float(arg("--shirt-offset", "0.005"))
shirt = mhg.make_waistcoat(body=body, top=shirt_top, bottom=shirt_bottom,
                           offset=shirt_offset, thickness=0.003,
                           name="shirtlayer")
shirt.name = "Shirt"
want_shirt = "MI_Shirt_boggan_childling_male"
mat_shirt = (bpy.data.materials.get(want_shirt)
             or bpy.data.materials.new(want_shirt))
shirt.data.materials.clear()
shirt.data.materials.append(mat_shirt)
smw = shirt.matrix_world
sp = [smw @ v.co for v in shirt.data.vertices]
mhg.log("shirt z %.3f..%.3f, %d verts"
        % (min(p.z for p in sp), max(p.z for p in sp),
           len(shirt.data.vertices)))

# --- the waistcoat, over it --------------------------------------------
vest = mhg.make_waistcoat(body=body, top=top, bottom=bottom,
                          offset=offset, thickness=0.004, name="under")
vest.name = "Waistcoat"

# Its own material, so it is one of the seven regions and survives a join.
want = "MI_Waistcoat_boggan_childling_male"
mat = bpy.data.materials.get(want) or bpy.data.materials.new(want)
vest.data.materials.clear()
vest.data.materials.append(mat)

vmw = vest.matrix_world
vp = [vmw @ v.co for v in vest.data.vertices]
mhg.log("waistcoat z %.3f..%.3f, %d verts"
        % (min(p.z for p in vp), max(p.z for p in vp), len(vest.data.vertices)))

# Does it stay UNDER the coat? Anything proud of the coat would read as the
# waistcoat worn over it.
if coat is not None:
    cinv = coat.matrix_world.inverted()
    cmw = coat.matrix_world
    proud = tested = 0
    for v in vest.data.vertices:
        p = vmw @ v.co
        ok, loc, nor, _ = coat.closest_point_on_mesh(cinv @ p)
        if not ok:
            continue
        tested += 1
        n = (cmw.to_3x3() @ nor).normalized()
        if (p - (cmw @ loc)).dot(n) > 0.002:
            proud += 1
    mhg.log("waistcoat outside the coat: %d of %d (%.1f%%) -- the open front "
            "accounts for most of it" % (proud, tested,
                                         100.0 * proud / max(tested, 1)))

# --- breeches, under the coat's skirt -----------------------------------
#
# The costume's trousers leave the thighs bare below the skirt. make_breeches
# cuts the body's own leg tubes below the leg merge, so it arrives as two
# legs rather than a skirt, and carries trim bands of its own -- which is
# where the seventh material region comes from.
# OFF unless asked for. The costume has its own trousers; a second pair
# under them is two garments competing for one space.
breeches = None
knee = arg("--knee")
if "--breeches" in args:
    breeches = mhg.make_breeches(body=body,
                                 knee=(float(knee) if knee else None),
                                 offset=0.006, thickness=0.005,
                                 trim=True, name="under")
    breeches.name = "Breeches"
    want_br = "MI_Breeches_boggan_childling_male"
    mat_br = (bpy.data.materials.get(want_br)
              or bpy.data.materials.new(want_br))
    if breeches.data.materials:
        breeches.data.materials[0] = mat_br
    else:
        breeches.data.materials.append(mat_br)
    brp = [breeches.matrix_world @ v.co for v in breeches.data.vertices]
    mhg.log("breeches z %.3f..%.3f, %d verts"
            % (min(p.z for p in brp), max(p.z for p in brp),
               len(breeches.data.vertices)))

mhg.log("body still bare where nothing covers it:")
bmw = body.matrix_world
bp = [bmw @ v.co for v in body.data.vertices]
lo, hi = min(p.z for p in bp), max(p.z for p in bp)
covers = [o for o in (coat, vest, shirt, breeches) if o is not None]
bands = {}
for v in body.data.vertices:
    p = bmw @ v.co
    inside = False
    for garment in covers:
        gi = garment.matrix_world.inverted()
        gm = garment.matrix_world
        ok, loc, nor, _ = garment.closest_point_on_mesh(gi @ p)
        if not ok:
            continue
        n = (gm.to_3x3() @ nor).normalized()
        if (p - (gm @ loc)).dot(n) < 0.0:
            inside = True
            break
    key = round((p.z - lo) / (hi - lo) * 10) / 10.0
    rec = bands.setdefault(key, [0, 0])
    rec[1] += 1
    if not inside:
        rec[0] += 1
for key in sorted(bands):
    bare, tot = bands[key]
    if tot < 50:
        continue
    mhg.log("   %.1f  %5d/%5d bare  %5.1f%%" % (key, bare, tot,
                                                100.0 * bare / tot))

bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(body_blend))
mhg.log("saved %s" % body_blend)
