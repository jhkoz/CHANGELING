"""Why does the garment fail USkeleton::IsCompatibleMesh against
metahuman_base_skel? Compare bone names and PARENT CHAINS, garment vs body.
Read-only."""
import os, traceback
import unreal

GARMENT = "/Game/_Custom/Garments/Dress_sidhe_grump_female_fitted_thin"
BODY = ("/Game/_Scratch/WardrobeProbe/Built/Probe_222432/Body/"
        "SKM_MHC_Probe_222432_BodyMesh")
REPORT = os.path.join(unreal.Paths.project_dir(), "Tools", "Export", "probe_bonechain.txt")
LINES = []
def log(m=""):
    LINES.append(str(m)); unreal.log(str(m))
def flush():
    with open(REPORT, "w", encoding="utf-8") as fh: fh.write("\n".join(LINES) + "\n")

def bones_of(path, label):
    mesh = unreal.load_asset(path)
    comp = unreal.new_object(unreal.SkeletalMeshComponent)
    comp.set_skeletal_mesh_asset(mesh)
    n = comp.get_num_bones()
    names = [str(comp.get_bone_name(i)) for i in range(n)]
    parents = {}
    for nm in names:
        try:
            parents[nm] = str(comp.get_parent_bone(nm))
        except Exception:
            parents[nm] = "<err>"
    log("  %-10s %-70s bones=%d" % (label, path.rsplit("/", 1)[-1], n))
    return names, parents

try:
    log("BONE CHAIN COMPARISON")
    log()
    gn, gp = bones_of(GARMENT, "garment")
    bn, bp = bones_of(BODY, "body")
    log()

    gs, bs = set(gn), set(bn)
    log("  in garment but NOT in body : %d" % len(gs - bs))
    for b in sorted(gs - bs)[:25]: log("      %s" % b)
    log("  in body but NOT in garment : %d" % len(bs - gs))
    for b in sorted(bs - gs)[:25]: log("      %s" % b)

    log()
    log("  PARENT MISMATCHES (bone present in both, different parent):")
    bad = 0
    for b in gn:
        if b in bp and gp.get(b) != bp.get(b):
            bad += 1
            if bad <= 30:
                log("      %-34s garment parent=%-26s body parent=%s"
                    % (b, gp.get(b), bp.get(b)))
    log("  total parent mismatches: %d" % bad)

    log()
    log("  ORDER: first 8 bones")
    log("    garment: %s" % gn[:8])
    log("    body   : %s" % bn[:8])
    log()
    idx = [i for i in range(min(len(gn), len(bn))) if gn[i] != bn[i]]
    log("  first index where bone ORDER diverges: %s"
        % (idx[0] if idx else "never (order identical)"))
    if idx:
        i = idx[0]
        log("    garment[%d]=%s   body[%d]=%s" % (i, gn[i], i, bn[i]))
except Exception:
    log(traceback.format_exc())
flush()
