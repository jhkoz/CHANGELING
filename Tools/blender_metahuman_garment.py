"""
blender_metahuman_garment.py

Blender half of the CHANGELING MetaHuman clothing pipeline. Takes the bind-pose
avatars written by Tools/export_metahuman_bodies.py, sets them up for garment
work, transfers skin weights onto a finished garment, and exports it back out
for Unreal.

The roster does NOT share a single skeleton. As of 2026-09-21, of the 34 bodies
under /Game/_Custom/Characters/Kith:
  30 bind to /MetaHumanCharacter/Female/Medium/NormalWeight/Body/metahuman_base_skel
     (the MetaHumanCharacter plugin) -- the current standard
   3 legacy Boggans still use the project-local /Game/MetaHumans copy
   1 (Boggan_Female_Grump) points at SK_Mannequin, which is a mislabelled
     reference: the mesh itself is a normal 341-bone MetaHuman body
So a garment is valid per-skeleton, not across the whole cast. Build against the
plugin skeleton unless you know otherwise.

Verified against Blender 5.0.0, 4.2.0 and 5.2.2 LTS.

-----------------------------------------------------------------------------
prep -- build a ready-to-open .blend from an avatar FBX (headless)

  blender -b --python Tools/blender_metahuman_garment.py -- prep \
    --avatar Tools/Export/MetaHumanBodies/SKM_MHC_Boggan_Female_Wilder_BodyMesh.fbx

  ...or every avatar at once:

  blender -b --python Tools/blender_metahuman_garment.py -- prep --all

-----------------------------------------------------------------------------
bind -- transfer skin weights from the avatar onto your garment, then parent it
        to the armature. Run inside Blender with the GARMENT selected:

  exec(open(r"D:/CHANGELING/Tools/blender_metahuman_garment.py").read())
  bind()

-----------------------------------------------------------------------------
export -- write the selected garment (+ armature) as UE-ready FBX:

  export_garment(r"D:/CHANGELING/Tools/Export/Garments/Coat.fbx")

-----------------------------------------------------------------------------
Scale: geometry is imported exactly as Unreal wrote it and is NEVER rescaled --
that is what keeps the round trip 1:1. The scene's unit scale is adjusted
instead, so measurements read correctly without moving a single vertex.
"""

import os
import sys

import bpy

def _project_root():
    """Where CHANGELING lives.

    Derived from this file's own location (<project>/Tools/...) so the scripts
    follow the project rather than pointing at a stale copy on another drive.
    CHANGELING_ROOT overrides it; the literal is only a last resort for when
    the file is exec()'d in Blender's text editor and __file__ is undefined.
    """
    env = os.environ.get("CHANGELING_ROOT")
    if env and os.path.isdir(env):
        return os.path.normpath(env)
    try:
        here = os.path.dirname(os.path.abspath(__file__))
        root = os.path.dirname(here)
        if os.path.isdir(os.path.join(root, "Content")):
            return root
    except NameError:
        pass
    return r"D:\CHANGELING"


PROJECT = _project_root()
AVATAR_DIR = os.path.join(PROJECT, "Tools", "Export", "MetaHumanBodies")
BLEND_DIR = os.path.join(PROJECT, "Tools", "Blender")


def log(msg):
    print("[mh-garment] %s" % msg)


# ---------------------------------------------------------------- import ---

def _import_fbx(path):
    """Import an FBX and return only the objects it added."""
    before = set(bpy.data.objects)
    kwargs = dict(
        filepath=path,
        use_anim=False,
        # MUST stay False. Unreal's exporter does not pad the skeleton with
        # throwaway leaf nodes, so Blender's leaf filter cannot tell a real
        # terminal bone from an artifact -- with it on, 19 genuine MetaHuman
        # bones are silently dropped, including head, every toe, and the
        # calf twist correctives. Measured: 341 bones kept vs 322 dropped.
        ignore_leaf_bones=False,
        # Preserve Unreal's bone orientation. UE matches bones by NAME on
        # import, so "nicer" auto-oriented bones buy nothing and risk the
        # round trip.
        automatic_bone_orientation=False,
    )
    try:
        bpy.ops.import_scene.fbx(use_custom_normals=True, **kwargs)
    except TypeError:
        bpy.ops.import_scene.fbx(**kwargs)
    return [o for o in bpy.data.objects if o not in before]


def _split(objs):
    arm = next((o for o in objs if o.type == "ARMATURE"), None)
    meshes = [o for o in objs if o.type == "MESH"]
    return arm, meshes


def _height(obj):
    zs = [(obj.matrix_world @ v.co).z for v in obj.data.vertices]
    return max(zs) - min(zs) if zs else 0.0


# ------------------------------------------------------------------ prep ---

def prep_avatar(fbx_path, blend_out=None, lock=True):
    """Import one avatar FBX into a clean scene and save it as a .blend."""
    fbx_path = os.path.abspath(fbx_path)
    if not os.path.isfile(fbx_path):
        raise IOError("no such FBX: %s" % fbx_path)

    stem = os.path.splitext(os.path.basename(fbx_path))[0]
    if blend_out is None:
        blend_out = os.path.join(BLEND_DIR, stem + ".blend")

    bpy.ops.wm.read_factory_settings(use_empty=True)

    objs = _import_fbx(fbx_path)
    arm, meshes = _split(objs)
    if not meshes:
        raise RuntimeError("%s contained no mesh" % stem)
    body = max(meshes, key=lambda m: len(m.data.vertices))

    raw_h = _height(body)
    # Unreal writes centimetres. Tell Blender that instead of scaling the mesh.
    if raw_h > 10.0:
        bpy.context.scene.unit_settings.system = "METRIC"
        bpy.context.scene.unit_settings.scale_length = 0.01
        bpy.context.scene.unit_settings.length_unit = "CENTIMETERS"
        log("body is %.1f units tall -> treating source as centimetres" % raw_h)
    else:
        log("body is %.2f units tall -> treating source as metres" % raw_h)

    body.name = "Avatar_" + stem.replace("SKM_MHC_", "").replace("_BodyMesh", "")
    if arm:
        arm.name = "Avatar_Armature"

    # The avatar is a fit reference, so by default it is locked -- it must not
    # be nudged out from under a garment halfway through modelling. Pass
    # lock=False when the avatar needs to be positioned in a scene rather than
    # fitted against. Note a piece separated from a locked avatar INHERITS the
    # locks, which is why make_dress_shell() clears them on the shell.
    for obj in [body] + ([arm] if arm else []):
        obj.lock_location = (lock, lock, lock)
        obj.lock_rotation = (lock, lock, lock)
        obj.lock_scale = (lock, lock, lock)

    coll = bpy.data.collections.new("Avatar")
    bpy.context.scene.collection.children.link(coll)
    for obj in objs:
        for parent in list(obj.users_collection):
            parent.objects.unlink(obj)
        coll.objects.link(obj)

    # A 170-unit-tall body clips badly against Blender's default view range.
    for screen in bpy.data.screens:
        for area in screen.areas:
            if area.type != "VIEW_3D":
                continue
            for space in area.spaces:
                if space.type == "VIEW_3D":
                    space.clip_start = 0.1
                    space.clip_end = 100000.0

    out_dir = os.path.dirname(blend_out)
    if out_dir and not os.path.isdir(out_dir):
        os.makedirs(out_dir)
    bpy.ops.wm.save_as_mainfile(filepath=blend_out)

    bones = len(arm.data.bones) if arm else 0
    log("%s -> %s  (%d verts, %d bones)"
        % (stem, blend_out, len(body.data.vertices), bones))
    return blend_out


def prep_all(avatar_dir=None, out_dir=None, lock=True, only=None):
    avatar_dir = avatar_dir or AVATAR_DIR
    out_dir = out_dir or BLEND_DIR
    fbxs = sorted(f for f in os.listdir(avatar_dir) if f.lower().endswith(".fbx"))
    if only:
        wanted = [o.strip().lower() for o in only if o.strip()]
        fbxs = [f for f in fbxs if any(w in f.lower() for w in wanted)]
    if not fbxs:
        log("no FBX files in %s" % avatar_dir)
        return []
    done = []
    for name in fbxs:
        out = os.path.join(out_dir, os.path.splitext(name)[0] + ".blend")
        try:
            done.append(prep_avatar(os.path.join(avatar_dir, name), out, lock=lock))
        except Exception as exc:
            log("FAILED %s: %s" % (name, exc))
    log("prepped %d/%d avatars" % (len(done), len(fbxs)))
    return done


# ------------------------------------------------------------- dress shell ---

# Bones whose geometry is an arm. A face is dropped if any of its vertices is
# dominantly weighted to one of these -- far more reliable than judging by X
# distance, because at shoulder height the torso is as wide as the arms.
# "toe" is excluded explicitly so middletoe/indextoe/ringtoe do not match the
# finger tokens of the same name.
_ARM_TOKENS = ("upperarm", "lowerarm", "hand", "wrist",
               "thumb", "index", "middle", "ring", "pinky")


def _is_arm_bone(name):
    low = name.lower()
    if "toe" in low:
        return False
    return any(tok in low for tok in _ARM_TOKENS)


def _dominant_group(vert, names):
    if not vert.groups:
        return ""
    best = max(vert.groups, key=lambda g: g.weight)
    return names.get(best.group, "")


def landmarks(body=None):
    """Print the world Z of useful bones, as fractions of body height.

    Use this to choose top/bottom for make_dress_shell() on a body whose
    proportions you do not know.
    """
    body = body or _find_body(exclude=None)
    arm = body.find_armature()
    zs = [(body.matrix_world @ v.co).z for v in body.data.vertices]
    zmin, zmax = min(zs), max(zs)
    span = zmax - zmin
    log("body '%s': z %.4f..%.4f (height %.4f)" % (body.name, zmin, zmax, span))
    if arm is None:
        return
    for name in ("neck_01", "clavicle_l", "spine_05", "spine_04", "spine_03",
                 "spine_02", "spine_01", "pelvis", "thigh_l", "calf_l", "foot_l"):
        bone = arm.data.bones.get(name)
        if bone is None:
            continue
        z = (arm.matrix_world @ bone.head_local).z
        log("   %-11s z=%.4f  fraction=%.3f" % (name, z, (z - zmin) / span))


def _ring_count(mesh_obj, world_z):
    """How many separate boundary loops a horizontal cut at world_z produces."""
    import bmesh
    from mathutils import Vector

    mw = mesh_obj.matrix_world
    inv = mw.inverted()
    bm = bmesh.new()
    bm.from_mesh(mesh_obj.data)
    bmesh.ops.bisect_plane(
        bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:],
        plane_co=inv @ Vector((0.0, 0.0, world_z)),
        plane_no=(inv.to_3x3() @ Vector((0.0, 0.0, 1.0))).normalized(),
        dist=1e-6, clear_inner=True)
    adjacency = {}
    for edge in bm.edges:
        if not edge.is_boundary:
            continue
        if any(abs((mw @ v.co).z - world_z) > 1e-4 for v in edge.verts):
            continue
        a, b = edge.verts
        adjacency.setdefault(a.index, []).append(b.index)
        adjacency.setdefault(b.index, []).append(a.index)
    seen, rings = set(), 0
    for start in adjacency:
        if start in seen:
            continue
        rings += 1
        stack = [start]
        while stack:
            cur = stack.pop()
            if cur in seen:
                continue
            seen.add(cur)
            stack.extend(adjacency.get(cur, []))
    bm.free()
    return rings


def find_leg_merge(body=None):
    """Fraction of body height at which the two legs become one ring.

    Below this the body is two separate leg tubes, so a garment cut there has
    two hems and no single ring to extrude a skirt from. Measured on the
    childling: 0.49. The body must be welded first -- MetaHuman bodies are 14
    loose shells and the seams otherwise register as extra rings.
    """
    body = body or _find_body(exclude=None)

    tmp = body.copy()
    tmp.data = body.data.copy()
    bpy.context.collection.objects.link(tmp)
    tmp.lock_location = (False, False, False)
    bpy.ops.object.select_all(action="DESELECT")
    tmp.select_set(True)
    bpy.context.view_layer.objects.active = tmp
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.remove_doubles(threshold=0.0001)
    bpy.ops.object.mode_set(mode="OBJECT")

    zs = [(tmp.matrix_world @ v.co).z for v in tmp.data.vertices]
    zmin, zmax = min(zs), max(zs)
    span = zmax - zmin

    merge = None
    was_split = False
    for step in range(40):
        frac = 0.35 + step * 0.01
        rings = _ring_count(tmp, zmin + span * frac)
        if rings >= 2:
            was_split = True
        elif rings == 1 and was_split:
            merge = frac
            break

    bpy.data.objects.remove(tmp, do_unlink=True)
    if merge is None:
        log("could not find a leg merge line -- leaving bottom as given")
    else:
        log("legs merge at fraction %.2f" % merge)
    return merge


def _flat_cut(shell, z_hi, z_lo):
    """Slice the shell dead flat at the two planes.

    Selecting faces can only ever produce a sawtooth boundary -- whole
    triangles are kept or dropped, so the hem follows triangle centres. Cutting
    with a plane creates new geometry exactly on the plane instead, giving a
    straight hem and neckline. bmesh.ops is used rather than bpy.ops.mesh.bisect
    because the operator needs a 3D-view context and fails headless.
    """
    import bmesh
    from mathutils import Vector

    inv = shell.matrix_world.inverted()
    # World Z plane expressed in the object's local space. MetaHuman avatars
    # carry a 0.01 delta scale, so this conversion is not optional.
    normal = (inv.to_3x3() @ Vector((0.0, 0.0, 1.0))).normalized()

    bm = bmesh.new()
    bm.from_mesh(shell.data)
    for world_z, clear in ((z_hi, "outer"), (z_lo, "inner")):
        co = inv @ Vector((0.0, 0.0, world_z))
        geom = bm.verts[:] + bm.edges[:] + bm.faces[:]
        bmesh.ops.bisect_plane(
            bm, geom=geom, plane_co=co, plane_no=normal, dist=1e-6,
            clear_outer=(clear == "outer"), clear_inner=(clear == "inner"))
    bm.to_mesh(shell.data)
    bm.free()
    shell.data.update()
    log("flat-cut hem and neckline -> %d verts" % len(shell.data.vertices))


def _remove_loose(obj):
    """Delete vertices that belong to no face.

    The source bodies carry orphan verts (visible as stray dots around the
    avatar). One landing inside a garment drags its bounding box metres out of
    true -- measured on boggan_wilder_female: a single vert at (-6.14, 14.89,
    -1.19) on a dress 0.5 m wide, reporting the hem 1.33 m below the floor
    while the actual geometry was correct.
    """
    import bmesh

    bm = bmesh.new()
    bm.from_mesh(obj.data)
    loose = [v for v in bm.verts if not v.link_faces]
    if loose:
        bmesh.ops.delete(bm, geom=loose, context="VERTS")
        bm.to_mesh(obj.data)
        obj.data.update()
        log("removed %d loose vertices" % len(loose))
    bm.free()
    return len(loose)


def _despike(shell, rounds=2):
    """Trim single-triangle spikes left on the armhole borders.

    The flat cut fixes the horizontal boundaries, but the armholes are cut by
    bone weighting and still end in tips. A face with two or more boundary
    edges is a spike by definition; removing those smooths the opening without
    eroding it.
    """
    import bmesh

    removed = 0
    for _ in range(rounds):
        bm = bmesh.new()
        bm.from_mesh(shell.data)
        spikes = [f for f in bm.faces
                  if sum(1 for e in f.edges if e.is_boundary) >= 2]
        if not spikes:
            bm.free()
            break
        bmesh.ops.delete(bm, geom=spikes, context="FACES")
        removed += len(spikes)
        bm.to_mesh(shell.data)
        bm.free()
    shell.data.update()
    if removed:
        log("removed %d boundary spikes" % removed)


def _armature_for(body):
    for mod in body.modifiers:
        if mod.type == "ARMATURE" and mod.object is not None:
            return mod.object
    if body.parent is not None and body.parent.type == "ARMATURE":
        return body.parent
    for obj in bpy.context.scene.objects:
        if obj.type == "ARMATURE":
            return obj
    return None


def _shoulder_caps(body, reach=0.80):
    """The shoulder joints, and how far down the arm to keep as a coat.

    A MetaHuman's DELTOID is weighted to upperarm, so stripping every
    arm-dominated vertex takes the whole shoulder with it. The shell then
    starts at the armpit and there is no armhole at all -- just a side
    opening at chest height, which is what add_sleeves was extruding those
    flat flaps from. Keeping the cap gives the coat real shoulders AND turns
    the opening into a ring around the upper arm for the sleeve to continue.
    """
    from mathutils import Vector
    arm = _armature_for(body)
    if arm is None:
        return []
    caps = []
    for side in ("l", "r"):
        upper = arm.data.bones.get("upperarm_%s" % side)
        lower = arm.data.bones.get("lowerarm_%s" % side)
        if upper is None or lower is None:
            continue
        head = arm.matrix_world @ upper.head_local
        elbow = arm.matrix_world @ lower.head_local
        caps.append((head, (elbow - head).length * reach))
    return caps


def make_dress_shell(top=0.93, bottom="auto", body=None, name="Dress",
                     strip_arms=True, weld=True, clean_cut=True,
                     keep_shoulders=True):
    """Duplicate a band of the body into a separate, unlocked garment shell.

    top/bottom are fractions of the body mesh's height (0 = lowest vertex,
    1 = highest). The defaults run from just under the collarbone to mid-thigh:
    measured on the childling body, clavicle sits at 0.967 and thigh at 0.592,
    so 0.93 clears the bust and 0.46 lands below the knee-line of the skirt's
    eventual start. Call landmarks() first to pick values against real bone
    positions on a body whose proportions you do not know.

    Returns the new object, selected and active, ready for bind().
    """
    import bmesh

    if bpy.context.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")

    body = body or _find_body(exclude=None)
    if body is None:
        raise RuntimeError("no avatar mesh in the scene -- open a prepped .blend")

    mw = body.matrix_world
    zs = [(mw @ v.co).z for v in body.data.vertices]
    zmin, zmax = min(zs), max(zs)
    span = zmax - zmin

    # A dress skirt is a cone hanging from the hips -- it must not contain leg
    # geometry. Cut below the crotch and the shell ends in two leg tubes with
    # two hems and no single ring to extrude from.
    if bottom == "auto":
        merge = find_leg_merge(body)
        bottom = (merge + 0.03) if merge is not None else 0.52
        log("bottom=auto -> %.3f (clear of the crotch)" % bottom)
    else:
        merge = None

    z_hi = zmin + span * top
    z_lo = zmin + span * bottom
    log("body '%s' height %.4f -> band z %.4f..%.4f" % (body.name, span, z_lo, z_hi))

    if merge is not None and bottom <= merge:
        log("WARNING: bottom %.3f is at or below the leg merge line %.3f -- the "
            "shell will come out as two leg tubes, not one hem ring" % (bottom, merge))

    arm_verts = set()
    if strip_arms:
        names = {g.index: g.name for g in body.vertex_groups}
        caps = _shoulder_caps(body) if keep_shoulders else []
        kept = 0
        for vert in body.data.vertices:
            if not _is_arm_bone(_dominant_group(vert, names)):
                continue
            point = mw @ vert.co
            # Anything AT OR ABOVE the shoulder joint is shoulder, whatever
            # bone owns it. A radius around the joint was not enough: the
            # coat kept coming out off-the-shoulder with bare skin across the
            # top of the chest, because the upper deltoid and the flesh over
            # the collarbone are arm-weighted and sat outside the sphere.
            if any(point.z >= head.z - 0.005 for head, _r in caps):
                kept += 1
                continue
            if any((point - head).length <= reach for head, reach in caps):
                kept += 1
                continue                    # deltoid: this is the shoulder
            arm_verts.add(vert.index)
        log("arm vertices excluded: %d (%d kept as shoulder cap)"
            % (len(arm_verts), kept))

    before = set(bpy.data.objects)
    bpy.ops.object.select_all(action="DESELECT")
    body.select_set(True)
    bpy.context.view_layer.objects.active = body
    bpy.ops.object.mode_set(mode="EDIT")

    bm = bmesh.from_edit_mesh(body.data)
    bm.faces.ensure_lookup_table()
    for face in bm.faces:
        face.select = False
    bm.select_flush(False)

    # Select generously -- margin past both planes. A face-centre test can only
    # ever give a sawtooth boundary, because it keeps or drops whole triangles.
    # The real hem and neckline are cut later with bisect_plane, which needs
    # material on both sides of each plane to slice through.
    margin = span * 0.04 if clean_cut else 0.0
    picked = 0
    for face in bm.faces:
        if strip_arms and any(v.index in arm_verts for v in face.verts):
            continue
        z = (mw @ face.calc_center_median()).z
        if (z_lo - margin) <= z <= (z_hi + margin):
            face.select = True
            picked += 1
    bm.select_flush(True)
    bmesh.update_edit_mesh(body.data)

    if not picked:
        bpy.ops.object.mode_set(mode="OBJECT")
        raise RuntimeError("no faces in that band -- run landmarks() and adjust "
                           "top/bottom")

    bpy.ops.mesh.duplicate()
    bpy.ops.mesh.separate(type="SELECTED")
    bpy.ops.object.mode_set(mode="OBJECT")

    new = [o for o in bpy.data.objects if o not in before]
    if not new:
        raise RuntimeError("separate produced no object")
    shell = new[0]
    shell.name = name

    # Drop inherited shape keys. A garment does not want the body's blend
    # shapes, and Solidify refuses outright on a mesh that has any --
    # "Modifier cannot be applied to a mesh with shape keys" -- which is
    # where this stopped dead on the Sidekick body. MetaHuman body meshes
    # export without them, so it never came up before.
    if shell.data.shape_keys:
        count = len(shell.data.shape_keys.key_blocks)
        shell.shape_key_clear()
        log("dropped %d inherited shape key(s) from %s" % (count, name))

    # A garment cut from a LOW-POLY body has nothing to tailor with. The
    # MetaHuman bodies are 32k verts and a shell off one lands around 5k;
    # the Synty body is 6.6k and its shell came out at 463, which collapsed
    # the skirt into a band and extruded the sleeves as flat ribbons -- there
    # were simply not enough edge loops to bend. Subdivide until there are.
    while len(shell.data.vertices) < 2500:
        before_verts = len(shell.data.vertices)
        mod = shell.modifiers.new("Densify", "SUBSURF")
        mod.subdivision_type = "SIMPLE"
        mod.levels = 1
        mod.render_levels = 1
        bpy.ops.object.select_all(action="DESELECT")
        shell.select_set(True)
        bpy.context.view_layer.objects.active = shell
        bpy.ops.object.modifier_apply(modifier=mod.name)
        log("densified %s: %d -> %d verts"
            % (name, before_verts, len(shell.data.vertices)))
        if len(shell.data.vertices) <= before_verts:
            break

    # The shell inherits the avatar's transform locks, which would leave it
    # immovable with no visible reason why.
    shell.lock_location = (False, False, False)
    shell.lock_rotation = (False, False, False)
    shell.lock_scale = (False, False, False)

    bpy.ops.object.select_all(action="DESELECT")
    shell.select_set(True)
    bpy.context.view_layer.objects.active = shell

    if weld:
        # MetaHuman bodies are assembled from separate shells -- the childling
        # body is 14 loose parts -- so a band cut from one arrives in pieces
        # with several competing "bottom loops". The shell borders are exactly
        # coincident, so a tiny merge threshold welds them into one continuous
        # garment without moving anything. Measured: 6 islands -> 1, 227 verts.
        before_verts = len(shell.data.vertices)
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.select_all(action="SELECT")
        bpy.ops.mesh.remove_doubles(threshold=0.0001)
        bpy.ops.object.mode_set(mode="OBJECT")
        log("welded seams: %d -> %d verts"
            % (before_verts, len(shell.data.vertices)))

    if clean_cut:
        _flat_cut(shell, z_hi, z_lo)
        _despike(shell)
    _remove_loose(shell)

    log("created '%s': %d faces, %d verts, %d vertex groups (already weighted)"
        % (shell.name, len(shell.data.polygons), len(shell.data.vertices),
           len(shell.vertex_groups)))
    log("next: shape it, extrude the skirt, then bind()")
    return shell


def dress_all(name_filter="Female", out_dir=None, only=None, **shell_kwargs):
    """Build a dress shell in every prepped .blend whose name matches.

    Opens each avatar, makes the shell, hides the armature, and saves a working
    file into Tools/Blender/Work/. Note "Male" is a substring of "Female", so
    filter on "Female" (safe) rather than "Male" (matches everything).

    `only` is an explicit list of name fragments. When given it replaces the
    filter entirely -- use it to name the exact characters to dress rather than
    relying on a substring to happen to select the right set.
    """
    out_dir = out_dir or os.path.join(BLEND_DIR, "Work")
    if not os.path.isdir(out_dir):
        os.makedirs(out_dir)

    blends = sorted(f for f in os.listdir(BLEND_DIR)
                    if f.lower().endswith(".blend"))
    if only:
        wanted = [o.strip().lower() for o in only if o.strip()]
        blends = [b for b in blends if any(w in b.lower() for w in wanted)]
        missing = [w for w in wanted
                   if not any(w in b.lower() for b in blends)]
        for name in missing:
            log("WARNING: no prepped .blend matched '%s'" % name)
    elif name_filter:
        blends = [b for b in blends if name_filter.lower() in b.lower()]
    if not blends:
        log("no prepped .blend matched '%s' in %s" % (name_filter, BLEND_DIR))
        return []

    log("building dress shells for %d avatar(s)" % len(blends))
    results = []
    for fname in blends:
        stem = os.path.splitext(fname)[0].replace("SKM_MHC_", "").replace("_BodyMesh", "")
        out = os.path.join(out_dir, stem.lower() + "_dress.blend")
        log("")
        log("--- %s ---" % stem)
        try:
            bpy.ops.wm.open_mainfile(filepath=os.path.join(BLEND_DIR, fname))
            shell = make_dress_shell(**shell_kwargs)

            arm = next((o for o in bpy.data.objects if o.type == "ARMATURE"), None)
            if arm:
                arm.hide_set(True)

            bpy.ops.object.select_all(action="DESELECT")
            shell.select_set(True)
            bpy.context.view_layer.objects.active = shell

            bpy.ops.wm.save_as_mainfile(filepath=out)
            zs = [(shell.matrix_world @ v.co).z for v in shell.data.vertices]
            results.append((stem, out, len(shell.data.polygons),
                            len(shell.data.vertices), min(zs), max(zs)))
            log("saved %s" % out)
        except Exception as exc:
            log("FAILED %s: %s" % (stem, exc))

    log("")
    log("=" * 68)
    log("%-34s %7s %7s  %s" % ("avatar", "faces", "verts", "z range"))
    for stem, _out, faces, verts, zlo, zhi in results:
        log("%-34s %7d %7d  %.4f..%.4f" % (stem, faces, verts, zlo, zhi))
    log("built %d/%d" % (len(results), len(blends)))
    log("=" * 68)
    return results


# ------------------------------------------------------------------ bind ---

def _find_body(exclude):
    """The avatar mesh: prefer the Avatar_ prefix, else the densest other mesh."""
    meshes = [o for o in bpy.context.scene.objects
              if o.type == "MESH" and o is not exclude]
    named = [o for o in meshes if o.name.startswith("Avatar_")]
    pool = named or meshes
    if not pool:
        return None
    return max(pool, key=lambda m: len(m.data.vertices))


def _unweighted(obj):
    return sum(1 for v in obj.data.vertices
               if not any(g.weight > 0.0 for g in v.groups))


