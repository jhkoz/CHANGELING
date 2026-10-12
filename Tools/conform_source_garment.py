"""
conform_source_garment.py -- push a bought garment out onto a Boggan's shape.

    blender -b --python Tools/conform_source_garment.py -- \
        --avatar SKM_MHC_Boggan_Male_Wilder_BodyMesh

WHY NOT SCALE

The frock coat is cut for a tall slim figure. Height-matched onto a Boggan,
93.6% of its vertices end up inside the body, and the correction needed varies
by height rather than being one factor:

    legs    z 0.08-0.32   x1.50 / y1.40
    thigh   z 0.32-0.69   x1.03 / y1.07
    torso   z 0.82-1.19   x1.18 / y1.91
    chest   z 1.19-1.31   x1.71 / y2.07

Any single non-uniform scale that clears the chest would balloon the legs, and
one that fits the legs leaves the belly through the waistcoat. Scaling also
distorts a tailored mesh: buttons go oval, the button stand skews.

WHAT THIS DOES INSTEAD

Only the vertices that are actually INSIDE the body are moved, and each is
pushed along the body's surface normal to sit `offset` clear of the skin.
Vertices already outside are left exactly where the modeller put them, which
is what keeps the coat's tails and skirt hanging free rather than being
shrink-wrapped onto the legs -- the failure mode of a plain Shrinkwrap
modifier, which moves every vertex to its nearest surface point.

A smoothing pass follows, because pushing individual vertices out leaves a
faceted boundary where moved and unmoved vertices meet.
"""

import math
import os
import sys

import bpy
from mathutils import Vector, kdtree


def log(msg):
    print("[conform] %s" % msg)


def opt(args, name, default=None):
    return args[args.index(name) + 1] if name in args else default


def find(name_prefix=None, exact=None):
    for obj in bpy.context.scene.objects:
        if obj.type != "MESH":
            continue
        if exact and obj.name == exact:
            return obj
        if name_prefix and obj.name.startswith(name_prefix):
            return obj
    return None


# Half a millimetre. Below this a garment vertex is not inside the body, it
# is ON it -- and the inside/outside test cannot tell which side of a
# coincident surface a point is on, because the dot product is zero and the
# sign is whatever the floating point rounds to.
#
# Measured: the body tested against ITSELF reported 48.3% "inside". Every
# garment cut from the body by make_dress_shell starts coincident with it,
# so readings of 40-60% were this artefact, not penetration -- and chasing
# them sent two fixes after a fault that was not there. Coincident geometry
# is still a real problem, it just z-fights rather than pokes through, so it
# is counted and reported separately.
TOUCHING = 0.0005


