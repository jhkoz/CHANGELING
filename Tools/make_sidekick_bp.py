"""Put the assembled Sidekick NPC on a Blueprint and stand it next to its
MetaHuman counterpart in the level, so the two can be judged side by side.

Parented to AChangelingNPC, the same native class the MetaHuman Boggans use,
so the comparison is of the ART and not of two different actor setups. The
Outfit component and BindOutfits come along for free.
"""
import os, unreal

PROJECT = os.path.normpath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "sidekick_bp.txt")

MESH_DIR = "/Game/_Custom/Characters/Sidekick"
BP_DIR = "/Game/_Custom/Characters/Sidekick/Blueprints"
# Which character to build, from a sidecar file. NOT the environment: this
# runs inside the editor process, which has its own, so anything exported by
# the calling shell is invisible here.
_ARGS = os.path.join(PROJECT, "Tools", "Export", "sidekick_target.txt")
NAME = "Boggan_Childling_Male"
if os.path.exists(_ARGS):
    _t = open(_ARGS).read().strip()
    if _t:
        NAME = _t

_lines = []
def log(m=""):
    _lines.append(str(m)); unreal.log(str(m))

def flush():
    d = os.path.dirname(OUT)
    if not os.path.isdir(d): os.makedirs(d)
    open(OUT, "w").write("\n".join(_lines))


def inspect(mesh):
    log("-" * 66)
    log("MESH  %s" % mesh.get_name())
    sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    try:
        log("   LODs      : %d" % sub.get_lod_count(mesh))
    except Exception as exc:
        log("   LODs      : ? (%s)" % exc)
    try:
        log("   verts LOD0: %d" % sub.get_num_verts(mesh, 0))
    except Exception:
        pass
    box = mesh.get_bounds().box_extent
    log("   extent    : x %.1f  y %.1f  z %.1f  (height %.1f cm)"
        % (box.x, box.y, box.z, box.z * 2.0))
    mats = mesh.get_editor_property("materials")
    log("   materials : %d" % len(mats))
    for slot in mats:
        iface = slot.get_editor_property("material_interface")
        log("       %-24s %s" % (slot.get_editor_property("material_slot_name"),
                                 iface.get_name() if iface else "*** NONE ***"))
    phys = mesh.get_editor_property("physics_asset")
    log("   physics   : %s" % (phys.get_name() if phys else "NONE"))


def make_bp(mesh):
    path = "%s/BP_SK_%s" % (BP_DIR, NAME)
    existing = unreal.load_asset(path)
    if existing is None:
        factory = unreal.BlueprintFactory()
        parent = unreal.load_object(None, "/Script/CHANGELING.ChangelingNPC")
        if parent is None:
            log("   ChangelingNPC not found -- falling back to Character")
            parent = unreal.Character.static_class()
        factory.set_editor_property("parent_class", parent)
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "BP_SK_%s" % NAME, BP_DIR, None, factory)
    else:
        bp = existing
    if bp is None:
        log("   FAILED to create Blueprint"); return None, None

    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    body = None
    for handle in sub.k2_gather_subobject_data_for_blueprint(bp):
        obj = lib.get_object(lib.get_data(handle))
        if isinstance(obj, unreal.SkeletalMeshComponent) and \
           not obj.component_has_tag("Outfit"):
            body = obj
            break
    if body is None:
        log("   no body mesh component"); return bp, path

    body.set_editor_property("skeletal_mesh_asset", mesh)

    # Fit the capsule to THIS character rather than keeping Character's 88cm
    # half-height default, which is a 176cm adult. A childling in an adult
    # capsule floats, collides at the wrong radius and cannot walk under
    # anything built for its size.
    extent = mesh.get_bounds().box_extent
    half = max(extent.z, 20.0)
    radius = max(extent.y, 12.0)
    for handle in sub.k2_gather_subobject_data_for_blueprint(bp):
        obj = lib.get_object(lib.get_data(handle))
        if isinstance(obj, unreal.CapsuleComponent):
            obj.set_editor_property("capsule_half_height", half)
            obj.set_editor_property("capsule_radius", radius)
            log("   capsule   : half-height %.1f, radius %.1f" % (half, radius))
            break

    # Rotator is (ROLL, PITCH, YAW). Passing -90 positionally puts it in pitch
    # and lays the character on its face; the usual mesh yaw is the third.
    body.set_editor_property("relative_location", unreal.Vector(0, 0, -half))
    body.set_editor_property("relative_rotation",
                             unreal.Rotator(0.0, 0.0, -90.0))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_asset(path)
    log("   Blueprint : %s  (mesh set on %s)" % (path, body.get_name()))
    return bp, path


def place(path):
    """Stand the test character where it can actually be seen.

    World origin is under the landscape in this map, so an actor spawned there
    renders as a silhouette against the underside of the terrain. The
    PlayerStart is on open ground and is where the character will first be met
    anyway, so the test actor goes beside it.
    """
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if sub is None:
        log("   no EditorActorSubsystem"); return

    where, rot = None, unreal.Rotator(0, 0, 0)
    for actor in sub.get_all_level_actors() or []:
        if actor.get_class().get_name() == "PlayerStart":
            loc = actor.get_actor_location()
            where = unreal.Vector(loc.x + 200.0, loc.y, loc.z)
            log("   beside PlayerStart at %.0f,%.0f,%.0f" % (loc.x, loc.y, loc.z))
            break
    if where is None:
        where = unreal.Vector(0, 0, 200)
        log("   no PlayerStart; placing at origin")

    bp_class = unreal.EditorAssetLibrary.load_blueprint_class(path)
    for actor in sub.get_all_level_actors() or []:
        if actor.get_class().get_name().startswith("BP_SK_"):
            sub.destroy_actor(actor)
    actor = sub.spawn_actor_from_class(bp_class, where, rot)
    if actor is None:
        log("   FAILED to spawn"); return
    actor.set_actor_label("SK_%s" % NAME)
    loc = actor.get_actor_location()
    log("   placed    : %s at %.0f,%.0f,%.0f -- the LEVEL is dirty, save it"
        " yourself" % (actor.get_actor_label(), loc.x, loc.y, loc.z))


def main():
    log("=" * 66)
    log("make_sidekick_bp")
    log("=" * 66)
    mesh = unreal.load_asset("%s/%s" % (MESH_DIR, NAME))
    if mesh is None:
        log("no imported mesh at %s/%s" % (MESH_DIR, NAME)); return
    inspect(mesh)
    log("")
    bp, path = make_bp(mesh)
    if path:
        place(path)
    log("=" * 66)

try:
    main()
finally:
    flush()
    unreal.log("report: %s" % OUT)
