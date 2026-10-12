"""READ-ONLY audit: does every Kith BP have a body and a head, is the body the
FULL mesh (not a clothed one with geometry culled), and do they belong to the
right character?"""
import os, re, traceback
import unreal

BP_DIR = "/Game/_Custom/Characters/Kith/Blueprints"
REPORT = os.path.join(unreal.Paths.project_dir(), "Tools", "Export", "kith_bp_meshes.txt")
LINES = []
def log(m=""):
    LINES.append(str(m)); unreal.log(str(m))

def verts(mesh):
    for lib, fn in ((unreal.SkeletalMeshEditorSubsystem, "get_num_verts"),
                    (getattr(unreal, "EditorSkeletalMeshLibrary", None), "get_num_verts")):
        if lib is None:
            continue
        try:
            obj = unreal.get_editor_subsystem(lib) if "Subsystem" in lib.get_name() else lib
            return getattr(obj, fn)(mesh, 0)
        except Exception:
            continue
    return None

def bones(mesh):
    try:
        c = unreal.new_object(unreal.SkeletalMeshComponent)
        c.set_skeletal_mesh_asset(mesh)
        return c.get_num_bones()
    except Exception:
        return None

def tokens(n):
    n = n.lower()
    for p in ("bp_", "skm_", "mhc_"):
        if n.startswith(p):
            n = n[len(p):]
    n = re.sub(r"_(bodymesh|facemesh)$", "", n)
    return frozenset(t for t in n.split("_") if t)

try:
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    rows = []
    for a in sorted(unreal.EditorAssetLibrary.list_assets(BP_DIR, recursive=False) or []):
        path = a.split(".")[0]
        name = path.rsplit("/", 1)[-1]
        bp = unreal.load_asset(path)
        found = {}
        for h in sub.k2_gather_subobject_data_for_blueprint(bp):
            d = sub.k2_find_subobject_data_from_handle(h)
            o = lib.get_object_for_blueprint(d, bp)
            if o is None or not isinstance(o, unreal.SkeletalMeshComponent):
                continue
            found.setdefault(o.get_name(), o)
        body = found.get("CharacterMesh0")
        face = (found.get("FaceMesh") or found.get("FaceMesh_GEN_VARIABLE"))
        rec = {"name": name}
        for key, comp in (("body", body), ("face", face)):
            if comp is None:
                rec[key] = ("NO COMPONENT", None, None, None); continue
            m = comp.get_editor_property("skeletal_mesh_asset")
            if m is None:
                rec[key] = ("EMPTY", None, None, None); continue
            rec[key] = ("ok", m.get_name(), verts(m), bones(m))
        rows.append(rec)

    log("%-34s %-46s %-9s %-46s" % ("BLUEPRINT", "BODY MESH", "VERTS", "HEAD MESH"))
    log("-" * 140)
    problems = []
    for r in rows:
        bst, bnm, bv, bb = r["body"]
        fst, fnm, fv, fb = r["face"]
        log("%-34s %-46s %-9s %-46s"
            % (r["name"], bnm or bst, bv if bv is not None else "?", fnm or fst))
        why = []
        if bst != "ok": why.append("body %s" % bst)
        if fst != "ok": why.append("head %s" % fst)
        if bnm and tokens(bnm) != tokens(r["name"]): why.append("body mesh is %s" % bnm)
        if fnm and tokens(fnm) != tokens(r["name"]): why.append("head mesh is %s" % fnm)
        if bv is not None and bv < 30000: why.append("body only %d verts (CLOTHED/culled?)" % bv)
        if why:
            problems.append((r["name"], why))
    log("")
    log("=" * 60)
    if problems:
        log("PROBLEMS: %d of %d" % (len(problems), len(rows)))
        for n, why in problems:
            log("  %-34s %s" % (n, "; ".join(why)))
    else:
        log("All %d blueprints have a full body and a head, matching their name." % len(rows))
except Exception:
    log(traceback.format_exc())
with open(REPORT, "w", encoding="utf-8") as fh:
    fh.write("\n".join(LINES) + "\n")
