"""
assemble_sidekick.py -- build one NPC from Synty Sidekick parts, in Blender.

    blender -b --python Tools/assemble_sidekick.py -- \
        --name Boggan_Childling_Male --height 0.72 --girth 1.18 --head 1.12

WHAT SIDEKICK ACTUALLY PROVIDES

Not ten whole characters. One body -- torso, hips, arms, hands, legs, feet --
two heads, and ten or eleven variants of each facial feature, every part
skinned to the single SKEL_Default_Sidekick. So:

    identity   comes from the face: 2 heads x 10 hair x 10 beards x 11 noses
               x 10 ears x 10 brows, over two million combinations
    build      comes from proportion scaling here, because there is only one
               body mesh to work with

That split is what makes a kith system practical. A Boggan and a Sidhe wear
the same body mesh at different proportions, and their faces differ by part
selection rather than by sculpting.

PROPORTION, AND WHY IT IS NOT A UNIFORM SCALE

Scaling the whole character uniformly just makes a smaller adult. A childling
is short with a proportionally LARGER head, and a Boggan is stout, so height,
girth and head size are separate multipliers:

    height   vertical scale of the whole figure
    girth    horizontal scale, applied below the neck so the head stays round
    head     extra scale on the head and its features, about the neck joint

Scaling is applied to the MESH, not to the object transform, so the export
carries real geometry rather than a scale Unreal would have to honour.

THE ARMATURE IS LEFT ALONE

Bones are not scaled. Every part stays bound to SKEL_Default_Sidekick exactly
as Synty rigged it, so one animation set drives every NPC regardless of
proportion. Scaling the skeleton instead would need a retarget per kith.
"""

import os
import re
import sys

import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, ".."))
PARTS_DIR = os.path.join(PROJECT, "Tools", "Export", "SidekickParts")
OUTFIT_DIR = os.path.join(PROJECT, "Tools", "Export", "SidekickOutfits")
OUT_DIR = os.path.join(PROJECT, "Tools", "Export", "Characters")

# Parts that belong to the head and follow the head scale.
HEAD_SLOTS = {"01HEAD", "02HAIR", "03EBRL", "04EBRR", "05EYEL", "06EYER",
              "07EARL", "08EARR", "09FCHR", "35NOSE", "36TETH", "37TONG"}


def log(msg=""):
    print("[assemble] %s" % msg)


def opt(args, name, default=None):
    return args[args.index(name) + 1] if name in args else default


def clear():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def import_parts(drop=(), outfit=None):
    """Bring in every slot the character needs. Returns (meshes, armature).

    drop names slots to leave out, which is how a face is cast rather than
    sculpted: a childling is a childling partly because it has no beard, so
    09FCHR simply does not get imported for one.

    outfit names a Sidekick outfit whose slots REPLACE the bare body's. This
    is the whole reason clothing works here and did not on the MetaHuman
    route. There, a garment cut for a tall slim figure had to be deformed onto
    a short stout one by pushing vertices around, with nothing to keep a coat
    skirt standing away from the body -- it ended up pressed flat against the
    breeches and z-fighting them. Here a dressed torso is simply a different
    10TORS, already skinned and already cut to the base body, so there is
    nothing to fit. Reshaping then applies to body and clothing alike, so the
    fit holds at any proportion.
    """
    if not os.path.isdir(PARTS_DIR):
        log("no parts at %s -- run export_sidekick_parts first" % PARTS_DIR)
        return [], None

    sources = {}
    for name in sorted(os.listdir(PARTS_DIR)):
        if name.lower().endswith(".fbx"):
            sources[os.path.splitext(name)[0]] = os.path.join(PARTS_DIR, name)

    if outfit:
        worn = os.path.join(OUTFIT_DIR, outfit)
        if not os.path.isdir(worn):
            log("no outfit at %s -- run export_sidekick_outfit first" % worn)
            return [], None
        taken = []
        for name in sorted(os.listdir(worn)):
            if not name.lower().endswith(".fbx"):
                continue
            slot = os.path.splitext(name)[0]
            taken.append(slot)
            sources[slot] = os.path.join(worn, name)
        log("outfit %s: %d slot(s) over the bare body" % (outfit, len(taken)))

    for slot in list(sources):
        if any(d.upper() in slot.upper() for d in drop):
            del sources[slot]
    if drop:
        log("dropping slot(s): %s" % ", ".join(drop))

    if not sources:
        log("nothing to import")
        return [], None

    for slot in sorted(sources):
        bpy.ops.import_scene.fbx(filepath=sources[slot])

    # A skeletal FBX brings its own armature per file. Keep the first and
    # re-point everything at it, or the join below fails across armatures.
    armatures = [o for o in bpy.data.objects if o.type == "ARMATURE"]
    meshes = [o for o in bpy.data.objects
              if o.type == "MESH" and not o.name.endswith("_LOD1")]
    for o in [o for o in bpy.data.objects
              if o.type == "MESH" and o.name.endswith("_LOD1")]:
        bpy.data.objects.remove(o, do_unlink=True)

    log("imported %d slot(s), %d armature(s)" % (len(meshes), len(armatures)))
    if not armatures:
        return meshes, None

    keep = armatures[0]
    for mesh in meshes:
        for mod in mesh.modifiers:
            if mod.type == "ARMATURE":
                mod.object = keep
        if mesh.parent in armatures and mesh.parent is not keep:
            mesh.parent = keep
    for extra in armatures[1:]:
        bpy.data.objects.remove(extra, do_unlink=True)
    return meshes, keep


