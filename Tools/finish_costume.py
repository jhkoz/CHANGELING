"""Boots, and the lapel and trim regions on a placed costume.

    blender -b --python Tools/finish_costume.py -- [--body <blend>]
        [--boot-top 0.30] [--lapel-from 0.72] [--hem-band 0.035]

BOOTS are geometry: the costume's own boots cover only part of the foot and
the breeches stop at the knee, so the calves and feet are bare. make_boots
builds them from the body's leg tubes.

LAPELS and COAT TRIM are not. The costume is sculpted and already HAS
lapels, a hem and cuffs -- what it lacks is material regions to shade them
with. It arrives as one mesh with two slots. So those are assigned by
position, the same way tailor_frock does it on a procedural coat: a face
belongs to the lapel if it faces forward high on the chest, and to the trim
if it lies within a band of an edge.

Nothing is subdivided. A material slot is per-FACE, so existing faces can
take a new slot; new geometry is only needed when a boundary falls mid-face,
and these do not.
"""
import bpy, os, sys, math
from mathutils import Vector

sys.path.insert(0, "D:/CHANGELING/Tools")
import blender_metahuman_garment as mhg

argv = sys.argv
args = argv[argv.index("--") + 1:] if "--" in argv else []


def arg(flag, default=None):
    return args[args.index(flag) + 1] if flag in args else default


body_blend = arg("--body", "Tools/Blender/Bodies/Boggan_Childling_Male.blend")
boot_top = float(arg("--boot-top", "0.30"))
lapel_from = float(arg("--lapel-from", "0.72"))
hem_band = float(arg("--hem-band", "0.035"))
tag = "boggan_childling_male"


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

for stale in [o for o in bpy.data.objects if o.name.startswith("Boots")]:
    bpy.data.objects.remove(stale, do_unlink=True)

# --- procedural boots are OFF unless asked for ---------------------------
#
# The costume has its own boots. Building a second pair under them leaves
# two sets competing for the same space, and the body trim that goes with
# them is destructive.
boots = None
if "--boots" in args:
    _bpts = [body.matrix_world @ v.co for v in body.data.vertices]
    if (max(p.z for p in _bpts) - min(p.z for p in _bpts)) < 0.9:
        sys.exit("body already trimmed -- re-run from an untrimmed body.")
    boots = mhg.make_boots(body=body, name="under", top=boot_top,
                           offset=0.022, thickness=0.009)
    boots.name = "Boots"
    want = "MI_Boots_%s" % tag
    mat = bpy.data.materials.get(want) or bpy.data.materials.new(want)
    if boots.data.materials:
        boots.data.materials[0] = mat
    else:
        boots.data.materials.append(mat)
    arm = mhg._armature_for(body)
    fb = arm.data.bones.get("foot_l") if arm else None
    if fb is not None:
        toe_z = (arm.matrix_world @ fb.head_local).z
        mhg.toe_box(boots, upto_z=toe_z)
        mhg.round_toe(boots, body)
        top_z = max((boots.matrix_world @ v.co).z for v in boots.data.vertices)
        mhg.trim_body_beneath(body, [boots], reach=0.075,
                              keep_above=top_z - 0.040, margin=0)
    bp = [boots.matrix_world @ v.co for v in boots.data.vertices]
    mhg.log("boots z %.3f..%.3f, %d verts"
            % (min(p.z for p in bp), max(p.z for p in bp),
               len(boots.data.vertices)))

