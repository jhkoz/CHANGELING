"""Build a frock coat, breeches and boots on the Sidekick-assembled body.

    blender -b --python Tools/make_sidekick_suit.py -- \
        --character Boggan_Childling_Male

The MetaHuman pipeline's numbers cannot be reused here, for one reason that
would ruin every piece: a MetaHuman body is HEADLESS, so a fraction like
"collar at 0.99 of height" lands at the neck. This body has a head and hair,
and 0.99 of its height is the top of the skull -- the coat would be cut
through the face.

So every landmark is read from a bone instead. Synty names its rig the same
way Unreal does -- pelvis, spine_01, clavicle_l, thigh_l, calf_l, ball_l are
all present -- so the existing garment code's bone lookups work untouched;
only the fractions handed to it have to be computed rather than assumed.

The cut follows the Met references: waist suppression with the skirt flaring
below it, a stand collar, lapels faced in their own cloth, a centre back
vent, and buttons stopping at the waist seam.
"""

import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.append(HERE)

PROJECT = os.path.normpath(os.path.join(HERE, ".."))
FBX_DIR = os.path.join(PROJECT, "Tools", "Export", "Characters")
OUT_DIR = os.path.join(PROJECT, "Tools", "Blender", "Sidekick")

import blender_metahuman_garment as mhg


def log(msg):
    print("[sidekick] %s" % msg)


def opt(args, name, default=None):
    return args[args.index(name) + 1] if name in args else default


def match_rig_to_mesh(body, armature):
    """Scale the armature onto the body it is supposed to drive.

    assemble_sidekick scales the GEOMETRY and leaves the skeleton alone, on
    purpose, so one animation set can drive every NPC. The cost is that on
    this body every bone sits about 30% too high and slightly too narrow,
    and every piece of garment code that asks the rig a question gets a wrong
    answer: the shoulder cap kept ZERO vertices because the shoulder joint it
    measured from was above the flesh, which collapsed the armholes and put
    the sleeves back to being flaps.

    Rather than teach each lookup about the mismatch, the rig is scaled onto
    the mesh once, per axis, from their bounding boxes. Both stand with their
    feet at z=0 so the scale is about the origin and nothing needs moving.

    Worth saying plainly: this is a measuring fix for garment construction.
    The exported character still has the mismatch, and it will deform badly
    when animated -- a knee that bends about a joint 30% above the knee.
    """
    mw = body.matrix_world
    pts = [mw @ v.co for v in body.data.vertices]
    heads = [armature.matrix_world @ b.head_local for b in armature.data.bones]
    if not heads:
        return 1.0

    def span(values):
        return max(max(values) - min(values), 1e-6)

    sx = span([p.x for p in pts]) / span([h.x for h in heads])
    sz = span([p.z for p in pts]) / span([h.z for h in heads])
    # Depth is unreliable: the rig has almost no spread in y, so its span is
    # noise. Use the width factor, which the girth scaling shared.
    sy = sx

    # MULTIPLY the existing scale, do not replace it. The FBX imports the
    # armature at 0.01 with its bone coordinates in centimetres; assigning
    # the ratio outright threw that away and the shoulder came back at
    # z 104.7 on a 1.343m body. Same mistake as fit_source_garment made
    # against this rig earlier, and it fails the same silent way: every
    # measurement is a hundred times too big, nothing raises, the shoulder
    # cap simply finds no vertices near a joint that is off in space.
    log("rig scaled onto the mesh: x %.3f  y %.3f  z %.3f (was %.4f)"
        % (sx, sy, sz, armature.scale.x))
    armature.scale = (armature.scale.x * sx,
                      armature.scale.y * sy,
                      armature.scale.z * sz)
    bpy.context.view_layer.update()
    bpy.ops.object.select_all(action="DESELECT")
    armature.select_set(True)
    bpy.context.view_layer.objects.active = armature
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return sz


def neck_from_mesh(body):
    """Find the neck by looking at the body, not at the rig.

    Needed because the rig cannot be trusted here: assemble_sidekick scales
    the GEOMETRY and deliberately leaves the skeleton alone, so this body is
    1.343m tall on a 1.761m armature and every bone sits well above the flesh
    it is supposed to drive. Reading the collar height straight off neck_01
    put it at 1.09 of the body's height -- above the top of the head.

    A neck is the narrowest horizontal slice in the upper part of a standing
    figure, which is true whatever the rig says and whatever the pose.
    """
    mw = body.matrix_world
    pts = [mw @ v.co for v in body.data.vertices]
    zs = [p.z for p in pts]
    lo, hi = min(zs), max(zs)
    span = max(hi - lo, 1e-6)

    best = None
    slices = 60
    for i in range(slices):
        f = i / float(slices)
        if not (0.60 <= f <= 0.92):
            continue
        a = lo + span * f
        b = lo + span * (f + 1.0 / slices)
        band = [p for p in pts if a <= p.z < b]
        if len(band) < 12:
            continue
        # Width across the body only; in T-pose the arms are the widest
        # thing at shoulder height and would drown the measurement.
        xs = sorted(abs(p.x) for p in band)
        width = xs[int(len(xs) * 0.6)]
        depth = max(p.y for p in band) - min(p.y for p in band)
        girth = width + depth
        if best is None or girth < best[0]:
            best = (girth, f)
    return best[1] if best else 0.82