def neck_height(armature):
    """Where the head starts, for scaling the head about its own joint."""
    for candidate in ("neck_01", "neck", "head", "spine_03"):
        bone = armature.data.bones.get(candidate)
        if bone:
            return (armature.matrix_world @ bone.head_local).z
    return None


def torso_half_width(meshes, height):
    """Half-width of the hips, which is the torso without the arms in the way.

    Measured low on the figure: at chest height the widest vertices are the
    outstretched arms, which is exactly what must not be counted.
    """
    pts = [m.matrix_world @ v.co for m in meshes for v in m.data.vertices]
    zs = [p.z for p in pts]
    lo, hi = min(zs), max(zs)
    band = [p for p in pts if lo + (hi - lo) * 0.45 <= p.z <= lo + (hi - lo) * 0.60]
    if not band:
        return max(abs(p.x) for p in pts) * 0.25
    return max(abs(p.x) for p in band)


def reshape(meshes, armature, height, girth, head_scale):
    """Apply proportion to the geometry, leaving the skeleton untouched."""
    torso_half = torso_half_width(meshes, height)
    log("torso half-width %.3f (arms shift rather than stretch)" % torso_half)
    neck = neck_height(armature) if armature else None
    if neck is None:
        zs = [(m.matrix_world @ v.co).z for m in meshes for v in m.data.vertices]
        neck = min(zs) + (max(zs) - min(zs)) * 0.82
        log("no neck bone found; estimating the head joint at z %.3f" % neck)

    for mesh in meshes:
        slot = mesh.name.split("_")[0]
        is_head = any(mesh.name.startswith(s) or s in mesh.name
                      for s in HEAD_SLOTS)
        mw = mesh.matrix_world
        inv = mw.inverted()
        for vert in mesh.data.vertices:
            world = mw @ vert.co
            # Height first, about the feet.
            world.z *= height
            # Girth below the neck only, so the head does not go oval.
            #
            # Depth scales outright, but WIDTH only widens the torso: the
            # character is in T-pose, so scaling x across the whole figure
            # stretches the arms longer instead of making him broader. Past
            # the torso each vertex is SHIFTED by the amount the torso gained,
            # which moves the arms outward with the shoulders while keeping
            # their length.
            if world.z < neck * height:
                world.y *= girth
                if abs(world.x) <= torso_half:
                    world.x *= girth
                else:
                    gain = torso_half * (girth - 1.0)
                    world.x += gain if world.x > 0 else -gain
            if is_head and head_scale != 1.0:
                pivot = neck * height
                world.x *= head_scale
                world.y *= head_scale
                world.z = pivot + (world.z - pivot) * head_scale
            vert.co = inv @ world
        mesh.data.update()
    log("reshaped: height x%.2f, girth x%.2f, head x%.2f"
        % (height, girth, head_scale))


