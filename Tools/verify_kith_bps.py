"""Final check: no clothing components, every BP compiles, body+head intact."""
import os, traceback
import unreal

BP_DIR = "/Game/_Custom/Characters/Kith/Blueprints"
REPORT = os.path.join(unreal.Paths.project_dir(), "Tools", "Export", "verify_kith_bps.txt")
WORDS = ("outfit", "cloth", "garment", "dress")
LINES = []
def log(m=""):
    LINES.append(str(m)); unreal.log(str(m))

try:
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    bad_compile, bad_cloth, bad_mesh = [], [], []
    log("%-34s %5s %9s %-8s %-8s %s" % ("BLUEPRINT", "COMPS", "CLOTHING", "BODY", "HEAD", "COMPILE"))
    log("-" * 96)
    for a in sorted(unreal.EditorAssetLibrary.list_assets(BP_DIR, recursive=False) or []):
        path = a.split(".")[0]
        name = path.rsplit("/", 1)[-1]
        bp = unreal.load_asset(path)
        comps, cloth, body, face = 0, 0, None, None
        for h in sub.k2_gather_subobject_data_for_blueprint(bp):
            d = sub.k2_find_subobject_data_from_handle(h)
            o = lib.get_object_for_blueprint(d, bp)
            if o is None:
                continue
            comps += 1
            cn = o.get_name()
            if any(w in cn.lower() for w in WORDS):
                cloth += 1
            if isinstance(o, unreal.SkeletalMeshComponent):
                m = o.get_editor_property("skeletal_mesh_asset")
                if cn == "CharacterMesh0":
                    body = m.get_name() if m else None
                elif "face" in cn.lower():
                    face = m.get_name() if m else None
        try:
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
            status = str(bp.get_editor_property("status"))
        except Exception as exc:
            status = "EXC %s" % exc
        ok = ("UP_TO_DATE" in status.upper()) or ("GOOD" in status.upper())
        if not ok:
            bad_compile.append((name, status))
        if cloth:
            bad_cloth.append(name)
        if not body or not face:
            bad_mesh.append(name)
        log("%-34s %5d %9d %-8s %-8s %s"
            % (name, comps, cloth, "yes" if body else "NO",
               "yes" if face else "NO", status.split(".")[-1]))
    log("")
    log("=" * 60)
    log("clothing components remaining : %d blueprint(s) %s"
        % (len(bad_cloth), bad_cloth or ""))
    log("missing body or head          : %d %s" % (len(bad_mesh), bad_mesh or ""))
    log("compile problems              : %d" % len(bad_compile))
    for n, s in bad_compile:
        log("    %-34s %s" % (n, s))
except Exception:
    log(traceback.format_exc())
with open(REPORT, "w", encoding="utf-8") as fh:
    fh.write("\n".join(LINES) + "\n")