def penetration(garment, body, sample=4000):
    """Returns (inside, tested, touching)."""
    gmw = garment.matrix_world
    inv = body.matrix_world.inverted()
    verts = garment.data.vertices
    step = max(1, len(verts) // sample)
    inside = tested = touching = 0
    for i in range(0, len(verts), step):
        local = inv @ (gmw @ verts[i].co)
        ok, loc, nor, _ = body.closest_point_on_mesh(local)
        if not ok:
            continue
        tested += 1
        gap = (local - loc).dot(nor)
        if gap < -TOUCHING:
            inside += 1
        elif gap < TOUCHING:
            touching += 1
    return inside, tested, touching


def body_axis(body, samples=40):
    """Centre line of the body, as (z, x, y) per height band.

    Radial pushing needs an axis to push away FROM. The body's bounding-box
    centre is not it: hips and chest sit at different x/y, and using one centre
    for the whole figure skews the garment.
    """
    mw = body.matrix_world
    pts = [mw @ v.co for v in body.data.vertices]
    zs = [p.z for p in pts]
    lo, hi = min(zs), max(zs)
    bands = []
    for i in range(samples):
        a = lo + (hi - lo) * i / samples
        b = lo + (hi - lo) * (i + 1) / samples
        band = [p for p in pts if a <= p.z < b]
        if not band:
            continue
        cx = sum(p.x for p in band) / len(band)
        cy = sum(p.y for p in band) / len(band)
        bands.append(((a + b) / 2.0, cx, cy))
    return bands


def axis_at(bands, z):
    """Interpolate the centre line at a height."""
    if not bands:
        return 0.0, 0.0
    if z <= bands[0][0]:
        return bands[0][1], bands[0][2]
    if z >= bands[-1][0]:
        return bands[-1][1], bands[-1][2]
    for i in range(len(bands) - 1):
        z0, x0, y0 = bands[i]
        z1, x1, y1 = bands[i + 1]
        if z0 <= z <= z1:
            t = (z - z0) / (z1 - z0) if z1 > z0 else 0.0
            return x0 + (x1 - x0) * t, y0 + (y1 - y0) * t
    return bands[-1][1], bands[-1][2]


def radius_map(body, bands, nz=56, ntheta=32, skip=None):
    """The body as a cylindrical height map: max radius per (height, bearing).

    A single scale cannot fit this coat -- the header records what each region
    would need, chest x1.71/y2.07 against legs x1.50/y1.40 -- and a per-vertex
    push cannot either, because it only moves vertices that are already inside
    and leaves their neighbours behind, which is what tore the skirt into
    shards. What is needed is the body's actual profile, so the garment can be
    widened smoothly to clear it everywhere.
    """
    mw = body.matrix_world
    every = [mw @ v.co for v in body.data.vertices]
    zs = [p.z for p in every]
    lo, hi = min(zs), max(zs)
    span = max(hi - lo, 1e-6)

    # Measure the BODY, not the arms. In A-pose the hands hang beside the
    # hips, so including them makes the figure read as 0.847 wide at the hip
    # against a true 0.33 -- and because the field is blurred down the
    # height, the coat SKIRT then opens to clear arms that are not there. It
    # came out 0.39 deep at the knee over a body 0.12 deep: a tent.
    pts = [p for p, limb in zip(every, skip) if not limb] if skip else every

    grid = [[0.0] * ntheta for _ in range(nz)]
    for p in pts:
        cx, cy = axis_at(bands, p.z)
        dx, dy = p.x - cx, p.y - cy
        iz = min(nz - 1, max(0, int((p.z - lo) / span * nz)))
        it = int((math.atan2(dy, dx) + math.pi) / (2.0 * math.pi) * ntheta) % ntheta
        r = math.hypot(dx, dy)
        if r > grid[iz][it]:
            grid[iz][it] = r
    # Fill cells no vertex landed in, from their neighbours, so a gap in the
    # sampling does not read as "the body has no width here".
    for iz in range(nz):
        for it in range(ntheta):
            if grid[iz][it] > 0.0:
                continue
            near = [grid[jz][jt]
                    for jz in range(max(0, iz - 2), min(nz, iz + 3))
                    for jt in ((it - 1) % ntheta, it, (it + 1) % ntheta)
                    if grid[jz][jt] > 0.0]
            if near:
                grid[iz][it] = sum(near) / len(near)
    return grid, lo, span


def sample_grid(grid, lo, span, z, theta):
    nz, ntheta = len(grid), len(grid[0])
    fz = (z - lo) / span * nz - 0.5
    iz0 = max(0, min(nz - 1, int(math.floor(fz))))
    iz1 = max(0, min(nz - 1, iz0 + 1))
    tz = max(0.0, min(1.0, fz - iz0))
    ft = (theta + math.pi) / (2.0 * math.pi) * ntheta - 0.5
    it0 = int(math.floor(ft)) % ntheta
    it1 = (it0 + 1) % ntheta
    tt = ft - math.floor(ft)
    a = grid[iz0][it0] * (1 - tt) + grid[iz0][it1] * tt
    b = grid[iz1][it0] * (1 - tt) + grid[iz1][it1] * tt
    return a * (1 - tz) + b * tz


def blur_field(field, rounds=4):
    """Smooth a (height x bearing) field, wrapping around the bearing."""
    nz, ntheta = len(field), len(field[0])
    for _ in range(rounds):
        nxt = [row[:] for row in field]
        for iz in range(nz):
            for it in range(ntheta):
                acc = [field[iz][(it - 1) % ntheta], field[iz][it],
                       field[iz][(it + 1) % ntheta]]
                if iz > 0:
                    acc.append(field[iz - 1][it])
                if iz < nz - 1:
                    acc.append(field[iz + 1][it])
                nxt[iz][it] = sum(acc) / len(acc)
        field = nxt
    return field


# Deliberately NOT clavicle. A clavicle owns the shoulder and the top of the
# chest, which is torso as far as a coat is concerned; excluding it left that
# band unwidened and the body speckled through the coat front.
LIMB_BONES = ("upperarm", "lowerarm", "hand", "finger", "thumb",
              "index", "middle", "pinky", "wrist")


def limb_lookup(body):
    """Tell torso from arm by BONE, not by width.

    Measuring a torso's half-width works on a T-posed figure and fails on this
    one: a MetaHuman stands in A-pose, so at hip height the widest vertices
    are the hands hanging beside the hips, and "torso half-width" came back as
    0.438 -- the entire body. Every arm vertex then counted as torso.

    The body is skinned, so each vertex already says which bone owns it. That
    answers the question directly and at any pose.
    """
    names = {i: g.name.lower() for i, g in enumerate(body.vertex_groups)}
    flags = []
    for vert in body.data.vertices:
        best, weight = None, -1.0
        for group in vert.groups:
            if group.weight > weight:
                weight, best = group.weight, group.group
        flags.append(any(k in names.get(best, "") for k in LIMB_BONES))

    tree = kdtree.KDTree(len(body.data.vertices))
    mw = body.matrix_world
    for i, vert in enumerate(body.data.vertices):
        tree.insert(mw @ vert.co, i)
    tree.balance()
    return tree, flags


def is_limb(lookup, world):
    tree, flags = lookup
    _, index, _ = tree.find(world)
    return bool(flags[index]) if index is not None else False


def expand_to_clear(garment, body, bands, base=0.022, hem=0.045, percentile=0.90):
    """Widen the GARMENT until it clears the body, before repairing anything.

    This is the step that was missing. conform_radial only moves vertices that
    are already inside the body, so on a figure the coat is simply too small
    for, it shoves them out one at a time and leaves their neighbours behind --
    a torn skirt -- and it pushes them to the body surface plus a hair, which
    is exactly where the breeches already are, so the two z-fight.

    Here the whole garment is scaled outward about the body's centre line by a
    SMOOTH field over height and bearing, so a ring of the coat opens up as a
    ring. Shape survives: every vertex keeps its own bearing and its height, so
    buttons stay on the centre front and the tails stay behind.

    Clearance grows toward the hem, because a coat skirt should hang away from
    the legs rather than grip them, and that gap is also what keeps it clear of
    the breeches underneath.
    """
    lookup = limb_lookup(body)
    grid, lo, span = radius_map(body, bands, skip=lookup[1])
    nz, ntheta = len(grid), len(grid[0])

    # A cylinder around the spine describes a torso well and an outstretched
    # arm not at all: a sleeve sits far from the centre line, so a radial push
    # sized for the chest sends it out sideways and the coat grows sleeves
    # past the fingertips. Widening is confined to the torso and faded out
    # across the shoulder, leaving the sleeves to the per-vertex conform.
    gmw = garment.matrix_world
    ginv = gmw.inverted()
    world = [gmw @ v.co for v in garment.data.vertices]
    gz = [w.z for w in world]
    g_lo = min(gz)
    hip = lo + span * 0.52

    # Fade the widening out at the top. The push is horizontal, outward from
    # the spine, so applying it at the collar does not make the coat roomier
    # -- it pulls the NECK OPENING wider and bares the shoulders. A collar
    # has to stay snug, so clearance ramps to nothing across the shoulders.
    shoulder = lo + span * 0.86
    top = lo + span * 0.97

    def clearance(z):
        if z >= top:
            return 0.0
        if z >= shoulder:
            return base * (top - z) / max(top - shoulder, 1e-6)
        if z >= hip:
            return base
        t = (hip - z) / max(hip - g_lo, 1e-6)
        return base + (hem - base) * max(0.0, min(1.0, t))

    # Open each vertex to ITS OWN bearing's requirement, and leave any
    # vertex that is already clear exactly where the modeller put it.
    #
    # An earlier version pooled the requirement into a (height x bearing)
    # field and blurred it, for smoothness. That is the wrong instrument for
    # a skirt. At knee height the body is two legs, so the radius pointing
    # SIDEWAYS is the outer edge of a leg (0.179) while the radius pointing
    # FORWARD is barely 0.06 -- and skirt vertices hanging near the centre
    # line between the legs ask for a 0.16 push. Blurred across the ring,
    # that push lands on the front and back as well, and an elliptical skirt
    # 0.18 deep came out a circular cone 0.39 deep over a body 0.12 deep.
    # A tent, and exactly what the character looked like in the level.
    opened = 0
    widest = 0.0
    for vert, w in zip(garment.data.vertices, world):
        if is_limb(lookup, w):
            continue
        cx, cy = axis_at(bands, w.z)
        dx, dy = w.x - cx, w.y - cy
        r = math.hypot(dx, dy)
        if r < 1e-5:
            continue
        theta = math.atan2(dy, dx)
        need = sample_grid(grid, lo, span, w.z, theta) + clearance(w.z)
        if r >= need:
            continue
        grow = need / r
        widest = max(widest, need - r)
        vert.co = ginv @ Vector((cx + dx * grow, cy + dy * grow, w.z))
        opened += 1
    garment.data.update()
    log("   opened %d vertices, largest push %.3f" % (opened, widest))
    return widest


def arm_segments(armature):
    """The arm bones as world-space segments, upper arm outward."""
    if armature is None:
        return []
    mw = armature.matrix_world
    out = []
    for bone in armature.data.bones:
        name = bone.name.lower()
        if any(k in name for k in ("upperarm", "lowerarm", "hand")):
            out.append((name, mw @ bone.head_local, mw @ bone.tail_local))
    return out


def on_segment(point, head, tail):
    """Where a point sits on a bone: (t along it, distance out, direction out)."""
    axis = tail - head
    length = axis.length
    if length < 1e-6:
        return 0.0, 0.0, Vector((0.0, 0.0, 1.0))
    unit = axis / length
    t = max(0.0, min(1.0, (point - head).dot(unit) / length))
    foot = head + unit * (t * length)
    radial = point - foot
    distance = radial.length
    if distance < 1e-6:
        return t, 0.0, Vector((0.0, 0.0, 1.0))
    return t, distance, radial / distance


def garment_limb_flags(garment):
    """Which garment vertices are sleeve, from the garment's OWN weights.

    Nearest-body-vertex works for an unbound garment and misleads here: in
    A-pose the forearm hangs beside the hip, so a forearm sleeve vertex's
    nearest body vertex is the HIP and it gets classed as torso and skipped --
    which left the coat covering the shoulders and the forearms still bare.

    A garment that has already been bound knows better: the sleeve vertices
    are the ones an arm bone owns.
    """
    if not garment.vertex_groups:
        return None

    # Skip groups that are not bones. make_skirt leaves a '_BodiceOnly'
    # marker group weighted 1.0 across 12,626 of this coat's vertices --
    # harmless in Unreal, which ignores any group without a matching bone,
    # but it wins every dominant-weight test and made the whole coat look
    # like it belonged to no bone at all.
    names = {i: g.name.lower() for i, g in enumerate(garment.vertex_groups)
             if not g.name.startswith("_")}
    flags = []
    for vert in garment.data.vertices:
        best, weight = None, -1.0
        for group in vert.groups:
            if group.group in names and group.weight > weight:
                weight, best = group.weight, group.group
        flags.append(any(k in names.get(best, "") for k in LIMB_BONES))
    return flags


def expand_sleeves(garment, body, armature, lookup, clearance=0.018, bins=14):
    """Open the sleeves around each ARM's own axis.

    The torso widening deliberately skips limb vertices, because a cylinder
    around the spine describes an arm not at all. But the sleeves still have
    to fit: this coat is cut for a slim figure, and left alone the Boggan's
    arms simply come out through them.

    So the same idea is applied per bone -- the arm's radius is measured along
    its own length, and the sleeve is opened to clear it. Every sleeve vertex
    is moved, not only the penetrating ones, so the sleeve opens as a tube
    instead of growing lumps where single vertices were pushed out.
    """
    segments = arm_segments(armature)
    if not segments:
        log("   no arm bones found; sleeves left alone")
        return 0

    names = {i: g.name.lower() for i, g in enumerate(body.vertex_groups)}
    bmw = body.matrix_world
    profile = {name: [0.0] * bins for name, _, _ in segments}

    for vert in body.data.vertices:
        best, weight = None, -1.0
        for group in vert.groups:
            if group.weight > weight:
                weight, best = group.weight, group.group
        owner = names.get(best, "")
        point = bmw @ vert.co
        for name, head, tail in segments:
            if name != owner:
                continue
            t, distance, _ = on_segment(point, head, tail)
            slot = min(bins - 1, int(t * bins))
            if distance > profile[name][slot]:
                profile[name][slot] = distance

    # Fill any empty bin from its neighbours, then soften the profile so the
    # sleeve does not step where the sampling did.
    for name in profile:
        row = profile[name]
        for i in range(bins):
            if row[i] > 0.0:
                continue
            near = [row[j] for j in range(max(0, i - 2), min(bins, i + 3))
                    if row[j] > 0.0]
            row[i] = sum(near) / len(near) if near else 0.0
        for _ in range(2):
            row[:] = [sum(row[max(0, i - 1):min(bins, i + 2)])
                      / len(row[max(0, i - 1):min(bins, i + 2)])
                      for i in range(bins)]

    gmw = garment.matrix_world
    ginv = gmw.inverted()
    own = garment_limb_flags(garment)
    moved = 0
    for index, vert in enumerate(garment.data.vertices):
        world = gmw @ vert.co
        sleeve = own[index] if own is not None else is_limb(lookup, world)
        if not sleeve:
            continue
        best = None
        for name, head, tail in segments:
            t, distance, direction = on_segment(world, head, tail)
            if best is None or distance < best[1]:
                best = (name, distance, direction, t, head, tail)
        if best is None:
            continue
        name, distance, direction, t, head, tail = best
        row = profile[name]
        fb = t * bins - 0.5
        i0 = max(0, min(bins - 1, int(math.floor(fb))))
        i1 = max(0, min(bins - 1, i0 + 1))
        f = max(0.0, min(1.0, fb - i0))
        want = row[i0] * (1 - f) + row[i1] * f + clearance
        if distance >= want or distance < 1e-6:
            continue
        axis = tail - head
        unit = axis.normalized() if axis.length > 1e-6 else Vector((0, 0, 1))
        foot = head + unit * (t * axis.length)
        vert.co = ginv @ (foot + direction * want)
        moved += 1
    garment.data.update()
    return moved


def conform_radial(garment, body, offset=0.015):
    """Push penetrating vertices OUTWARD from the body's centre line.

    The nearest-surface conform moves each vertex to the closest point on the
    skin, which drags it sideways as well as outward. On a garment that is
    mostly inside the body -- 93% of the frock coat -- that collapses the whole
    thing onto the figure and it renders as a decal rather than a coat.

    Pushing radially keeps each vertex on its own bearing from the body's
    axis, so the garment's angular layout survives: a button stays on the
    centre front, a tail stays behind, a sleeve stays out at the arm. Only the
    distance from the axis changes.

    The outer surface is found by casting INWARD from well outside the figure,
    so the first hit is the skin the garment has to clear rather than some
    interior fold.
    """
    from mathutils import Vector, kdtree

    bands = body_axis(body)
    gmw = garment.matrix_world
    ginv = gmw.inverted()
    binv = body.matrix_world.inverted()
    bmw = body.matrix_world

    # Radial pushing is right for a torso and wrong for a limb. A vertex
    # inside the forearm has a bearing from the SPINE that runs straight down
    # the arm, so pushing it out along that bearing sends it to the fingertip.
    # Out past the torso, each vertex is pushed along the body's own surface
    # normal instead, which is locally correct everywhere.
    lookup = limb_lookup(body)

    moved = 0
    for vert in garment.data.vertices:
        world = gmw @ vert.co

        # Only move a vertex that is genuinely INSIDE the body. Comparing
        # radius along a bearing is not an inside test: along an outstretched
        # arm the "surface radius" is the FINGERTIP, so every sleeve vertex
        # nearer than that was being flung out to the fingertip and the coat
        # grew sleeves past the hands.
        local = binv @ world
        hit, loc, nor, _ = body.closest_point_on_mesh(local)
        if not hit or (local - loc).dot(nor) >= 0.0:
            continue

        cx, cy = axis_at(bands, world.z)

        if is_limb(lookup, world):
            vert.co = ginv @ (bmw @ (loc + nor * offset))
            moved += 1
            continue

        direction = Vector((world.x - cx, world.y - cy, 0.0))
        if direction.length < 1e-6:
            continue
        direction.normalize()

        far = Vector((cx, cy, world.z)) + direction * 5.0
        ok, loc, nor, _ = body.ray_cast(binv @ far,
                                        binv.to_3x3() @ (-direction))
        if not ok:
            continue
        surface = bmw @ loc
        want = (Vector((surface.x - cx, surface.y - cy, 0.0)).length) + offset
        have = Vector((world.x - cx, world.y - cy, 0.0)).length
        if have >= want:
            continue                       # already clear; leave it alone
        target = Vector((cx, cy, world.z)) + direction * want
        vert.co = ginv @ target
        moved += 1

    garment.data.update()
    return moved


def conform(garment, body, offset=0.012):
    """Move only the penetrating vertices out to the body surface + offset."""
    gmw = garment.matrix_world
    ginv = gmw.inverted()
    binv = body.matrix_world.inverted()
    bmw = body.matrix_world

    moved = 0
    for vert in garment.data.vertices:
        world = gmw @ vert.co
        local = binv @ world
        ok, loc, nor, _ = body.closest_point_on_mesh(local)
        if not ok:
            continue
        if (local - loc).dot(nor) >= 0.0:
            continue                      # already outside; leave it alone
        target_local = loc + nor * offset
        vert.co = ginv @ (bmw @ target_local)
        moved += 1
    garment.data.update()
    return moved


def smooth(garment, iterations=3, factor=0.5):
    """Relax the boundary between moved and unmoved vertices."""
    mod = garment.modifiers.new("ConformSmooth", "SMOOTH")
    mod.iterations = iterations
    mod.factor = factor
    bpy.ops.object.select_all(action="DESELECT")
    garment.select_set(True)
    bpy.context.view_layer.objects.active = garment
    bpy.ops.object.modifier_apply(modifier=mod.name)


def main():
    argv = sys.argv
    args = argv[argv.index("--") + 1:] if "--" in argv else []

    project = os.path.normpath(
        os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
    work = os.path.join(project, "Tools", "Blender", "Work")
    avatar = opt(args, "--avatar", "SKM_MHC_Boggan_Male_Wilder_BodyMesh")
    # 30mm of clearance, opening to 55mm at the hem. At 22mm the body still
    # speckled through the chest; at 30mm penetration reaches zero and the
    # shirt and cravat read cleanly. The hem is wider because a coat skirt
    # should hang away from the legs -- that gap is also what keeps it from
    # z-fighting the breeches underneath.
    offset = float(opt(args, "--offset", "0.030"))
    # No extra flare at the hem by default. 55mm read fine as a number and
    # fanned this coat's front skirt panels outward like wings -- it is a
    # frock coat, so the skirt is already split at the front, and pushing it
    # radially splays the opening. The conform keeps it off the legs.
    hem = float(opt(args, "--hem", "0.030"))

    blend = os.path.join(work, "%s_frockfit.blend" % avatar)
    if not os.path.isfile(blend):
        log("no fitted blend at %s -- run fit_source_garment first" % blend)
        return
    bpy.ops.wm.open_mainfile(filepath=blend)

    garment = find(exact="SourceGarment")
    body = find(name_prefix="Avatar_")
    if garment is None or body is None:
        log("missing SourceGarment or Avatar_ body")
        return

    before, tested, _touch = penetration(garment, body)
    log("before: %d of %d sampled inside (%.1f%%)"
        % (before, tested, 100.0 * before / tested if tested else 0))

    # Widen the garment FIRST. Repairing vertex by vertex cannot invent fabric
    # that is not there, and this coat is genuinely too small for a Boggan --
    # the header records chest x1.71/y2.07. Opening the whole garment up
    # smoothly leaves the conform below with only stragglers to tidy.
    bands = body_axis(body)
    widest = expand_to_clear(garment, body, bands, base=offset, hem=hem)
    opened, tested, _touch = penetration(garment, body)
    armature = None
    for obj in bpy.context.scene.objects:
        if obj.type == "ARMATURE":
            armature = obj
            break
    sleeved = expand_sleeves(garment, body, armature, limb_lookup(body),
                             clearance=offset * 0.8)
    log("   sleeves opened: %d vertices" % sleeved)

    _p = [garment.matrix_world @ v.co for v in garment.data.vertices]
    log("   after widening: x %.3f..%.3f  y %.3f..%.3f"
        % (min(q.x for q in _p), max(q.x for q in _p),
           min(q.y for q in _p), max(q.y for q in _p)))
    log("widened: up to x%.2f, now %.1f%% inside (clearance %.0fmm, hem %.0fmm)"
        % (widest, 100.0 * opened / tested if tested else 0,
           offset * 1000, hem * 1000))

    # Iterate, and END on a conform. Smoothing relaxes the seam between moved
    # and unmoved vertices, but it averages toward neighbours and so drags some
    # of them back inside the body -- a single conform+smooth left 14.1%
    # penetrating. Re-pushing after each smooth converges instead.
    for round_index in range(3):
        moved = conform_radial(garment, body, offset=offset)
        inside, tested, _touch = penetration(garment, body)
        log("round %d: moved %d, now %.1f%% inside"
            % (round_index + 1, moved,
               100.0 * inside / tested if tested else 0))
        if inside == 0:
            break
        if round_index < 2:
            smooth(garment, iterations=2, factor=0.35)

    after, tested = penetration(garment, body)
    pct = 100.0 * after / tested if tested else 0
    log("after : %d of %d sampled inside (%.1f%%)" % (after, tested, pct))

    mw = garment.matrix_world
    pts = [mw @ v.co for v in garment.data.vertices]
    xs = [p.x for p in pts]
    ys = [p.y for p in pts]
    zs = [p.z for p in pts]
    log("garment now: x %.3f..%.3f  y %.3f..%.3f  z %.3f..%.3f"
        % (min(xs), max(xs), min(ys), max(ys), min(zs), max(zs)))

    out = os.path.join(work, "%s_frock.blend" % avatar)
    bpy.ops.wm.save_as_mainfile(filepath=out)
    log("saved %s" % out)
    log("")
    if pct > 5.0:
        log("Still penetrating. Raise --offset, or the body has folds the")
        log("nearest-surface test cannot resolve.")
    else:
        log("Clear of the body. Next: bind() for weights, then export.")


# Only when run as a script. Guarded on __name__, not on "--" in sys.argv:
# an importing script is itself invoked with "--", so that test was true on
# import and this module quietly ran a full conform on the DEFAULT avatar,
# replacing the caller's scene out from under it.
if __name__ == "__main__":
    main()
