"""
fit_source_garment.py -- fit a bought garment mesh onto a Boggan body.

    blender -b --python Tools/fit_source_garment.py -- \
        --avatar SKM_MHC_Boggan_Male_Wilder_BodyMesh --measure

FIRST PASS ONLY. This measures rather than assumes. The downloaded frock coat
is modelled for a tall, slim figure and a Boggan is short and stout, so a
uniform scale will not do:

    frock coat   x 119.6   y  31.9   z 153.3   (cm, ankle to collar)
    Boggan male                       z 131.7  (overall, headless body)

Matching ankle-to-neck gives a scale near 0.81, which brings the arm span to
about 96 cm against the body's 94 -- close. But the depth goes to roughly
26 cm against a torso that is 41 cm front to back, so the coat would sit
inside the chest. That is what the numbers here are for: they say how much
non-uniform correction is needed before anything is bound or exported.

Reports, after fitting:
    the scale applied, the aligned bounds, and how many garment vertices end
    up INSIDE the body -- which is the number that matters, because those are
    the ones that will poke through.
"""

import os
import sys

import bpy


def log(msg):
    print("[fit] %s" % msg)


def opt(args, name, default=None):
    return args[args.index(name) + 1] if name in args else default


def clear_scene_imports(before):
    return [o for o in bpy.data.objects if o not in before]


def world_bounds(obj):
    mw = obj.matrix_world
    pts = [mw @ v.co for v in obj.data.vertices]
    xs = [p.x for p in pts]
    ys = [p.y for p in pts]
    zs = [p.z for p in pts]
    return (min(xs), max(xs)), (min(ys), max(ys)), (min(zs), max(zs))


def find_body():
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    named = [o for o in meshes if o.name.startswith("Avatar_")]
    pool = named or meshes
    return max(pool, key=lambda m: len(m.data.vertices)) if pool else None


def bone_z(body, name):
    arm = body.find_armature()
    if arm is None:
        return None
    bone = arm.data.bones.get(name)
    return (arm.matrix_world @ bone.head_local).z if bone else None


