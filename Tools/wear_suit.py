"""Import a generated suit and put it on its Kith Blueprint and placed actors.

    python Tools/ue_remote.py Tools/wear_suit.py

The Blueprint is set as well as the instance, because a placed actor carries
its own serialised component values and will not pick up a change made only
to the Blueprint -- and a Blueprint left unset would dress nothing that gets
placed later.
"""

import os

import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
GDIR = os.path.join(PROJECT, "Tools", "Export", "Garments")
DEST = "/Game/_Custom/Garments"
# Black, per the reference coat. Plum read as pink in daylight.
FABRIC = "/Game/_Custom/Materials/Garments/MI_CourtFabric_Black"
TRIM = "/Game/_Custom/Materials/Garments/M_CourtTrim"

NAME = "Suit_boggan_childling_male"
BODY = "Body_Suit_boggan_childling_male"
BLUEPRINT = "/Game/_Custom/Characters/Kith/Blueprints/BP_Boggan_Childling_Male"


def log(msg):
    unreal.log("[suit] %s" % msg)


def skeleton_of(path):
    existing = unreal.load_asset(path)
    return existing.get_editor_property("skeleton") if existing else None


def bring_in(name, skeleton):
    path = "%s/%s" % (DEST, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(GDIR, "%s.fbx" % name))
    task.set_editor_property("destination_path", DEST)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", True)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("import_animations", False)
    if skeleton:
        options.set_editor_property("skeleton", skeleton)
    options.skeletal_mesh_import_data.set_editor_property("convert_scene", True)
    task.set_editor_property("options", options)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    return unreal.load_asset(path), path


def dress_asset(mesh, path):
    """Court fabric on the garment, gold on the trim bands.

    Fresh SkeletalMaterial structs, not the ones the getter hands back:
    mutating those and setting the array again is accepted and silently does
    nothing. And slot 0 arrives as the BODY's skin material, because the coat
    shell is built from body geometry.
    """
    fabric, trim = unreal.load_asset(FABRIC), unreal.load_asset(TRIM)
    fresh = []
    for slot in mesh.get_editor_property("materials"):
        name = str(slot.get_editor_property("material_slot_name"))
        made = unreal.SkeletalMaterial()
        made.set_editor_property("material_interface",
                                 trim if "Trim" in name else fabric)
        made.set_editor_property("material_slot_name", name)
        fresh.append(made)
    mesh.set_editor_property("materials", fresh)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    for slot in unreal.load_asset(path).get_editor_property("materials"):
        which = slot.get_editor_property("material_interface")
        log("   %-36s -> %s" % (slot.get_editor_property("material_slot_name"),
                                which.get_name() if which else "NONE"))


def body_material(component_mesh):
    """The skin material, from the ORIGINAL body asset.

    Reading it off whatever body is currently assigned poisons itself: on a
    second run that body IS the trimmed one, whose material is whatever was
    copied onto it last -- and the first run copied nothing, so it came back
    WorldGridMaterial and the character stood there in grey legs.
    """
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    for asset in registry.get_assets_by_path(
            "/Game/_Custom/Characters/Kith", recursive=True):
        name = str(asset.asset_name)
        if not name.endswith("_BodyMesh") or "Childling_Male" not in name:
            continue
        if "Female" in name:
            continue
        mesh = unreal.load_asset(str(asset.package_name))
        if mesh is None:
            continue
        slots = mesh.get_editor_property("materials")
        if slots:
            skin = slots[0].get_editor_property("material_interface")
            if skin is not None and skin.get_name() != "WorldGridMaterial":
                log("skin material from %s: %s" % (name, skin.get_name()))
                return skin
    if component_mesh is not None:
        slots = component_mesh.get_editor_property("materials")
        if slots:
            return slots[0].get_editor_property("material_interface")
    return None


def dress_body(mesh, skin):
    """Put the skin material back on the trimmed body."""
    if skin is None:
        return
    fresh = []
    for slot in mesh.get_editor_property("materials"):
        made = unreal.SkeletalMaterial()
        made.set_editor_property("material_interface", skin)
        made.set_editor_property("material_slot_name",
                                 slot.get_editor_property("material_slot_name"))
        fresh.append(made)
    mesh.set_editor_property("materials", fresh)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)