def bind(garment=None, body=None, vert_mapping="POLYINTERP_NEAREST"):
    """Transfer weights from the avatar onto the garment and rig it.

    Select the garment and call bind(). Everything else is inferred.
    """
    if bpy.context.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")

    garment = garment or bpy.context.active_object
    if garment is None or garment.type != "MESH":
        raise RuntimeError("select the garment mesh first")

    body = body or _find_body(exclude=garment)
    if body is None:
        raise RuntimeError("no avatar mesh in the scene -- open a prepped .blend")

    arm = body.find_armature()
    if arm is None:
        arm = next((m.object for m in body.modifiers
                    if m.type == "ARMATURE" and m.object), None)
    if arm is None:
        raise RuntimeError("avatar %s has no armature" % body.name)

    log("garment=%s  avatar=%s  armature=%s" % (garment.name, body.name, arm.name))

    bpy.ops.object.select_all(action="DESELECT")
    garment.select_set(True)
    bpy.context.view_layer.objects.active = garment

    # Drop previous rigging so re-running bind() is idempotent.
    for mod in [m for m in garment.modifiers
                if m.type in {"ARMATURE", "DATA_TRANSFER"}]:
        garment.modifiers.remove(mod)

    mod = garment.modifiers.new("WeightTransfer", "DATA_TRANSFER")
    mod.object = body
    mod.use_vert_data = True
    mod.data_types_verts = {"VGROUP_WEIGHTS"}
    mod.vert_mapping = vert_mapping
    mod.layers_vgroup_select_src = "ALL"
    mod.layers_vgroup_select_dst = "NAME"

    # Creates the destination vertex groups; without this the apply is a no-op.
    bpy.ops.object.datalayout_transfer(modifier=mod.name)
    bpy.ops.object.modifier_apply(modifier=mod.name)

    # Parent WITHOUT disturbing the garment's world transform. Setting only
    # matrix_parent_inverse cancels the armature's 0.01 delta scale, which
    # silently leaves the garment 100x the size of the body -- it does not
    # show up until something reads matrix_world after binding.
    world_before = garment.matrix_world.copy()
    garment.parent = arm
    garment.matrix_parent_inverse = arm.matrix_world.inverted()
    garment.matrix_world = world_before

    arm_mod = garment.modifiers.new("Armature", "ARMATURE")
    arm_mod.object = arm

    stray = _unweighted(garment)
    log("transferred %d vertex groups" % len(garment.vertex_groups))
    if stray:
        log("WARNING: %d of %d vertices have no weight. Transfer samples the "
            "nearest body surface, so anything hanging free of the body -- "
            "skirt hems, coat tails, capes -- lands unweighted and collapses "
            "to the origin on the first frame. Weight those by hand."
            % (stray, len(garment.data.vertices)))
    else:
        log("every vertex is weighted")
    return garment


# ----------------------------------------------------------------- skirt ---

def _hem_ring(bm, mw, tol=0.02):
    """Boundary edges of the skirt hem.

    NOT simply the lowest boundary: once sleeves exist, a hanging cuff can drop
    below the shell's hem, and a cuff is neither flat nor the biggest opening.
    Picking the LARGEST flat boundary loop finds the hem reliably -- the hem
    carries ~200 verts against a cuff's ~23.
    """
    best = None
    for comp in _boundary_loops(bm):
        zs = [(mw @ v.co).z for v in comp]
        if max(zs) - min(zs) > tol:
            continue                      # not a flat cut
        if best is None or len(comp) > len(best[0]):
            best = (comp, sum(zs) / len(zs))
    if best is None:
        return [], 0.0
    verts = set(best[0])
    edges = [e for e in bm.edges
             if e.is_boundary and all(v in verts for v in e.verts)]
    return edges, best[1]


def _ordered_ring(edges):
    """Walk a closed edge loop into vertex order, or [] if it is not closed."""
    if not edges:
        return []
    adj = {}
    for e in edges:
        a, b = e.verts
        adj.setdefault(a, []).append(b)
        adj.setdefault(b, []).append(a)
    if any(len(v) != 2 for v in adj.values()):
        return []
    start = next(iter(adj))
    order, prev, cur = [start], None, start
    while True:
        nxt = next((n for n in adj[cur] if n is not prev), None)
        if nxt is None or nxt is start:
            break
        order.append(nxt)
        prev, cur = cur, nxt
    return order


def make_skirt(dress=None, body=None, passes=4, clearance=0.015,
               flare=None, fold=True, train=0.0, save=False, drop=None):
    """Extrude the hem ring down with a flare.

    Lengths are measured per body: the drop is (hem height - the body's lowest
    vertex - clearance), so a 1.09 m childling and a 1.55 m Sidhe each get a
    skirt that reaches their own feet. Passing flare=None sizes the widening so
    the hem finishes about 35% wider than the character's stance, which keeps
    the legs from punching through mid-stride.

    drop overrides that with an explicit length in metres, for garments that
    stop somewhere other than the floor -- a coat skirt ends at the knee, and
    computing it from the floor would put a man's coat around his ankles.

    train > 0 drags the back half of the hem backwards by that many metres.
    """
    import bmesh
    from mathutils import Vector

    if bpy.context.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")

    dress = dress or bpy.context.active_object
    if dress is None or dress.type != "MESH":
        raise RuntimeError("select the dress mesh first")
    body = body or _find_body(exclude=dress)
    if body is None:
        raise RuntimeError("no avatar mesh in the scene")

    mw = dress.matrix_world
    inv = mw.inverted()

    floor = min((body.matrix_world @ v.co).z for v in body.data.vertices)

    bm = bmesh.new()
    bm.from_mesh(dress.data)
    bm.verts.ensure_lookup_table()

    hem_edges, hem_z = _hem_ring(bm, mw)
    if not hem_edges:
        bm.free()
        raise RuntimeError("could not find a hem ring to extrude")

    if drop is None:
        drop = hem_z - floor - clearance
    if drop <= 0:
        bm.free()
        raise RuntimeError("hem is already at or below floor level")

    ring = [v for e in hem_edges for v in e.verts]
    hip_w = max((mw @ v.co).x for v in ring) - min((mw @ v.co).x for v in ring)
    low = [(body.matrix_world @ v.co) for v in body.data.vertices
           if (body.matrix_world @ v.co).z < floor + (hem_z - floor) * 0.18]
    ankle_w = (max(c.x for c in low) - min(c.x for c in low)) if low else hip_w

    if flare is None:
        target = max(1.45, (ankle_w * 1.35) / hip_w if hip_w else 1.6)
        flare = target ** (1.0 / passes)

    step = drop / passes
    log("skirt: floor %.4f, hem %.4f -> drop %.4f over %d passes (%.4f each)"
        % (floor, hem_z, drop, passes, step))
    log("       flare %.3f per pass (hem %.3f -> %.3f wide)"
        % (flare, hip_w, hip_w * (flare ** passes)))

    # A world-space downward step, expressed in the object's local space --
    # the avatars carry a 0.01 delta scale, so this conversion is not optional.
    local_step = inv.to_3x3() @ Vector((0.0, 0.0, -step))

    for _ in range(passes):
        ret = bmesh.ops.extrude_edge_only(bm, edges=hem_edges)
        new_verts = [g for g in ret["geom"] if isinstance(g, bmesh.types.BMVert)]
        new_edges = [g for g in ret["geom"]
                     if isinstance(g, bmesh.types.BMEdge) and g.is_boundary]
        if not new_verts:
            break
        cx = sum(v.co.x for v in new_verts) / len(new_verts)
        cy = sum(v.co.y for v in new_verts) / len(new_verts)
        for v in new_verts:
            v.co += local_step
            v.co.x = cx + (v.co.x - cx) * flare
            v.co.y = cy + (v.co.y - cy) * flare
        hem_edges = new_edges

    if fold and hem_edges:
        ordered = _ordered_ring(hem_edges)
        if ordered:
            cx = sum(v.co.x for v in ordered) / len(ordered)
            cy = sum(v.co.y for v in ordered) / len(ordered)
            for i, v in enumerate(ordered):
                if i % 2:
                    continue
                v.co.x = cx + (v.co.x - cx) * 1.05
                v.co.y = cy + (v.co.y - cy) * 1.05
            log("       fluted hem on %d of %d verts" % (len(ordered) // 2,
                                                         len(ordered)))

    if train > 0.0 and hem_edges:
        verts = {v for e in hem_edges for v in e.verts}
        cy_world = sum((mw @ v.co).y for v in verts) / len(verts)
        back = [v for v in verts if (mw @ v.co).y < cy_world]
        local_train = inv.to_3x3() @ Vector((0.0, -train, 0.0))
        for v in back:
            v.co += local_train
        log("       train: %d back verts pulled %.3f" % (len(back), train))

    bm.to_mesh(dress.data)
    bm.free()
    dress.data.update()

    zs = [(mw @ v.co).z for v in dress.data.vertices]
    log("       dress now spans %.4f .. %.4f (floor %.4f)"
        % (min(zs), max(zs), floor))
    if save and bpy.data.filepath:
        bpy.ops.wm.save_mainfile()
    return dress


def add_skirt_loops(dress=None, split_z=None, cuts=2):
    """Insert horizontal edge loops down the skirt.

    Four extrude passes leave a floor-length skirt with rings ~23 cm apart,
    which reads as hard facets in profile and makes the pleats look like
    creases rather than folds. This subdivides the vertical skirt edges so the
    cone is smooth and the skirt has something to bend around.

    Run BEFORE add_thickness -- subdividing after Solidify means cutting the
    inner shell and the rim too.
    """
    import bmesh

    if bpy.context.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")
    dress = dress or bpy.context.active_object
    if dress is None or dress.type != "MESH":
        raise RuntimeError("select the dress mesh first")
    if split_z is None:
        raise RuntimeError("add_skirt_loops needs split_z (where the skirt starts)")
    if cuts < 1:
        return dress

    mw = dress.matrix_world
    bm = bmesh.new()
    bm.from_mesh(dress.data)

    # A skirt edge that spans height is one of the verticals between rings;
    # horizontal ring edges have effectively no rise.
    # BOTH ends must be in the skirt. Testing only the lower end also catches
    # edges straddling the bodice/skirt boundary, which leaves those faces
    # partially subdivided -- use_grid_fill then mangles one of their verts and
    # flings it metres away (measured: a vert at (-6.14, 14.89) on a 0.5 m
    # dress, with 4 linked faces, so it is not loose geometry and cannot be
    # cleaned up afterwards).
    verticals = []
    for e in bm.edges:
        z0 = (mw @ e.verts[0].co).z
        z1 = (mw @ e.verts[1].co).z
        if abs(z0 - z1) > 0.02 and max(z0, z1) <= split_z + 1e-3:
            verticals.append(e)

    if not verticals:
        bm.free()
        log("skirt loops: no vertical skirt edges found")
        return dress

    before = len(bm.verts)
    bmesh.ops.subdivide_edges(bm, edges=verticals, cuts=cuts,
                              use_grid_fill=True)
    bm.to_mesh(dress.data)
    bm.free()
    dress.data.update()

    log("skirt loops: %d vertical edges x %d cuts -> %d verts (was %d)"
        % (len(verticals), cuts, len(dress.data.vertices), before))
    _repair_outliers(dress)
    return dress