# --- lapel and trim regions on the costume -------------------------------
if coat is not None:
    def slot_for(name):
        material = bpy.data.materials.get(name) or bpy.data.materials.new(name)
        for i, m in enumerate(coat.data.materials):
            if m is not None and m.name == name:
                return i
        coat.data.materials.append(material)
        return len(coat.data.materials) - 1

    lapel_slot = slot_for("MI_Lapels_%s" % tag)
    trim_slot = slot_for("MI_CourtTrim_%s" % tag)
    boots_slot = slot_for("MI_Boots_%s" % tag)
    breech_slot = slot_for("MI_Breeches_%s" % tag)
    boot_z = float(arg("--boot-z", "0.105"))
    hip_z = float(arg("--hip-z", "0.78"))
    leg_r = float(arg("--leg-r", "0.17"))

    # THE STRAY CUBE HAS TO GO, and not for tidiness.
    #
    # The hem band was taken from the mesh's lowest point, and the mesh's
    # lowest point is a detached cube sitting on the floor -- so the trim
    # landed at floor level as gold specks around the feet. Only the island
    # holding the very lowest vertex is removed, and only if it is small:
    # this costume is 267 separate shells and most of them are buttons.
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(coat.data)
    bm.verts.ensure_lookup_table()
    lowest = min(bm.verts, key=lambda v: (coat.matrix_world @ v.co).z)
    stack, island = [lowest], set()
    island.add(lowest.index)
    while stack:
        here = stack.pop()
        for edge in here.link_edges:
            other = edge.other_vert(here)
            if other.index not in island:
                island.add(other.index)
                stack.append(other)
    if len(island) < 50:
        doomed = [bm.verts[i] for i in island]
        mhg.log("costume: dropping the %d-vert island at the lowest point"
                % len(doomed))
        bmesh.ops.delete(bm, geom=doomed, context="VERTS")
        bm.to_mesh(coat.data)
        coat.data.update()
    else:
        mhg.log("costume: lowest island has %d verts -- left alone"
                % len(island))
    bm.free()

    mw = coat.matrix_world
    pts = [mw @ v.co for v in coat.data.vertices]
    lo = min(p.z for p in pts)
    hi = max(p.z for p in pts)

    # THE COAT'S HEM IS NOT THE MESH'S FLOOR EITHER. The costume carries its
    # own trousers and boots down to the ground, so the skirt's edge has to
    # be found rather than assumed: scanning upward, the count per slice
    # jumps where the skirt starts, because a skirt is far wider than two
    # legs.
    # THE LOWEST JUMP, NOT THE BIGGEST ONE.
    #
    # Scanning up from the floor the count per slice is small (two legs),
    # jumps where the skirt's hem begins, and keeps climbing to the widest
    # part of the skirt at the hip. Taking the LARGEST jump found the hip:
    # the trim band came out at z 0.597, around the waist. The hem is the
    # FIRST place the count jumps.
    hem_z = None
    for i in range(1, 40):
        z = lo + (hi - lo) * 0.02 * i
        below = sum(1 for p in pts if abs(p.z - (z - 0.015)) < 0.012)
        above = sum(1 for p in pts if abs(p.z - (z + 0.015)) < 0.012)
        if below >= 8 and above > below * 1.6 and above - below > 25:
            hem_z = z
            mhg.log("costume: count %d -> %d across z %.3f" % (below, above, z))
            break
    if hem_z is None:
        hem_z = lo + hem_band
    mhg.log("costume: skirt hem found at z %.3f (mesh floor %.3f)"
            % (hem_z, lo))
    span = hi - lo
    cx = sum(p.x for p in pts) / len(pts)
    cy = sum(p.y for p in pts) / len(pts)

    lapel_z = lo + span * lapel_from
    lapels = trims = booted = breeched = 0
    for face in coat.data.polygons:
        centre = mw @ face.center
        forward = centre.y - cy

        # The lapel: forward-facing, high on the chest, near the opening.
        if centre.z >= lapel_z and forward > 0.0 and abs(centre.x - cx) < 0.12:
            face.material_index = lapel_slot
            lapels += 1
            continue

        # The trim: a band at the skirt's hem.
        if hem_z <= centre.z <= hem_z + hem_band:
            face.material_index = trim_slot
            trims += 1
            continue

        # THE COSTUME'S OWN LEGWEAR, by height and by how far out it sits.
        #
        # Measured: below z 0.25 there is nothing but legs and boots -- the
        # skirt's hem is at 0.239 and the slice count jumps 52 to 404 there.
        # Within that, the max radius falls 0.201 to 0.156 between z 0.07 and
        # 0.11, which is the top of the boot: these are ankle shoes, not the
        # tall boots the silhouette suggests.
        #
        # Above the boots, the legs are INBOARD and the coat's skirt is
        # outboard, so radius separates trouser from skirt where height alone
        # cannot.
        radius = math.hypot(centre.x - cx, centre.y - cy)
        if centre.z < boot_z:
            face.material_index = boots_slot
            booted += 1
            continue
        if centre.z < hip_z and radius < leg_r:
            face.material_index = breech_slot
            breeched += 1
            continue

    coat.data.update()
    mhg.log("costume slots: %s"
            % [m.name if m else "-" for m in coat.data.materials])
    mhg.log("   lapels %d, trim %d, boots %d, breeches %d, of %d faces"
            % (lapels, trims, booted, breeched, len(coat.data.polygons)))

bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(body_blend))
mhg.log("saved %s" % body_blend)
