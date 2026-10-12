"""Join a generated coat and breeches into one garment and export it.

    blender -b --python Tools/export_suit_joined.py -- \
        --suit boggan_childling_male_suit --name Suit_boggan_childling_male

AChangelingNPC carries ONE Outfit slot, and make_court_suit produces two
objects. Rather than add a second component -- which the level's actors would
each need individually, and which costs a second draw call for a garment that
is always worn as a set -- the two are joined into a single skeletal mesh.

Both are already bound to the same armature with the same vertex groups, so
joining is safe: the weights come across unchanged.
"""

import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.append(HERE)


def log(msg):
    print("[suit] %s" % msg)


def opt(args, name, default=None):
    return args[args.index(name) + 1] if name in args else default


def main():
    argv = sys.argv
    args = argv[argv.index("--") + 1:] if "--" in argv else []
    suit = opt(args, "--suit", "boggan_childling_male_suit")
    name = opt(args, "--name", "Suit_boggan_childling_male")

    project = os.path.normpath(os.path.join(HERE, ".."))
    work = os.path.join(project, "Tools", "Blender", "Work")
    out_dir = os.path.join(project, "Tools", "Export", "Garments")
    path = os.path.join(work, "%s.blend" % suit)
    if not os.path.isfile(path):
        log("no suit blend at %s" % path)
        return
    bpy.ops.wm.open_mainfile(filepath=path)

    coat = armature = None
    pieces = []
    for obj in bpy.context.scene.objects:
        if obj.type == "ARMATURE":
            armature = obj
        elif obj.type == "MESH":
            if obj.name.startswith("Coat"):
                coat = obj
            elif obj.name.startswith(("Breeches", "Waistcoat", "Boots")):
                pieces.append(obj)
    if coat is None:
        log("no Coat in %s" % suit)
        return

    if pieces:
        # Names first: join FREES the source objects, and reading .name off
        # one afterwards raises "StructRNA of type Object has been removed".
        merged = ", ".join(piece.name.lower() for piece in pieces)
        bpy.ops.object.select_all(action="DESELECT")
        for piece in pieces:
            piece.select_set(True)
        coat.select_set(True)
        bpy.context.view_layer.objects.active = coat
        bpy.ops.object.join()
        log("joined %s into coat" % merged)
    joined = bpy.context.view_layer.objects.active or coat
    joined.name = name
    log("%s: %d verts, %d faces, %d groups, %d material slot(s)"
        % (name, len(joined.data.vertices), len(joined.data.polygons),
           len(joined.vertex_groups), len(joined.data.materials)))

    # A garment built FROM the body still penetrates it in places: the skirt
    # is extruded downward past the hips, and solidify pushes an inner
    # surface back through the thigh. Those show as patches of bare body
    # through the coat. A light pass along the body's own surface normals
    # clears them without touching anything already hanging free.
    import conform_source_garment as fit
    import blender_metahuman_garment as mhg
    body = None
    for obj in bpy.context.scene.objects:
        if obj.type == "MESH" and obj.name.startswith("Avatar_"):
            body = obj
    if body is not None:
        inside, tested = fit.penetration(joined, body)
        log("penetration before: %d of %d (%.1f%%)"
            % (inside, tested, 100.0 * inside / tested if tested else 0))
        for _ in range(2):
            fit.conform(joined, body, offset=0.020)
        inside, tested = fit.penetration(joined, body)
        log("penetration after : %d of %d (%.1f%%)"
            % (inside, tested, 100.0 * inside / tested if tested else 0))

    # Delete the body under the cloth. This is the definitive answer to
    # poke-through and it is free at runtime: geometry that does not exist
    # cannot show. It is also the only thing that works here, because the
    # garment's bind pose and the skeleton's reference pose disagree about
    # the arm -- clearance was raised twice with no change at all, so the
    # sleeve cannot be fitted around the problem.
    #
    # reach has to clear the drape: the coat now sits 16mm off the body
    # before solidify, so a body vertex under cloth can be 30mm from it.
    trimmed = None
    if body is not None:
        try:
            trimmed = mhg.trim_body_under(dress=joined, body=body,
                                          reach=0.035, margin=1,
                                          out_name="Body_%s" % name)
        except Exception as exc:
            log("body trim failed: %s" % exc)

    if not os.path.isdir(out_dir):
        os.makedirs(out_dir)
    out = os.path.join(out_dir, "%s.fbx" % name)

    mhg.export_garment(out, garment=joined, armature=armature)
    log("exported %s (%.1f MB)" % (out, os.path.getsize(out) / 1048576.0))

    if trimmed is not None:
        body_out = os.path.join(out_dir, "Body_%s.fbx" % name)
        mhg.export_garment(body_out, garment=trimmed, armature=armature)
        log("exported trimmed body %s (%.1f MB)"
            % (body_out, os.path.getsize(body_out) / 1048576.0))


main()
