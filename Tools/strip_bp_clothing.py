"""Remove Blueprint-added clothing components from the Kith character BPs.

Clothing is now authored in MetaHuman and applied by script; the native Outfit
component and BindOutfits are gone from AChangelingNPC. What remains are
per-Blueprint garment components, which would otherwise be (a) dead weight and
(b) mistaken for the face by ConfigureLODSync, which identifies the face as the
sole skeletal child of the body.

Only NON-INHERITED components whose name looks like clothing are touched.
Set apply=true in Tools/kith_transform_config.json style config below.
"""
import json, os, traceback
import unreal

PROJECT = unreal.Paths.project_dir()
BP_DIR = "/Game/_Custom/Characters/Kith/Blueprints"
CONFIG = os.path.join(PROJECT, "Tools", "strip_clothing_config.json")
REPORT = os.path.join(PROJECT, "Tools", "Export", "strip_bp_clothing.txt")
WORDS = ("outfit", "cloth", "garment", "dress")
LINES = []
def log(m=""):
    LINES.append(str(m)); unreal.log(str(m))

def apply_mode():
    try:
        with open(CONFIG, encoding="utf-8") as fh:
            return bool(json.load(fh).get("apply"))
    except Exception:
        return False

try:
    apply_it = apply_mode()
    log("strip_bp_clothing  apply=%s" % apply_it)
    log("")
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    removed = failed = 0
    for a in sorted(unreal.EditorAssetLibrary.list_assets(BP_DIR, recursive=False) or []):
        path = a.split(".")[0]
        name = path.rsplit("/", 1)[-1]
        bp = unreal.load_asset(path)
        handles = sub.k2_gather_subobject_data_for_blueprint(bp)
        if not handles:
            continue
        root = handles[0]
        targets = []
        for h in handles:
            d = sub.k2_find_subobject_data_from_handle(h)
            o = lib.get_object_for_blueprint(d, bp)
            if o is None:
                continue
            cn = o.get_name()
            if not any(w in cn.lower() for w in WORDS):
                continue
            try:
                if lib.is_inherited_component(d):
                    log("  %-34s SKIP inherited %s" % (name, cn))
                    continue
            except Exception:
                pass
            targets.append((h, cn))
        if not targets:
            continue
        for h, cn in targets:
            if not apply_it:
                log("  %-34s would remove %s" % (name, cn)); continue
            try:
                n = sub.delete_subobject(root, h, bp)
                log("  %-34s removed %-28s (deleted %s)" % (name, cn, n))
                removed += 1
            except Exception as exc:
                log("  %-34s FAILED %s: %s" % (name, cn, exc))
                failed += 1
        if apply_it:
            try:
                unreal.BlueprintEditorLibrary.compile_blueprint(bp)
                unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
                log("  %-34s compiled + saved" % name)
            except Exception as exc:
                log("  %-34s compile/save FAILED: %s" % (name, exc))
                failed += 1
    log("")
    log("removed: %d   problems: %d" % (removed, failed))
except Exception:
    log(traceback.format_exc())
with open(REPORT, "w", encoding="utf-8") as fh:
    fh.write("\n".join(LINES) + "\n")
