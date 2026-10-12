"""Delete the stale baked outfit, then assemble, so there is nothing to reuse.

Assembly reports "done in 0.0 min" and leaves Clothing/<name>_Outfits at its
old timestamp even when the source ChaosOutfitAsset has been rebuilt -- on
Grump it regenerated the body and skipped the outfit entirely. Its dirty
tracking does not follow the outfit asset.

Removing the baked mesh first makes the question moot. The path is unchanged,
so the Blueprint's reference resolves again once it is rebuilt.
"""
import os, time, unreal

PROJECT = os.path.normpath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "force_outfit.txt")
KITH = "/Game/_Custom/Characters/Kith/Boggan"
TARGETS = ["MHC_Boggan_Childling_Male", "MHC_Boggan_Grump_Male"]

lines = []
def log(m=""):
    lines.append(str(m)); unreal.log(str(m))

sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
for name in TARGETS:
    log(""); log("=== %s" % name)
    path = "%s/%s" % (KITH, name)
    outfit_path = "%s/%s/Clothing/%s_Outfits" % (KITH, name, name)
    character = unreal.load_asset(path)
    if character is None:
        log("   could not load character"); continue

    if unreal.EditorAssetLibrary.does_asset_exist(outfit_path):
        ok = unreal.EditorAssetLibrary.delete_asset(outfit_path)
        log("   deleted stale baked outfit: %s" % ok)
    else:
        log("   no baked outfit present")

    try: sub.remove_object_to_edit(character)
    except Exception: pass
    if not sub.try_add_object_to_edit(character):
        log("   could not register for editing"); continue
    try:
        params = unreal.MetaHumanCharacterEditorBuildParameters()
        params.set_editor_property("pipeline_type", unreal.MetaHumanDefaultPipelineType.OPTIMIZED)
        params.set_editor_property("pipeline_quality", unreal.MetaHumanQualityLevel.HIGH)
        t0 = time.time()
        sub.build_meta_human(character, params)
        unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
        log("   built in %.1f min" % ((time.time()-t0)/60.0))
    except Exception as exc:
        log("   FAILED: %s" % exc)
    finally:
        try: sub.remove_object_to_edit(character)
        except Exception: pass

    m = unreal.load_asset(outfit_path)
    if m is None:
        log("   VERIFY: no outfit mesh came back")
    else:
        mats = [str(s.get_editor_property("material_slot_name"))
                for s in m.get_editor_property("materials")]
        log("   VERIFY: %d slots -- %s" % (len(mats), ", ".join(mats)[:70]))

with open(OUT, "w") as h:
    h.write("\n".join(lines))
