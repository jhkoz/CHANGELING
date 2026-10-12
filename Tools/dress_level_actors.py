"""
dress_level_actors.py -- put the outfit on the Boggans already placed in the level.

    python Tools/ue_remote.py Tools/dress_level_actors.py

WHY THE BLUEPRINT WAS NOT ENOUGH

The Blueprints carry the right mesh and the assembled outfit assets are
correct -- 68,489 verts with all twelve frock-coat materials assigned. But a
placed actor reads:

    Outfit   mesh=NONE   tag=True   leader=NONE

These actors were placed in the level BEFORE the native Outfit component
existed. The level serialises each actor's components, so they carry an empty
slot of their own and never pick up the Blueprint's value. That is why the
characters showed bare arms and no coat while every asset-level check passed:
the outfit was never on them.

BindOutfits correctly reported no leader, because it skips a slot with no mesh
rather than binding nothing.

WHAT THIS DOES

For each placed Boggan, works out which assembled outfit belongs to it from
its Blueprint class, sets the mesh on the instance, and points its leader pose
at the body so it follows the animation. The level is left dirty for you to
save -- this does not save the map itself.

Mind the naming: the Blueprint is forward-ordered (BP_Boggan_Wilder_Male) and
so is the character folder the outfit was assembled into
(MHC_Boggan_Wilder_Male), even though the BODY it references is the
reversed-name one. The outfit is found from the Blueprint name, not the body.
"""

import os

import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "dress_level_actors.txt")
KITH = "/Game/_Custom/Characters/Kith"

_lines = []


def log(msg=""):
    text = str(msg)
    _lines.append(text)
    unreal.log(text)


def flush():
    out_dir = os.path.dirname(OUT)
    if not os.path.isdir(out_dir):
        os.makedirs(out_dir)
    with open(OUT, "w") as handle:
        handle.write("\n".join(_lines))


GARMENTS = "/Game/_Custom/Garments"


def outfit_for(class_name):
    """BP_Boggan_Wilder_Male_C -> the skinned garment fitted to that character.

    Now the imported garment rather than an assembled <Character>_Outfits.
    Nothing is assembled any more: the garment is already skinned to
    metahuman_base_skel, so it goes straight onto the component and the whole
    cloth-asset / outfit-asset / build_meta_human chain is skipped.

    Both name spellings are tried. Most garments are named for a
    forward-ordered avatar, but a character never assembled into a
    forward-named folder -- MHC_Boggan_Grump_Female -- was fitted against the
    legacy reversed blend and carries that spelling instead.
    """
    stem = class_name[:-2] if class_name.endswith("_C") else class_name
    if not stem.startswith("BP_"):
        return None, None
    parts = stem[3:].split("_")
    if len(parts) < 3:
        return None, None
    clan, seeming, sex = parts[0], parts[1], parts[2]
    prefix = "Gown" if sex.lower() == "female" else "Frock"
    # Outfit_<clan>_<seeming>_<sex> first: that is what make_outfit and
    # export_outfit produce, a joined four-piece garment, and where one exists
    # it supersedes the older single-piece Frock/Gown below. Kept in step with
    # the same list in wear_garments.garment_for.
    for name in ("Outfit_%s_%s_%s" % (clan.lower(), seeming.lower(), sex.lower()),
                 "%s_%s_%s_%s" % (prefix, clan.lower(), seeming.lower(), sex.lower()),
                 "%s_%s_%s_%s" % (prefix, clan.lower(), sex.lower(), seeming.lower())):
        path = "%s/%s" % (GARMENTS, name)
        asset = unreal.load_asset(path)
        if asset is not None:
            return path, asset
    return "%s/%s_%s_*" % (GARMENTS, prefix, clan.lower()), None


def main():
    log("=" * 70)
    log("dress_level_actors")
    log("=" * 70)

    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if sub is None:
        log("no EditorActorSubsystem")
        return
    actors = sub.get_all_level_actors() or []

    dressed, already, missing, skipped = 0, 0, 0, 0
    for actor in actors:
        class_name = actor.get_class().get_name()
        if not class_name.startswith("BP_"):
            continue

        slot = None
        body = None
        for comp in actor.get_components_by_class(unreal.SkeletalMeshComponent):
            if comp.component_has_tag("Outfit"):
                slot = comp
            elif comp.get_name() in ("CharacterMesh0", "Mesh"):
                body = comp
        if slot is None:
            skipped += 1
            continue

        path, outfit = outfit_for(class_name)
        if outfit is None:
            log("   %-34s no assembled outfit (%s)"
                % (actor.get_actor_label(), path))
            missing += 1
            continue

        current = slot.get_editor_property("skeletal_mesh_asset")
        if current is not None and current.get_name() == outfit.get_name():
            already += 1
            continue

        slot.set_editor_property("skeletal_mesh_asset", outfit)
        if body is not None:
            # Follow the body rather than evaluating the same skeleton twice.
            slot.set_leader_pose_component(body)
        log("   %-34s <- %s" % (actor.get_actor_label(), outfit.get_name()))
        dressed += 1

    log("")
    log("=" * 70)
    log("VERIFY -- read the instances back")
    log("=" * 70)
    for actor in actors:
        if not actor.get_class().get_name().startswith("BP_"):
            continue
        for comp in actor.get_components_by_class(unreal.SkeletalMeshComponent):
            if not comp.component_has_tag("Outfit"):
                continue
            mesh = comp.get_editor_property("skeletal_mesh_asset")
            leader = comp.get_editor_property("leader_pose_component")
            log("   %-34s %-40s leader=%s"
                % (actor.get_actor_label(),
                   mesh.get_name() if mesh else "*** NONE ***",
                   leader.get_name() if leader else "NONE"))

    log("")
    log("dressed %d, already correct %d, no outfit built %d, not NPCs %d"
        % (dressed, already, missing, skipped))
    log("")
    log("The LEVEL is dirty and not saved here -- save it yourself once the")
    log("characters look right, so a bad result costs nothing.")
    log("=" * 70)


try:
    main()
finally:
    flush()
    unreal.log("report: %s" % OUT)
