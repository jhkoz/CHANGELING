"""Build a full outfit -- coat, waistcoat, breeches, boots -- on a MetaHuman body.

    blender -b --python Tools/make_outfit.py -- \
        --source Tools/Blender/Bodies/Boggan_Childling_Male.blend

Works from the imported character rather than from a prepped avatar, so the
body is whichever one the Unreal actor actually wears.

Order matters and is not arbitrary: each piece is cut clear of whatever is
already on the figure, so breeches go on before boots, boots before the
waistcoat, and the coat last over all of it.

THE FACE IS NOT THE BODY. The imported character has two meshes, and every
garment call takes `body` explicitly for that reason -- left to find a body
on its own, the helpers can pick up the face and cut a coat from a head.
"""

import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.append(HERE)

PROJECT = os.path.normpath(os.path.join(HERE, ".."))
OUT_DIR = os.path.join(PROJECT, "Tools", "Blender", "Outfits")

import blender_metahuman_garment as mhg


def log(msg):
    print("[outfit] %s" % msg)


def opt(args, name, default=None):
    return args[args.index(name) + 1] if name in args else default


def fraction_of(body, armature, bone_name, default):
    """Where a bone sits, as a fraction of the body's height."""
    if armature is None:
        return default
    bone = armature.data.bones.get(bone_name)
    if bone is None:
        return default
    mw = body.matrix_world
    zs = [(mw @ v.co).z for v in body.data.vertices]
    lo, hi = min(zs), max(zs)
    z = (armature.matrix_world @ bone.head_local).z
    return (z - lo) / max(hi - lo, 1e-6)