def unify_materials(meshes):
    """Collapse the per-part material copies back down to Synty's originals.

    Every part is imported from its own FBX, so Blender gives each one a
    private copy of the material it references -- M_Default_Sidekick,
    M_Default_Sidekick.001, .002 and so on. Joining then keeps all of them,
    and the character arrives in Unreal with two dozen material slots that are
    two dozen draw calls for what Synty authored as ONE material over a colour
    atlas.

    Stripping the .NNN suffix identifies the original, so the whole character
    collapses to a single slot.
    """
    canon = {}
    for mesh in meshes:
        for slot in mesh.material_slots:
            mat = slot.material
            if mat is None:
                continue
            base = re.sub(r"\.\d+$", "", mat.name)
            keep = canon.get(base)
            if keep is None:
                keep = bpy.data.materials.get(base) or mat
                canon[base] = keep
            if slot.material is not keep:
                slot.material = keep
    for mesh in meshes:
        seen = []
        for slot in mesh.material_slots:
            if slot.material is not None and slot.material not in seen:
                seen.append(slot.material)
    log("materials: %d unique (%s)"
        % (len(canon), ", ".join(sorted(canon)) or "none"))
    return canon


def join(meshes):
    bpy.ops.object.select_all(action="DESELECT")
    for mesh in meshes:
        mesh.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    body = bpy.context.view_layer.objects.active
    collapse_slots(body)
    return body


def collapse_slots(body):
    """Leave one material slot per material on the joined mesh.

    unify_materials points every slot at the original material, but joining
    keeps one SLOT per part regardless, and a slot is what becomes a section
    in Unreal -- so the character still arrives as two dozen draw calls that
    all bind the same texture. Faces are re-pointed at the first slot holding
    their material and the emptied slots are dropped.
    """
    mats = body.data.materials
    if len(mats) < 2:
        return
    first = {}
    remap = {}
    for index, mat in enumerate(mats):
        key = mat.name if mat else ""
        if key not in first:
            first[key] = len(first)
        remap[index] = first[key]
    for poly in body.data.polygons:
        poly.material_index = remap.get(poly.material_index, 0)

    keep = []
    seen = set()
    for mat in mats:
        key = mat.name if mat else ""
        if key not in seen:
            seen.add(key)
            keep.append(mat)
    before = len(mats)
    mats.clear()
    for mat in keep:
        mats.append(mat)
    log("slots: %d -> %d" % (before, len(mats)))


def main():
    argv = sys.argv
    args = argv[argv.index("--") + 1:] if "--" in argv else []
    name = opt(args, "--name", "Sidekick_NPC")
    height = float(opt(args, "--height", "1.0"))
    girth = float(opt(args, "--girth", "1.0"))
    head = float(opt(args, "--head", "1.0"))
    drop = [d for d in (opt(args, "--drop", "") or "").split(",") if d]
    outfit = opt(args, "--outfit", None)

    log("=" * 62)
    log("assemble_sidekick  %s" % name)
    log("=" * 62)

    clear()
    meshes, armature = import_parts(drop, outfit)
    if not meshes:
        return
    if armature is None:
        log("no armature -- the parts would export unskinned")
        return

    reshape(meshes, armature, height, girth, head)

    unify_materials(meshes)

    body = join(meshes)
    body.name = name
    log("joined: %d verts, %d faces, %d vertex groups"
        % (len(body.data.vertices), len(body.data.polygons),
           len(body.vertex_groups)))

    pts = [body.matrix_world @ v.co for v in body.data.vertices]
    xs = [p.x for p in pts]
    ys = [p.y for p in pts]
    zs = [p.z for p in pts]
    log("size: x %.3f  y %.3f  z %.3f" % (max(xs) - min(xs),
                                          max(ys) - min(ys),
                                          max(zs) - min(zs)))

    if not os.path.isdir(OUT_DIR):
        os.makedirs(OUT_DIR)
    out = os.path.join(OUT_DIR, "%s.fbx" % name)

    for obj in (body, armature):
        try:
            obj.hide_set(False)
        except RuntimeError:
            pass
        obj.hide_viewport = False
    bpy.ops.object.select_all(action="DESELECT")
    body.select_set(True)
    armature.select_set(True)
    bpy.context.view_layer.objects.active = armature

    # The armature object's name becomes the root bone in Unreal.
    original = armature.name
    armature.name = "root"
    try:
        bpy.ops.export_scene.fbx(
            filepath=out, use_selection=True,
            object_types={"ARMATURE", "MESH"}, use_mesh_modifiers=True,
            mesh_smooth_type="FACE", add_leaf_bones=False,
            use_armature_deform_only=False, bake_anim=False,
            primary_bone_axis="Y", secondary_bone_axis="X",
            apply_scale_options="FBX_SCALE_NONE", global_scale=1.0,
            path_mode="COPY", embed_textures=False)
    finally:
        armature.name = original
    log("exported %s (%.1f MB)" % (out, os.path.getsize(out) / 1048576.0))


main()