def set_body(trimmed):
    """Swap the body for the trimmed one, on the Blueprint and the instances.

    The garment's bind pose and the skeleton's reference pose disagree about
    the arm, so the body pushes out through the sleeve no matter how much
    clearance the cloth is given -- raising it twice changed nothing. Deleting
    the covered body is the only thing that actually settles it, and it costs
    nothing at runtime.
    """
    blueprint = unreal.load_asset(BLUEPRINT)
    skin = None
    if blueprint is not None:
        sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
        lib = unreal.SubobjectDataBlueprintFunctionLibrary
        for handle in sub.k2_gather_subobject_data_for_blueprint(blueprint):
            obj = lib.get_object(lib.get_data(handle))
            if isinstance(obj, unreal.SkeletalMeshComponent) and                     not obj.component_has_tag("Outfit") and                     obj.get_name() in ("CharacterMesh0", "Mesh"):
                skin = body_material(obj.get_editor_property("skeletal_mesh_asset"))
                dress_body(trimmed, skin)
                obj.set_editor_property("skeletal_mesh_asset", trimmed)
                unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
                unreal.EditorAssetLibrary.save_asset(BLUEPRINT)
                log("Blueprint body <- %s" % trimmed.get_name())
                break

    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for actor in sub.get_all_level_actors() or []:
        if not actor.get_class().get_name().startswith("BP_Boggan_Childling_Male"):
            continue
        body = outfit = None
        for comp in actor.get_components_by_class(unreal.SkeletalMeshComponent):
            if comp.component_has_tag("Outfit"):
                outfit = comp
            elif comp.get_name() == "CharacterMesh0":
                body = comp
        if body is None:
            continue
        body.set_editor_property("skeletal_mesh_asset", trimmed)
        if outfit is not None:
            outfit.set_leader_pose_component(body)
        log("%-30s body <- %s" % (actor.get_actor_label(), trimmed.get_name()))


def set_on_blueprint(mesh):
    blueprint = unreal.load_asset(BLUEPRINT)
    if blueprint is None:
        log("no Blueprint at %s" % BLUEPRINT)
        return
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    for handle in sub.k2_gather_subobject_data_for_blueprint(blueprint):
        obj = lib.get_object(lib.get_data(handle))
        if isinstance(obj, unreal.SkeletalMeshComponent) and \
                obj.component_has_tag("Outfit"):
            obj.set_editor_property("skeletal_mesh_asset", mesh)
            unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
            unreal.EditorAssetLibrary.save_asset(BLUEPRINT)
            log("Blueprint Outfit <- %s" % mesh.get_name())
            return
    log("no Outfit component on the Blueprint")


def set_on_actors(mesh):
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    dressed = 0
    for actor in sub.get_all_level_actors() or []:
        if not actor.get_class().get_name().startswith("BP_Boggan_Childling_Male"):
            continue
        body = slot = None
        for comp in actor.get_components_by_class(unreal.SkeletalMeshComponent):
            if comp.component_has_tag("Outfit"):
                slot = comp
            elif comp.get_name() == "CharacterMesh0":
                body = comp
        if slot is None:
            continue
        slot.set_editor_property("skeletal_mesh_asset", mesh)
        if body is not None:
            slot.set_leader_pose_component(body)
        dressed += 1
        log("%-30s Outfit <- %s (leader %s)"
            % (actor.get_actor_label(), mesh.get_name(),
               "set" if body else "NONE"))
    log("dressed %d placed actor(s); the LEVEL is dirty and not saved here"
        % dressed)


def main():
    log("=" * 66)
    skeleton = skeleton_of("%s/Frock_boggan_childling_male" % DEST)
    log("skeleton: %s" % (skeleton.get_name() if skeleton else "NONE"))
    mesh, path = bring_in(NAME, skeleton)
    if mesh is None:
        log("import FAILED")
        return
    bounds = mesh.get_bounds().box_extent
    log("%s: extent x %.1f y %.1f z %.1f" % (NAME, bounds.x, bounds.y, bounds.z))
    dress_asset(mesh, path)
    set_on_blueprint(mesh)
    set_on_actors(mesh)

    trimmed, _ = bring_in(BODY, skeleton)
    if trimmed is None:
        log("trimmed body: import FAILED")
    else:
        bounds = trimmed.get_bounds().box_extent
        log("%s: extent x %.1f y %.1f z %.1f"
            % (BODY, bounds.x, bounds.y, bounds.z))
        set_body(trimmed)
    log("=" * 66)


main()