def main():
    argv = sys.argv
    args = argv[argv.index("--") + 1:] if "--" in argv else []
    source = opt(args, "--source",
                 os.path.join(PROJECT, "Tools", "Blender", "Bodies",
                              "Boggan_Childling_Male.blend"))
    if not os.path.isabs(source):
        source = os.path.join(PROJECT, source)
    if not os.path.isfile(source):
        log("no character at %s" % source)
        return

    bpy.ops.wm.open_mainfile(filepath=source)

    body = bpy.data.objects.get("Body")
    face = bpy.data.objects.get("Face")
    if body is None:
        meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
        if not meshes:
            log("no meshes in %s" % source)
            return
        body = max(meshes, key=lambda o: len(o.data.vertices))
    armature = mhg._armature_for(body)

    mw = body.matrix_world
    zs = [(mw @ v.co).z for v in body.data.vertices]
    height = max(zs) - min(zs)
    log("body %s: %d verts, %.3f m (headless: the face is a separate mesh)"
        % (body.name, len(body.data.vertices), height))
    if face is not None:
        log("face %s: %d verts -- kept for the preview, not used for cutting"
            % (face.name, len(face.data.vertices)))

    knee = fraction_of(body, armature, "calf_l", 0.28)
    hip = fraction_of(body, armature, "pelvis", 0.52)
    chest = fraction_of(body, armature, "spine_05", 0.78)
    ankle = fraction_of(body, armature, "foot_l", 0.06)

    # Midcalf: where the boot stops and the breeches take over. Slightly
    # above halfway, which is where a riding boot actually sits.
    midcalf = ankle + (knee - ankle) * 0.58
    log("landmarks: hip %.3f  chest %.3f  knee %.3f  ankle %.3f  midcalf %.3f"
        % (hip, chest, knee, ankle, midcalf))

    # THE BODY HAS NO NECK, AND THE FACE MESH IS NOT JUST A FACE.
    #
    # Measured: the body runs to z 1.1355 and at the 0.99 cut (z 1.1245) its
    # cross-section is still 337mm wide -- the shoulders. There is no neck to
    # cut a neck hole from. Meanwhile the FACE mesh starts at z 1.0939,
    # BELOW the body's top, and is 290mm wide at z 1.12: it carries the neck
    # AND the upper chest, overlapping the body.
    #
    # So the coat was being cut 11mm below the top of a body that itself
    # stops below the visible chest surface, and the face slab sat above the
    # coat's edge looking like bare shoulders. Cut at the very top instead
    # and let the collar cover what remains.
    collar = 0.999
    hem = max(0.12, knee - 0.02)

    # LAYER OFFSETS, AND WHY THEY HAVE TO BE SPREAD.
    #
    # add_thickness shrinkwraps each garment onto the body at its offset,
    # which also undoes the drape. So the offset is not a hint -- it is the
    # final distance from the skin, and four garments built at 4-10mm all
    # land in the same shell and z-fight into a mottled mess. Spread them and
    # each layer sits outside the one beneath.
    #
    # Boots sit outside the breeches because they overlap at the calf.
    # Measured on the built outfit: the waistcoat sits 19-23mm off the body
    # and the coat's closest vertices 33mm, so only 10mm separated them and
    # the waistcoat streaked through wherever the coat's bodice was thin or
    # ragged. Widening the coat to 42mm roughly doubles the gap without
    # making it look inflated, since the drape already carries most of the
    # volume.
    # Boots stand well off the leg: a leather boot is not a second skin, and
    # at 15mm this one followed every contour of the calf.
    offsets = {"breeches": 0.007, "boots": 0.026,
               "waistcoat": 0.018, "coat": 0.042}
    log("layer offsets: %s" % ", ".join(
        "%s %.0fmm" % (k, v * 1000) for k, v in sorted(
            offsets.items(), key=lambda kv: kv[1])))

    pieces = []

    log("-" * 60)
    log("breeches")
    # Down PAST the boot top, so the hem is tucked inside the boot rather
    # than ending in mid-air at the knee with bare shin below it.
    pieces.append(mhg.make_breeches(body=body, name="outfit",
                                    waist=hip + 0.04,
                                    knee=max(0.08, midcalf - 0.030),
                                    offset=offsets["breeches"]))

    log("-" * 60)
    log("boots")
    # To MIDCALF, where the breeches take over. Running to the knee left
    # nothing of the breeches visible at all.
    pieces.append(mhg.make_boots(body=body, name="outfit",
                                 top=midcalf,
                                 offset=offsets["boots"],
                                 thickness=0.010))

    log("-" * 60)
    log("waistcoat")
    pieces.append(mhg.make_waistcoat(body=body, name="outfit",
                                     top=min(0.96, collar - 0.03),
                                     bottom=hip + 0.02,
                                     offset=offsets["waistcoat"]))

    log("-" * 60)
    log("frock coat")
    pieces.append(mhg.make_coat(body=body, name="outfit", collar=collar,
                                skirt_to=hem, offset=offsets["coat"]))

    stem_tag = os.path.splitext(os.path.basename(source))[0].lower()

    log("-" * 60)

    # ONE MATERIAL PER GARMENT, OR THE JOIN EATS THEM.
    #
    # Every shell is duplicated from the body, so every one of them arrives
    # carrying the body's own material -- coat, waistcoat, breeches and boots
    # all on MI_Body_Baked_VT. Blender merges identical materials when
    # objects are joined, so the four collapse into ONE slot: the exported
    # Outfit FBX reads 3 materials, not 6, and nothing downstream can shade
    # a boot differently from a coat.
    #
    # Naming slot 0 per garment keeps them apart through the join. The trim
    # and facing slots are already distinct and are left alone.
    log("materials")
    for piece in pieces:
        if piece is None or not piece.data.materials:
            continue
        label = piece.name.split("_")[0]
        want = "MI_%s_%s" % (label, stem_tag)
        current = piece.data.materials[0]
        if current is not None and current.name == want:
            continue
        fresh = bpy.data.materials.get(want) or bpy.data.materials.new(want)
        piece.data.materials[0] = fresh
        log("   %-10s slot 0 -> %s" % (piece.name, want))

        # The facing slot carries the collar and the lapels. Named like the
        # rest so the seven regions read as a set, and so lift_slots -- which
        # matches on Trim/Facing/Lapel -- still finds it.
        for index, mat in enumerate(piece.data.materials):
            if mat is not None and mat.name == "Facing":
                lap = "MI_Lapels_%s" % stem_tag
                piece.data.materials[index] = (bpy.data.materials.get(lap)
                                               or bpy.data.materials.new(lap))
                log("   %-10s slot %d -> %s" % (piece.name, index, lap))

    boots = next((p for p in pieces if p is not None
                  and p.name.startswith("Boots")), None)
    foot_bone = (armature.data.bones.get("foot_l")
                 if armature is not None else None)

    # Round the forefoot off into a toe box. The boot was shrinkwrapped to
    # the foot, so it carries every toe as a ridge in its own leather. This
    # needs no body -- make_boots' clearance pass has already run -- so it
    # happens before the bind, and the trim below can then take the foot.
    if boots is not None and foot_bone is not None:
        toe_z = (armature.matrix_world @ foot_bone.head_local).z
        # The hull first, to close the gaps between the toes, then the toe
        # box to state the shape a hull cannot: a hull keeps every convex
        # bump, and a toe is one.
        mhg.toe_box(boots, upto_z=toe_z)
        mhg.round_toe(boots, body)
        # A light pass afterwards only to take the facet edges off the hull;
        # the shape itself is already right.
        mhg._smooth_region(boots, lambda p: p.z <= toe_z + 0.020,
                           rounds=6, factor=0.4, name="_ToeEase")

    # --- rig, WHILE THE BODY IS STILL WHOLE --------------------------------
    #
    # ORDER MATTERS HERE. bind() transfers weights from the nearest body
    # surface, and the trim below deletes the whole leg below the boot top.
    # Bind after that and the boot's lower half has no donor within reach: it
    # comes back unweighted, base_weights dumps all of it on 'pelvis', and the
    # boots stop following the feet entirely. So bind first, then trim.
    #
    # Safe in the other direction too -- the trim only touches the body, and
    # toe_box and _smooth_region move the boot's vertices without adding or
    # removing any, so the weights they carry survive both.
    log("-" * 60)
    log("rigging")
    for piece in pieces:
        if piece is None:
            continue
        mhg.bind(garment=piece, body=body)
        mhg.base_weights(dress=piece, bone="pelvis")

    # --- and only now, remove the foot from under the boot -----------------
    #
    # Nothing else worked on the instep: the per-vertex clearance gives every
    # toe its own bubble, smoothing merges them but leaves the instep bare,
    # and fitting a last let them through whatever the taper. Skin that does
    # not exist cannot poke through cloth.
    if boots is not None and armature is not None:
        # CUT THE LINE INSIDE THE SHAFT, NOT AT THE ANKLE.
        #
        # A face goes only when ALL of its verts are covered, so the faces
        # straddling keep_above always survive -- they have verts above the
        # line, which are never even tested. Put that line at the ankle and
        # the surviving ring lands on the instep, in plain view: measured, 56
        # body verts stood up to 31mm proud of the boot there. Put it well
        # down inside the shaft instead and the same ring is buried under
        # 40mm of boot.
        bmw = boots.matrix_world
        boot_top = max((bmw @ v.co).z for v in boots.data.vertices)
        ankle_z = boot_top - 0.040
        log("trimming the leg from inside the boots (below z %.3f, "
            "boot top %.3f)" % (ankle_z, boot_top))
        # Generous reach and no erosion. The boot stands 26mm off the leg and
        # is 10mm thick, so a covered vertex can be 40mm from its surface; at
        # 50mm with a ring eroded back, patches of instep survived and showed
        # through. Nothing here needs the safety ring -- the boot goes well
        # above the ankle, so there is no hem for a gap to open under.
        mhg.trim_body_beneath(body, [boots], reach=0.075,
                              keep_above=ankle_z, margin=0)

    if not os.path.isdir(OUT_DIR):
        os.makedirs(OUT_DIR)
    stem = os.path.splitext(os.path.basename(source))[0]
    out = os.path.join(OUT_DIR, "%s_outfit.blend" % stem.lower())
    bpy.ops.wm.save_as_mainfile(filepath=out)

    total = 0
    for piece in pieces:
        if piece is None:
            continue
        total += len(piece.data.vertices)
        log("%-12s %6d verts, %6d faces, %d material slot(s)"
            % (piece.name, len(piece.data.vertices),
               len(piece.data.polygons), len(piece.data.materials)))
    log("outfit total %d verts" % total)
    log("saved %s (%.1f MB)" % (out, os.path.getsize(out) / 1048576.0))


main()