def landmarks(body, armature):
    """Height fractions for the bones the garments need.

    The rig is scaled differently to the mesh, so bone heights are mapped
    through a factor solved from a landmark BOTH can see: the neck. Finding
    it in the mesh and dividing by the neck bone's height gives the scale the
    geometry was built at, and every other bone maps through the same factor.
    """
    mw = body.matrix_world
    zs = [(mw @ v.co).z for v in body.data.vertices]
    lo, hi = min(zs), max(zs)
    span = max(hi - lo, 1e-6)

    neck_f = neck_from_mesh(body)
    neck_z = lo + span * neck_f

    bone_neck = armature.data.bones.get("neck_01")
    scale = 1.0
    if bone_neck is not None:
        bz = (armature.matrix_world @ bone_neck.head_local).z
        if bz > 1e-6:
            scale = neck_z / bz
    log("   rig/mesh scale solved from the neck: %.3f" % scale)

    found = {"neck": neck_f, "_span": span, "_lo": lo, "_scale": scale}
    for key, bone_name in (("chest", "spine_05"), ("waist", "spine_01"),
                           ("hip", "pelvis"), ("knee", "calf_l"),
                           ("ankle", "foot_l"), ("toe", "ball_l"),
                           ("shoulder", "upperarm_l")):
        bone = armature.data.bones.get(bone_name)
        if bone is None:
            continue
        z = (armature.matrix_world @ bone.head_local).z * scale
        found[key] = (z - lo) / span
    return found


def main():
    argv = sys.argv
    args = argv[argv.index("--") + 1:] if "--" in argv else []
    name = opt(args, "--character", "Boggan_Childling_Male")

    fbx = os.path.join(FBX_DIR, "%s.fbx" % name)
    if not os.path.isfile(fbx):
        log("no character at %s" % fbx)
        return

    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=fbx)

    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    armature = next((o for o in bpy.context.scene.objects
                     if o.type == "ARMATURE"), None)
    if not meshes or armature is None:
        log("expected a mesh and an armature")
        return
    body = max(meshes, key=lambda o: len(o.data.vertices))
    body.name = "Avatar_%s" % name
    armature.name = "Avatar_Armature"

    match_rig_to_mesh(body, armature)
    marks = landmarks(body, armature)
    log("%s: %d verts, %.3f m tall" % (name, len(body.data.vertices),
                                       marks["_span"]))
    for key in ("neck", "chest", "waist", "hip", "knee", "ankle", "toe"):
        if key in marks:
            log("   %-6s at %.3f of height" % (key, marks[key]))

    neck = marks.get("neck", 0.82)
    knee = marks.get("knee", 0.28)
    ankle = marks.get("ankle", 0.06)

    # THE COLLAR MUST CLEAR THE SHOULDER, not merely reach the neck.
    #
    # make_coat's own docstring records what happens otherwise: cut below the
    # shoulder and the garment has no shoulder in it and no armhole either,
    # just a side opening at chest height, and add_sleeves extrudes flat
    # panels from it. On this body the neck is at 0.783 and the shoulder at
    # 0.779 -- only 4mm apart -- so "neck minus a margin" landed BELOW the
    # shoulder and collapsed the armholes to a single opening. Two boundary
    # loops where four are needed.
    shoulder = marks.get("shoulder", neck - 0.01)
    collar = min(0.97, max(neck, shoulder) + 0.020)
    hem = max(0.12, knee - 0.02)

    log("cutting: collar %.3f (neck %.3f, shoulder %.3f), hem %.3f (knee %.3f)"
        % (collar, neck, shoulder, hem, knee))

    # Breeches first, then boots, then the coat over the top: each piece is
    # cut clear of whatever is already worn.
    breeches = mhg.make_breeches(body=body, name=name,
                                 waist=marks.get("hip", 0.52) + 0.04,
                                 knee=knee - 0.01)
    boots = mhg.make_boots(body=body, name=name, top=max(0.10, knee - 0.04))
    coat = mhg.make_coat(body=body, name=name, collar=collar, skirt_to=hem)

    if not os.path.isdir(OUT_DIR):
        os.makedirs(OUT_DIR)
    out = os.path.join(OUT_DIR, "%s_suit.blend" % name.lower())
    bpy.ops.wm.save_as_mainfile(filepath=out)

    for piece in (coat, breeches, boots):
        if piece is not None:
            log("%-10s %6d verts, %6d faces, %d material slot(s)"
                % (piece.name, len(piece.data.vertices),
                   len(piece.data.polygons), len(piece.data.materials)))
    log("saved %s (%.1f MB)" % (out, os.path.getsize(out) / 1048576.0))


main()