def _repair_outliers(obj, factor=8.0):
    """Snap any vertex flung far from its own neighbours back onto them.

    Subdividing occasionally throws a single vertex metres out of place --
    measured on boggan_wilder_female, a vert at (-6.14, 14.89, -1.19) on a
    0.5 m dress. It keeps its 4 faces, so it is not loose geometry and cannot
    be deleted without tearing a hole; the fix is to move it back. Judged
    against the median edge length so it adapts to any garment's scale.
    """
    import bmesh
    from mathutils import Vector

    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.verts.ensure_lookup_table()

    lengths = sorted(e.calc_length() for e in bm.edges)
    if not lengths:
        bm.free()
        return 0
    median = lengths[len(lengths) // 2]
    limit = median * factor

    moved = 0
    for v in bm.verts:
        neighbours = [e.other_vert(v) for e in v.link_edges]
        if len(neighbours) < 2:
            continue
        avg = Vector((0.0, 0.0, 0.0))
        for n in neighbours:
            avg += n.co
        avg /= len(neighbours)
        if (v.co - avg).length > limit:
            v.co = avg
            moved += 1

    if moved:
        bm.to_mesh(obj.data)
        obj.data.update()
        log("repaired %d outlier vert(s) (limit %.4f local units)" % (moved, limit))
    bm.free()
    return moved


def _penetration(garment, body, sample=3000):
    """What fraction of a garment's vertices sit inside the body.

    Deliberately local rather than imported: this has to be callable from
    the middle of add_thickness without dragging in another module.
    """
    if body is None:
        return 0.0, 0
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
        # Half a millimetre. Inside that, the vertex is ON the body, not in
        # it, and the sign of the dot product is floating-point noise: the
        # body tested against ITSELF read 48.3% "inside". Garments cut from
        # the body start coincident, so the two must be counted apart.
        if gap < -0.0005:
            inside += 1
        elif gap < 0.0005:
            touching += 1
    return ((100.0 * inside / tested if tested else 0.0),
            tested,
            (100.0 * touching / tested if tested else 0.0))


def _report_penetration(stage, garment, body):
    pct, tested, touch = _penetration(garment, body)
    log("   %-26s inside %5.1f%%  touching %5.1f%%  (of %d)"
        % (stage, pct, touch, tested))
    return pct


def _clear_of_body(dress, body, offset, only_above_z=None, above=None,
                   max_move=None, relax=6, offset_fn=None, skip_body=None):
    from mathutils import Vector
    """Push every vertex to the body's surface plus offset, along its normal.

    Replaces the Shrinkwrap modifier, which was doing the opposite of its
    job. Instrumented, the breeches went INTO the shrinkwrap at 0.0% inside
    and 100% touching -- exactly right for a shell cut from the body -- and
    came OUT of a 6mm clearance pass at 40.0% inside. The waistcoat went
    0.0% to 36.2%. Whatever direction Blender chose for the offset on
    coincident geometry, for a large share of vertices it was inward.

    Doing it by hand is unambiguous: closest_point_on_mesh gives the surface
    point and its outward normal, and the vertex goes to point + normal *
    offset. Verified before use -- a copy of the body pushed 30mm this way
    measured 0.1% inside, against 48.3% for the untouched copy.

    only_above_z leaves lower geometry alone, which a coat skirt needs: it
    hangs clear of the legs and must not be snapped back onto them.
    """
    if body is None:
        return 0
    mw = dress.matrix_world
    inv = mw.inverted()
    bmw = body.matrix_world
    binv = bmw.inverted()

    # Nothing above the body's top can be inside it, and pretending
    # otherwise destroys the collar: a collar rises ABOVE the neck stump, so
    # closest_point_on_mesh finds the body's top edge and this pass snaps the
    # whole thing back down onto the shoulders. The collar was being built
    # correctly -- four converging rings -- and flattened one step later,
    # which is why it kept reading as "no collar".
    ceiling = max((bmw @ v.co).z for v in body.data.vertices)

    # CLEAR BOTH SURFACES, TAKING WHICHEVER PUSHES FURTHER OUT.
    #
    # The body stops at the collarbone, so a collar rising past it had
    # nothing to clear against and was left alone -- that was the collar
    # push-through. But clearing it against the FACE alone is worse: the
    # face's neck is narrower than the body's shoulders, so face + 33mm sits
    # INSIDE the shoulder, and penetration went 2.1% to 5.4%.
    #
    # A vertex has to be clear of everything it overlaps, so both are tested
    # and the further target wins.
    # OFFSETS ARE IN METRES; closest_point_on_mesh WORKS IN LOCAL SPACE.
    #
    # The body is parented to an armature carrying the FBX's 0.01 scale, so
    # one body-local unit is 10mm of world. Pushing a vertex out by "offset"
    # in that space delivered a hundredth of what was asked: 42mm of coat
    # clearance was really 0.42mm, and 15mm of boot clearance 0.15mm. That
    # is every layer fighting every other layer, all session, from one
    # missing conversion -- and it is why raising the numbers never helped.
    def _local_per_metre(matrix):
        step = (matrix @ Vector((1.0, 0.0, 0.0))) - (matrix @ Vector((0.0, 0.0, 0.0)))
        return 1.0 / step.length if step.length > 1e-9 else 1.0

    body_units = _local_per_metre(bmw)
    b_offset = offset * body_units

    # ONE CLEARANCE DOES NOT FIT A WHOLE GARMENT.
    #
    # 42mm off the torso is a loose coat. 42mm off a wrist is a sack: the
    # arm measures 32mm there, so the pass was pushing the cuff out to 74mm
    # whatever the sleeve had been fitted to. The sleeve clamp was working
    # perfectly -- measured, it sized the last ring to 38mm -- and then this
    # ran and undid it, which is why narrowing the sleeves, tapering their
    # clearance and fixing _arm_radius all changed nothing at the cuff.
    #
    # offset_fn lets the caller vary it by where the vertex is.

    amw = above.matrix_world if above is not None else None
    ainv = amw.inverted() if amw is not None else None
    a_offset = offset * _local_per_metre(amw) if amw is not None else offset
    # Only consider the face where the garment actually reaches it. For a
    # vertex at the ankle the nearest point ON the face is a metre away, and
    # "furthest target wins" then flung the boots up to the chin -- the whole
    # figure left the frame.
    a_floor = (min((amw @ v.co).z for v in above.data.vertices) - 0.02
               if above is not None else None)

    def target_on(mesh, m_mw, m_inv, world, local_offset):
        """Where this vertex would go to clear one mesh, or None.

        local_offset is the metre offset expressed in THAT mesh's own units.
        """
        ok, loc, nor, _ = mesh.closest_point_on_mesh(m_inv @ world)
        if not ok:
            return None, 0.0
        gap = (m_inv @ world - loc).dot(nor)
        if gap >= local_offset:
            return None, gap
        return m_mw @ (loc + nor * local_offset), gap

    # A CLEARANCE PASS MUST NOT TEAR THE SURFACE.
    #
    # Moving every vertex straight onto its own target, independently and
    # without limit, is what shredded the collar. This body has no neck: a
    # vertex at the neckline finds its nearest body point on the shoulder
    # slab, and two ADJACENT vertices -- one a little forward, one a little
    # back -- can land on different faces whose normals oppose, and be pushed
    # 130mm in opposite directions. Measured on the coat, that left edges
    # 264mm long running from the front of the neck straight through to the
    # back, and Solidify then blew the worst of them out to 2478mm. Slivers
    # went 534 -> 1555 across this one call, and 5233 by the end.
    #
    # So the displacement is computed for every vertex FIRST, then capped,
    # then smoothed across the mesh before any of it is applied. Neighbours
    # travel together, the surface stays continuous, and the neckline follows
    # the shoulder it is lying on instead of exploding off it.
    if max_move is None:
        max_move = offset * 3.0

    worlds = [mw @ v.co for v in dress.data.vertices]
    moves = [None] * len(worlds)

    for index, world in enumerate(worlds):
        if only_above_z is not None and world.z < only_above_z:
            continue

        picks = []
        # THE BODY IS THE WRONG THING TO CLEAR A COLLAR AGAINST.
        #
        # This body has no neck -- it stops at the collarbone -- so a collar
        # vertex standing at z 1.21 finds its nearest body point on the
        # SHOULDER CAP, whose normal points straight up. The target is then
        # shoulder + offset, and since the furthest target wins, it beat the
        # face's radial one and pulled the collar down onto the shoulders.
        #
        # Measured: tailor_frock built the collar to z 1.2124 and this pass
        # returned it at 1.1704 -- exactly the 42mm offset above the
        # shoulder. Every build did that, which is why the collar read as a
        # wide low funnel no matter what the rings were told to do.
        #
        # Up there the face is the only thing worth clearing, and it is the
        # mesh that actually carries the neck.
        if skip_body is None or not skip_body(world):
            here = b_offset
            if offset_fn is not None:
                here = offset_fn(world) * body_units
            want, _gap = target_on(body, bmw, binv, world, here)
            if want is not None:
                picks.append(want)
        if above is not None and a_floor is not None and world.z >= a_floor:
            # THE FACE CARRIES THE NECK, so the per-region offset has to
            # apply here too. Applying it only to the body left the collar
            # being pushed to 42mm off the throat by this second target --
            # and since the furthest target wins, that is the one that
            # decided the collar. The rings were converging on 85mm and the
            # finished collar measured 150mm.
            a_here = a_offset
            if offset_fn is not None:
                a_here = offset_fn(world) * _local_per_metre(amw)
            want, _gap = target_on(above, amw, ainv, world, a_here)
            if want is not None:
                picks.append(want)
        if not picks:
            continue

        # Furthest from where it is now: satisfying the nearer one would
        # leave it inside the other.
        target = max(picks, key=lambda q: (q - world).length)
        shift = target - world
        if shift.length < 1e-6:
            continue
        # A pass enforcing a 42mm offset has no business moving anything a
        # quarter of a metre; past this the "target" is a different part of
        # the body altogether.
        if shift.length > max_move:
            shift = shift.normalized() * max_move
        moves[index] = shift

    # Relax the displacement field. Vertices with no target count as zero, so
    # a moved vertex beside an unmoved one eases in rather than stepping.
    if relax > 0:
        neighbours = [[] for _ in worlds]
        for edge in dress.data.edges:
            a, b = edge.vertices
            neighbours[a].append(b)
            neighbours[b].append(a)
        zero = Vector((0.0, 0.0, 0.0))
        current = [m if m is not None else zero.copy() for m in moves]
        touched = [m is not None for m in moves]
        for _ in range(relax):
            nxt = current[:]
            for index, near in enumerate(neighbours):
                if not near:
                    continue
                mean = zero.copy()
                for other in near:
                    mean = mean + current[other]
                mean = mean / float(len(near))
                nxt[index] = current[index] * 0.45 + mean * 0.55
            current = nxt
        moves = [current[i] if touched[i] else None
                 for i in range(len(moves))]

    moved = 0
    for index, shift in enumerate(moves):
        if shift is None or shift.length < 1e-6:
            continue
        dress.data.vertices[index].co = inv @ (worlds[index] + shift)
        moved += 1

    dress.data.update()
    return moved


def add_thickness(dress=None, body=None, offset=0.008, thickness=0.006,
                  bodice_above_z=None, smooth_below_z=None, offset_fn=None,
                  skip_body=None):
    """Shrinkwrap clear of the skin, then Solidify -- applied in that order.

    Reversing them shrinkwraps the already-thickened shell and pinches it.

    bodice_above_z RESTRICTS the shrinkwrap to geometry above that height, and
    is essential once a skirt exists: NEAREST_SURFACEPOINT snaps every vertex
    onto the closest point of the body, so a skirt flaring away from the legs
    gets dragged inward and downward onto the feet. Measured without this on a
    floor-length gown: hems ending up to 0.29 BELOW floor level.
    """
    if bpy.context.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")
    dress = dress or bpy.context.active_object
    body = body or _find_body(exclude=dress)

    bpy.ops.object.select_all(action="DESELECT")
    dress.select_set(True)
    bpy.context.view_layer.objects.active = dress

    for mod in [m for m in dress.modifiers
                if m.type in {"SHRINKWRAP", "SOLIDIFY"}]:
        dress.modifiers.remove(mod)

    _mw = dress.matrix_world
    _p = [_mw @ v.co for v in dress.data.vertices]
    if _p:
        log("   extent before: x %.2f..%.2f  y %.2f..%.2f  z %.2f..%.2f"
            % (min(q.x for q in _p), max(q.x for q in _p),
               min(q.y for q in _p), max(q.y for q in _p),
               min(q.z for q in _p), max(q.z for q in _p)))
        _far = sorted(((q - _mw.translation).length for q in _p),
                      reverse=True)[:1]
        log("   furthest vertex from origin: %.2f m" % (_far[0] if _far else 0))
    _report_penetration("before shrinkwrap", dress, body)

    if body is not None:
        above = None
        if bodice_above_z is not None:
            above = bodice_above_z
            log("   clearance limited to the bodice (above z %.3f)" % above)
        moved = _clear_of_body(dress, body, offset, only_above_z=above,
                               above=bpy.data.objects.get("Face"),
                               offset_fn=offset_fn, skip_body=skip_body)
        log("   cleared %d verts to body + %.0fmm" % (moved, offset * 1000))
        _report_penetration("after clearance %.0fmm" % (offset * 1000),
                            dress, body)

    # Smooth a region BETWEEN clearing and solidify, never after.
    #
    # The clearance works per vertex against the nearest body surface, so on
    # a foot it finds each toe and gives every one its own bubble. Smoothing
    # merges them -- but done after solidify it smooths the inner and outer
    # shells together, collapsing the boot to no thickness, which renders as
    # bare patches of foot showing through.
    sol = dress.modifiers.new("Thickness", "SOLIDIFY")
    sol.thickness = thickness
    # OUTWARD. Solidify extrudes inward by default, so the shrinkwrap puts
    # the outer surface at body+offset and the inner shell then lands right
    # on the skin -- which is why every garment measured about half its
    # vertices INSIDE the body (breeches 53.5%, waistcoat 45.2%) and why the
    # bodice z-fought into a mottled mess. Thickening outward leaves the
    # inner surface where the shrinkwrap put it, clear of the body.
    sol.offset = 1.0
    sol.use_even_offset = True
    sol.use_rim = True
    sol.use_rim_only = False
    bpy.ops.object.modifier_apply(modifier=sol.name)
    _report_penetration("after solidify %.0fmm" % (thickness * 1000), dress, body)

    log("thickness: offset %.3f, solidify %.3f -> %d verts"
        % (offset, thickness, len(dress.data.vertices)))
    return dress


def skirt_weights(dress=None, split_z=None, hip_blend=0.25, bone="pelvis"):
    """Make the skirt hang from the hips instead of following the legs.

    Nearest-surface weight transfer gives a floor-length skirt whatever body
    part happens to be closest -- measured on a first attempt: calf_twist and
    foot bones dominating the lower skirt. A formal gown weighted that way
    splits and tears with every stride.

    Everything below split_z is reassigned to `bone`, except the top
    hip_blend fraction of the skirt, which keeps some thigh influence so the
    gown still breaks around a step rather than moving as a rigid cone.
    """
    dress = dress or bpy.context.active_object
    if dress is None or dress.type != "MESH":
        raise RuntimeError("select the dress mesh first")
    if split_z is None:
        raise RuntimeError("skirt_weights needs split_z (where the skirt starts)")

    mw = dress.matrix_world
    zs = [(mw @ v.co).z for v in dress.data.vertices]
    hem_z = min(zs)
    blend_top = hem_z + (split_z - hem_z) * (1.0 - hip_blend)

    pelvis = dress.vertex_groups.get(bone) or dress.vertex_groups.new(name=bone)
    thigh_l = dress.vertex_groups.get("thigh_l")
    thigh_r = dress.vertex_groups.get("thigh_r")

    reassigned = 0
    for vert in dress.data.vertices:
        z = (mw @ vert.co).z
        if z >= split_z:
            continue
        for g in list(vert.groups):
            grp = dress.vertex_groups[g.group]
            if grp.name != bone:
                grp.remove([vert.index])
        if z >= blend_top and (thigh_l or thigh_r):
            # Near the hip, let the legs carry some of the skirt.
            t = (z - blend_top) / max(split_z - blend_top, 1e-6)
            leg = 0.45 * t
            pelvis.add([vert.index], 1.0 - leg, "REPLACE")
            side = thigh_l if (mw @ vert.co).x >= 0 else thigh_r
            if side:
                side.add([vert.index], leg, "REPLACE")
        else:
            pelvis.add([vert.index], 1.0, "REPLACE")
        reassigned += 1

    log("skirt weights: %d verts below %.4f now hang from '%s' "
        "(top %.0f%% blended with thigh)"
        % (reassigned, split_z, bone, hip_blend * 100))
    return reassigned


def sleeve_weights(dress=None, body=None, blend_in=0.30, reach=0.14):
    """Bind the sleeves to the PRIMARY arm bones only.

    bind() transfers weights from the body by proximity, and on a MetaHuman
    the surface of an arm is owned by corrective and twist helpers --
    upperarm_out_l, upperarm_twistCor_02_l, lowerarm_twist_01_l, and the
    finger metacarpals at the cuff. Those are correct weights for the BODY,
    which is why the torso survives: it binds to plain spine bones.

    They do not survive the trip to Unreal. The sleeve comes back collapsed
    onto the torso, leaving the coat looking sleeveless with the gold wrist
    trim stranded at the hip. Re-weighting the sleeve to upperarm / lowerarm
    / hand -- which are unambiguous in both packages -- makes it follow the
    arm.

    The shoulder cap is left alone: it is part of the coat's body and blends
    in over the first blend_in of the upper arm.
    """
    from mathutils import Vector

    dress = dress or bpy.context.active_object
    body = body or _find_body(exclude=dress)
    arm = _armature_of(dress, body)
    if arm is None:
        log("sleeve weights: no armature")
        return dress

    chains = {}
    for side in ("l", "r"):
        bones = [arm.data.bones.get("%s_%s" % (n, side))
                 for n in ("upperarm", "lowerarm", "hand")]
        if any(b is None for b in bones):
            continue
        chains[side] = [arm.matrix_world @ b.head_local for b in bones]

    groups = {}
    for side in chains:
        for stem in ("upperarm", "lowerarm", "hand"):
            key = "%s_%s" % (stem, side)
            groups[key] = (dress.vertex_groups.get(key)
                           or dress.vertex_groups.new(name=key))

    names = {i: g.name for i, g in enumerate(dress.vertex_groups)}
    mw = dress.matrix_world
    touched = 0

    for vert in dress.data.vertices:
        point = mw @ vert.co

        best = None
        for side, (shoulder, elbow, wrist) in chains.items():
            for index, (a, b) in enumerate(((shoulder, elbow), (elbow, wrist))):
                axis = b - a
                length = axis.length
                if length < 1e-6:
                    continue
                u = max(0.0, min(1.0, (point - a).dot(axis) / (length * length)))
                distance = (point - (a + axis * u)).length
                if best is None or distance < best[0]:
                    best = (distance, side, index, u, a, shoulder, elbow, wrist)
        if best is None:
            continue
        distance, side, index, u, _, shoulder, elbow, wrist = best
        if distance > reach:
            continue

        # How far down the arm, as a fraction of the whole chain.
        upper = (elbow - shoulder).length
        lower = (wrist - elbow).length
        along = (u * upper) if index == 0 else (upper + u * lower)
        total = max(upper + lower, 1e-6)
        t = along / total
        if t <= 0.0:
            continue

        # Ease in, so the shoulder cap keeps the weights it shares with the
        # coat's body and only the sleeve proper is reassigned.
        ease = min(1.0, t / max(blend_in, 1e-6))
        if ease <= 0.0:
            continue

        if index == 0:
            share = {"upperarm_%s" % side: 1.0 - u, "lowerarm_%s" % side: u}
        else:
            share = {"lowerarm_%s" % side: 1.0 - u, "hand_%s" % side: u}

        for group in list(vert.groups):
            name = names.get(group.group, "")
            if name.startswith("_"):
                continue
            existing = dress.vertex_groups.get(name)
            if existing is not None:
                existing.add([vert.index], group.weight * (1.0 - ease), "REPLACE")
        for key, weight in share.items():
            if key in groups:
                groups[key].add([vert.index], weight * ease, "ADD")
        touched += 1

    dress.data.update()
    log("sleeve weights: %d vertices re-bound to upperarm/lowerarm/hand"
        % touched)
    return dress


def base_weights(dress=None, bone="pelvis"):
    """Give every unweighted vertex a full weight on one bone.

    A floor-length skirt hangs free of the body, so weight transfer leaves all
    of it unweighted and it would collapse to the origin. This is the safety
    net; refine it by hand in Weight Paint afterwards.
    """
    dress = dress or bpy.context.active_object
    if dress is None or dress.type != "MESH":
        raise RuntimeError("select the dress mesh first")

    group = dress.vertex_groups.get(bone)
    if group is None:
        group = dress.vertex_groups.new(name=bone)

    stray = [v.index for v in dress.data.vertices
             if not any(g.weight > 0.0 for g in v.groups)]
    for idx in stray:
        group.add([idx], 1.0, "REPLACE")

    log("base weights: %d stray vertices assigned to '%s'" % (len(stray), bone))
    return len(stray)


# ---------------------------------------------------------------- export ---

def export_garment(out_path, garment=None, armature=None):
    """Export the garment plus its armature as an Unreal-ready FBX."""
    from mathutils import Matrix

    if bpy.context.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")

    garment = garment or bpy.context.active_object
    if garment is None or garment.type != "MESH":
        raise RuntimeError("select the garment mesh first")
    armature = armature or garment.find_armature() or garment.parent
    if armature is None or armature.type != "ARMATURE":
        raise RuntimeError("garment has no armature -- run bind() first")

    out_dir = os.path.dirname(os.path.abspath(out_path))
    if out_dir and not os.path.isdir(out_dir):
        os.makedirs(out_dir)

    # Unhide first. prep/dress hide the armature so the viewport is clean to model
    # in, and Blender's FBX exporter SILENTLY SKIPS HIDDEN OBJECTS even when they
    # are selected -- the export succeeds, the file has no skeleton, no vertex
    # groups and no weights, and Unreal then correctly imports it as a STATIC
    # mesh. Nothing anywhere reports an error. Cost: 60 garments imported wrong.
    for obj in (garment, armature):
        try:
            obj.hide_set(False)
        except RuntimeError:
            pass                    # not in this view layer; hide_viewport covers it
        obj.hide_viewport = False

    # APPLY THE GARMENT'S OBJECT SCALE FIRST.
    #
    # The shells come out of make_dress_shell carrying scale 0.01 while the
    # armature sits at 1.0, and the exporter writes bind-pose matrices from
    # that mismatch. The FBX SDK rejects them, and Unreal says so and carries
    # on:
    #
    #   "Imported skeletal mesh has some invalid bind poses. Skeletal mesh
    #    skinning has been rebind using the time zero pose."
    #
    # A mesh rebound to time zero is skinned against a different skeleton
    # state than the body it has to sit on, which is why the garment measured
    # 0.0% penetration in Blender and punched through everywhere in Unreal,
    # why raising clearance twice changed nothing, and why the sleeves would
    # not stay on the arms. Applying the scale makes the bind pose valid.
    if any(abs(v - 1.0) > 1e-6 for v in garment.scale):
        bpy.ops.object.select_all(action="DESELECT")
        garment.select_set(True)
        bpy.context.view_layer.objects.active = garment
        bpy.ops.object.transform_apply(location=False, rotation=False,
                                       scale=True)
        log("applied object scale on %s (was %.4f)"
            % (garment.name, garment.scale.x))

    bpy.ops.object.select_all(action="DESELECT")
    garment.select_set(True)
    armature.select_set(True)
    bpy.context.view_layer.objects.active = armature

    # THE ARMATURE OBJECT'S NAME BECOMES THE ROOT BONE IN UNREAL.
    # Blender's importer consumes Unreal's `root` bone as the Armature object,
    # so the armature here holds 341 bones topped by `pelvis` with no `root` at
    # all; on export the object node comes back as bone 0. prep() names it
    # "Avatar_Armature" for a readable viewport, and that name rode all the way
    # into Unreal -- giving a 342-bone mesh whose bone 0 is `Avatar_Armature`
    # where the body has `root`, and whose `pelvis` therefore has the wrong
    # parent. Every other bone matched exactly, so the mesh imported and bound
    # fine and only failed later, inside a MetaHuman build, as
    # "This skeletal mesh asset has an incompatible Skeleton"
    # (USkeleton::IsCompatibleMesh fails its parent-chain check).
    # Rename for the duration of the export only.
    # CLEAR THE POSE BEFORE EXPORTING.
    #
    # The FBX import leaves floating-point residue in the pose: 329 of this
    # rig's 341 bones sit 1e-6 off identity. That is invisible in the
    # viewport and fatal to the bind pose, because the exporter writes a
    # pose that does not quite equal rest and the FBX SDK rejects it --
    # which is what Unreal was reporting on every import:
    #
    #   "Imported skeletal mesh has some invalid bind poses. Skeletal mesh
    #    skinning has been rebind using the time zero pose."
    #
    # Cleared for the duration of the export and put back afterwards, so
    # nothing in the working file changes.
    saved_pose = {}
    for pose_bone in armature.pose.bones:
        saved_pose[pose_bone.name] = pose_bone.matrix_basis.copy()
        pose_bone.matrix_basis = Matrix.Identity(4)
    saved_position = armature.data.pose_position
    armature.data.pose_position = "REST"
    bpy.context.view_layer.update()

    original_arm_name = armature.name
    armature.name = "root"
    try:
        bpy.ops.export_scene.fbx(
            filepath=os.path.abspath(out_path),
            use_selection=True,
            object_types={"ARMATURE", "MESH"},
            use_mesh_modifiers=True,
            mesh_smooth_type="FACE",
            # UE creates a junk bone per leaf if these are exported.
            add_leaf_bones=False,
            # Keep every bone: stripping non-deform bones makes UE see a
            # different skeleton and refuse to match metahuman_base_skel.
            use_armature_deform_only=False,
            bake_anim=False,
            primary_bone_axis="Y",
            secondary_bone_axis="X",
            apply_scale_options="FBX_SCALE_NONE",
            global_scale=1.0,
            path_mode="AUTO",
        )
    finally:
        armature.name = original_arm_name
        armature.data.pose_position = saved_position
        for pose_bone in armature.pose.bones:
            if pose_bone.name in saved_pose:
                pose_bone.matrix_basis = saved_pose[pose_bone.name]
        bpy.context.view_layer.update()
    log("exported %s -> %s (root bone exported as 'root')" % (garment.name, out_path))
    log("In Unreal: import as Skeletal Mesh and pick the EXISTING skeleton "
        "metahuman_base_skel. Never let it create a new one.")
    return out_path


# ------------------------------------------------------------ court dress ---

def make_court_dress(passes=4, clearance=0.015, train=0.0, waist=0.92,
                     offset=0.008, thickness=0.006, do_bind=True, loops=2,
                     sleeve=None, strap="thin", trim=False, uvs=False,
                     name=None):
    """Everything mechanical about a floor-length court dress, in one call.

    Cinches the waist, drops the skirt to this body's own floor with a flare,
    adds clearance and thickness, binds, and catches every stray vertex on
    'pelvis' so nothing collapses to the origin.

    What it deliberately does NOT do, because these are judgement calls:
    neckline shape, sleeves, and refining the skirt weights so it moves the way
    you want. Those are yours.
    """
    import bmesh

    dress = bpy.data.objects.get("Dress") or bpy.context.active_object
    body = _find_body(exclude=dress)
    if dress is None or body is None:
        raise RuntimeError("need a 'Dress' and an avatar in the scene")

    bpy.ops.object.select_all(action="DESELECT")
    dress.select_set(True)
    bpy.context.view_layer.objects.active = dress

    _remove_loose(dress)

    # Straps and sleeves MUST come first: add_thickness seals every opening,
    # so a finished dress has no armholes left to extrude from.
    if sleeve:
        add_straps(dress=dress, body=body, width=STRAP_WIDTHS.get(strap, 3))
        add_sleeves(dress=dress, body=body, style=sleeve)
        bpy.ops.object.select_all(action="DESELECT")
        dress.select_set(True)
        bpy.context.view_layer.objects.active = dress

    # Cinch the waist: the widest ring in the upper half of the bodice.
    if waist and waist != 1.0:
        mw = dress.matrix_world
        bm = bmesh.new()
        bm.from_mesh(dress.data)
        zs = [(mw @ v.co).z for v in bm.verts]
        lo, hi = min(zs), max(zs)
        band = [v for v in bm.verts
                if lo + (hi - lo) * 0.55 < (mw @ v.co).z < lo + (hi - lo) * 0.75]
        if band:
            cx = sum(v.co.x for v in band) / len(band)
            cy = sum(v.co.y for v in band) / len(band)
            for v in band:
                v.co.x = cx + (v.co.x - cx) * waist
                v.co.y = cy + (v.co.y - cy) * waist
            log("waist: %d verts cinched to %.2f" % (len(band), waist))
        bm.to_mesh(dress.data)
        bm.free()
        dress.data.update()

    # Where the skirt begins -- the hem of the shell, before it is extruded.
    mw = dress.matrix_world
    split_z = min((mw @ v.co).z for v in dress.data.vertices)

    make_skirt(dress=dress, body=body, passes=passes,
               clearance=clearance, train=train)
    add_skirt_loops(dress=dress, split_z=split_z, cuts=loops)
    add_thickness(dress=dress, body=body, offset=offset, thickness=thickness,
                  bodice_above_z=split_z)
    # Run again after Solidify/Shrinkwrap: those can fling vertices too, and
    # the repair must be the last thing that touches the geometry. Iterate --
    # a cluster of bad verts hides each other from a single pass, because each
    # one drags its neighbours' average out with it.
    for _ in range(4):
        if not _repair_outliers(dress, factor=4.0):
            break

    if do_bind:
        bind(garment=dress, body=body)
        skirt_weights(dress=dress, split_z=split_z)
        base_weights(dress=dress, bone="pelvis")

    label = name or dress.name
    if uvs:
        prep_uvs(dress=dress, name=label)
    if trim:
        where = ("hem", "neck", "wrist") if sleeve else ("hem", "neck")
        add_trim_bands(dress=dress, name=label, where=where)
        for _ in range(4):
            if not _repair_outliers(dress, factor=4.0):
                break
        clamp_to_floor(dress=dress, body=body)
    return dress


def court_all(only=None, out_dir=None, **kwargs):
    """Turn every dress shell in Work/ into a floor-length court dress."""
    work = out_dir or os.path.join(BLEND_DIR, "Work")
    blends = sorted(f for f in os.listdir(work)
                    if f.lower().endswith("_dress.blend"))
    if only:
        wanted = [o.strip().lower() for o in only if o.strip()]
        blends = [b for b in blends if any(w in b.lower() for w in wanted)]
    if not blends:
        log("no dress shells matched in %s" % work)
        return []

    done = []
    for fname in blends:
        src = os.path.join(work, fname)
        out = src.replace("_dress.blend", "_court.blend")
        if os.path.isfile(out):
            log("SKIP %s -- %s already exists" % (fname, os.path.basename(out)))
            continue
        log("")
        log("--- %s ---" % fname.replace("_dress.blend", ""))
        try:
            bpy.ops.wm.open_mainfile(filepath=src)
            dress = make_court_dress(name=character, **kwargs)
            bpy.ops.wm.save_as_mainfile(filepath=out)
            zs = [(dress.matrix_world @ v.co).z for v in dress.data.vertices]
            done.append((fname.replace("_dress.blend", ""),
                         len(dress.data.polygons), len(dress.data.vertices),
                         min(zs), max(zs)))
            log("saved %s" % out)
        except Exception as exc:
            log("FAILED %s: %s" % (fname, exc))

    log("")
    log("=" * 70)
    log("%-32s %7s %7s  %s" % ("character", "faces", "verts", "z range"))
    for name, faces, verts, zlo, zhi in done:
        log("%-32s %7d %7d  %.4f..%.4f" % (name, faces, verts, zlo, zhi))
    log("built %d/%d" % (len(done), len(blends)))
    log("=" * 70)
    return done


# -------------------------------------------------- straps and sleeves ---

def _boundary_loops(bm):
    """Every closed run of boundary edges, as lists of verts."""
    adj = {}
    for e in bm.edges:
        if not e.is_boundary:
            continue
        a, b = e.verts
        adj.setdefault(a, set()).add(b)
        adj.setdefault(b, set()).add(a)
    seen, loops = set(), []
    for v in adj:
        if v in seen:
            continue
        comp, stack = [], [v]
        while stack:
            c = stack.pop()
            if c in seen:
                continue
            seen.add(c)
            comp.append(c)
            stack.extend(adj[c])
        loops.append(comp)
    return loops


def _ordered_loop(bm, comp):
    """Walk a boundary component into vertex order around the cycle."""
    verts = set(comp)
    adj = {}
    for e in bm.edges:
        if not e.is_boundary:
            continue
        a, b = e.verts
        if a in verts and b in verts:
            adj.setdefault(a, []).append(b)
            adj.setdefault(b, []).append(a)
    if not adj or any(len(v) != 2 for v in adj.values()):
        return []
    start = comp[0]
    order, prev, cur = [start], None, start
    while True:
        nxt = next((x for x in adj[cur] if x is not prev), None)
        if nxt is None or nxt is start:
            break
        order.append(nxt)
        prev, cur = cur, nxt
    return order


def _armature_of(dress, body):
    arm = dress.find_armature() or (body.find_armature() if body else None)
    if arm is None:
        arm = next((o for o in bpy.context.scene.objects
                    if o.type == "ARMATURE"), None)
    return arm


def add_straps(dress=None, body=None, width=3):
    """Bridge front to back across each shoulder.

    A shell cut from a body is strapless: its entire top edge is ONE boundary
    loop running front neckline -> round one arm -> back neckline -> round the
    other. There are no armholes to extrude a sleeve from until something
    crosses the shoulder. Bridging here splits that single loop into a
    neckline plus two real armholes.

    width is how many vertices of the top edge each strap spans -- 2 for a thin
    strap, 5-6 for broad court shoulders.
    """
    import bmesh

    if bpy.context.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")
    dress = dress or bpy.context.active_object
    body = body or _find_body(exclude=dress)
    arm = _armature_of(dress, body)
    if arm is None:
        raise RuntimeError("no armature to locate the shoulders")

    mw = dress.matrix_world
    bm = bmesh.new()
    bm.from_mesh(dress.data)

    loops = _boundary_loops(bm)
    if not loops:
        bm.free()
        raise RuntimeError("no open boundary -- straps must go on the SHELL, "
                           "before add_thickness seals it")
    top = max(loops, key=lambda c: sum((mw @ v.co).z for v in c) / len(c))

    # Walk the top edge as an ordered cycle. A shoulder gap is the one place
    # where two vertices are FAR apart along the loop but CLOSE in space -- the
    # front and back of the same shoulder. Splitting by y instead is fragile:
    # it kept picking verts from the same arc, so the "bridge" filled along the
    # edge and never separated the loop (10 faces built, still 2 loops).
    order = _ordered_loop(bm, top)
    if len(order) < 8:
        bm.free()
        raise RuntimeError("top boundary is not a clean cycle")

    n = len(order)
    min_sep = max(n // 6, 4)
    best = {}
    for i in range(n):
        pi = mw @ order[i].co
        sign = 1.0 if pi.x > 0 else -1.0
        for step in range(min_sep, n - min_sep):
            j = (i + step) % n
            pj = mw @ order[j].co
            if pj.x * sign <= 0:
                continue
            d = (pi - pj).length
            if sign not in best or d < best[sign][0]:
                best[sign] = (d, i, j)

    made = 0
    for sign in (1.0, -1.0):
        if sign not in best:
            continue
        _d, i, j = best[sign]
        front = [order[(i + k) % n] for k in range(width)]
        back = [order[(j - k) % n] for k in range(width)]
        for k in range(width - 1):
            try:
                bm.faces.new((front[k], front[k + 1], back[k + 1], back[k]))
                made += 1
            except ValueError:
                pass   # face already exists

    bm.normal_update()
    bm.to_mesh(dress.data)
    bm.free()
    dress.data.update()

    bm2 = bmesh.new()
    bm2.from_mesh(dress.data)
    count = len(_boundary_loops(bm2))
    bm2.free()
    log("straps: %d faces bridged, boundary loops now %d "
        "(want 4: hem + neckline + 2 armholes)" % (made, count))

    # A thin strap can miss on an awkward shoulder. Widen and retry rather
    # than letting the whole variant fail downstream on "expected 3
    # boundaries", which is where 22 of 60 variants died.
    if count < 4 and width < 10:
        log("   only %d loops -- retrying with a wider strap" % count)
        return add_straps(dress=dress, body=body, width=width + 2)
    return dress


def _arm_points(body, arm, side):
    """World positions of the body vertices this arm owns."""
    from mathutils import Vector
    keys = ("upperarm_%s" % side, "lowerarm_%s" % side, "hand_%s" % side)
    names = {i: g.name.lower() for i, g in enumerate(body.vertex_groups)}
    mw = body.matrix_world
    pts = []
    for vert in body.data.vertices:
        best, weight = None, -1.0
        for group in vert.groups:
            if group.weight > weight:
                weight, best = group.weight, group.group
        name = names.get(best, "")
        if any(name.startswith(k.split("_")[0]) and name.endswith("_%s" % side)
               for k in keys) or name in keys:
            pts.append(mw @ vert.co)
    return pts


def _arm_radius(pts, centre, axis, slab=0.016, bound=0.075):
    """How thick the arm is where this sleeve ring sits.

    A MAXIMUM over a 70mm slab is not a radius -- it is the widest thing
    anywhere near, and near the wrist the widest thing near is the HAND.
    Measured on the childling: it reported 72mm around a 31mm wrist, so the
    cuff was sized to more than twice the arm and the sleeve read as a
    dressing gown. Tapering the clearance could not touch that, because the
    number the clearance is added TO was already wrong.

    A high percentile rather than the max, so one stray vertex cannot set
    the width; a radial bound, so the torso does not count as arm; and a
    slab short enough to be local to the ring.

    Returns 0.0 when nothing qualifies, which the caller reads as "do not
    resize this ring" -- better than crushing a sleeve onto an arm that was
    never measured.
    """
    ranked = []
    for point in pts:
        offset = point - centre
        along = offset.dot(axis)
        if abs(along) > slab:
            continue
        perp = (offset - axis * along).length
        if perp > bound:
            continue
        ranked.append(perp)
    if len(ranked) < 8:
        return 0.0
    ranked.sort()
    return ranked[int(len(ranked) * 0.90)]


def _loop_count(obj):
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    n = len(_boundary_loops(bm))
    bm.free()
    return n


def fill_small_holes(obj, limit=24):
    """Close stray openings left where the shoulder cap was cut.

    Keeping the deltoid means some faces straddle kept and stripped vertices,
    and dropping those leaves little holes around the cap. Solidify would turn
    each into a visible slit in the finished coat.
    """
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    filled = 0
    for comp in _boundary_loops(bm):
        if len(comp) > limit:
            continue                       # hem, neckline, armholes: leave them
        edges = [e for e in bm.edges
                 if e.is_boundary and all(v in set(comp) for v in e.verts)]
        if not edges:
            continue
        try:
            bmesh.ops.holes_fill(bm, edges=edges, sides=0)
            filled += 1
        except Exception:
            pass
    bm.normal_update()
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()
    if filled:
        log("filled %d stray hole(s) around the shoulder cap" % filled)
    return filled


def add_sleeves(dress=None, body=None, style="fitted",
                segments=6, clearance=0.020, cuff=0.006):
    """Extrude each armhole down the arm.

    The arms are in A-pose, so sleeves run diagonally -- the direction comes
    from each body's own upperarm/lowerarm/hand bone positions rather than any
    fixed vector. Run AFTER add_straps and BEFORE add_thickness.

    style 'fitted' tapers to the wrist; 'hanging' flares past the hand.

    Clearance is generous on purpose. The garment's bind pose and the
    skeleton's reference pose do not agree exactly about the arm -- the torso
    binds through plain spine bones and sits right, the arm binds through
    twist and corrective helpers and comes back rotated a degree or two,
    which twenty centimetres from the shoulder is a centimetre of error. A
    sleeve cut close punches through; a sleeve with room to spare does not.

    EACH RING IS SIZED TO THE ARM. Extruding along the bone and tapering by a
    flat factor per step -- which is all this did -- never asks how thick the
    arm actually is, so on any figure stouter than the one the ring was cut
    for, the sleeve ends up INSIDE the arm: the body renders over it and the
    character appears bare-armed with a cuff floating at the wrist. The arm's
    own radius is measured where each ring lands and the ring is opened to
    clear it.
    """
    import bmesh
    from mathutils import Vector

    if bpy.context.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")
    dress = dress or bpy.context.active_object
    body = body or _find_body(exclude=dress)
    arm = _armature_of(dress, body)
    if arm is None:
        raise RuntimeError("no armature to follow")

    mw = dress.matrix_world
    inv = mw.inverted()
    bm = bmesh.new()
    bm.from_mesh(dress.data)

    loops = _boundary_loops(bm)
    if len(loops) < 3:
        bm.free()
        raise RuntimeError("expected 3 boundaries (neckline + 2 armholes) -- "
                           "run add_straps() first")

    # The hem is lowest; the neckline is the centred one of what remains.
    loops.sort(key=lambda c: sum((mw @ v.co).z for v in c) / len(c))
    upper = loops[1:]

    # Pick ONE armhole per side, the opening nearest that shoulder -- and only
    # if it is plausibly close to it. Taking "the two most lateral openings"
    # instead let both picks land on the same side or on a spurious hole, and
    # extruding along the wrong arm vector produced slats up to 3.1 m long on
    # a 0.4 m body (3 of 60 variants).
    def centre(comp):
        pts = [mw @ v.co for v in comp]
        return sum(pts, Vector((0.0, 0.0, 0.0))) / len(pts)

    armholes = []
    for side, sgn in (("l", 1.0), ("r", -1.0)):
        bone = arm.data.bones.get("upperarm_%s" % side)
        if bone is None:
            continue
        shoulder = arm.matrix_world @ bone.head_local

        # AN ARMHOLE IS LATERAL. THE NECKLINE IS NOT.
        #
        # "Centre is on this side" is satisfied by the neckline too, whose
        # centre sits at x about zero, and it was being chosen: traced
        # through the build, the 118-vertex loop at z 1.128 -- the neckline --
        # came out of add_sleeves at z 0.788, extruded down an arm as a
        # sleeve. That one bug is the missing collar, the oversized sleeve
        # and the shoulder rendering at the elbow.
        #
        # A real armhole sits well out toward the shoulder, so require at
        # least a third of the way there.
        lateral = abs(shoulder.x) * 0.33
        near = [c for c in upper
                if centre(c).x * sgn > 0
                and abs(centre(c).x) >= lateral
                and c not in armholes]
        if not near:
            log("   no lateral opening on the %s side (need |x| >= %.3f) -- "
                "skipping that sleeve" % (side, lateral))
            continue
        best = min(near, key=lambda c: (centre(c) - shoulder).length)
        reach = (centre(best) - shoulder).length
        if reach > 0.35:
            log("   no armhole near the %s shoulder (nearest %.3f away) -- "
                "skipping that sleeve" % (side, reach))
            continue
        armholes.append(best)

    made = 0
    opened = 0
    for hole in armholes:
        cxw = sum((mw @ v.co).x for v in hole) / len(hole)
        side = "l" if cxw > 0 else "r"
        ua = arm.data.bones.get("upperarm_%s" % side)
        lo = arm.data.bones.get("lowerarm_%s" % side)
        ha = arm.data.bones.get("hand_%s" % side)
        if not (ua and lo and ha):
            continue
        p_ua = arm.matrix_world @ ua.head_local
        p_lo = arm.matrix_world @ lo.head_local
        p_ha = arm.matrix_world @ ha.head_local

        limb_pts = _arm_points(body, arm, side)
        log("   %s arm: %d body vertices to clear" % (side, len(limb_pts)))

        upper_seg = (p_lo - p_ua) / max(segments // 2, 1)
        lower_seg = (p_ha - p_lo) / max(segments - segments // 2, 1)

        edges = [e for e in bm.edges
                 if e.is_boundary and all(v in set(hole) for v in e.verts)]
        for i in range(segments):
            step = upper_seg if i < segments // 2 else lower_seg
            if style == "hanging":
                scale = 1.0 if i < segments // 2 else 1.22
            else:
                scale = 0.94
            ret = bmesh.ops.extrude_edge_only(bm, edges=edges)
            new_v = [g for g in ret["geom"] if isinstance(g, bmesh.types.BMVert)]
            new_e = [g for g in ret["geom"]
                     if isinstance(g, bmesh.types.BMEdge) and g.is_boundary]
            if not new_v:
                break
            local_step = inv.to_3x3() @ Vector(step)
            cx = sum(v.co.x for v in new_v) / len(new_v)
            cy = sum(v.co.y for v in new_v) / len(new_v)
            cz = sum(v.co.z for v in new_v) / len(new_v)
            for v in new_v:
                v.co += local_step
                v.co.x = cx + (v.co.x - cx) * scale
                v.co.y = cy + (v.co.y - cy) * scale
                v.co.z = cz + (v.co.z - cz) * scale

            # Put the ring ON the arm, not merely parallel to it. Extruding
            # by the bone's step vector carries the armhole's own centre
            # along, so if that opening sits at the side of the torso the
            # whole sleeve runs beside the arm instead of around it -- which
            # is why the sleeve read as a flap hanging at the shoulder and
            # why so few of its vertices ever measured as too close. The
            # ring is eased onto the bone across the first few segments so
            # it stays attached at the armhole.
            if i < segments // 2:
                target = p_ua + upper_seg * (i + 1)
            else:
                target = p_lo + lower_seg * (i + 1 - segments // 2)
            here = sum((mw @ v.co for v in new_v),
                       Vector((0.0, 0.0, 0.0))) / len(new_v)
            ease = min(1.0, float(i + 1) / max(segments // 2, 1))
            shift = inv.to_3x3() @ ((target - here) * ease)
            for v in new_v:
                v.co += shift

            # Open the ring to clear the arm it has just landed on.
            axis = Vector(step)
            if axis.length > 1e-6 and limb_pts:
                axis = axis.normalized()
                centre = sum((mw @ v.co for v in new_v),
                             Vector((0.0, 0.0, 0.0))) / len(new_v)
                # TAPER THE CLEARANCE, AND BITE EARLY.
                #
                # One constant clearance the whole way down is what makes a
                # sleeve read as a bathrobe: 15mm is a little ease on an
                # upper arm and a wide tube at a wrist half the thickness.
                # A tailored sleeve is generous at the top, where the arm has
                # to move, and close at the cuff.
                #
                # And the clamp has to reach full strength within a segment
                # or two. Easing it across the whole upper half left the top
                # of the sleeve at the armhole's width, which is a dropped
                # kimono seam -- the other half of the dressing-gown read.
                fall = i / float(max(segments - 1, 1))
                here = clearance + (cuff - clearance) * fall
                arm_r = _arm_radius(limb_pts, centre, axis)
                want = (arm_r + here) if arm_r > 1e-6 else None
                # CLOSE THE RING AS WELL AS OPEN IT.
                #
                # This only ever widened: a ring already bigger than the arm
                # was left alone. So the sleeve inherited the ARMHOLE's width
                # -- cut from a shell standing 42mm off the body -- and
                # carried it to the wrist, which is the whole of why the
                # sleeves are baggy. The taper could not recover it either,
                # at 0.97 a segment over six segments: 17%.
                #
                # Eased in over the first half, because the top of the sleeve
                # has to meet the armhole it grew from. Clamp it at segment
                # one and the shoulder pinches.
                grip = min(1.0, max(0.0, (i - 0.5) / 1.5))
                for v in (new_v if want is not None else []):
                    world = mw @ v.co
                    offset = world - centre
                    along = offset.dot(axis)
                    radial = offset - axis * along
                    radius = radial.length
                    if radius < 1e-6:
                        continue
                    if radius < want:
                        target = want
                    else:
                        target = radius + (want - radius) * grip
                    if abs(target - radius) < 1e-6:
                        continue
                    v.co = inv @ (centre + axis * along
                                  + radial.normalized() * target)
                    opened += 1
            edges = new_e
            made += 1

    bm.normal_update()
    bm.to_mesh(dress.data)
    bm.free()
    dress.data.update()
    log("sleeves: %s, %d extrusions across %d armhole(s), %d vertices opened"
        " to clear the arm" % (style, made, len(armholes), opened))
    return dress


# ------------------------------------------------------------ trim bands ---

def add_trim_bands(dress=None, where=("hem", "neck"), width=0.015, gap=0.010,
                   inset=0.010, height=0.004, name=None):
    """Raise a double band of trim at the hem and/or neckline.

    Each location gets TWO parallel bands separated by `gap`, pushed radially
    outward by `height` so they catch light as raised braid. The band faces go
    into a second material slot so they can take a metal while the gown keeps
    its fabric.

    Works on a finished (sealed) court dress: the bands are found by height
    rather than by boundary edges, which Solidify has already closed over.
    """
    import bmesh

    dress = dress or bpy.context.active_object
    if dress is None or dress.type != "MESH":
        raise RuntimeError("select the dress mesh first")

    mesh = dress.data
    trim_name = "MI_CourtTrim_%s" % (name or dress.name)
    if any(m and m.name == trim_name for m in mesh.materials):
        # Running twice would inset bands on top of bands.
        log("   already trimmed -- skipping")
        return dress
    trim = bpy.data.materials.get(trim_name) or bpy.data.materials.new(trim_name)
    trim.use_nodes = True
    if trim.name not in [m.name for m in mesh.materials if m]:
        mesh.materials.append(trim)
    trim_index = len(mesh.materials) - 1

    scale = dress.matrix_world.to_scale()[0] or 1.0
    local_height = height / scale

    bm = bmesh.new()
    bm.from_mesh(mesh)
    mw = dress.matrix_world

    zs = [(mw @ v.co).z for v in bm.verts]
    z_lo, z_hi = min(zs), max(zs)
    cx = sum(v.co.x for v in bm.verts) / len(bm.verts)
    cy = sum(v.co.y for v in bm.verts) / len(bm.verts)

    ranges = []
    if "hem" in where:
        base = z_lo + inset
        ranges.append((base, base + width))
        ranges.append((base + width + gap, base + 2 * width + gap))
    if "neck" in where:
        base = z_hi - inset
        ranges.append((base - width, base))
        ranges.append((base - 2 * width - gap, base - width - gap))
    # Wrist bands cannot be height ranges -- a horizontal slice at cuff height
    # would also catch the skirt. They are found by distance out along the arm
    # from the shoulder, so they adapt to any sleeve length or body size.
    wrist_specs = []
    if "wrist" in where:
        arm_obj = _armature_of(dress, _find_body(exclude=dress))
        # Sleeves live in the UPPER half; a flared skirt is wider than any
        # sensible lateral cutoff, so filtering on |x| alone drags hem faces
        # into the wrist bands and insets them downward through the floor.
        mid_z = z_lo + (z_hi - z_lo) * 0.5
        upper = [(mw @ v.co) for v in bm.verts if (mw @ v.co).z > mid_z]
        reach = max((abs(c.x) for c in upper), default=0.0)
        for side, sgn in (("l", 1.0), ("r", -1.0)):
            bone = arm_obj.data.bones.get("upperarm_%s" % side) if arm_obj else None
            if bone is None:
                continue
            shoulder = arm_obj.matrix_world @ bone.head_local
            out = [c for c in upper if c.x * sgn > reach * 0.55]
            if not out:
                continue
            cuff = max((c - shoulder).length for c in out)
            wrist_specs.append((sgn, shoulder, cuff - inset - width, cuff - inset))
            wrist_specs.append((sgn, shoulder,
                                cuff - inset - 2 * width - gap,
                                cuff - inset - width - gap))
        if not wrist_specs:
            log("   note: 'wrist' asked for but no sleeves found")

    # Cut loops at every band edge FIRST. Selecting by face centre alone cannot
    # work: skirt faces are ~76 mm tall and a band is ~15 mm, so a band either
    # swallows a whole ring or misses entirely -- measured 3 bands instead of 4,
    # one of them 0.3 mm tall. Bisecting gives each band its exact height.
    from mathutils import Vector
    inv = mw.inverted()
    normal = (inv.to_3x3() @ Vector((0.0, 0.0, 1.0))).normalized()
    for lo, hi in ranges:
        for plane_z in (lo, hi):
            bmesh.ops.bisect_plane(
                bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:],
                plane_co=inv @ Vector((0.0, 0.0, plane_z)),
                plane_no=normal, dist=1e-6,
                clear_inner=False, clear_outer=False)
    bm.verts.ensure_lookup_table()
    bm.faces.ensure_lookup_table()

    band_faces = []
    for lo, hi in ranges:
        for face in bm.faces:
            z = (mw @ face.calc_center_median()).z
            if lo + 1e-5 <= z <= hi - 1e-5:
                band_faces.append(face)
    for sgn, shoulder, lo, hi in wrist_specs:
        for face in bm.faces:
            c = mw @ face.calc_center_median()
            # Upper half only -- see the note where wrist_specs is built.
            if c.x * sgn <= 0 or c.z <= z_lo + (z_hi - z_lo) * 0.5:
                continue
            d = (c - shoulder).length
            if lo <= d <= hi:
                band_faces.append(face)
    band_faces = list(dict.fromkeys(band_faces))

    # Inset THEN push out. Simply displacing the band's vertices does nothing
    # visible: a single row of faces shares every vertex with the fabric above
    # and below (measured: 0 exclusive verts, 1600 shared), so the fabric moves
    # with it and the result is a soft bulge, not braid. inset_region gives the
    # band its own border and raises it with proper side walls.
    faced = len(band_faces)
    if band_faces:
        # use_even_offset must stay OFF. Even-offset solves a corner equation
        # per boundary vertex, and on a scattered or non-contiguous face set --
        # which the wrist bands can produce -- that solution blows up: measured
        # a band exploding to |x| 3.1 m and z -1.0 on a 0.4 m dress.
        res = bmesh.ops.inset_region(
            bm, faces=band_faces, thickness=local_height * 0.3,
            depth=local_height, use_even_offset=False, use_boundary=True)
        for face in band_faces:
            face.material_index = trim_index
        for face in res.get("faces", []):
            face.material_index = trim_index

    bm.to_mesh(mesh)
    bm.free()
    mesh.update()

    log("trim: %d bands, %d faces raised %.4f into slot '%s'"
        % (len(ranges), faced, height, trim_name))
    return dress


def clamp_to_floor(dress=None, body=None, clearance=0.012):
    """Lift any vertex that has ended up below the character's floor.

    The trim inset pushes along face normals, and at the very bottom front of
    the hem those point down-and-out -- measured 4 verts of 20,090 poking 4 cm
    through the ground. Too few to matter to the silhouette, but enough to
    report the whole gown as floor-breaching.
    """
    dress = dress or bpy.context.active_object
    body = body or _find_body(exclude=dress)
    if dress is None or body is None:
        return 0

    mw = dress.matrix_world
    inv = mw.inverted()
    floor = min((body.matrix_world @ v.co).z for v in body.data.vertices)
    target = floor + clearance

    lifted = 0
    for v in dress.data.vertices:
        world = mw @ v.co
        if world.z < target:
            world.z = target
            v.co = inv @ world
            lifted += 1
    if lifted:
        dress.data.update()
        log("clamped %d vert(s) up to floor + %.3f" % (lifted, clearance))
    return lifted


def trim_all(only=None, work_dir=None, suffix="_court.blend", **kwargs):
    """Add trim bands to every finished court dress."""
    work = work_dir or os.path.join(BLEND_DIR, "Work")
    blends = sorted(f for f in os.listdir(work) if f.endswith(suffix))
    if only:
        wanted = [o.strip().lower() for o in only if o.strip()]
        blends = [b for b in blends if any(w in b.lower() for w in wanted)]
    done = []
    for fname in blends:
        character = fname.replace(suffix, "")
        log("")
        log("--- %s ---" % character)
        try:
            bpy.ops.wm.open_mainfile(filepath=os.path.join(work, fname))
            dress = bpy.data.objects.get("Dress")
            if dress is None:
                log("   no Dress object")
                continue
            add_trim_bands(dress=dress, name=character, **kwargs)
            bpy.ops.wm.save_mainfile()
            done.append(character)
        except Exception as exc:
            log("   FAILED: %s" % exc)
    log("")
    log("trimmed %d/%d" % (len(done), len(blends)))
    return done


# -------------------------------------------------------------- materials ---

def prep_uvs(dress=None, name=None, aspect=True):
    """Strip the body material and give the garment garment-shaped UVs.

    A shell cut from a body inherits the body's skin material and its UDIM body
    UVs (measured: MI_Body_Baked_VT, U running 1.03-1.97 in UDIM tile 2). Those
    describe a body, not a dress, and the extruded skirt carries stretched
    interpolated values that mean nothing.

    Replaces them with a cylindrical projection: U wraps the circumference, V
    runs floor to neckline. That is the right shape for a garment -- tiling
    brocade wraps continuously and a hem trim is a horizontal band -- where an
    automatic unwrap would chop the dress into islands that no tiling pattern
    can cross.

    With aspect=True, U is scaled so a square texture is not stretched.
    """
    import bmesh
    import math

    dress = dress or bpy.context.active_object
    if dress is None or dress.type != "MESH":
        raise RuntimeError("select the dress mesh first")

    mesh = dress.data
    old = [m.name if m else None for m in mesh.materials]
    mesh.materials.clear()
    mat_name = "MI_Court_%s" % (name or dress.name)
    mat = bpy.data.materials.get(mat_name) or bpy.data.materials.new(mat_name)
    mat.use_nodes = True
    mesh.materials.append(mat)
    log("material: %s -> %s" % (old or "<none>", mat_name))

    for layer in list(mesh.uv_layers):
        mesh.uv_layers.remove(layer)
    mesh.uv_layers.new(name="GarmentUV")

    bm = bmesh.new()
    bm.from_mesh(mesh)
    uvl = bm.loops.layers.uv.active

    co = [v.co for v in bm.verts]
    cx = sum(c.x for c in co) / len(co)
    cy = sum(c.y for c in co) / len(co)
    zmin = min(c.z for c in co)
    zmax = max(c.z for c in co)
    height = max(zmax - zmin, 1e-6)
    radius = sum(math.hypot(c.x - cx, c.y - cy) for c in co) / len(co)
    u_scale = (2.0 * math.pi * radius / height) if aspect else 1.0

    for face in bm.faces:
        raw = []
        for loop in face.loops:
            p = loop.vert.co
            ang = math.atan2(p.y - cy, p.x - cx)
            raw.append(ang / (2.0 * math.pi) + 0.5)
        # Faces crossing the atan2 wrap would otherwise stretch right across
        # the texture; lift the low side into the next tile instead.
        if max(raw) - min(raw) > 0.5:
            raw = [u + 1.0 if u < 0.5 else u for u in raw]
        for loop, u in zip(face.loops, raw):
            v = (loop.vert.co.z - zmin) / height
            loop[uvl].uv = (u * u_scale, v)

    bm.to_mesh(mesh)
    bm.free()
    mesh.update()
    log("UVs: cylindrical, U x%.2f (circumference %.3f / height %.3f)"
        % (u_scale, 2.0 * math.pi * radius, height))
    return dress


def uvs_all(only=None, work_dir=None, suffix="_court.blend"):
    """Give every finished court dress its own material and garment UVs."""
    work = work_dir or os.path.join(BLEND_DIR, "Work")
    blends = sorted(f for f in os.listdir(work) if f.endswith(suffix))
    if only:
        wanted = [o.strip().lower() for o in only if o.strip()]
        blends = [b for b in blends if any(w in b.lower() for w in wanted)]
    if not blends:
        log("nothing matched in %s" % work)
        return []

    done = []
    for fname in blends:
        path = os.path.join(work, fname)
        character = fname.replace(suffix, "")
        log("")
        log("--- %s ---" % character)
        try:
            bpy.ops.wm.open_mainfile(filepath=path)
            dress = bpy.data.objects.get("Dress")
            if dress is None:
                log("   no Dress object")
                continue
            prep_uvs(dress=dress, name=character)
            bpy.ops.wm.save_mainfile()
            done.append(character)
        except Exception as exc:
            log("   FAILED: %s" % exc)

    log("")
    log("prepared %d/%d" % (len(done), len(blends)))
    return done


# ------------------------------------------------------------- body trim ---

def trim_body_under(dress=None, body=None, reach=0.015, margin=1,
                    out_name=None):
    """Delete the body geometry the garment covers, so it cannot poke through.

    The definitive fix for poke-through, and free at runtime: geometry that
    does not exist cannot show. Cloth simulation, thicker offsets and pushing
    the garment further out all merely reduce the odds.

    Coverage is decided by casting a ray OUTWARD from the body's vertical axis
    through each vertex: if it meets the dress within `reach`, that vertex is
    under cloth. This deliberately avoids inside/outside tests -- a solidified
    garment is double-walled, so its interior is the fabric itself and every
    such test answers the wrong question.

    `margin` keeps that many rings of body alive along the garment's edges, so
    nothing gapes at the neckline, cuffs or hem.

    Works on a COPY; the locked fit avatar is left alone.
    """
    import bmesh
    from mathutils import Vector

    if bpy.context.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")
    dress = dress or bpy.data.objects.get("Dress") or bpy.context.active_object
    body = body or _find_body(exclude=dress)
    if dress is None or body is None:
        raise RuntimeError("need both a dress and a body")

    trimmed = body.copy()
    trimmed.data = body.data.copy()
    trimmed.name = out_name or (body.name.replace("Avatar_", "Body_") + "_Trimmed")
    bpy.context.collection.objects.link(trimmed)
    trimmed.lock_location = (False, False, False)
    trimmed.lock_rotation = (False, False, False)
    trimmed.lock_scale = (False, False, False)

    bmw = trimmed.matrix_world
    dinv = dress.matrix_world.inverted()

    pts = [(bmw @ v.co) for v in trimmed.data.vertices]
    cx = sum(p.x for p in pts) / len(pts)
    cy = sum(p.y for p in pts) / len(pts)

    # Proximity, not ray casting. Rays under-detect badly on a garment that
    # hugs the body: measured 6.4% of faces flagged when 19.5% of vertices sit
    # within 5 mm of the cloth. Nearest-surface distance is direction-agnostic
    # and does not care that the garment is double-walled.
    dmw = dress.matrix_world
    covered = set()
    for i, p in enumerate(pts):
        ok, loc, _n, _i = dress.closest_point_on_mesh(dinv @ p)
        if ok and (p - (dmw @ loc)).length <= reach:
            covered.add(i)

    bm = bmesh.new()
    bm.from_mesh(trimmed.data)
    bm.verts.ensure_lookup_table()

    # Erode: a vertex only counts as safely covered if its neighbours are too,
    # which keeps a living border under every garment edge.
    for _ in range(margin):
        keep = set()
        for idx in covered:
            v = bm.verts[idx]
            if all(e.other_vert(v).index in covered for e in v.link_edges):
                keep.add(idx)
        covered = keep

    doomed = [f for f in bm.faces
              if all(v.index in covered for v in f.verts)]
    removed = len(doomed)
    if doomed:
        bmesh.ops.delete(bm, geom=doomed, context="FACES")
    bm.to_mesh(trimmed.data)
    bm.free()
    trimmed.data.update()

    log("body trim: %d of %d faces removed under the garment -> '%s'"
        % (removed, len(body.data.polygons), trimmed.name))
    return trimmed


def _covered_verts(dress, body, reach):
    """Body vertex indices whose nearest dress surface is within `reach`."""
    dinv = dress.matrix_world.inverted()
    dmw = dress.matrix_world
    bmw = body.matrix_world
    covered = set()
    for i, v in enumerate(body.data.vertices):
        p = bmw @ v.co
        ok, loc, _n, _i = dress.closest_point_on_mesh(dinv @ p)
        if ok and (p - (dmw @ loc)).length <= reach:
            covered.add(i)
    return covered


def _erode(body, covered, rounds):
    """Shrink a vertex set so a border survives along the garment's edges."""
    import bmesh
    if rounds < 1:
        return covered
    bm = bmesh.new()
    bm.from_mesh(body.data)
    bm.verts.ensure_lookup_table()
    for _ in range(rounds):
        keep = set()
        for idx in covered:
            v = bm.verts[idx]
            if all(e.other_vert(v).index in covered for e in v.link_edges):
                keep.add(idx)
        covered = keep
    bm.free()
    return covered


def _fill_tri(img, size, a, b, c):
    """Paint one UV triangle black into the mask. Returns 1 if anything landed."""
    import numpy

    x0 = max(int(min(a[0], b[0], c[0])), 0)
    x1 = min(int(max(a[0], b[0], c[0])) + 1, size)
    y0 = max(int(min(a[1], b[1], c[1])), 0)
    y1 = min(int(max(a[1], b[1], c[1])) + 1, size)
    if x1 <= x0 or y1 <= y0:
        return 0
    den = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
    if abs(den) < 1e-12:
        return 0

    ys, xs = numpy.mgrid[y0:y1, x0:x1]
    sx = xs + 0.5
    sy = ys + 0.5
    w1 = ((b[1] - c[1]) * (sx - c[0]) + (c[0] - b[0]) * (sy - c[1])) / den
    w2 = ((c[1] - a[1]) * (sx - c[0]) + (a[0] - c[0]) * (sy - c[1])) / den
    inside = (w1 >= -0.001) & (w2 >= -0.001) & (w1 + w2 <= 1.001)
    if not inside.any():
        return 0
    block = img[y0:y1, x0:x1]
    block[inside] = 0.0
    img[y0:y1, x0:x1] = block
    return 1


def make_body_mask(dress=None, body=None, size=2048, reach=0.015, margin=2,
                   out_path=None, covered=None):
    """Bake a body-hiding mask into the BODY's UV space.

    BLACK where the garment covers the body, WHITE elsewhere. Drive the body
    material's Opacity Mask with it (Blend Mode: Masked) and one body works
    under any outfit -- unlike trim_body_under(), which cuts geometry and so
    ties that body to a single garment.

    The MetaHuman body lives entirely in UDIM tile 1002 (U 1-2, V 0-1), so U
    is shifted down by one to land in image space. Name the file <...>.1002.png
    if you want Unreal to treat it as a UDIM.
    """
    import numpy
    import bmesh

    dress = dress or bpy.data.objects.get("Dress")
    body = body or _find_body(exclude=dress)
    if dress is None or body is None:
        raise RuntimeError("need both a dress and a body")
    if out_path is None:
        out_path = os.path.join(PROJECT, "Tools", "Export", "BodyMasks",
                                "%s_mask.1002.png" % body.name)

    if covered is None:
        covered = _covered_verts(dress, body, reach)
    covered = _erode(body, covered, margin)
    log("mask: %d of %d body verts under cloth"
        % (len(covered), len(body.data.vertices)))

    img = numpy.ones((size, size), dtype=numpy.float32)

    bm = bmesh.new()
    bm.from_mesh(body.data)
    uvl = bm.loops.layers.uv.active
    if uvl is None:
        bm.free()
        raise RuntimeError("body has no UVs to bake into")

    painted = 0
    for face in bm.faces:
        if not all(v.index in covered for v in face.verts):
            continue
        pix = []
        for loop in face.loops:
            uv = loop[uvl].uv
            pix.append(((uv.x - 1.0) * size, uv.y * size))   # UDIM 1002 -> 0-1
        # fan-triangulate, then fill each triangle. Barycentric coordinates are
        # evaluated with numpy over the triangle's bounding box -- a per-pixel
        # Python loop is minutes per character at 2048.
        for k in range(1, len(pix) - 1):
            painted += _fill_tri(img, size, pix[0], pix[k], pix[k + 1])
    bm.free()

    out_dir = os.path.dirname(out_path)
    if out_dir and not os.path.isdir(out_dir):
        os.makedirs(out_dir)

    image = bpy.data.images.new("BodyMask", width=size, height=size, alpha=False)
    flat = numpy.repeat(img.reshape(-1, 1), 4, axis=1)
    flat[:, 3] = 1.0
    image.pixels.foreach_set(flat.ravel())
    image.filepath_raw = out_path
    image.file_format = "PNG"
    image.save()

    hidden = float((img == 0.0).sum()) / (size * size)
    log("mask: %d triangles painted, %.1f%% of the texture hidden -> %s"
        % (painted, hidden * 100.0, out_path))
    return out_path


def make_union_mask(character, size=2048, reach=0.015, margin=2,
                    work_dir=None, out_dir=None):
    """One mask per character, covering EVERY variant that character has.

    A mask baked from a single dress leaves skin showing under a different
    sleeve or strap. Unioning the coverage of all four variants gives one
    texture that is safe under any of them -- at the cost of hiding slightly
    more skin than the smallest outfit strictly needs.

    Vertex indices are comparable across variants because every variant of a
    character is cut from the same shell, hence the same body mesh.
    """
    import glob

    work = work_dir or os.path.join(BLEND_DIR, "Work", "Variants")
    out_dir = out_dir or os.path.join(PROJECT, "Tools", "Export", "BodyMasks")
    paths = sorted(glob.glob(os.path.join(work, "%s_*_court.blend" % character)))
    if not paths:
        log("no variants for %s" % character)
        return None

    union = set()
    for path in paths:
        bpy.ops.wm.open_mainfile(filepath=path)
        dress = bpy.data.objects.get("Dress")
        body = _find_body(exclude=dress)
        if dress is None or body is None:
            continue
        got = _covered_verts(dress, body, reach)
        union |= got
        log("   %-22s %5d verts (union now %d)"
            % (os.path.basename(path).replace("_court.blend", "")
               .replace(character + "_", ""), len(got), len(union)))

    if not union:
        log("no coverage found for %s" % character)
        return None

    # Bake from the last opened file -- same body mesh, so indices still apply.
    dress = bpy.data.objects.get("Dress")
    body = _find_body(exclude=dress)
    out = os.path.join(out_dir, "%s_mask.1002.png" % character)
    return make_body_mask(dress=dress, body=body, size=size, reach=reach,
                          margin=margin, out_path=out, covered=union)


def masks_all(only=None, size=2048, **kwargs):
    """Union mask for every character that has variants."""
    import glob
    work = os.path.join(BLEND_DIR, "Work", "Variants")
    chars = sorted({os.path.basename(p).rsplit("_court.blend", 1)[0]
                    .rsplit("_", 2)[0]
                    for p in glob.glob(os.path.join(work, "*_court.blend"))})
    if only:
        wanted = [o.strip().lower() for o in only if o.strip()]
        chars = [c for c in chars if any(w in c.lower() for w in wanted)]
    done = []
    for c in chars:
        log("")
        log("=== %s ===" % c)
        try:
            out = make_union_mask(c, size=size, **kwargs)
            if out:
                done.append(c)
        except Exception as exc:
            log("FAILED %s: %s" % (c, exc))
    log("")
    log("union masks written: %d/%d" % (len(done), len(chars)))
    return done


def export_all(only=None, work_dir=None, out_dir=None, suffix="_court.blend"):
    """Export every finished garment to FBX, ready for import_garments.py."""
    work = work_dir or os.path.join(BLEND_DIR, "Work", "Variants")
    out_dir = out_dir or os.path.join(PROJECT, "Tools", "Export", "Garments")
    if not os.path.isdir(out_dir):
        os.makedirs(out_dir)

    blends = sorted(f for f in os.listdir(work) if f.endswith(suffix))
    if only:
        wanted = [o.strip().lower() for o in only if o.strip()]
        blends = [b for b in blends if any(w in b.lower() for w in wanted)]

    done, failed = [], []
    for fname in blends:
        tag = fname.replace(suffix, "")
        out = os.path.join(out_dir, "Dress_%s.fbx" % tag)
        if os.path.isfile(out):
            log("SKIP %s" % tag)
            continue
        try:
            bpy.ops.wm.open_mainfile(filepath=os.path.join(work, fname))
            dress = bpy.data.objects.get("Dress")
            if dress is None:
                failed.append(tag)
                continue
            export_garment(out, garment=dress)
            done.append(tag)
        except Exception as exc:
            log("FAILED %s: %s" % (tag, exc))
            failed.append(tag)

    log("")
    log("exported %d, failed %d -> %s" % (len(done), len(failed), out_dir))
    for tag in failed:
        log("   FAILED %s" % tag)
    return done


# ---------------------------------------------------------- variant matrix ---

STRAP_WIDTHS = {"thin": 2, "broad": 6}
SLEEVE_STYLES = ("fitted", "hanging")


def make_variant(shell_path, out_path, sleeve="fitted", strap="thin",
                 trim=True, **court_kwargs):
    """One complete dress. A thin wrapper over make_court_dress, which now owns
    the whole sequence -- there is no second code path to drift out of step."""
    bpy.ops.wm.open_mainfile(filepath=shell_path)
    if bpy.data.objects.get("Dress") is None:
        raise RuntimeError("no Dress in %s" % shell_path)

    name = os.path.basename(out_path).replace("_court.blend", "")
    make_court_dress(sleeve=sleeve, strap=strap, trim=trim, uvs=True,
                     name=name, **court_kwargs)
    bpy.ops.wm.save_as_mainfile(filepath=out_path)
    return bpy.data.objects.get("Dress")


def variants_all(only=None, work_dir=None, styles=None, trim=True,
                 **court_kwargs):
    """Build every sleeve x strap combination for every dress shell."""
    work = work_dir or os.path.join(BLEND_DIR, "Work")
    out_dir = os.path.join(work, "Variants")
    if not os.path.isdir(out_dir):
        os.makedirs(out_dir)

    shells = sorted(f for f in os.listdir(work) if f.endswith("_dress.blend"))
    if only:
        wanted = [o.strip().lower() for o in only if o.strip()]
        shells = [s for s in shells if any(w in s.lower() for w in wanted)]

    combos = styles or [(sl, st) for sl in SLEEVE_STYLES
                        for st in STRAP_WIDTHS]
    done, failed = [], []
    for fname in shells:
        character = fname.replace("_dress.blend", "")
        for sleeve, strap in combos:
            tag = "%s_%s_%s" % (character, sleeve, strap)
            out = os.path.join(out_dir, tag + "_court.blend")
            if os.path.isfile(out):
                log("SKIP %s" % tag)
                continue
            log("")
            log("=== %s ===" % tag)
            try:
                make_variant(os.path.join(work, fname), out,
                             sleeve=sleeve, strap=strap, trim=trim,
                             **court_kwargs)
                done.append(tag)
                log("saved %s" % out)
            except Exception as exc:
                log("FAILED %s: %s" % (tag, exc))
                failed.append(tag)

    log("")
    log("=" * 60)
    log("variants built %d, failed %d" % (len(done), len(failed)))
    for tag in failed:
        log("   FAILED %s" % tag)
    log("=" * 60)
    return done


# ------------------------------------------------------------------- cli ---

# ----------------------------------------------------- male court attire ---
#
# Everything above makes dresses, and dress_all filters on "Female", so the
# male half of every kith has nothing to wear to court. A coat and breeches are
# the same two operations a dress already uses, pointed at different bands of
# the body:
#
#   coat      collar down to just above the crotch, then a SHORT extruded
#             skirt -- the same cone as a dress, stopped at the knee rather
#             than the floor, which is what make_skirt's `drop` is for.
#   breeches  waist down to below the knee, cut deliberately BELOW the leg
#             merge so the shell comes out as two leg tubes. make_dress_shell
#             warns about that case because two hems ruin a skirt; for
#             breeches it is the entire point.
#
# Boggans are short, stout and house-proud rather than grand, so the defaults
# are a long full-skirted coat over full knee breeches: respectable, superbly
# made, and a century out of fashion. That reads as a Boggan at court far
# better than a slim silhouette would, and it flatters a body that is wider
# than it is tall in a way a fitted coat does not.
#
# Heights come off the skeleton, not off fixed fractions of body height. A
# Boggan childling and a Sidhe grump do not keep their knees in the same place
# proportionally, and a breeches hem guessed at 0.25 of height lands on the
# shin of one and the thigh of another.


def _bone_fraction(body, bone_name, default=None):
    """Height fraction (0 = lowest vertex, 1 = highest) of a bone's head."""
    arm = body.find_armature()
    if arm is None:
        return default
    bone = arm.data.bones.get(bone_name)
    if bone is None:
        return default
    zs = [(body.matrix_world @ v.co).z for v in body.data.vertices]
    zmin, zmax = min(zs), max(zs)
    span = zmax - zmin
    if span <= 0:
        return default
    z = (arm.matrix_world @ bone.head_local).z
    return (z - zmin) / span


def _height_of(body):
    zs = [(body.matrix_world @ v.co).z for v in body.data.vertices]
    return min(zs), max(zs), max(zs) - min(zs)


def hang_loose(coat, from_z, to_z=None, body=None, bins_z=26, bins_a=36,
               rounds=3, ease=0.020, most=0.014):
    """Let the coat hang off the body instead of wrapping it.

    A clearance pass pushes every too-close vertex to exactly `offset` from
    the nearest skin, and the surface that produces is an OFFSET SURFACE: it
    reproduces the anatomy faithfully, 42mm out. On the seat that means the
    coat models both buttocks and the cleft between them, which is why the
    back read as body paint however much clearance it was given -- more
    clearance only moves the same shape further out.

    Cloth does not do that. It hangs from the high points and spans the
    hollows between them. So below the waist the coat is rewritten as a
    smooth tube: every vertex becomes (height, angle, radius) about the
    slice's own centre, the radius is taken per cell as the enclosing
    maximum and then smoothed across the grid, and the vertex moves back out
    to it. A radius that varies smoothly with height and angle cannot have a
    cleft in it.

    This is the technique that failed on the boot, and it failed there for a
    reason that does not apply here: the last's axis ran heel to ball, the
    toes carried on past it, and the far cells extrapolated into a hollow. A
    skirt is a tube about the vertical axis with nothing hanging off the end.

    OUTWARD ONLY -- AND MEASURED AGAINST THE BODY, NOT THE AXIS.
    #
    Refusing to shrink the radius is not enough, and assuming it was cost a
    build: penetration went 2.4% to 3.2%. Outward from the slice's axis is
    not the same as away from the skin. In a slice at thigh height the legs
    sit either side of the centre, so a vertex hanging between them moves
    radially "out" and straight into a thigh.

    So every move is checked against the body it has to clear, and one that
    would end up closer to the skin than it started is dropped. That is the
    only test that means anything here.
    """
    import math
    from mathutils import Vector

    mw = coat.matrix_world
    inv = mw.inverted()

    # A BAND AT THE SEAT, NOT THE WHOLE SKIRT.
    #
    # The fault is the cleft, and the cleft is at the seat. Below that the
    # skirt has already left the legs and hangs free, so easing it outward
    # does not smooth anything -- it just adds girth, and the coat barrels
    # into a shelf at the hip that looks worse than the seat ever did.
    # Treating the whole skirt was measurably wrong: a mean move of 24mm
    # applied from waist to hem.
    if to_z is None:
        to_z = from_z - 0.150

    picked = []
    for vert in coat.data.vertices:
        world = mw @ vert.co
        if world.z >= from_z or world.z < to_z:
            continue
        picked.append((vert, world))
    if len(picked) < 64:
        log("   hang: %d vert(s) below %.3f -- skipped" % (len(picked), from_z))
        return 0

    z_lo = min(w.z for _v, w in picked)
    z_hi = max(w.z for _v, w in picked)
    if z_hi - z_lo < 1e-5:
        return 0

    def z_bin(z):
        i = int((z - z_lo) / (z_hi - z_lo) * (bins_z - 1) + 0.5)
        return min(max(i, 0), bins_z - 1)

    # Each slice about its OWN centre: the coat leans forward down its
    # length, and a single axis would read that lean as extra radius at the
    # back and none at the front.
    sums = [[0.0, 0.0, 0] for _ in range(bins_z)]
    for _vert, world in picked:
        cell = sums[z_bin(world.z)]
        cell[0] += world.x
        cell[1] += world.y
        cell[2] += 1
    centres = []
    for total_x, total_y, count in sums:
        centres.append((total_x / count, total_y / count) if count else None)
    for index in range(bins_z):
        if centres[index] is None:
            near = [c for c in centres if c is not None]
            centres[index] = near[0] if near else (0.0, 0.0)

    grid = [[0.0] * bins_a for _ in range(bins_z)]
    seen = [[False] * bins_a for _ in range(bins_z)]
    polar = []
    for vert, world in picked:
        i = z_bin(world.z)
        cx, cy = centres[i]
        dx, dy = world.x - cx, world.y - cy
        radius = math.hypot(dx, dy)
        angle = math.atan2(dy, dx)
        j = int((angle + math.pi) / (2.0 * math.pi) * bins_a) % bins_a
        # Keep the FRACTIONAL cell as well: reading the grid at the nearest
        # cell makes every vertex in a band take one radius, and the step to
        # the next band shows as a ridge. The first build of this banded the
        # whole seat horizontally for exactly that reason.
        fi = (world.z - z_lo) / (z_hi - z_lo) * (bins_z - 1)
        fj = (angle + math.pi) / (2.0 * math.pi) * bins_a
        polar.append((vert, world, i, j, radius, angle, fi, fj))
        if radius > grid[i][j]:
            grid[i][j] = radius
        seen[i][j] = True

    for j in range(bins_a):
        last = None
        for i in range(bins_z):
            if seen[i][j]:
                last = grid[i][j]
            elif last is not None:
                grid[i][j] = last

    # SMOOTH AROUND THE SLICE, BARELY ALONG IT.
    #
    # The cleft is an ANGULAR feature: at a given height it is a notch at one
    # bearing, with the buttocks either side of it. Smoothing equally in both
    # directions fixes it, but it also carries the widest band -- the hip --
    # up and down the skirt, and the coat barrels out into a shelf that looks
    # worse than the seat did. Spreading the correction the way the fault
    # runs keeps the silhouette.
    for _ in range(rounds):
        nxt = [row[:] for row in grid]
        for i in range(bins_z):
            for j in range(bins_a):
                i0, i1 = max(i - 1, 0), min(i + 1, bins_z - 1)
                j0, j1 = (j - 1) % bins_a, (j + 1) % bins_a
                nxt[i][j] = (grid[i][j] * 0.40
                             + (grid[i0][j] + grid[i1][j]) * 0.06
                             + (grid[i][j0] + grid[i][j1]) * 0.24)
        grid = nxt

    gap_to_body = None
    if body is not None:
        bmw = body.matrix_world
        binv = bmw.inverted()

        def gap_to_body(point):
            ok, loc, nor, _ = body.closest_point_on_mesh(binv @ point)
            if not ok:
                return None
            surface = bmw @ loc
            normal = (bmw.to_3x3() @ nor).normalized()
            return (point - surface).dot(normal)

    moved = 0
    total = 0.0
    def sample(fi, fj):
        """The smoothed radius, interpolated between cells rather than
        snapped to one, so the surface has no steps in it."""
        i0 = int(math.floor(fi))
        ti = fi - i0
        i0 = min(max(i0, 0), bins_z - 1)
        i1 = min(i0 + 1, bins_z - 1)
        j0 = int(math.floor(fj))
        tj = fj - j0
        j0 %= bins_a
        j1 = (j0 + 1) % bins_a
        top = grid[i0][j0] * (1.0 - tj) + grid[i0][j1] * tj
        bot = grid[i1][j0] * (1.0 - tj) + grid[i1][j1] * tj
        return top * (1.0 - ti) + bot * ti

    for vert, world, i, j, radius, angle, fi, fj in polar:
        want = sample(fi, fj)
        # Fade at BOTH ends, so neither the waist nor the point where the
        # band stops shows as a step in the cloth.
        high = (world.z - (from_z - ease)) / max(ease, 1e-6)
        low = ((to_z + ease) - world.z) / max(ease, 1e-6)
        weight = 1.0
        if high > 0.0:
            weight = min(weight, max(0.0, 1.0 - high))
        if low > 0.0:
            weight = min(weight, max(0.0, 1.0 - low))
        want = radius + (want - radius) * weight
        # A HOLLOW IS LOCAL; CAP THE FILL.
        #
        # Smoothing a maximum over many rounds makes each ring near-circular
        # at its WIDEST radius, and the mean move ran to 30mm -- the coat
        # stopped following the seat and started standing off it as a shelf.
        # The cleft spans two or three bearings out of thirty-six, so a few
        # rounds reach it and a cap keeps everything else where the drape
        # left it.
        if want > radius + most:
            want = radius + most
        if want <= radius + 1e-6:
            continue                      # never draw the coat in
        cx, cy = centres[i]
        target = Vector((cx + math.cos(angle) * want,
                         cy + math.sin(angle) * want,
                         world.z))
        if gap_to_body is not None:
            was = gap_to_body(world)
            now = gap_to_body(target)
            if was is not None and now is not None and now < was - 1e-6:
                continue        # radially out, but into a leg
        vert.co = inv @ target
        total += want - radius
        moved += 1

    coat.data.update()
    log("   hang: %d of %d vert(s) eased out below %.3f (mean %.1fmm)"
        % (moved, len(picked), from_z, 1000.0 * total / max(moved, 1)))
    return moved


def lift_slots(obj, slots, lift=0.006, body=None):
    """Stand the trim and facing proud of the cloth they sit on.

    Both are built with relief -- the trim bands are inset and pushed out
    4mm, the lapels roll 12mm -- and both arrive flat, because every pass
    after them re-conforms the whole coat to one distance from the body.
    Measured on the finished garment: trim kept 1.9mm of its 4mm and facing
    1.3mm of its 12mm, and a tenth of both sat 20-28mm BEHIND the base
    cloth, which is the poke-through.

    So the relief is re-applied at the end, where nothing will flatten it.

    Only vertices whose faces are ALL in these slots move. A vertex on the
    border belongs to the cloth as much as to the band, and moving it would
    drag the cloth up with the trim instead of leaving a step; held still,
    it becomes the band's side wall.

    AWAY FROM THE BODY, NOT ALONG THE NORMAL. A lapel is folded, so its
    underside's normals point back at the coat, and lifting along them drove
    those faces further in -- measured, raising the lift from 6mm to 14mm
    moved the facing's gap the WRONG WAY, 36.9mm to 34.8mm. Taking the
    direction from the skin instead, every lifted vertex is guaranteed to
    end up further out than it started, whichever way its face happens to
    point.
    """
    import bmesh
    from mathutils import Vector

    # LIFT IS IN METRES; vert.co IS IN LOCAL UNITS.
    #
    # The coat carries the FBX's 0.01 scale, so one local unit is 10mm of
    # world and a 6mm lift applied raw moves 0.06mm. It logged 10898
    # vertices raised and the finished mesh measured byte-identical:
    # relief +1.3mm and +1.9mm, exactly as before. Same conversion
    # _clear_of_body needs, and the same mistake that made 42mm of coat
    # clearance 0.42mm earlier.
    matrix = obj.matrix_world
    step = (matrix @ Vector((1.0, 0.0, 0.0))) - (matrix @ Vector((0.0, 0.0, 0.0)))
    per_metre = 1.0 / step.length if step.length > 1e-9 else 1.0
    local_lift = lift * per_metre

    body_mw = body.matrix_world if body is not None else None
    body_inv = body_mw.inverted() if body_mw is not None else None

    targets = set(slots)
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.verts.ensure_lookup_table()
    bm.normal_update()

    moved = 0
    for vert in bm.verts:
        if not vert.link_faces:
            continue
        if any(f.material_index not in targets for f in vert.link_faces):
            continue
        out = vert.normal
        if body is not None:
            world = matrix @ vert.co
            ok, loc, _n, _i = body.closest_point_on_mesh(body_inv @ world)
            if ok:
                away = world - (body_mw @ loc)
                if away.length > 1e-6:
                    out = (matrix.inverted().to_3x3()
                           @ away.normalized()).normalized()
        vert.co = vert.co + out * local_lift
        moved += 1

    bm.normal_update()
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()
    log("   lift: %d vert(s) raised %.0fmm (%.4f local) in slot(s) %s"
        % (moved, lift * 1000, local_lift, sorted(targets)))
    return moved


def make_coat(body=None, collar=0.99, skirt_to=None, flare=1.06, passes=3,
              sleeve="fitted", strap="broad", trim=True, offset=0.010,
              thickness=0.007, loops=1, do_bind=True, name=None):
    """A full-skirted court coat: shell, coat skirt, sleeves, trim.

    skirt_to is the height fraction the coat hem reaches; None puts it just
    below the knee, which is where a frock coat belongs.

    COLLAR MUST CLEAR THE SHOULDER. The MetaHuman body mesh is headless, so
    its top is the neck stump and the shoulder sits only centimetres below it:
    on the Boggan male, body top 1.310 and upperarm_l 1.252. At the old
    default of 0.93 the shell was cut at z 1.218 -- BELOW the shoulder -- so
    there was no shoulder in the garment and no armhole either, just a side
    opening at chest height. add_sleeves extruded from that, outward and
    backwards, producing flat panels 15 cm behind the arm that touched none of
    upperarm, elbow or hand. Measured against the arm bones:

        collar 0.93   upperarm  0   elbow  0   hand  0
        collar 0.97   upperarm  0   elbow 34   hand 39
        collar 0.99   upperarm 19   elbow 34   hand 37

    0.99 puts the cut above the shoulder, which is what makes the opening a
    real armhole and the sleeve a tube down the arm.
    """
    body = body or _find_body(exclude=None)
    if body is None:
        raise RuntimeError("no avatar mesh in the scene -- open a prepped .blend")

    zmin, _, span = _height_of(body)

    # Just above the crotch: below it the shell splits into leg tubes and the
    # coat skirt has no single ring to extrude from.
    merge = find_leg_merge(body)
    shell_bottom = (merge + 0.03) if merge is not None else 0.52

    if skirt_to is None:
        knee = _bone_fraction(body, "calf_l", default=0.28)
        skirt_to = max(0.10, knee - 0.02)
    log("coat: shell %.3f..%.3f, skirt hem at %.3f (knee %.3f)"
        % (shell_bottom, collar, skirt_to,
           _bone_fraction(body, "calf_l", default=-1.0)))

    coat = make_dress_shell(top=collar, bottom=shell_bottom, body=body,
                            name="Coat")

    # Before anything is attached to it: the shell is a copy of the body and
    # has to stop being one, or every later step decorates a nude.
    drape_shell(dress=coat, body=body)

    # And before the collar: a neck-shaped hole, not the horizontal plane cut
    # make_dress_shell leaves, which on this neckless body is a flat edge
    # shoulder to shoulder.
    _face = bpy.data.objects.get("Face")
    _report_loops("before neck cut", coat)
    cut_neck_hole(coat, body, face=_face)
    _report_loops("after neck cut", coat)

    # Sleeves BEFORE thickness, which seals every opening.
    #
    # Straps first, exactly as a gown needs them, and for a reason that is not
    # obvious on a garment that plainly has shoulders: strip_arms drops every
    # vertex whose dominant weight is an arm bone, and on these bodies that
    # takes the whole shoulder cap with it. Neckline and both armholes then run
    # together into ONE opening across the top, so add_sleeves counts 2
    # boundary loops where it needs 4 and refuses. add_straps bridges the
    # shoulders and splits that single hole back into neck + 2 armholes.
    #
    # Broad by default: this is a coat, not an evening gown, and a 2-vertex
    # strap on a Boggan reads as braces rather than a shoulder.
    if sleeve:
        fill_small_holes(coat)
        _report_loops("after fill_small_holes", coat)
        # Straps were a WORKAROUND for the missing shoulder cap described
        # above: with the deltoid stripped, neckline and both armholes ran
        # together into one opening and had to be bridged back apart. The
        # shell now keeps the cap, so the coat already has shoulders and real
        # armholes -- bridging them again only welds the armhole shut.
        loops = _loop_count(coat)
        if loops < 4:
            log("coat: %d boundary loop(s) -- bridging shoulders" % loops)
            add_straps(dress=coat, body=body,
                       width=STRAP_WIDTHS.get(strap, 6))
            _report_loops("after add_straps", coat)
        else:
            log("coat: %d boundary loop(s) -- shoulders already closed" % loops)
        add_sleeves(dress=coat, body=body, style=sleeve)
        _report_loops("after add_sleeves", coat)
        bpy.ops.object.select_all(action="DESELECT")
        coat.select_set(True)
        bpy.context.view_layer.objects.active = coat

    split_z = min((coat.matrix_world @ v.co).z for v in coat.data.vertices)
    drop = split_z - (zmin + span * skirt_to)
    if drop > 0:
        make_skirt(dress=coat, body=body, passes=passes, flare=flare,
                   fold=False, drop=drop)
        _report_loops("after make_skirt", coat)
        add_skirt_loops(dress=coat, split_z=split_z, cuts=loops)
    else:
        log("coat: shell already reaches %.3f -- no skirt extruded" % skirt_to)

    # BEFORE thickness and BEFORE bind. Solidify seals every boundary, so a
    # collar extruded afterwards has no neckline to grow from; and anything
    # added after bind() -- collar, buttons -- carries no weights at all and
    # stays behind when the character moves.
    tailor_frock(coat=coat, body=body)

    # CLOSER ON THE ARMS THAN ON THE BODY.
    #
    # The coat wants 42mm off the torso and nothing like that off a wrist,
    # where the arm is 32mm thick and the same figure makes a sack. Measured:
    # the sleeve clamp sized the last ring to 38mm and the clearance pass
    # then pushed it to 74mm, which is the whole reason the sleeves read as a
    # dressing gown however they were fitted.
    _arm_segs = []
    _a = _armature_for(body)
    if _a is not None:
        for _pair in (("upperarm_l", "lowerarm_l"), ("lowerarm_l", "hand_l"),
                      ("upperarm_r", "lowerarm_r"), ("lowerarm_r", "hand_r")):
            _b0 = _a.data.bones.get(_pair[0])
            _b1 = _a.data.bones.get(_pair[1])
            if _b0 is not None and _b1 is not None:
                _arm_segs.append(((_a.matrix_world @ _b0.head_local),
                                  (_a.matrix_world @ _b1.head_local)))

    import math
    _nx, _ny, _nr = neck_axis(body, bpy.data.objects.get("Face"))
    _collar_floor = max((body.matrix_world @ v.co).z
                        for v in body.data.vertices) + 0.004
    _shoulder_from = _collar_floor - 0.060

    def _sleeve_offset(world, _segs=_arm_segs, _wide=offset):
        """Full clearance on the body, a sleeve's worth down the arms.

        And a collar's worth at the neck. The collar rings converge on a
        radius just clear of the throat, and then this pass used to push
        every one of them to 42mm off it -- measured, the collar stood 85mm
        off a 73mm neck, which is a funnel. Same fault as the cuff, same
        cure.

        Tightening by distance from the arm's AXIS alone was not enough: the
        shoulder is within a sleeve's reach of the upperarm bone, so the coat
        was drawn to 14mm there -- inside the waistcoat, which sits at 18mm --
        and the waistcoat punched through both shoulders.

        So it ramps ALONG the arm as well. At the shoulder the coat keeps its
        full clearance, because up there it is still a coat over a waistcoat;
        by the elbow it is a sleeve.
        """
        # The neck first: above the shoulders and near the throat's axis,
        # this is collar, whatever else it might be close to.
        # The same tight zone the body target uses. Drawn at _nr * 2.4 this
        # was a 180mm cylinder over the whole upper chest, and it dropped the
        # coat to 12mm off the FACE across all of it -- the face mesh carries
        # the chest as well as the neck, so penetration held at 6.2% even
        # after the body's zone was narrowed.
        if world.z >= _collar_floor:
            reach = math.hypot(world.x - _nx, world.y - _ny)
            if reach <= 0.100:
                return 0.012

        # THE SHOULDER IS NOT THE SKIRT EITHER.
        #
        # 42mm over the shoulders puts a bolster round the neck: measured,
        # the cloth stood at 126-150mm radius and rose to within 10mm of the
        # collar's own top, so the collar had nothing to stand proud of and
        # the throat read bare whatever the rings did. A coat sits close on
        # the shoulder; the ease belongs lower down, where the body moves.
        #
        # Safe against the waistcoat, which stops around z 1.09 -- well below
        # where this begins.
        if world.z >= _shoulder_from:
            climb = min(1.0, (world.z - _shoulder_from) / 0.055)
            return _wide + (0.015 - _wide) * climb

        if not _segs:
            return _wide
        best = None
        for index, (start, end) in enumerate(_segs):
            span = end - start
            length = span.length
            if length < 1e-6:
                continue
            t = max(0.0, min(1.0, (world - start).dot(span) / (length * length)))
            gap = (world - (start + span * t)).length
            # 0 at the shoulder, 1 at the hand. Segments come in pairs per
            # side: upperarm then lowerarm.
            down = ((index % 2) + t) * 0.5
            if best is None or gap < best[0]:
                best = (gap, down)
        if best is None:
            return _wide
        gap, down = best

        near, far = 0.075, 0.150
        if gap >= far:
            return _wide
        # How far down the arm before the sleeve may close in at all.
        start_at, full_at = 0.22, 0.60
        if down <= start_at:
            return _wide
        ramp = min(1.0, (down - start_at) / (full_at - start_at))
        tight = _wide + (0.014 - _wide) * ramp
        if gap <= near:
            return tight
        ride = (gap - near) / (far - near)
        return tight + (_wide - tight) * ride

    _shoulder_top = max((body.matrix_world @ v.co).z
                        for v in body.data.vertices)

    def _neck_zone(world, _floor=_shoulder_top + 0.004, _x=_nx, _y=_ny):
        """Where the collar lives, and NOWHERE ELSE.

        Drawn generously -- a 120mm cylinder starting below the shoulders --
        this also covered the coat's upper chest and shoulders. Those then
        never got cleared against the body at all and sank into it:
        penetration went 0.53% to 7.67%. Skipping a target is not free; it
        means that cloth is trusted to be where the drape left it, and over
        the chest the drape left it on the skin.

        So: strictly above the shoulder cap, and no wider than the collar.
        """
        if world.z < _floor:
            return False
        return math.hypot(world.x - _x, world.y - _y) <= 0.100

    add_thickness(dress=coat, body=body, offset=offset, thickness=thickness,
                  bodice_above_z=split_z, offset_fn=_sleeve_offset,
                  skip_body=_neck_zone)
    _report_penetration("coat: after thickness", coat, body)

    for _ in range(4):
        if not _repair_outliers(coat, factor=4.0):
            break
    _report_penetration("coat: after repair", coat, body)

    # AFTER the clearance, or it is simply undone.
    #
    # add_thickness clears, solidifies and clears again, and each of those
    # passes re-conforms the coat to the body. Anything that smooths the
    # garment has to come after them -- the collar spent this whole session
    # being tuned before a stage that overrode it, and looked wrong every
    # time for that reason and no other.
    #
    # Before bind, though, so the weights transfer from the shape that will
    # actually be worn.
    _arm = _armature_for(body)
    _waist = None
    if _arm is not None:
        _bone = (_arm.data.bones.get("spine_03")
                 or _arm.data.bones.get("pelvis"))
        if _bone is not None:
            _waist = (_arm.matrix_world @ _bone.head_local).z
    if _waist is None:
        _pts = [(coat.matrix_world @ v.co).z for v in coat.data.vertices]
        _waist = min(_pts) + (max(_pts) - min(_pts)) * 0.55
    hang_loose(coat, from_z=_waist, body=body)
    _report_penetration("coat: after hang", coat, body)


    if do_bind:
        bind(garment=coat, body=body)
        _report_penetration("coat: after bind", coat, body)
        # The coat skirt swings from the waist; weighting it to the thighs
        # would make it clip through itself every stride.
        skirt_weights(dress=coat, split_z=split_z)
        if sleeve:
            sleeve_weights(dress=coat, body=body)
        base_weights(dress=coat, bone="pelvis")

    if trim:
        where = ("hem", "neck", "wrist") if sleeve else ("hem", "neck")
        add_trim_bands(dress=coat, name=name or coat.name, where=where)
        for _ in range(4):
            if not _repair_outliers(coat, factor=4.0):
                break
        _report_penetration("coat: after trim", coat, body)

        # Trim bands raise faces off the surface and push some of them the
        # wrong way: measured, the coat goes 0.7% inside to 3.7% across this
        # step alone. Clear it again, bodice only, so the skirt keeps hanging
        # free.
        again = _clear_of_body(coat, body, offset, only_above_z=split_z,
                               above=bpy.data.objects.get("Face"),
                               offset_fn=_sleeve_offset,
                               skip_body=_neck_zone)
        log("   re-cleared %d verts after trim" % again)
        _report_penetration("coat: after re-clear", coat, body)

    # LAST OF ALL.
    #
    # Placed before the trim bands and the re-clear, the lift measured
    # exactly nothing: facing relief read +1.2mm against +1.3mm without it.
    # The re-clear runs after the trim precisely BECAUSE the trim pushes
    # faces the wrong way, so it flattens the relief it was called in to
    # fix, and anything raised before it goes the same way.
    #
    # After it, both slots exist and nothing follows.
    _raise = [i for i, m in enumerate(coat.data.materials)
              if m and any(k in m.name
                           for k in ("Trim", "Facing", "Lapel"))]
    if _raise:
        # 14mm, because the braid does not start level with the cloth --
        # it starts UNDER it. Measured before any lift, trim sat at a 34.3mm
        # gap against the base coat's 41.7mm, so the first 7mm only brings it
        # back to the surface and everything after that is the relief. At 6mm
        # the trim still measured -0.3mm against the base, which is the
        # poke-through: the cloth was in front of the braid, not behind it.
        lift_slots(coat, _raise, lift=0.010, body=body)
        _report_penetration("coat: after lift", coat, body)
    return coat


def _forward_of(body):
    """Which way the character faces, from its own feet.

    Taken from foot to ball of the foot: toes point forward on every rig, and
    guessing an axis instead gets lapels put on somebody's back.
    """
    from mathutils import Vector
    arm = _armature_for(body)
    if arm is not None:
        foot = arm.data.bones.get("foot_l")
        ball = arm.data.bones.get("ball_l")
        if foot is not None and ball is not None:
            step = (arm.matrix_world @ ball.head_local) -                    (arm.matrix_world @ foot.head_local)
            step.z = 0.0
            if step.length > 1e-4:
                return step.normalized()
    return Vector((0.0, -1.0, 0.0))


def tailor_coat(coat=None, body=None, collar_h=0.048, lapel_drop=0.30,
                lapel_out=0.040, rows=6, button_r=0.014, spread=0.044,
                open_from=0.46, trim_slot=None):
    """Cut the coat into a double-breasted military frock: stand collar,
    folded lapels, two rows of buttons, and a skirt open at the front.

    Sized boldly on purpose. At a childling's scale -- a 1.1m figure whose
    whole chest is 30cm across -- a 22mm lapel fold and 8mm buttons vanish;
    they were in the mesh and read as nothing. The reference coat carries
    its collar, lapels and buttons as the strongest shapes in the
    silhouette, so they are cut to be seen.

    Everything is placed from the body's own landmarks and its facing, so it
    lands the same way on a Boggan grump as on a childling.
    """
    import bmesh
    from mathutils import Vector

    coat = coat or bpy.context.active_object
    body = body or _find_body(exclude=coat)
    forward = _forward_of(body)
    side = Vector((-forward.y, forward.x, 0.0))

    mw = coat.matrix_world
    inv = mw.inverted()
    pts = [mw @ v.co for v in coat.data.vertices]
    z_lo, z_hi = min(p.z for p in pts), max(p.z for p in pts)
    span = max(z_hi - z_lo, 1e-6)
    centre_x = sum(p.x for p in pts) / len(pts)
    centre_y = sum(p.y for p in pts) / len(pts)
    axis = Vector((centre_x, centre_y, 0.0))

    made = []

    # --- stand collar: extrude the neckline straight up -------------------
    bm = bmesh.new()
    bm.from_mesh(coat.data)
    loops = _boundary_loops(bm)
    log("   collar: %d boundary loop(s)" % len(loops))
    for _i, _c in enumerate(loops):
        _z = sum((mw @ v.co).z for v in _c) / len(_c)
        log("      loop %d: %3d verts, mean z %.4f" % (_i, len(_c), _z))
    if loops:
        neck = max(loops, key=lambda c: sum((mw @ v.co).z for v in c) / len(c))
        edges = [e for e in bm.edges
                 if e.is_boundary and all(v in set(neck) for v in e.verts)]
        log("   collar: chose a %d-vert loop, %d boundary edge(s)"
            % (len(neck), len(edges)))
        if edges:
            ret = bmesh.ops.extrude_edge_only(bm, edges=edges)
            new_v = [g for g in ret["geom"] if isinstance(g, bmesh.types.BMVert)]
            lift = inv.to_3x3() @ Vector((0.0, 0.0, collar_h))
            for v in new_v:
                v.co += lift
            made.append("collar")
    bm.normal_update()
    bm.to_mesh(coat.data)
    bm.free()
    coat.data.update()

    # --- lapels: fold the front chest outward -----------------------------
    pts = [mw @ v.co for v in coat.data.vertices]
    top = max(p.z for p in pts)
    waist = z_lo + span * lapel_drop
    rolled = 0
    for vert, world in zip(coat.data.vertices, pts):
        if world.z < waist or world.z > top:
            continue
        offset = world - Vector((axis.x, axis.y, world.z))
        ahead = offset.dot(forward)
        across = offset.dot(side)
        if ahead <= 0.0:
            continue                       # back of the coat
        # A V that opens toward the collar, so the fold is wide at the neck
        # and narrows to nothing at the waist -- a notched lapel, not a bib.
        reach = (world.z - waist) / max(top - waist, 1e-6)
        if abs(across) > 0.075 * reach + 0.010:
            continue
        push = forward * (-lapel_out * reach) +                side * (lapel_out * 1.4 * reach * (1.0 if across >= 0 else -1.0))
        vert.co = inv @ (world + push)
        rolled += 1
    coat.data.update()
    if rolled:
        made.append("lapels(%d)" % rolled)

    # --- open the skirt at the centre front -------------------------------
    hem_open = z_lo + span * 0.02
    waist_open = z_lo + span * open_from
    bm = bmesh.new()
    bm.from_mesh(coat.data)
    doomed = []
    for face in bm.faces:
        mid = mw @ face.calc_center_median()
        if not (hem_open <= mid.z <= waist_open):
            continue
        offset = mid - Vector((axis.x, axis.y, mid.z))
        if offset.dot(forward) <= 0.0:
            continue
        if abs(offset.dot(side)) > 0.022:
            continue
        doomed.append(face)
    if doomed:
        bmesh.ops.delete(bm, geom=doomed, context="FACES")
        made.append("front opening(%d faces)" % len(doomed))
    bm.normal_update()
    bm.to_mesh(coat.data)
    bm.free()
    coat.data.update()

    # --- double-breasted buttons ------------------------------------------
    button_mat = trim_slot
    if button_mat is None and len(coat.data.materials) > 1:
        button_mat = len(coat.data.materials) - 1
    chest_top = top - collar_h - 0.02
    chest_bot = z_lo + span * (open_from + 0.02)
    placed = 0
    bm = bmesh.new()
    bm.from_mesh(coat.data)
    tree = None
    for row in range(rows):
        t = row / float(max(rows - 1, 1))
        z = chest_top + (chest_bot - chest_top) * t
        for sign in (-1.0, 1.0):
            anchor = Vector((axis.x, axis.y, z)) + side * (spread * sign)
            # Sit the button on the coat's own surface, not in mid-air.
            best, bestd = None, 1e9
            for v in bm.verts:
                w = mw @ v.co
                if abs(w.z - z) > 0.02:
                    continue
                if (w - Vector((axis.x, axis.y, w.z))).dot(forward) <= 0.0:
                    continue
                d = (w - anchor).length
                if d < bestd:
                    bestd, best = d, w
            if best is None:
                continue
            here = best + forward * (button_r * 0.6)
            ret = bmesh.ops.create_uvsphere(bm, u_segments=8, v_segments=6,
                                            radius=button_r)
            for v in ret["verts"]:
                v.co = inv @ (here + (mw.to_3x3() @ v.co))
            if button_mat is not None:
                for f in {f for v in ret["verts"] for f in v.link_faces}:
                    f.material_index = button_mat
            placed += 1
    bm.normal_update()
    bm.to_mesh(coat.data)
    bm.free()
    coat.data.update()
    if placed:
        made.append("%d buttons" % placed)

    log("tailored: %s" % (", ".join(made) or "nothing"))
    return coat


def _cut_slot(obj, planes, inside, label="slot"):
    """Delete a region, leaving STRAIGHT edges along the given planes.

    Deleting whole faces on a face-centre test can only ever give a sawtooth
    edge: a face is kept or dropped entire, so the boundary staircases around
    whichever quads happened to straddle the line. Every opening cut into
    this coat came out ragged for that reason -- the chest opening, the back
    vent, the skirt front.

    Bisecting along the cut line first puts real vertices ON it, so the faces
    either side part cleanly and the edge follows the plane. _flat_cut
    already does this for the hem and neckline; these cuts just never used
    it.

    planes is a list of (point, normal). inside(centre) decides what goes.
    """
    import bmesh

    mw = obj.matrix_world
    inv = mw.inverted()

    bm = bmesh.new()
    bm.from_mesh(obj.data)
    for point, normal in planes:
        local_p = inv @ point
        local_n = (inv.to_3x3() @ normal).normalized()
        bmesh.ops.bisect_plane(
            bm, geom=list(bm.faces) + list(bm.edges) + list(bm.verts),
            plane_co=local_p, plane_no=local_n,
            clear_inner=False, clear_outer=False)

    doomed = [f for f in bm.faces if inside(mw @ f.calc_center_median())]
    if doomed:
        bmesh.ops.delete(bm, geom=doomed, context="FACES")
    bm.normal_update()
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()
    log("   %s: %d faces removed along %d straight cut(s)"
        % (label, len(doomed), len(planes)))
    return len(doomed)


def add_lapels(coat, cx, cy, forward, side, low_z, high_z,
               width=0.045, roll=0.012, drop=0.006, facing_name=None,
               waist_z=None, skirt_share=0.40):
    """Fold the coat's front edge back on itself: a real lapel, and a seam.

    What this replaces pushed a patch of chest vertices outward and called
    the result lapels. That is a bulge: it has no fold, no edge, and nothing
    to catch the light, which is why the front read as a crumpled panel.
    Worse, it ran BEFORE the chest opening was cut, so the V was then sliced
    straight through the middle of it.

    A lapel is the coat's own front turned back along a roll line, showing
    its facing. Built that way here -- the opening's boundary is extruded
    twice, once a few millimetres proud to make the roll, then outward and
    down to lay the lapel back against the chest -- it comes with the thing
    that was also missing: the opening gets a real edge instead of a raw
    boundary. That edge is what seams the V. A raw boundary could never look
    seamed, because the clearance pass moves its vertices with neighbours on
    one side only and Solidify then rims whatever it is handed.

    Returns the number of faces added, which are put in the facing slot so
    the lapel reads in contrasting cloth.
    """
    import bmesh
    from mathutils import Vector

    mw = coat.matrix_world
    inv = mw.inverted()

    slot = 0
    if facing_name is not None:
        facing = bpy.data.materials.get(facing_name)
        if facing is None:
            facing = bpy.data.materials.new(facing_name)
        if facing.name not in [m.name for m in coat.data.materials if m]:
            coat.data.materials.append(facing)
        slot = next(i for i, m in enumerate(coat.data.materials)
                    if m and m.name == facing.name)

    bm = bmesh.new()
    bm.from_mesh(coat.data)
    bm.verts.ensure_lookup_table()

    def on_front(vert):
        world = mw @ vert.co
        if world.z < low_z or world.z > high_z:
            return False
        offset = world - Vector((cx, cy, world.z))
        return offset.dot(forward) > 0.0

    edges = [e for e in bm.edges
             if e.is_boundary and all(on_front(v) for v in e.verts)]
    if not edges:
        bm.free()
        log("   lapels: no front opening to fold -- skipped")
        return 0

    def lean_of(world):
        offset = world - Vector((cx, cy, world.z))
        return 1.0 if offset.dot(side) >= 0.0 else -1.0

    def radial_of(world):
        out = Vector((world.x - cx, world.y - cy, 0.0))
        return out.normalized() if out.length > 1e-6 else forward.copy()

    added = []

    # The roll: a few millimetres proud of the coat, standing along the
    # opening. This is the fold itself, and the seam that was missing.
    ret = bmesh.ops.extrude_edge_only(bm, edges=edges)
    ring = [g for g in ret["geom"] if isinstance(g, bmesh.types.BMVert)]
    for vert in ring:
        world = mw @ vert.co
        vert.co = inv @ (world + radial_of(world) * roll)
    added += [g for g in ret["geom"] if isinstance(g, bmesh.types.BMFace)]
    edges = [g for g in ret["geom"]
             if isinstance(g, bmesh.types.BMEdge) and g.is_boundary]

    # The lapel face: back outward from the roll and a little down, so it
    # lies on the chest instead of standing off it like a fin.
    ret = bmesh.ops.extrude_edge_only(bm, edges=edges)
    ring = [g for g in ret["geom"] if isinstance(g, bmesh.types.BMVert)]
    for vert in ring:
        world = mw @ vert.co
        lean = lean_of(world)
        # A LAPEL ABOVE THE WAIST, A FACING BELOW IT.
        #
        # The same fold runs the whole front, because it is one edge, but it
        # is not the same width all the way down: a 45mm turn-back on the
        # skirt would read as a second pair of lapels round the knees. Below
        # the waist it narrows to a facing, which is what actually edges a
        # frock coat's skirts.
        here = width
        if waist_z is not None and world.z < waist_z:
            here = width * skirt_share
        # STAY PROUD. Pulling the band back toward the coat as it widened
        # left it about 4mm off the surface, so it read as a thick lip along
        # the opening rather than a lapel lying on the chest -- the fold was
        # there, but there was nothing of it to catch the light. The band
        # keeps the full roll and tilts a little further out instead, which
        # is also what puts its facing where it can be seen.
        shift = side * (here * lean) + radial_of(world) * (roll * 0.25)
        vert.co = inv @ (world + shift + Vector((0.0, 0.0, -drop)))
    added += [g for g in ret["geom"] if isinstance(g, bmesh.types.BMFace)]

    for face in added:
        face.material_index = slot

    bm.normal_update()
    bm.to_mesh(coat.data)
    bm.free()
    coat.data.update()
    log("   lapels: %d faces folded back off the front opening "
        "(roll %.0fmm, width %.0fmm)" % (len(added), roll * 1000, width * 1000))
    return len(added)


def tailor_frock(coat=None, body=None, waist=0.62, suppress=0.88, flare=1.28,
                 collar_h=0.085, lapel_drop=0.26, lapel_out=0.030,
                 rows=4, button_r=0.013, spread=0.040, vent_wide=0.020,
                 facing_name="Facing"):
    """Cut the coat to the Met references rather than to guesswork.

    Four things every one of those garments has and the generated coat did
    not, in the order they matter:

    WAIST SUPPRESSION. "The coat's diameter around the waist is less than
    around the chest", pulled in by a high horizontal waist seam and flaring
    out again below it. This is the whole silhouette -- without it the coat
    is a tube, which is what it looked like -- and it is why the 1840s coats
    in the reference read as tailoring and the generated one read as a sack.

    A FACING IN DIFFERENT CLOTH. "The outer edge of the lapel is often cut
    from a separate piece of cloth from the main body." The dark green 1840s
    coat has a velvet collar against a wool body and the join is visible
    across the room. A lapel folded in the SAME cloth has no edge to see,
    which is why the last one read as nothing however far it was folded.
    Collar and lapel get their own material slot.

    A CENTRE VENT. Every back view in the reference set has one.

    BUTTONS THAT STOP AT THE WAIST. "The formal frock coat only buttoned
    down to the waist seam, which was decorated at the back with a pair of
    buttons." Running them down the chest and stopping nowhere is not a
    frock coat front.
    """
    import bmesh
    import math
    from mathutils import Vector

    coat = coat or bpy.context.active_object
    body = body or _find_body(exclude=coat)
    forward = _forward_of(body)
    side = Vector((-forward.y, forward.x, 0.0))

    mw = coat.matrix_world
    inv = mw.inverted()
    pts = [mw @ v.co for v in coat.data.vertices]
    z_lo, z_hi = min(p.z for p in pts), max(p.z for p in pts)
    span = max(z_hi - z_lo, 1e-6)
    cx = sum(p.x for p in pts) / len(pts)
    cy = sum(p.y for p in pts) / len(pts)

    waist_z = z_lo + span * waist
    made = []

    # --- waist suppression, and the skirt flaring below it ----------------
    for vert, world in zip(coat.data.vertices, pts):
        out = Vector((world.x - cx, world.y - cy, 0.0))
        if out.length < 1e-5:
            continue
        if world.z >= waist_z:
            # Chest down to the waist: ease from full width to suppressed.
            t = (world.z - waist_z) / max(z_hi - waist_z, 1e-6)
            scale = suppress + (1.0 - suppress) * min(1.0, t * 1.6)
        else:
            # Waist down to the hem: open out.
            t = (waist_z - world.z) / max(waist_z - z_lo, 1e-6)
            scale = suppress + (flare - suppress) * (t ** 1.2)
        vert.co = inv @ Vector((cx + out.x * scale, cy + out.y * scale,
                                world.z))
    coat.data.update()
    made.append("waist %.2f/flare %.2f" % (suppress, flare))

    # --- stand collar ------------------------------------------------------
    bm = bmesh.new()
    bm.from_mesh(coat.data)
    loops = _boundary_loops(bm)
    for _i, _c in enumerate(loops):
        _zs = [(mw @ v.co).z for v in _c]
        log("      loop %d: %3d verts, mean z %.4f, max z %.4f"
            % (_i, len(_c), sum(_zs) / len(_zs), max(_zs)))
    if loops:
        # By MAX z, not mean. A collar has to grow from the highest EDGE, and
        # an armhole's mean height can beat the neckline's when the neckline
        # dips at the front -- which is how the collar ended up being built
        # on an armhole 15cm down the chest.
        neck = max(loops, key=lambda c: max((mw @ v.co).z for v in c))

        edges = [e for e in bm.edges
                 if e.is_boundary and all(v in set(neck) for v in e.verts)]

        # ONLY IF IT REALLY IS THE NECKLINE.
        #
        # add_sleeves used to consume the neckline -- it accepted any loop
        # whose centre was on the right side, and the neckline's centre sits
        # at x about zero -- so the highest remaining loop was an armhole and
        # the collar got built into the sleeve. That is fixed at the source,
        # but the check is worth keeping: better no collar than a collar
        # welded into an armhole.
        #
        # It has to come AFTER edges is assigned. Written before it, the
        # assignment simply overwrote the guard and it never did anything.
        # A COLLAR RISES FROM THE NECK OPENING, NOT FROM THE SHOULDER SEAM.
        #
        # The neckline loop follows the body's outline, which at this height
        # runs out across the shoulders. Extruding the whole loop lifted
        # those shoulder sections into thin horizontal fins -- the spikes
        # standing out either side of the neck. Only the part of the loop
        # actually near the neck axis should become collar.
        import math
        _face = bpy.data.objects.get("Face")
        _nx, _ny, _nr = neck_axis(body, _face)
        _face_pts = ([_face.matrix_world @ v.co for v in _face.data.vertices]
                     if _face is not None else [])

        def _out(edge):
            return max((Vector(((mw @ v.co).x - _nx,
                                (mw @ v.co).y - _ny, 0.0))).length
                       for v in edge.verts)

        # Drop the shoulder TIPS, keep the rest. Requiring proximity to the
        # neck was far too strict -- only 10 of 118 edges qualified, which is
        # a strip rather than a collar -- because this neckline is a wide
        # boat that runs shoulder to shoulder. Cutting at a percentile of the
        # loop's own spread adapts to whatever shape it is, and the tips are
        # what lifted into fins.
        if edges:
            # KEEP THE WHOLE RING.
            #
            # Dropping the outermost 28% of the loop broke it into arcs, and
            # each arc extruded as its own ribbon -- a torn crown of spikes
            # rather than a band, and nothing that could encompass a neck.
            # The filter existed because the opening was a boat running
            # shoulder to shoulder, so its far edges really would have lifted
            # into fins. The opening is now a neck-sized circle at the join,
            # so every edge of it belongs to the collar and all of them stay.
            log("   collar: %d edge(s), whole ring" % len(edges))

        _top = max((mw @ v.co).z for v in coat.data.vertices)
        _loop_top = max((mw @ v.co).z for v in neck)
        if _loop_top < _top - 0.06:
            log("   collar: highest loop is %.0fmm below the coat's top -- "
                "that is not a neckline. Skipping the collar."
                % ((_top - _loop_top) * 1000))
            edges = []
        if edges:
            # A COLLAR HAS TO CONVERGE, NOT JUST RISE.
            #
            # The shell is cut by a horizontal plane and this body has no
            # neck: measured, its cross-section at the cut is 337mm across,
            # which is shoulder to shoulder. The neckline therefore starts as
            # a wide flat boat by construction, and one extrusion drawn in
            # 45% gets nowhere near closing it -- which is why the collar kept
            # reading as "no collar", a ragged band lying across the
            # shoulders.
            #
            # Built as a stack of rings instead, each rising a little and
            # drawing further in, so the opening closes from shoulder width
            # down to something a neck fits through.
            # ONE RADIUS, MEASURED LOW, HELD ALL THE WAY UP.
            #
            # Measuring the neck at each ring's own height sounds better and
            # is worse: by the top ring the collar is level with the JAW, so
            # the measurement returns the jaw's width and the collar flares
            # out to meet it. That is the same trap that had neck_axis
            # reporting a 129mm neck.
            #
            # A standing collar does not follow the jaw. It holds the neck's
            # own radius up its whole height, which is what makes it read as
            # a collar rather than a funnel. So the neck is measured once,
            # just above the shoulders where it is still neck.
            # AND MEASURE IT ABOUT THE NECK'S OWN AXIS.
            #
            # neck_axis centres on the BODY'S TOP CAP, which is the right
            # place to cut a hole in the shell and the wrong place to measure
            # a neck from: the cap's centroid sits about 50mm behind the
            # throat. Measured about it, this neck came back as 141mm; about
            # its own centre, 78mm. Everything downstream inherited that --
            # the collar was built to clear a neck twice the size of the one
            # it is on.
            # The neck's own girth plus a little ease, held all the way up:
            # a standing collar does not follow the jaw outward.
            _collar_stand = _nr + 0.012
            _join_z, _neck_top = neck_span(body)
            _target_z = _join_z + (_neck_top - _join_z) * 0.5
            log("   collar: neck %.0fmm at (%.3f, %.3f); standing %.0fmm, "
                "rising to z %.4f (halfway up a neck of %.0fmm)"
                % (_nr * 1000, _nx, _ny, _collar_stand * 1000, _target_z,
                   (_neck_top - _join_z) * 1000))
            log("   collar: standing at %.0fmm about the neck axis"
                % (_collar_stand * 1000))

            rings = 4
            # CLOSE ON THE FIRST RING, THEN RISE.
            #
            # The loop this grows from is the shell's own top edge, which
            # runs shoulder to shoulder at about 150mm. An even 0.55 a ring
            # cannot close that onto a 70mm neck in four steps: measured, the
            # rings landed at 126, 140, 150 and 93mm -- a cone, touching the
            # neck only at its very top, which is why the throat read bare
            # however high the collar went.
            #
            # Closing almost all of it on the first ring puts the SHOULDER in
            # as well: the cloth runs inward from the shoulder to the neck's
            # own join, and the collar stands up from there. The rings after
            # it only have to hold the line.
            pulls = (0.90, 0.70, 0.55, 0.45)
            _from_z = max((mw @ v.co).z for v in neck)
            step_up = max((_target_z - _from_z) / float(rings), 0.004)
            hub = None
            grown = 0
            for ring in range(rings):
                ret = bmesh.ops.extrude_edge_only(bm, edges=edges)
                fresh = [g for g in ret["geom"]
                         if isinstance(g, bmesh.types.BMVert)]
                if not fresh:
                    break
                if hub is None:
                    hub = sum((mw @ v.co for v in fresh),
                              Vector((0.0, 0.0, 0.0))) / len(fresh)
                pull = pulls[min(ring, len(pulls) - 1)]
                # CONVERGE ON THE NECK, NOT ON A FRACTION.
                #
                # Pulling each ring a fixed share of the way to the hub takes
                # no account of what is underneath it. Compounded over four
                # rings that is 0.66*0.70*0.74*0.78 = 0.267, so a 93mm opening
                # closes to 25mm around a 75mm neck -- the collar ends up 50mm
                # INSIDE the throat, and the clearance pass then shoves every
                # vertex of it back out by however much each one happens to be
                # buried. That uneven shove is the crinkled fringe around the
                # neckline; the collar was never standing where it looked, it
                # was being pushed there.
                #
                # Converging on a radius instead, just clear of the neck, the
                # rings land where they are meant to and the clearance pass
                # has nothing left to correct.
                # CONVERGE ON THE NECK ITSELF, AT THIS HEIGHT.
                #
                # Converging on `hub` -- the centroid of the first ring --
                # and on one radius for the whole collar left it standing
                # 65-72mm off a 77mm neck: a funnel. hub sits about 40mm
                # from the throat's own axis, because the shell's neckline is
                # centred on the body's top cap and the neck is not, so even
                # a correct radius about hub lands wide on one side.
                #
                # Nor could the clearance pass rescue it: that only moves
                # vertices which are TOO CLOSE, and a ring already 149mm out
                # is clear of anything. It can stop a collar being pushed
                # out; it cannot pull one in.
                #
                # The neck is measured from the face mesh at each ring's own
                # height, about the neck's own axis, and the ring closes onto
                # it.
                stand = _collar_stand
                for v in fresh:
                    world = (mw @ v.co) + Vector((0.0, 0.0, step_up))
                    out = Vector((world.x - _nx, world.y - _ny, 0.0))
                    reach_r = out.length
                    if reach_r < 1e-6:
                        v.co = inv @ world
                        continue
                    want = reach_r + (stand - reach_r) * pull
                    if reach_r > stand:
                        want = max(want, stand)
                    out = out.normalized() * want
                    v.co = inv @ Vector((_nx + out.x, _ny + out.y,
                                         world.z))
                edges = [e for e in ret["geom"]
                         if isinstance(e, bmesh.types.BMEdge) and e.is_boundary]
                grown += 1
                log("      ring %d: %3d verts, top z %.4f, %d edge(s) on"
                    % (ring, len(fresh),
                       max((mw @ v.co).z for v in fresh), len(edges)))
                if not edges:
                    break

            # A SEAM ON TOP, so the collar ends in an edge rather than a cut.
            #
            # What the rings leave is a raw boundary, and a raw boundary
            # cannot survive what comes after it: the clearance pass moves
            # those vertices with neighbours on one side only, and Solidify
            # rims whatever it is handed. That is what left the top of the
            # collar ragged, and it is the same fault that had the coat's V
            # looking torn until the lapels gave it a folded edge.
            #
            # Rolled the same way: out a little, then turned back down, so
            # the collar finishes in a defined line. The facing pass below
            # picks these faces up with the rest of the collar, so the seam
            # reads in the facing cloth.
            if edges:
                seam = 0
                for step, (grow, rise) in enumerate(((0.004, 0.003),
                                                     (0.002, -0.004))):
                    ret = bmesh.ops.extrude_edge_only(bm, edges=edges)
                    fresh = [g for g in ret["geom"]
                             if isinstance(g, bmesh.types.BMVert)]
                    if not fresh:
                        break
                    for v in fresh:
                        world = mw @ v.co
                        out = Vector((world.x - _nx, world.y - _ny, 0.0))
                        if out.length > 1e-6:
                            out = out.normalized() * grow
                        v.co = inv @ (world + out
                                      + Vector((0.0, 0.0, rise)))
                    edges = [e for e in ret["geom"]
                             if isinstance(e, bmesh.types.BMEdge)
                             and e.is_boundary]
                    seam += 1
                    if not edges:
                        break
                log("      collar seam: %d fold(s) on the top edge" % seam)
                made.append("collar seam")

            made.append("collar(%d rings)" % grown)
    bm.normal_update()
    bm.to_mesh(coat.data)
    bm.free()
    coat.data.update()
    pts = [mw @ v.co for v in coat.data.vertices]
    top = max(p.z for p in pts)

    # --- lapels, folded back ----------------------------------------------
    lapel_from = z_lo + span * lapel_drop

    # Edge loops through the lapel and collar zone FIRST. A fold needs edges
    # to fold along; without them the turn-back smears across whatever quads
    # happen to lie under it, which is why the lapels read as a crumpled
    # patch instead of a crease.
    def _in_lapel_zone(point):
        if point.z < lapel_from - 0.01:
            return False
        offset = point - Vector((cx, cy, point.z))
        if offset.dot(forward) <= 0.0:
            return False
        return abs(offset.dot(side)) <= 0.075

    # NO BULGE, AND NO LOOPS FOR IT.
    #
    # What stood here pushed a patch of chest vertices outward and called it
    # lapels. A lapel is a FOLD -- it has an edge, and an underside in the
    # facing cloth -- and a displaced patch has neither, which is why the
    # front read as a crumpled panel however the numbers were tuned. It also
    # ran before the chest opening was cut, so the V was then sliced straight
    # through the middle of it.
    #
    # The real lapels are folded off the opening's own edge by add_lapels,
    # below, once that edge exists. The subdivision that used to run here
    # went with the bulge: it existed to give the fold something to fold
    # along, and it was costing 154 sliver faces across the chest.
    pts = [mw @ v.co for v in coat.data.vertices]
    top = max(p.z for p in pts)
    rolled = set()

    # --- the facing: collar and lapel in their own cloth -------------------
    facing = bpy.data.materials.get(facing_name)
    if facing is None:
        facing = bpy.data.materials.new(facing_name)
    if facing.name not in [m.name for m in coat.data.materials if m]:
        coat.data.materials.append(facing)
    facing_slot = len(coat.data.materials) - 1
    collar_from = top - collar_h - 0.010
    faced = 0
    for face in coat.data.polygons:
        centre = mw @ face.center
        offset = centre - Vector((cx, cy, centre.z))
        on_front = offset.dot(forward) > 0.0
        near_lapel = any(v in rolled for v in face.vertices)
        if centre.z >= collar_from or (on_front and near_lapel):
            face.material_index = facing_slot
            faced += 1
    coat.data.update()
    made.append("facing(%d faces)" % faced)

    # --- the chest opening, so the waistcoat is FRAMED not leaking ---------
    #
    # A frock coat is worn open over a waistcoat, and without an opening
    # there is nothing for the waistcoat to show through: measured, 94% of
    # its chest sat outside the coat, so the two read as two shells fighting
    # rather than as layers.
    #
    # The opening is a V -- closed at the waist where the coat buttons, wide
    # at the collar -- which is what makes the waistcoat read as being worn
    # UNDER something rather than merely visible.
    chest_top = top - collar_h * 0.5
    narrow = vent_wide * 0.6
    wide = vent_wide * 3.0

    def _chest_edge(sign):
        """One side of the V, as a plane through the waist and the collar."""
        low = Vector((cx, cy, waist_z)) + side * (narrow * sign)
        high = Vector((cx, cy, chest_top)) + side * (wide * sign)
        along = (high - low).normalized()
        return low, forward.cross(along).normalized()

    def _in_chest(centre):
        if centre.z < waist_z or centre.z > chest_top:
            return False
        offset = centre - Vector((cx, cy, centre.z))
        if offset.dot(forward) <= 0.0:
            return False
        rise = (centre.z - waist_z) / max(chest_top - waist_z, 1e-6)
        return abs(offset.dot(side)) <= narrow + (wide - narrow) * rise

    cut = _cut_slot(coat, [_chest_edge(1.0), _chest_edge(-1.0)],
                    _in_chest, label="chest opening")
    if cut:
        made.append("chest opening(%d faces)" % cut)

    # --- and down through the skirt ----------------------------------------
    #
    # A frock coat is open all the way down: the V is not a keyhole in the
    # chest, it is the front edge of a coat whose skirts part. Stopping the
    # cut at the waist left the skirt closed across the front, so the opening
    # read as a slit with a panel hung under it.
    #
    # Widening toward the hem, because the skirts swing open as they fall --
    # parallel edges would read as a zip.
    skirt_low = z_lo + span * 0.015
    front_narrow = vent_wide * 0.8
    front_wide = vent_wide * 2.2

    def _front_half(z):
        reach = ((waist_z - z) / max(waist_z - skirt_low, 1e-6))
        reach = max(0.0, min(1.0, reach))
        return front_narrow + (front_wide - front_narrow) * reach

    def _skirt_edge(sign):
        high = Vector((cx, cy, waist_z)) + side * (front_narrow * sign)
        low = Vector((cx, cy, skirt_low)) + side * (front_wide * sign)
        along = (high - low).normalized()
        return low, forward.cross(along).normalized()

    def _in_front_skirt(centre):
        if centre.z > waist_z or centre.z < skirt_low:
            return False
        offset = centre - Vector((cx, cy, centre.z))
        if offset.dot(forward) <= 0.0:
            return False
        return abs(offset.dot(side)) <= _front_half(centre.z)

    skirt_cut = _cut_slot(coat, [_skirt_edge(1.0), _skirt_edge(-1.0)],
                          _in_front_skirt, label="skirt front")
    if skirt_cut:
        made.append("skirt front(%d faces)" % skirt_cut)

    # ONE fold, over the whole front edge, now that the whole front edge
    # exists. Folding the chest alone would have seamed the V and left the
    # skirt opening raw -- and a raw boundary cannot look seamed, because the
    # clearance pass moves its vertices with neighbours on one side only and
    # Solidify rims whatever it is handed.
    if cut or skirt_cut:
        folded = add_lapels(coat, cx, cy, forward, side,
                            low_z=skirt_low, high_z=chest_top + collar_h,
                            facing_name=facing_name, waist_z=waist_z)
        if folded:
            made.append("lapels(%d faces)" % folded)
    pts = [mw @ v.co for v in coat.data.vertices]

    # --- centre back vent --------------------------------------------------
    vent_low = z_lo + span * 0.02

    def _in_vent(centre):
        if centre.z > waist_z or centre.z < vent_low:
            return False
        offset = centre - Vector((cx, cy, centre.z))
        if offset.dot(forward) >= 0.0:
            return False
        return abs(offset.dot(side)) <= vent_wide

    vent_planes = [(Vector((cx, cy, waist_z)) + side * vent_wide, side),
                   (Vector((cx, cy, waist_z)) - side * vent_wide, side),
                   (Vector((cx, cy, waist_z)), Vector((0.0, 0.0, 1.0)))]
    cut = _cut_slot(coat, vent_planes, _in_vent, label="back vent")
    if cut:
        made.append("back vent(%d faces)" % cut)
    pts = [mw @ v.co for v in coat.data.vertices]

    # --- double-breasted buttons, stopping at the waist --------------------
    bm = bmesh.new()
    bm.from_mesh(coat.data)
    placed = [0]

    def stud(anchor, normal):
        ret = bmesh.ops.create_uvsphere(bm, u_segments=10, v_segments=7,
                                        radius=button_r)
        here = anchor + normal * (button_r * 0.5)
        for v in ret["verts"]:
            v.co = inv @ (here + (mw.to_3x3() @ v.co))
        for f in set(f for v in ret["verts"] for f in v.link_faces):
            f.material_index = facing_slot
        placed[0] += 1

    def surface(z, bearing, lateral):
        best = None
        far = 1e9
        for point in pts:
            if abs(point.z - z) > 0.022:
                continue
            offset = point - Vector((cx, cy, point.z))
            if offset.dot(bearing) <= 0.0:
                continue
            gap = abs(offset.dot(side) - lateral)
            if gap < far:
                far = gap
                best = point
        return best

    chest_top = top - collar_h - 0.045
    for row in range(rows):
        t = row / float(max(rows - 1, 1))
        z = chest_top + (waist_z - chest_top) * t
        for sign in (-1.0, 1.0):
            anchor = surface(z, forward, spread * sign)
            if anchor is not None:
                stud(anchor, forward)
    for sign in (-1.0, 1.0):
        anchor = surface(waist_z, -forward, spread * 0.55 * sign)
        if anchor is not None:
            stud(anchor, -forward)

    bm.normal_update()
    bm.to_mesh(coat.data)
    bm.free()
    coat.data.update()
    made.append("%d buttons" % placed[0])

    log("tailored to reference: %s" % ", ".join(made))
    return coat


def make_breeches(body=None, waist=None, knee=None, offset=0.006,
                  thickness=0.005, trim=True, do_bind=True, name=None):
    """Knee breeches: the body's own leg tubes, thickened and rebound.

    Cut below the leg merge on purpose, so the shell arrives as two tubes.
    """
    body = body or _find_body(exclude=None)
    if body is None:
        raise RuntimeError("no avatar mesh in the scene -- open a prepped .blend")

    if waist is None:
        # spine_01 sits at the natural waist; fall back just above the crotch.
        merge = find_leg_merge(body)
        waist = _bone_fraction(
            body, "spine_01",
            default=(merge + 0.08) if merge is not None else 0.58)
    if knee is None:
        knee = _bone_fraction(body, "calf_l", default=0.28)
        knee = max(0.08, knee - 0.04)      # finish below the knee, not on it
    log("breeches: %.3f..%.3f" % (knee, waist))

    breeches = make_dress_shell(top=waist, bottom=knee, body=body,
                                name="Breeches")

    add_thickness(dress=breeches, body=body, offset=offset,
                  thickness=thickness, bodice_above_z=None)
    for _ in range(4):
        if not _repair_outliers(breeches, factor=4.0):
            break

    if do_bind:
        bind(garment=breeches, body=body)
        # NO skirt_weights here. Breeches are on the legs and must follow the
        # thigh and calf bones; forcing them to the pelvis like a skirt would
        # leave them rigid while the legs walk out of them. The shell is cut
        # from the body, so it already carries the body's own leg weights --
        # base_weights only catches whatever the transfer missed.
        base_weights(dress=breeches, bone="pelvis")

    if trim:
        add_trim_bands(dress=breeches, name=name or breeches.name,
                       where=("hem",))
        for _ in range(4):
            if not _repair_outliers(breeches, factor=4.0):
                break
    return breeches


def drape_shell(dress=None, body=None, rounds=16, factor=0.6, inflate=0.016):
    """Take the BODY'S ANATOMY out of the garment.

    make_dress_shell builds the shell by duplicating a band of the body, so
    the garment starts life as a copy of the figure: navel, pectorals, the
    line of the belly, all of it. Offsetting that by a few millimetres does
    not make a coat, it makes body paint -- which is exactly what the thing
    looked like in the level.

    Cloth does not follow anatomy. It hangs off the shoulders and the chest
    and bridges the hollows between. Smoothing erases the anatomy; the
    outward push afterwards puts back the volume smoothing removes and lifts
    the cloth clear of the figure so it reads as something worn.
    """
    from mathutils import Vector

    dress = dress or bpy.context.active_object
    body = body or _find_body(exclude=dress)

    mod = dress.modifiers.new("Drape", "SMOOTH")
    mod.iterations = rounds
    mod.factor = factor
    bpy.ops.object.select_all(action="DESELECT")
    dress.select_set(True)
    bpy.context.view_layer.objects.active = dress
    bpy.ops.object.modifier_apply(modifier=mod.name)

    # Centre line of the body, per height band, to push away FROM. The
    # bounding-box centre will not do: hips and chest sit at different x/y
    # and one centre for the whole figure skews the garment sideways.
    mwb = body.matrix_world
    pts = [mwb @ v.co for v in body.data.vertices]
    zs = [p.z for p in pts]
    lo, hi = min(zs), max(zs)
    bands = []
    for i in range(40):
        a = lo + (hi - lo) * i / 40.0
        b = lo + (hi - lo) * (i + 1) / 40.0
        band = [p for p in pts if a <= p.z < b]
        if band:
            bands.append(((a + b) / 2.0,
                          sum(p.x for p in band) / len(band),
                          sum(p.y for p in band) / len(band)))

    def centre_at(z):
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

    mw = dress.matrix_world
    inv = mw.inverted()
    pushed = 0
    for vert in dress.data.vertices:
        world = mw @ vert.co
        cx, cy = centre_at(world.z)
        out = Vector((world.x - cx, world.y - cy, 0.0))
        if out.length < 1e-5:
            continue
        vert.co = inv @ (world + out.normalized() * inflate)
        pushed += 1
    dress.data.update()
    log("draped: smoothed %d rounds, pushed %d verts out %.0fmm"
        % (rounds, pushed, inflate * 1000))
    return dress


def neck_axis(body, face=None):
    """Where the neck is, and how thick.

    THE BONES KNOW. neck_01, neck_02 and head sit at (0.000, 0.014),
    (0.000, 0.007) and (0.000, 0.001) on this figure, so the neck runs up
    almost exactly at (0, 0.01).

    Two other candidates were tried and both are wrong. The BODY'S TOP CAP
    centroid is (0.000, 0.070) -- 60mm forward, because the cap is a
    shoulder slab that leans. The FACE MESH's slice centroid drifts to
    (0.000, -0.116) by chin height, because the face mesh carries the whole
    skull and the back of the head drags the mean backwards. Measured about
    either, this 60mm neck came back as 78mm or 141mm, and everything built
    on those numbers stood off the neck by as much again.

    The girth still comes from the face, because the body has no neck to
    measure -- but only from points near the axis, so the skull cannot count
    as neck.

    Returns (x, y, radius).
    """
    import math

    mw = body.matrix_world
    ceiling = max((mw @ v.co).z for v in body.data.vertices)

    cx = cy = 0.0
    arm = _armature_for(body)
    bone = None
    if arm is not None:
        bone = (arm.data.bones.get("neck_01") or arm.data.bones.get("neck")
                or arm.data.bones.get("neck_02"))
    if bone is not None:
        head = arm.matrix_world @ bone.head_local
        cx, cy = head.x, head.y
    else:
        cap = [(mw @ v.co) for v in body.data.vertices
               if (mw @ v.co).z >= ceiling - 0.006]
        if cap:
            cx = sum(q.x for q in cap) / len(cap)
            cy = sum(q.y for q in cap) / len(cap)

    radius = None
    if face is not None:
        pts = [face.matrix_world @ v.co for v in face.data.vertices]
        # START WELL ABOVE THE SHOULDERS.
        #
        # The face mesh begins BELOW the body's top -- it carries the upper
        # chest as well as the neck -- so bands taken just above the join
        # measure chest and trapezius, not neck. Sampled from +12mm at the
        # 80th percentile this 60mm neck read as 85mm, and the collar built
        # on it stood 37mm clear and looked like an open boat.
        #
        # From +25mm it is unambiguously neck, and a middling percentile
        # keeps the jaw out of it.
        ranked = []
        for step in range(7):
            z = ceiling + 0.025 + 0.006 * step
            band = [p for p in pts
                    if abs(p.z - z) < 0.005
                    and math.hypot(p.x - cx, p.y - cy) < 0.090]
            if len(band) < 15:
                continue
            here = sorted(math.hypot(p.x - cx, p.y - cy) for p in band)
            ranked.append(here[int(len(here) * 0.70)])
        if ranked:
            ranked.sort()
            radius = ranked[len(ranked) // 2]

    if radius is None:
        zs = [(mw @ v.co).z for v in body.data.vertices]
        radius = (max(zs) - min(zs)) * 0.045
    return cx, cy, radius


def neck_span(body):
    """Where the neck joins the torso, and where the head takes over."""
    arm = _armature_for(body)
    mw = body.matrix_world
    join = max((mw @ v.co).z for v in body.data.vertices)
    top = join + 0.085
    if arm is not None:
        head = arm.data.bones.get("head")
        if head is not None:
            top = (arm.matrix_world @ head.head_local).z
    return join, top


def _report_loops(stage, obj):
    """Boundary loops and where their tops sit, for tracking a neckline."""
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    loops = _boundary_loops(bm)
    mw = obj.matrix_world
    top = max((mw @ v.co).z for v in obj.data.vertices)
    parts = []
    for comp in sorted(loops,
                       key=lambda c: -max((mw @ v.co).z for v in c)):
        parts.append("%dv@%.3f" % (len(comp),
                                   max((mw @ v.co).z for v in comp)))
    bm.free()
    log("   loops %-22s top %.3f | %s"
        % (stage, top, ", ".join(parts) or "none"))


def cut_neck_hole(shell, body, face=None, ease=1.25, reach=0.055,
                  from_cap=True):
    """Open a NECK-SHAPED hole in the top of a shell.

    make_dress_shell cuts its top with a horizontal plane, which on a body
    that has no neck gives a flat edge shoulder to shoulder -- a boat neck by
    construction, and the reason no collar grown from it ever looked right.

    This removes a neck-sized disc instead, so what is left is a garment with
    shoulders and a hole for the neck to come through, which is what a
    collar can actually be built on.

    reach is how far down from the top of the shell to consider, so the cut
    opens the neckline without punching a hole through the chest.
    """
    import bmesh
    from mathutils import Vector

    cx, cy, radius = neck_axis(body, face)
    opening = radius * ease

    # CUT AT THE JOIN, AROUND THE NECK.
    #
    # This used to size the hole from the shoulder CAP -- 0.92, later 0.45,
    # of the cap's own radius -- because a neck-sized disc "missed the cap
    # almost entirely". It missed because the cut window was measured down
    # from the shell's SUMMIT, and the summit is over the shoulders, so the
    # window sat above the neck entirely. Widening the hole to catch the cap
    # produced a boat running shoulder to shoulder, whose boundary then had
    # to have its outer 28% discarded as fins -- which is what broke the
    # collar ring into arcs.
    #
    # Floored at the neck/torso join instead, a neck-sized circle about the
    # neck's own axis cuts exactly what it should, and every edge of what is
    # left belongs to the collar.
    join_z = max((body.matrix_world @ v.co).z
                 for v in body.data.vertices) - 0.015

    mw = shell.matrix_world
    inv = mw.inverted()
    pts = [mw @ v.co for v in shell.data.vertices]
    if not pts:
        return shell
    top = max(p.z for p in pts)
    floor = join_z

    bm = bmesh.new()
    bm.from_mesh(shell.data)
    doomed = []
    for facet in bm.faces:
        centre = mw @ facet.calc_center_median()
        if centre.z < floor:
            continue
        if (Vector((centre.x - cx, centre.y - cy, 0.0))).length > opening:
            continue
        doomed.append(facet)
    if doomed:
        bmesh.ops.delete(bm, geom=doomed, context="FACES")

    # ROUND THE HOLE OFF.
    #
    # Deleting whole faces on a face-centre test can only give a staircase:
    # a face is kept or dropped entire, so the opening zig-zags around
    # whichever quads straddled the line. That is the same flaw _cut_slot
    # exists to fix for the chest opening and the back vent -- but it matters
    # more here, because the collar is EXTRUDED from this loop, so every
    # notch in it is carried up through all four rings and reads as a
    # serrated edge lying across the shoulders.
    #
    # Bisecting is the wrong tool for a round hole: the planes are global, so
    # tangents to the neck circle would slice the whole coat. Instead the
    # loop it left is pushed out onto the circle it should have been, and its
    # height relaxed along itself, which needs no new geometry at all.
    bm.verts.ensure_lookup_table()
    ring = [v for v in bm.verts
            if v.is_boundary
            and (mw @ v.co).z >= floor
            and (Vector(((mw @ v.co).x - cx, (mw @ v.co).y - cy, 0.0)).length
                 <= opening * 2.2)]
    if len(ring) >= 6:
        for _ in range(4):
            heights = {}
            for vert in ring:
                near = [e.other_vert(vert) for e in vert.link_edges
                        if e.is_boundary and e.other_vert(vert) in ring]
                if not near:
                    continue
                mean = sum((mw @ o.co).z for o in near) / float(len(near))
                heights[vert] = (mw @ vert.co).z * 0.4 + mean * 0.6
            for vert, z in heights.items():
                world = mw @ vert.co
                vert.co = inv @ Vector((world.x, world.y, z))
        for vert in ring:
            world = mw @ vert.co
            out = Vector((world.x - cx, world.y - cy, 0.0))
            if out.length < 1e-6:
                continue
            out = out.normalized() * opening
            vert.co = inv @ Vector((cx + out.x, cy + out.y, world.z))
        log("neck hole: %d boundary verts rounded onto the opening" % len(ring))

    bm.normal_update()
    bm.to_mesh(shell.data)
    bm.free()
    shell.data.update()

    log("neck hole: %d faces removed, radius %.0fmm about (%.3f, %.3f), "
        "top %.0fmm of the shell" % (len(doomed), opening * 1000, cx, cy,
                                     reach * 1000))
    return shell


def trim_body_beneath(body, garments, reach=0.045, keep_above=None,
                      margin=1):
    """Delete body geometry the garments cover, IN PLACE.

    The definitive answer to poke-through, and free at runtime: geometry
    that does not exist cannot show. Used here for the feet, where every
    other approach failed -- each toe is its own lobe, so the per-vertex
    clearance gives each one its own bubble, smoothing merges them but
    leaves the instep bare, and fitting a last let them through whatever the
    taper. With the toes gone there is nothing left to poke.

    keep_above leaves everything above that height alone, so this can take
    the feet without touching the rest of the figure.

    margin keeps that many rings of body alive along each garment's edge, so
    nothing gapes where the boot ends and the leg continues.
    """
    import bmesh

    mw = body.matrix_world
    covered = set()
    for garment in garments:
        if garment is None:
            continue
        ginv = garment.matrix_world.inverted()
        gmw = garment.matrix_world
        for index, vert in enumerate(body.data.vertices):
            if index in covered:
                continue
            point = mw @ vert.co
            if keep_above is not None and point.z > keep_above:
                continue
            ok, loc, _n, _i = garment.closest_point_on_mesh(ginv @ point)
            if ok and (point - (gmw @ loc)).length <= reach:
                covered.add(index)

    if not covered:
        log("   trim: nothing covered")
        return 0

    # Erode, so a vertex only counts as safely covered when its neighbours
    # are too. Without this the cut reaches the very edge of the garment and
    # a seam of body vanishes from under the hem.
    bm = bmesh.new()
    bm.from_mesh(body.data)
    bm.verts.ensure_lookup_table()
    for _ in range(max(0, margin)):
        shrunk = set()
        for index in covered:
            vert = bm.verts[index]
            if all(other.index in covered
                   for edge in vert.link_edges
                   for other in (edge.other_vert(vert),)):
                shrunk.add(index)
        covered = shrunk
        if not covered:
            break

    doomed = [f for f in bm.faces
              if all(v.index in covered for v in f.verts)]
    if doomed:
        bmesh.ops.delete(bm, geom=doomed, context="FACES")
    bm.normal_update()
    bm.to_mesh(body.data)
    bm.free()
    body.data.update()
    log("   trim: %d body faces removed beneath %d garment(s)"
        % (len(doomed), len([g for g in garments if g])))
    return len(doomed)


def toe_box(boot, upto_z, band=0.018, name="_ToeBox"):
    """Replace the forefoot with its own convex hull: a shoe's toe box.

    Smoothing cannot do this. Toe ridges are a large-scale feature, not
    high-frequency noise, so Laplacian averaging shaves them a little per
    round and never removes them -- 26 rounds at 0.72 still left five
    countable toes. Worse, the clearance pass re-carves them afterwards,
    because it works per vertex against the nearest body surface and each toe
    is its own nearest surface.

    A convex hull cannot be concave, by definition, so pushing the forefoot
    out onto the hull of its own points erases the valleys between the toes
    outright and leaves the wedge that encloses them -- which is the shape a
    boot actually has. Safe to do only after the foot beneath has been
    trimmed away, since it moves cloth outward with nothing left to clear.

    band fades the move out above upto_z so the toe box joins the vamp
    smoothly instead of leaving a crease ring around the foot.
    """
    import bmesh

    mw = boot.matrix_world
    inv = mw.inverted()
    picked = [v for v in boot.data.vertices if (mw @ v.co).z <= upto_z + band]
    if len(picked) < 8:
        log("   toe box: only %d vert(s) in range, skipped" % len(picked))
        return 0

    # --- the hull, as a temporary object we can raycast against ------------
    hb = bmesh.new()
    for vert in picked:
        hb.verts.new(mw @ vert.co)
    hb.verts.ensure_lookup_table()
    result = bmesh.ops.convex_hull(hb, input=hb.verts, use_existing_faces=False)
    # One call, deduped: the two lists overlap, so deleting them in turn
    # hands the second call verts the first already freed.
    scrap = {}
    for key in ("geom_interior", "geom_unused"):
        for element in result.get(key, []):
            scrap[id(element)] = element
    if scrap:
        bmesh.ops.delete(hb, geom=list(scrap.values()), context="VERTS")
    mesh = bpy.data.meshes.new(name + "_hull")
    hb.to_mesh(mesh)
    hb.free()
    hull = bpy.data.objects.new(name + "_hull", mesh)
    bpy.context.scene.collection.objects.link(hull)

    moved = 0
    total = 0.0
    for vert in picked:
        world = mw @ vert.co
        ok, loc, _n, _i = hull.closest_point_on_mesh(world)
        if not ok:
            continue
        # Full strength on the forefoot, fading to nothing at the top of the
        # band, so the new shape blends into the vamp.
        rise = (world.z - upto_z) / band
        weight = 1.0 if rise <= 0.0 else max(0.0, 1.0 - rise)
        if weight <= 0.0:
            continue
        shift = loc - world
        if shift.length < 1e-6:
            continue
        vert.co = inv @ (world + shift * weight)
        total += shift.length * weight
        moved += 1

    bpy.data.objects.remove(hull, do_unlink=True)
    bpy.data.meshes.remove(mesh)
    boot.data.update()
    log("   toe box: %d/%d forefoot verts pushed onto the hull "
        "(mean %.1fmm)" % (moved, len(picked), 1000.0 * total / max(moved, 1)))
    return moved


def round_toe(boot, body, fullness=0.82, blunt=0.62):
    """Give the boot a toe box instead of a wedge.

    toe_box pushed the forefoot onto its own convex hull, which fills the
    valleys between the toes but keeps the toes: a toe is a convex bump, so
    it lies ON the hull. What came out was a flattened blade -- measured from
    the ball forward, the section went 94mm wide and 89mm tall down to 36mm
    by 31mm at the tip, tapering to a point with the toes still legible in
    it.

    A boot does not taper to a point. It holds most of its width to near the
    end and then rounds off. So the shape is stated rather than inherited:
    an elliptical section about the foot's own centre line, holding
    `fullness` of the width at the ball until `blunt` of the way along, then
    closing on a circular arc to the tip. Every forefoot vertex is pushed out
    onto it.

    OUTWARD ONLY, and safe to do at all only because the foot underneath has
    been trimmed away -- there is nothing left in there to be pushed into.
    """
    import math
    from mathutils import Vector

    arm = _armature_for(body)
    if arm is None:
        log("   toe: no armature, skipped")
        return 0

    mw = boot.matrix_world
    inv = mw.inverted()
    amw = arm.matrix_world
    moved = 0

    for side in ("l", "r"):
        heel = arm.data.bones.get("foot_%s" % side)
        ball = arm.data.bones.get("ball_%s" % side)
        if heel is None or ball is None:
            continue
        heel_w = amw @ heel.head_local
        ball_w = amw @ ball.head_local
        forward = Vector((ball_w.x - heel_w.x, ball_w.y - heel_w.y, 0.0))
        if forward.length < 1e-6:
            continue
        forward = forward.normalized()
        across = Vector((-forward.y, forward.x, 0.0))
        sign = 1.0 if heel_w.x >= 0.0 else -1.0

        picked = []
        for vert in boot.data.vertices:
            world = mw @ vert.co
            if (world.x >= 0.0) != (sign >= 0.0):
                continue
            along = (world - ball_w).dot(forward)
            if along < -0.010:
                continue
            picked.append((vert, world, along))
        if len(picked) < 32:
            continue

        reach = max(p[2] for p in picked)
        if reach < 0.020:
            continue

        # The section at the ball sets the toe box's size.
        base = [p for p in picked if abs(p[2]) < 0.012]
        if len(base) < 8:
            base = [p for p in picked if p[2] < reach * 0.25]
        if len(base) < 8:
            continue
        half_w = max(abs((w - ball_w).dot(across)) for _v, w, _a in base)
        z_lo = min(w.z for _v, w, _a in base)
        z_hi = max(w.z for _v, w, _a in base)
        half_h = (z_hi - z_lo) * 0.5
        z_mid = (z_hi + z_lo) * 0.5
        if half_w < 1e-5 or half_h < 1e-5:
            continue

        for vert, world, along in picked:
            t = max(0.0, min(1.0, along / reach))
            # Hold most of the width, then round off over the last stretch.
            shape = fullness + (1.0 - fullness) * (1.0 - t)
            if t > blunt:
                ride = (t - blunt) / (1.0 - blunt)
                shape *= math.sqrt(max(0.0, 1.0 - ride * ride))
            want_w = half_w * shape
            want_h = half_h * shape
            if want_w < 1e-5 or want_h < 1e-5:
                continue
            lateral = (world - ball_w).dot(across)
            rise = world.z - z_mid
            here = math.hypot(lateral / want_w, rise / want_h)
            if here >= 1.0 or here < 1e-6:
                continue                      # already out on the envelope
            scale = 1.0 / here
            vert.co = inv @ (world
                             + across * (lateral * (scale - 1.0))
                             + Vector((0.0, 0.0, rise * (scale - 1.0))))
            moved += 1

    boot.data.update()
    log("   toe: %d forefoot vert(s) pushed onto the toe box" % moved)
    return moved


def make_waistcoat(body=None, top=0.95, bottom=None, offset=0.004,
                   thickness=0.004, name=None, do_bind=True):
    """A waistcoat to sit BEHIND the coat's front opening.

    Cutting a real opening in the coat exposes whatever is underneath, and
    underneath is the MetaHuman body in its underwear -- which is the grey
    panel down the middle of the chest. A frock coat is worn over a
    waistcoat; without one the opening reads as a hole rather than as
    tailoring.

    Deliberately thin and close to the body, so it fills the gap without
    fighting the coat for the same space.
    """
    body = body or _find_body(exclude=None)
    if body is None:
        raise RuntimeError("no avatar mesh in the scene")

    if bottom is None:
        merge = find_leg_merge(body)
        bottom = (merge + 0.05) if merge is not None else 0.55

    log("waistcoat: band %.3f..%.3f" % (bottom, top))
    vest = make_dress_shell(top=top, bottom=bottom, body=body,
                            name="Waistcoat", keep_shoulders=True)
    add_thickness(dress=vest, body=body, offset=offset, thickness=thickness)
    for _ in range(3):
        if not _repair_outliers(vest, factor=4.0):
            break
    if do_bind:
        bind(garment=vest, body=body)
        base_weights(dress=vest, bone="pelvis")
    log("waistcoat: %d verts, %d faces"
        % (len(vest.data.vertices), len(vest.data.polygons)))
    return vest


def cap_toes(dress=None, body=None, swell=0.013):
    """Close the toes into a single cap.

    Smoothing softens toes but cannot merge them: they are separate lobes of
    the mesh and averaging a vertex against its neighbours keeps each lobe.
    The boot still came out as a black bare foot. Swelling everything below
    the ankle closes the few millimetres between the toes so the forefoot
    reads as one rounded cap -- which is what the front of a boot is.
    """
    from mathutils import Vector

    dress = dress or bpy.context.active_object
    body = body or _find_body(exclude=dress)
    arm = _armature_for(body)

    ankle = None
    if arm is not None:
        bone = arm.data.bones.get("foot_l")
        if bone is not None:
            ankle = (arm.matrix_world @ bone.head_local).z
    mw = dress.matrix_world
    pts = [mw @ v.co for v in dress.data.vertices]
    if ankle is None:
        zs = [p.z for p in pts]
        ankle = min(zs) + (max(zs) - min(zs)) * 0.20

    below = [p for p in pts if p.z <= ankle]
    if not below:
        log("toe cap: nothing below the ankle")
        return dress
    cx = sum(p.x for p in below) / len(below)
    cy = sum(p.y for p in below) / len(below)

    inv = mw.inverted()
    swelled = 0
    for vert, world in zip(dress.data.vertices, pts):
        if world.z > ankle:
            continue
        # Full swell at the toes, easing off toward the ankle so the boot
        # does not balloon at the joint.
        out = Vector((world.x - cx, world.y - cy, 0.0))
        if out.length < 1e-5:
            continue
        vert.co = inv @ (world + out.normalized() * swell)
        swelled += 1
    dress.data.update()
    log("toe cap: swelled %d verts below the ankle by %.0fmm"
        % (swelled, swell * 1000))
    return dress


def _smooth_region(obj, keep, rounds=12, factor=0.6, name="_Region"):
    """Smooth only the vertices `keep(world_position)` selects.

    Blender's Smooth modifier takes a vertex group, so the region is written
    as one, smoothed, and the group removed.
    """
    mw = obj.matrix_world
    picked = [v.index for v in obj.data.vertices if keep(mw @ v.co)]
    if not picked:
        return 0
    group = obj.vertex_groups.get(name)
    if group:
        obj.vertex_groups.remove(group)
    group = obj.vertex_groups.new(name=name)
    group.add(picked, 1.0, "REPLACE")

    mod = obj.modifiers.new("RegionSmooth", "SMOOTH")
    mod.iterations = rounds
    mod.factor = factor
    mod.vertex_group = group.name
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    try:
        bpy.ops.object.modifier_apply(modifier=mod.name)
    except Exception as exc:
        log("   region smooth failed: %s" % exc)
        if mod.name in [m.name for m in obj.modifiers]:
            obj.modifiers.remove(mod)
    # Look the group up again by name: the earlier handle can be stale once
    # a modifier has been applied, and indexing a removed group takes the
    # whole process down without a traceback.
    stale = obj.vertex_groups.get(name)
    if stale is not None:
        obj.vertex_groups.remove(stale)
    return len(picked)


def _add_loops(obj, keep, cuts=2, label="region"):
    """Cut extra edge loops through a region, so it has geometry to fold on.

    A fold needs edges to fold ALONG. Without them a lapel turn-back just
    smears across whatever quads happen to lie under it, which is why the
    lapels read as a crumpled patch rather than a crease.
    """
    import bmesh

    mw = obj.matrix_world
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    wanted = [e for e in bm.edges
              if keep(mw @ ((e.verts[0].co + e.verts[1].co) * 0.5))]
    if wanted:
        bmesh.ops.subdivide_edges(bm, edges=wanted, cuts=cuts,
                                  use_grid_fill=True)
    bm.normal_update()
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()
    log("   %s: %d edge(s) subdivided x%d -> %d verts"
        % (label, len(wanted), cuts, len(obj.data.vertices)))
    return len(wanted)


def boot_last(dress=None, body=None, bins=18, toe_round=1.0, sole_lift=0.004,
              toe_room=0.014):
    """Reshape the foot into a boot LAST, instead of decorating a foot.

    Smoothing and swelling both failed here for the same reason: toes are
    separate lobes of the mesh, and any operation that averages a vertex
    against its neighbours keeps every lobe it started with. The boot came
    out a black bare foot.

    A last is built, not derived. Each cross-section along the foot's own
    forward axis is replaced by a smooth ellipse whose width and height come
    from the widest part of that section, tapered to a round cap at the toe.
    Toes stop existing rather than being softened, which is the difference.

    The sole is kept flat: a boot sits ON the floor, and an ellipse would
    round the bottom off and leave the character hovering.
    """
    import math
    from mathutils import Vector

    dress = dress or bpy.context.active_object
    body = body or _find_body(exclude=dress)
    arm = _armature_for(body)
    if arm is None:
        log("boot last: no armature")
        return dress

    mw = dress.matrix_world
    inv = mw.inverted()
    pts = [mw @ v.co for v in dress.data.vertices]
    floor = min(p.z for p in pts)

    for side in ("l", "r"):
        foot = arm.data.bones.get("foot_%s" % side)
        ball = arm.data.bones.get("ball_%s" % side)
        if foot is None or ball is None:
            continue
        ankle = arm.matrix_world @ foot.head_local
        toe = arm.matrix_world @ ball.head_local

        forward = Vector((toe.x - ankle.x, toe.y - ankle.y, 0.0))
        if forward.length < 1e-5:
            continue
        forward.normalize()
        lateral = Vector((-forward.y, forward.x, 0.0))

        # This side's vertices: below the ankle and nearer this foot.
        other = arm.data.bones.get("foot_%s" % ("r" if side == "l" else "l"))
        other_head = (arm.matrix_world @ other.head_local) if other else None
        mine = []
        for index, point in enumerate(pts):
            if point.z > ankle.z + 0.01:
                continue
            if other_head is not None and                     (point - other_head).length < (point - ankle).length:
                continue
            mine.append(index)
        if not mine:
            continue

        us = [(pts[i] - ankle).dot(forward) for i in mine]
        u_lo, u_hi = min(us), max(us)
        span = max(u_hi - u_lo, 1e-6)

        # Measure each slice: how wide, how tall, and where its centre sits.
        wide = [0.0] * bins
        tall = [0.0] * bins
        mid = [0.0] * bins
        seen = [0] * bins
        for index, u in zip(mine, us):
            slot = min(bins - 1, max(0, int((u - u_lo) / span * bins)))
            offset = pts[index] - ankle
            wide[slot] = max(wide[slot], abs(offset.dot(lateral)))
            tall[slot] = max(tall[slot], abs(pts[index].z - ankle.z))
            mid[slot] += pts[index].z
            seen[slot] += 1
        for slot in range(bins):
            mid[slot] = (mid[slot] / seen[slot]) if seen[slot] else ankle.z

        def soften(row):
            for _ in range(3):
                row = [sum(row[max(0, i - 1):min(bins, i + 2)])
                       / len(row[max(0, i - 1):min(bins, i + 2)])
                       for i in range(bins)]
            return row

        wide, tall, mid = soften(wide), soften(tall), soften(mid)

        # A BOOT ENCLOSES THE TOES; IT DOES NOT TAPER INTO THEM.
        #
        # This narrowed the last two-fifths of the foot by up to 25%, which
        # pulled the boot INSIDE the toes and let them poke straight out the
        # front -- clearly visible as bare toes sticking through the toe cap.
        # toe_round now defaults to 1.0, so the cap keeps the foot's full
        # width, and the taper is left available for a pointed boot if one is
        # ever wanted.
        for slot in range(bins):
            t = slot / float(bins - 1)
            if t > 0.6 and toe_round < 1.0:
                fade = (t - 0.6) / 0.4
                shrink = 1.0 - (1.0 - toe_round) * fade * fade
                wide[slot] *= shrink
                tall[slot] *= shrink

        for index, u in zip(mine, us):
            f = (u - u_lo) / span * (bins - 1)
            i0 = max(0, min(bins - 1, int(f)))
            i1 = max(0, min(bins - 1, i0 + 1))
            k = f - i0
            a = wide[i0] * (1 - k) + wide[i1] * k
            b = tall[i0] * (1 - k) + tall[i1] * k
            cz = mid[i0] * (1 - k) + mid[i1] * k

            point = pts[index]
            offset = point - ankle
            v = offset.dot(lateral)
            w = point.z - cz
            angle = math.atan2(w, v) if (abs(v) > 1e-9 or abs(w) > 1e-9) else 0.0
            new_v = a * math.cos(angle)
            new_w = b * math.sin(angle)

            # Room past the toes. The last measures stop where the foot
            # stops, so without this the boot ends flush with the toe tips
            # and there is nothing in front of them.
            reach = u
            if u > u_hi - 0.02:
                reach = u + toe_room

            placed = ankle + forward * reach + lateral * new_v
            placed.z = cz + new_w
            # Flat sole, and never below the floor the character stands on.
            if placed.z < floor + sole_lift:
                placed.z = floor
            dress.data.vertices[index].co = inv @ placed

    dress.data.update()
    log("boot last: both feet rebuilt as lasts (%d slices each)" % bins)
    return dress


def make_boots(body=None, top=None, offset=0.006, thickness=0.006,
               name=None, do_bind=True):
    """Boots to the knee, over the foot.

    make_dress_shell warns that cutting below the crotch gives two leg tubes
    rather than one hem ring. For a skirt that is a failure; for boots it is
    exactly what is wanted, so the warning is expected here.

    The top sits just under the knee, read from the calf bone, so it lands
    right on a figure of any proportion.
    """
    body = body or _find_body(exclude=None)
    if body is None:
        raise RuntimeError("no avatar mesh in the scene")
    if top is None:
        knee = _bone_fraction(body, "calf_l", default=0.28)
        top = max(0.12, knee - 0.03)

    log("boots: floor to %.3f (knee %.3f)"
        % (top, _bone_fraction(body, "calf_l", default=-1.0)))
    # BELOW the floor, not at it. bottom=0.0 puts the cut plane exactly on
    # the lowest vertex of the body, so _flat_cut slices through the sole of
    # the foot and solidify then rims the opening -- which is the jagged
    # skirt of shards around the forefoot. Cutting beneath the mesh leaves
    # the sole whole and the plane does nothing.
    boot = make_dress_shell(top=top, bottom=-0.03, body=body, name="Boots",
                            strip_arms=False, keep_shoulders=False)

    # Same trap the coat fell into: the shell is a copy of the body, so a
    # boot made this way is a FOOT -- toes, arch, ankle bones -- and renders
    # as a black bare foot rather than as footwear. Smooth harder than the
    # coat, because toes are sharper than a belly, and push out further so
    # the result reads as a thick leather boot.
    # NO LAST-FITTING. boot_last replaced each cross-section of the foot
    # with a fitted ellipse, and however it was tuned the toes came through:
    # tapering the cap pulled the boot inside them, and extending it threw
    # stray vertices into spikes. The foot's OWN shape, smoothed and pushed
    # outward, already encloses the foot -- that is what a boot is -- and it
    # cannot leave the toes outside because every point of it is derived
    # from a point further in.
    #
    # Smoothed firmly but not hard. At 34 rounds and factor 0.75 the mesh
    # began collapsing on itself -- 166 degenerate edges, and vertices
    # measuring 45cm from the body on a foot -- which showed as streaking
    # all down the leg.
    # Pushed well out: a boot has a shaft with room in it, and at 14mm this
    # shrink-wrapped the calf.
    drape_shell(dress=boot, body=body, rounds=16, factor=0.55, inflate=0.024)

    # Close the toes into one mass BEFORE any clearance runs. Cleared
    # per-vertex, each toe gets its own bubble and the surface between them
    # is pushed out of the gaps as shards -- which is the lumpy, spiky
    # forefoot. Swollen first, there are no gaps left to push out of.
    cap_toes(dress=boot, body=body, swell=0.022)

    # THE FOOT NEEDS THE CLEARANCE MOST OF ALL. Excluding it left the
    # forefoot bare: drape_shell pushes radially from the body's VERTICAL
    # axis, so a toe -- which points forward, not outward -- gets moved
    # sideways rather than away from itself, and ends up inside the foot
    # with nothing left to correct it. cap_toes above already filled the
    # gaps between the toes, so clearing here no longer carves them back
    # out as shards.
    # The ankle, so the toe-blob smoothing knows where the foot ends.
    arm_b = _armature_for(body)
    ankle_z = None
    if arm_b is not None:
        bone_b = arm_b.data.bones.get("foot_l")
        if bone_b is not None:
            ankle_z = (arm_b.matrix_world @ bone_b.head_local).z + 0.015

    add_thickness(dress=boot, body=body, offset=offset, thickness=thickness)

    # MERGE THE TOE BLOBS -- AFTER solidify, AND WITHOUT RE-CLEARING.
    #
    # The clearance works per vertex against the nearest body surface, so on
    # a foot it finds each toe and gives every one its own bubble. Smoothing
    # merges them into a toe box.
    #
    # Both orderings were tried. Smoothing BEFORE solidify means the
    # re-clear that follows immediately re-carves the individual toes, and
    # the forefoot comes back lumpier than it started. Smoothing after, and
    # not clearing again, keeps the merged shape -- the clearance has
    # already done its work and re-running it only undoes this.
    if ankle_z is not None:
        touched = _smooth_region(boot, lambda p: p.z <= ankle_z,
                                 rounds=18, factor=0.7, name="_Forefoot")
        log("   forefoot: %d verts smoothed to merge the toe bubbles"
            % touched)

    for _ in range(3):
        if not _repair_outliers(boot, factor=4.0):
            break
    if do_bind:
        bind(garment=boot, body=body)
        base_weights(dress=boot, bone="pelvis")
    log("boots: %d verts, %d faces"
        % (len(boot.data.vertices), len(boot.data.polygons)))
    return boot


def make_court_suit(name=None, coat_kwargs=None, breeches_kwargs=None):
    """Coat and breeches on one prepped avatar, in the order that works.

    Breeches first: the coat's skirt is shrinkwrapped clear of whatever is
    under it, so the legs have to be dressed before the coat goes over them.
    """
    body = _find_body(exclude=None)
    if body is None:
        raise RuntimeError("no avatar mesh in the scene -- open a prepped .blend")

    breeches = make_breeches(body=body, name=name, **(breeches_kwargs or {}))
    # Waistcoat before the coat, for the same reason breeches come first:
    # the coat is cut clear of whatever is already worn.
    make_waistcoat(body=body, name=name)
    make_boots(body=body, name=name)
    coat = make_coat(body=body, name=name, **(coat_kwargs or {}))
    return coat, breeches


def suit_all(only=None, out_dir=None, name_filter="Male", **kwargs):
    """Build a coat and breeches for every prepped male avatar.

    Reads the prepped avatars from Tools/Blender and writes finished suits to
    Tools/Blender/Work, matching dress_all.

    "Male" is a substring of "Female", so the filter also excludes anything
    with "female" in it -- filtering on "Male" alone selects every character
    in the project, which would put breeches on all fifteen women.
    """
    out_dir = out_dir or os.path.join(BLEND_DIR, "Work")
    if not os.path.isdir(out_dir):
        os.makedirs(out_dir)

    blends = sorted(f for f in os.listdir(BLEND_DIR)
                    if f.lower().endswith(".blend"))
    if only:
        wanted = [o.strip().lower() for o in only if o.strip()]
        blends = [b for b in blends if any(w in b.lower() for w in wanted)]
        missing = [w for w in wanted
                   if not any(w in b.lower() for b in blends)]
        for name in missing:
            log("WARNING: no prepped .blend matched '%s'" % name)
    elif name_filter:
        blends = [b for b in blends
                  if name_filter.lower() in b.lower()
                  and "female" not in b.lower()]
    if not blends:
        log("no prepped .blend matched '%s' in %s" % (name_filter, BLEND_DIR))
        return []

    log("building court suits for %d avatar(s)" % len(blends))
    done = []
    for fname in blends:
        src = os.path.join(BLEND_DIR, fname)
        tag = (os.path.splitext(fname)[0]
               .replace("SKM_MHC_", "").replace("_BodyMesh", "").lower())
        out = os.path.join(out_dir, "%s_suit.blend" % tag)
        if os.path.isfile(out):
            log("SKIP %s -- %s already exists" % (tag, os.path.basename(out)))
            continue
        log("")
        log("--- %s ---" % tag)
        try:
            bpy.ops.wm.open_mainfile(filepath=src)
            coat, breeches = make_court_suit(name=tag, **kwargs)
            bpy.ops.wm.save_as_mainfile(filepath=out)
            done.append((tag,
                         len(coat.data.polygons), len(coat.data.vertices),
                         len(breeches.data.polygons),
                         len(breeches.data.vertices)))
            log("saved %s" % out)
        except Exception as exc:
            import traceback
            log("FAILED %s: %s" % (tag, exc))
            for line in traceback.format_exc().splitlines()[-4:]:
                log("    %s" % line)

    log("")
    log("=" * 74)
    log("%-40s %8s %8s %8s %8s"
        % ("character", "coat f", "coat v", "brch f", "brch v"))
    for tag, cf, cv, bf, bv in done:
        log("%-40s %8d %8d %8d %8d" % (tag, cf, cv, bf, bv))
    log("built %d/%d" % (len(done), len(blends)))
    log("=" * 74)
    return done


def make_suit_mask(character, size=2048, reach=0.015, margin=2,
                   work_dir=None, out_dir=None):
    """One body-hiding mask covering BOTH pieces of a character's suit.

    make_union_mask unions the four variants of a dress; a suit is not four
    variants of one garment but two garments worn at once, so the union here
    is across Coat and Breeches rather than across files. Masking from either
    one alone leaves skin showing wherever the other covers -- a coat-only
    mask leaves the legs bare under the breeches.
    """
    work = work_dir or os.path.join(BLEND_DIR, "Work")
    out_dir = out_dir or os.path.join(PROJECT, "Tools", "Export", "BodyMasks")
    path = os.path.join(work, "%s_suit.blend" % character)
    if not os.path.isfile(path):
        log("no suit for %s" % character)
        return None

    bpy.ops.wm.open_mainfile(filepath=path)
    garments = [bpy.data.objects.get(n) for n in ("Coat", "Breeches")]
    garments = [g for g in garments if g is not None]
    if not garments:
        log("%s: no Coat or Breeches in %s" % (character, path))
        return None

    body = _find_body(exclude=garments[0])
    if body is None:
        log("%s: no avatar mesh" % character)
        return None

    union = set()
    for garment in garments:
        got = _covered_verts(garment, body, reach)
        union |= got
        log("   %-10s %5d verts (union now %d)"
            % (garment.name, len(got), len(union)))

    if not union:
        log("no coverage found for %s" % character)
        return None

    out = os.path.join(out_dir, "%s_mask.1002.png" % character)
    return make_body_mask(dress=garments[0], body=body, size=size,
                          reach=reach, margin=margin, out_path=out,
                          covered=union)


def suit_masks_all(only=None, size=2048, work_dir=None, **kwargs):
    """Body mask for every character that has a finished suit."""
    import glob
    work = work_dir or os.path.join(BLEND_DIR, "Work")
    chars = sorted(os.path.basename(p).replace("_suit.blend", "")
                   for p in glob.glob(os.path.join(work, "*_suit.blend")))
    if only:
        wanted = [o.strip().lower() for o in only if o.strip()]
        chars = [c for c in chars if any(w in c.lower() for w in wanted)]

    done = []
    for character in chars:
        log("")
        log("=== %s ===" % character)
        try:
            if make_suit_mask(character, size=size, work_dir=work, **kwargs):
                done.append(character)
        except Exception as exc:
            log("FAILED %s: %s" % (character, exc))
    log("")
    log("suit masks written: %d/%d" % (len(done), len(chars)))
    return done


def export_suits(only=None, work_dir=None, out_dir=None):
    """Export every finished suit -- coat and breeches as separate FBX."""
    work = work_dir or os.path.join(BLEND_DIR, "Work")
    out_dir = out_dir or os.path.join(PROJECT, "Tools", "Export", "Garments")
    if not os.path.isdir(out_dir):
        os.makedirs(out_dir)

    blends = sorted(f for f in os.listdir(work) if f.endswith("_suit.blend"))
    if only:
        wanted = [o.strip().lower() for o in only if o.strip()]
        blends = [b for b in blends if any(w in b.lower() for w in wanted)]

    done, failed = [], []
    for fname in blends:
        tag = fname.replace("_suit.blend", "")
        try:
            bpy.ops.wm.open_mainfile(filepath=os.path.join(work, fname))
            for obj_name, prefix in (("Coat", "Coat"), ("Breeches", "Breeches")):
                garment = bpy.data.objects.get(obj_name)
                if garment is None:
                    log("   %s: no '%s' object" % (tag, obj_name))
                    failed.append("%s/%s" % (tag, obj_name))
                    continue
                out = os.path.join(out_dir, "%s_%s.fbx" % (prefix, tag))
                if os.path.isfile(out):
                    log("SKIP %s" % os.path.basename(out))
                    continue
                export_garment(out, garment=garment)
                done.append(os.path.basename(out))
        except Exception as exc:
            log("FAILED %s: %s" % (tag, exc))
            failed.append(tag)

    log("")
    log("exported %d, failed %d -> %s" % (len(done), len(failed), out_dir))
    for tag in failed:
        log("   FAILED %s" % tag)
    return done


def _cli():
    argv = sys.argv
    args = argv[argv.index("--") + 1:] if "--" in argv else []

    def opt(name, default=None):
        return args[args.index(name) + 1] if name in args else default

    if args and args[0] == "prep":
        if "--all" in args:
            only = opt("--only")
            prep_all(opt("--dir"), opt("--out-dir"),
                     lock="--unlocked" not in args,
                     only=only.split(",") if only else None)
            return
        avatar = opt("--avatar")
        if not avatar:
            log("prep needs --avatar FBX or --all")
            return
        prep_avatar(avatar, opt("--out"), lock="--unlocked" not in args)
        return

    if args and args[0] == "dress":
        kwargs = {}
        if "--top" in args:
            kwargs["top"] = float(opt("--top"))
        if "--bottom" in args:
            raw = opt("--bottom")
            kwargs["bottom"] = raw if raw == "auto" else float(raw)
        only = opt("--only")
        dress_all(name_filter=opt("--filter", "Female"),
                  out_dir=opt("--out-dir"),
                  only=only.split(",") if only else None, **kwargs)
        return

    if args and args[0] == "court":
        kwargs = {}
        for flag, cast in (("--passes", int), ("--train", float),
                           ("--waist", float), ("--clearance", float),
                           ("--thickness", float), ("--offset", float),
                           ("--loops", int)):
            if flag in args:
                kwargs[flag[2:]] = cast(opt(flag))
        if "--sleeve" in args:
            kwargs["sleeve"] = opt("--sleeve")
            kwargs["strap"] = opt("--strap", "thin")
        if "--trim" in args:
            kwargs["trim"] = True
        if "--uvs" in args:
            kwargs["uvs"] = True
        if "--no-bind" in args:
            kwargs["do_bind"] = False
        only = opt("--only")
        court_all(only=only.split(",") if only else None, **kwargs)
        return

    if args and args[0] == "variants":
        kwargs = {}
        for flag, cast in (("--passes", int), ("--train", float),
                           ("--waist", float), ("--clearance", float),
                           ("--loops", int)):
            if flag in args:
                kwargs[flag[2:]] = cast(opt(flag))
        only = opt("--only")
        styles = None
        if "--sleeve" in args or "--strap" in args:
            sleeves = (opt("--sleeve") or ",".join(SLEEVE_STYLES)).split(",")
            straps = (opt("--strap") or ",".join(STRAP_WIDTHS)).split(",")
            styles = [(sl.strip(), st.strip()) for sl in sleeves for st in straps]
        variants_all(only=only.split(",") if only else None,
                     styles=styles, **kwargs)
        return

    if args and args[0] == "suit":
        kwargs, coat_kwargs, breeches_kwargs = {}, {}, {}
        if "--collar" in args:
            coat_kwargs["collar"] = float(opt("--collar"))
        if "--skirt-to" in args:
            coat_kwargs["skirt_to"] = float(opt("--skirt-to"))
        if "--flare" in args:
            coat_kwargs["flare"] = float(opt("--flare"))
        if "--sleeve" in args:
            coat_kwargs["sleeve"] = opt("--sleeve")
        if "--strap" in args:
            coat_kwargs["strap"] = opt("--strap")
        if "--waist" in args:
            breeches_kwargs["waist"] = float(opt("--waist"))
        if "--knee" in args:
            breeches_kwargs["knee"] = float(opt("--knee"))
        if "--no-trim" in args:
            coat_kwargs["trim"] = breeches_kwargs["trim"] = False
        if coat_kwargs:
            kwargs["coat_kwargs"] = coat_kwargs
        if breeches_kwargs:
            kwargs["breeches_kwargs"] = breeches_kwargs
        only = opt("--only")
        suit_all(only=only.split(",") if only else None,
                 name_filter=opt("--filter", "Male"), **kwargs)
        return

    if args and args[0] == "suitmasks":
        kwargs = {}
        if "--size" in args:
            kwargs["size"] = int(opt("--size"))
        if "--reach" in args:
            kwargs["reach"] = float(opt("--reach"))
        if "--margin" in args:
            kwargs["margin"] = int(opt("--margin"))
        only = opt("--only")
        suit_masks_all(only=only.split(",") if only else None, **kwargs)
        return

    if args and args[0] == "exportsuits":
        only = opt("--only")
        export_suits(only=only.split(",") if only else None,
                     out_dir=opt("--out-dir"))
        return

    if args and args[0] == "export":
        only = opt("--only")
        export_all(only=only.split(",") if only else None,
                   out_dir=opt("--out-dir"))
        return

    if args and args[0] == "masks":
        kwargs = {}
        if "--size" in args:
            kwargs["size"] = int(opt("--size"))
        if "--reach" in args:
            kwargs["reach"] = float(opt("--reach"))
        if "--margin" in args:
            kwargs["margin"] = int(opt("--margin"))
        only = opt("--only")
        masks_all(only=only.split(",") if only else None, **kwargs)
        return

    if args and args[0] == "trim":
        kwargs = {}
        for flag, cast in (("--width", float), ("--gap", float),
                           ("--inset", float), ("--height", float)):
            if flag in args:
                kwargs[flag[2:]] = cast(opt(flag))
        where = opt("--where", "hem,neck")
        kwargs["where"] = tuple(w.strip() for w in where.split(","))
        only = opt("--only")
        trim_all(only=only.split(",") if only else None, **kwargs)
        return

    if args and args[0] == "uvs":
        only = opt("--only")
        uvs_all(only=only.split(",") if only else None)
        return

    log("usage: blender -b --python blender_metahuman_garment.py -- <command>")
    log("  prep  [--all | --avatar FBX] [--out BLEND] [--out-dir DIR]")
    log("  dress [--filter Female] [--top 0.93] [--bottom auto] [--out-dir DIR]")


if bpy.app.background and "--" in sys.argv:
    _cli()