def inside_count(garment, body, sample=4000):
    """How many garment vertices sit inside the body surface.

    Uses closest_point_on_mesh and compares against the surface normal: a
    point on the inner side of the surface is penetrating. Sampled, because
    69k verts against a 32k body is slow and a proportion is all we need.
    """
    import mathutils

    bmw = body.matrix_world
    inv = bmw.inverted()
    gmw = garment.matrix_world
    verts = garment.data.vertices
    step = max(1, len(verts) // sample)
    inside = 0
    tested = 0
    for i in range(0, len(verts), step):
        world = gmw @ verts[i].co
        local = inv @ world
        ok, loc, nor, _ = body.closest_point_on_mesh(local)
        if not ok:
            continue
        tested += 1
        if (local - loc).dot(nor) < 0.0:
            inside += 1
    return inside, tested


def main():
    argv = sys.argv
    args = argv[argv.index("--") + 1:] if "--" in argv else []

    project = os.path.normpath(
        os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
    blend_dir = os.path.join(project, "Tools", "Blender")
    source = opt(args, "--source",
                 os.path.join(project, "Tools", "Export", "SourceGarments",
                              "Frock_coat.fbx"))
    avatar = opt(args, "--avatar", "SKM_MHC_Boggan_Male_Wilder_BodyMesh")
    # Widen the garment BEFORE conforming.
    #
    # Conforming can only push vertices out onto the body; it cannot create
    # volume the source does not have. The frock coat is cut for a tall slim
    # figure and is smaller than a Boggan almost everywhere, so conforming it
    # flattened 93% of its vertices onto the skin and the assembled result came
    # out with the body's own depth -- 40.4 cm against the body's 40.7 -- a
    # coat painted on rather than worn.
    #
    # The gown needed none of this: it was already 0.85 m deep against a 0.38 m
    # body, so 99% of it kept its original shape.
    #
    # Depth is widened much more than width: a stout figure needs room front to
    # back, while scaling x equally would fling the sleeves past the hands.
    inflate_x = float(opt(args, "--inflate-x", "1.0"))
    inflate_y = float(opt(args, "--inflate-y", "1.0"))

    blend = os.path.join(blend_dir, avatar + ".blend")
    if not os.path.isfile(blend):
        log("no prepped avatar at %s" % blend)
        return
    bpy.ops.wm.open_mainfile(filepath=blend)

    body = find_body()
    if body is None:
        log("no body mesh in %s" % blend)
        return
    (bx0, bx1), (by0, by1), (bz0, bz1) = world_bounds(body)
    log("body '%s': x %.3f..%.3f  y %.3f..%.3f  z %.3f..%.3f"
        % (body.name, bx0, bx1, by0, by1, bz0, bz1))
    ankle = bone_z(body, "foot_l")
    neck = bz1
    log("     ankle %.3f  neck(top) %.3f  span %.3f"
        % (ankle if ankle else -1, neck, (neck - ankle) if ankle else -1))

    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=source)
    new = [o for o in clear_scene_imports(before) if o.type == "MESH"]
    if not new:
        log("nothing imported from %s" % source)
        return
    garment = max(new, key=lambda m: len(m.data.vertices))
    garment.name = "SourceGarment"
    log("imported '%s': %d verts, %d material slot(s)"
        % (garment.name, len(garment.data.vertices), len(garment.material_slots)))

    (gx0, gx1), (gy0, gy1), (gz0, gz1) = world_bounds(garment)
    log("garment raw: x %.3f..%.3f  y %.3f..%.3f  z %.3f..%.3f"
        % (gx0, gx1, gy0, gy1, gz0, gz1))
    log("     size x %.3f  y %.3f  z %.3f" % (gx1 - gx0, gy1 - gy0, gz1 - gz0))

    # Uniform scale on height first: the garment spans ankle to collar, so
    # match it to the body's ankle-to-neck rather than to overall height.
    target = (neck - ankle) if ankle else (bz1 - bz0)
    scale = target / (gz1 - gz0) if (gz1 - gz0) else 1.0
    log("uniform scale to match ankle-to-neck: %.4f" % scale)

    # MULTIPLY the existing scale, do not replace it. The FBX arrives with a
    # 0.01 object scale converting Unreal centimetres to metres; assigning
    # (scale, scale, scale) throws that away and bakes centimetre-sized mesh
    # data into the scene, giving a garment 100x too big -- which then reports
    # 0% penetration because it is nowhere near the body at all.
    garment.scale = tuple(component * scale for component in garment.scale)
    bpy.context.view_layer.update()
    bpy.ops.object.select_all(action="DESELECT")
    garment.select_set(True)
    bpy.context.view_layer.objects.active = garment
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)

    if inflate_x != 1.0 or inflate_y != 1.0:
        centre_x = (gx0 + gx1) / 2.0
        centre_y = (gy0 + gy1) / 2.0
        inv = garment.matrix_world.inverted()
        mw = garment.matrix_world
        for vert in garment.data.vertices:
            world = mw @ vert.co
            world.x = centre_x + (world.x - centre_x) * inflate_x
            world.y = centre_y + (world.y - centre_y) * inflate_y
            vert.co = inv @ world
        garment.data.update()
        (gx0, gx1), (gy0, gy1), (gz0, gz1) = world_bounds(garment)
        log("inflated x%.2f y%.2f -> size x %.3f y %.3f"
            % (inflate_x, inflate_y, gx1 - gx0, gy1 - gy0))

    # Align: centre on the body in x/y, sit the garment's base at the ankle.
    (gx0, gx1), (gy0, gy1), (gz0, gz1) = world_bounds(garment)
    bcx, bcy = (bx0 + bx1) / 2.0, (by0 + by1) / 2.0
    gcx, gcy = (gx0 + gx1) / 2.0, (gy0 + gy1) / 2.0
    garment.location.x += (bcx - gcx)
    garment.location.y += (bcy - gcy)
    garment.location.z += ((ankle if ankle else bz0) - gz0)
    bpy.context.view_layer.update()

    (gx0, gx1), (gy0, gy1), (gz0, gz1) = world_bounds(garment)
    log("garment fitted: x %.3f..%.3f  y %.3f..%.3f  z %.3f..%.3f"
        % (gx0, gx1, gy0, gy1, gz0, gz1))
    log("     size x %.3f  y %.3f  z %.3f" % (gx1 - gx0, gy1 - gy0, gz1 - gz0))
    log("     body size x %.3f  y %.3f  z %.3f"
        % (bx1 - bx0, by1 - by0, bz1 - bz0))

    inside, tested = inside_count(garment, body)
    pct = (100.0 * inside / tested) if tested else 0.0
    log("")
    log("PENETRATION: %d of %d sampled vertices are INSIDE the body (%.1f%%)"
        % (inside, tested, pct))
    log("")
    if pct > 25.0:
        log("Too much to bind as-is. The garment is cut for a slimmer figure;")
        log("it needs widening across x/y before it will sit outside a Boggan.")
    elif pct > 5.0:
        log("Mostly outside. A modest outward offset or per-axis widening")
        log("should clear the rest.")
    else:
        log("Sits outside the body. Ready to bind.")

    out = os.path.join(blend_dir, "Work", "%s_frockfit.blend" % avatar)
    if not os.path.isdir(os.path.dirname(out)):
        os.makedirs(os.path.dirname(out))
    bpy.ops.wm.save_as_mainfile(filepath=out)
    log("saved %s" % out)


main()
