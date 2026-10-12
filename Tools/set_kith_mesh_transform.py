"""Set CharacterMesh0 to z=-89 / yaw=-90 on the Kith character Blueprints.

Only CharacterMesh0 (the body). FaceMesh and Outfit are children of it and
stay at 0,0,0 -- offsetting them too would double up.

CharacterMesh0 is INHERITED from AChangelingNPC, but Python hands back each
Blueprint's own CDO subobject (Default__BP_<name>_C:CharacterMesh0), so a write
touches that Blueprint alone. Rotator is built with KEYWORDS: positional args
are (roll, pitch, yaw) and getting that wrong silently rolls the character.

Config via Tools/kith_transform_config.json:
    {"only": ["BP_Boggan_Childling_Female"], "apply": false}
apply=false reports what would change and writes nothing.
"""
import json
import os
import traceback

import unreal

PROJECT = unreal.Paths.project_dir()
BP_DIR = "/Game/_Custom/Characters/Kith/Blueprints"
CONFIG = os.path.join(PROJECT, "Tools", "kith_transform_config.json")
REPORT = os.path.join(PROJECT, "Tools", "Export", "kith_transform.txt")
# -89 is deliberate: it matches BP_ThirdPersonCharacter, so the Kith sit at
# the same height in their capsules as the player.
TARGET_Z = -89.0
TARGET_YAW = -90.0
COMPONENT = "CharacterMesh0"

LINES = []


def log(m=""):
    LINES.append(str(m))
    unreal.log(str(m))


def flush():
    with open(REPORT, "w", encoding="utf-8") as fh:
        fh.write("\n".join(LINES) + "\n")


def conf():
    if os.path.isfile(CONFIG):
        with open(CONFIG, encoding="utf-8") as fh:
            return json.load(fh)
    return {}


def body_component(bp):
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    for h in sub.k2_gather_subobject_data_for_blueprint(bp):
        d = sub.k2_find_subobject_data_from_handle(h)
        o = lib.get_object_for_blueprint(d, bp)
        if o is not None and o.get_name() == COMPONENT:
            return o
    return None


def main():
    c = conf()
    apply_it = bool(c.get("apply"))
    only = [o.lower() for o in c.get("only", [])]

    log("set_kith_mesh_transform  apply=%s  target z=%s yaw=%s"
        % (apply_it, TARGET_Z, TARGET_YAW))
    log("")

    assets = unreal.EditorAssetLibrary.list_assets(BP_DIR, recursive=False) or []
    changed = skipped = failed = 0
    for a in sorted(assets):
        path = a.split(".")[0]
        name = path.rsplit("/", 1)[-1]
        if only and not any(o in name.lower() for o in only):
            continue
        bp = unreal.load_asset(path)
        if bp is None:
            log("  %-40s LOAD FAILED" % name)
            failed += 1
            continue
        comp = body_component(bp)
        if comp is None:
            log("  %-40s no %s" % (name, COMPONENT))
            failed += 1
            continue

        loc = comp.get_editor_property("relative_location")
        rot = comp.get_editor_property("relative_rotation")
        needs = (abs(loc.z - TARGET_Z) > 1e-4 or abs(rot.yaw - TARGET_YAW) > 1e-4
                 or abs(loc.x) > 1e-4 or abs(loc.y) > 1e-4
                 or abs(rot.pitch) > 1e-4 or abs(rot.roll) > 1e-4)
        if not needs:
            log("  %-40s already z=%.1f yaw=%.1f" % (name, loc.z, rot.yaw))
            skipped += 1
            continue

        log("  %-40s z %.1f -> %.1f   yaw %.1f -> %.1f%s"
            % (name, loc.z, TARGET_Z, rot.yaw, TARGET_YAW,
               "" if apply_it else "   (dry run)"))
        if not apply_it:
            changed += 1
            continue

        try:
            comp.set_editor_property(
                "relative_location", unreal.Vector(0.0, 0.0, TARGET_Z))
            comp.set_editor_property(
                "relative_rotation",
                unreal.Rotator(roll=0.0, pitch=0.0, yaw=TARGET_YAW))
            unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
            # Read back from the live object; a separate run re-reads from disk.
            loc2 = comp.get_editor_property("relative_location")
            rot2 = comp.get_editor_property("relative_rotation")
            ok = (abs(loc2.z - TARGET_Z) < 1e-4
                  and abs(rot2.yaw - TARGET_YAW) < 1e-4)
            log("      %s in memory: z=%.1f yaw=%.1f"
                % ("OK" if ok else "DID NOT TAKE", loc2.z, rot2.yaw))
            changed += 1 if ok else 0
            failed += 0 if ok else 1
        except Exception:
            log(traceback.format_exc())
            failed += 1

    log("")
    log("%s: %d   already correct: %d   problems: %d"
        % ("changed" if apply_it else "would change", changed, skipped, failed))
    flush()


try:
    main()
except Exception:
    log(traceback.format_exc())
    flush()
