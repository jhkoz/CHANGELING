"""READ-ONLY: what are the Mesh component transforms on the Kith BPs, and what
does the project's own working character use?"""
import os, traceback
import unreal

BP_DIR = "/Game/_Custom/Characters/Kith/Blueprints"
REFS = ["/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter",
        "/Game/Characters/BP_ThirdPersonCharacter"]
REPORT = os.path.join(unreal.Paths.project_dir(), "Tools", "Export", "kith_bp_transforms.txt")
LINES = []
def log(m=""):
    LINES.append(str(m)); unreal.log(str(m))

def mesh_of(bp_path):
    bp = unreal.load_asset(bp_path)
    if not bp:
        return None, "not found"
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    out = []
    for h in sub.k2_gather_subobject_data_for_blueprint(bp):
        d = sub.k2_find_subobject_data_from_handle(h)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(d)
        if obj is None or not isinstance(obj, unreal.SkeletalMeshComponent):
            continue
        try:
            loc = obj.get_editor_property("relative_location")
            rot = obj.get_editor_property("relative_rotation")
        except Exception as exc:
            out.append((obj.get_name(), "unreadable: %s" % exc, None)); continue
        out.append((obj.get_name(), loc, rot))
    return out, None

try:
    log("REFERENCE CHARACTERS")
    for r in REFS:
        comps, err = mesh_of(r)
        if err:
            log("  %-52s %s" % (r, err)); continue
        for name, loc, rot in comps:
            log("  %-30s %-22s loc=%s" % (r.rsplit('/',1)[-1], name, loc))
            log("      %-52s rot=%s" % ("", rot))

    log("")
    log("KITH BLUEPRINTS")
    names = unreal.EditorAssetLibrary.list_assets(BP_DIR, recursive=False) or []
    for a in sorted(names):
        path = a.split(".")[0]
        comps, err = mesh_of(path)
        if err:
            log("  %-40s %s" % (path.rsplit('/',1)[-1], err)); continue
        if not comps:
            log("  %-40s NO SkeletalMeshComponent" % path.rsplit('/',1)[-1]); continue
        for name, loc, rot in comps:
            log("  %-40s %-14s loc=%s rot=%s"
                % (path.rsplit('/',1)[-1], name, loc, rot))
except Exception:
    log(traceback.format_exc())
with open(REPORT, "w", encoding="utf-8") as fh:
    fh.write("\n".join(LINES) + "\n")
