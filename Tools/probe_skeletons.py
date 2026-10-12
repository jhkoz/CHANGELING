"""Which skeleton does the built body use vs. our garments, and what pipeline
object should a SkeletalMesh wardrobe item carry? Read-only."""
import os, traceback
import unreal

REPORT = os.path.join(unreal.Paths.project_dir(), "Tools", "Export", "probe_skeletons.txt")
LINES = []
def log(m=""):
    LINES.append(str(m)); unreal.log(str(m))
def flush():
    with open(REPORT, "w", encoding="utf-8") as fh: fh.write("\n".join(LINES) + "\n")

def describe(path, label):
    obj = unreal.load_asset(path)
    if not obj:
        log("  %-22s %s -> NOT FOUND" % (label, path)); return None
    sk = obj.get_editor_property("skeleton")
    log("  %-22s %s" % (label, path))
    log("      skeleton: %s" % (sk.get_path_name() if sk else "<none>"))
    try:
        log("      LODs    : %d" % obj.get_lod_num())
    except Exception:
        try:
            log("      LODs    : %d" % len(obj.get_editor_property("lod_info")))
        except Exception as e:
            log("      LODs    : unreadable (%s)" % e)
    try:
        ls = obj.get_editor_property("lod_settings")
        log("      LODSettings: %s" % (ls.get_path_name() if ls else "<none>"))
    except Exception as e:
        log("      LODSettings unreadable (%s)" % e)
    return sk

try:
    log("SKELETON COMPARISON")
    log()
    g = describe("/Game/_Custom/Garments/Dress_sidhe_grump_female_fitted_thin", "garment")
    log()
    b = describe("/Game/_Scratch/WardrobeProbe/Built/Probe_Sidhe_Grump_Female/Body/"
                 "SKM_MHC_Probe_Sidhe_Grump_Female_BodyMesh", "BUILT body")
    log()
    log("  MATCH: %s" % (g and b and g.get_path_name() == b.get_path_name()))
    if g and b and g.get_path_name() != b.get_path_name():
        try:
            log("  built body compatible skeletons: %s"
                % b.get_editor_property("compatible_skeletons"))
        except Exception as e:
            log("  compatible_skeletons unreadable (%s)" % e)

    log()
    log("PIPELINE OBJECTS AVAILABLE")
    for p in ("/MetaHumanCharacter/BuildPipeline/DefaultWardobePipeline/DefaultSkeletalMeshPipeline",
              "/MetaHumanCharacter/BuildPipeline/DefaultWardobePipeline/DefaultOutfitPipeline"):
        o = unreal.load_asset(p)
        log("  %-90s %s" % (p, o.get_class().get_name() if o else "-- not loaded"))
    log("  can instantiate MetaHumanDefaultSkeletalMeshPipeline: %s"
        % bool(unreal.new_object(unreal.MetaHumanDefaultSkeletalMeshPipeline)))
except Exception:
    log(traceback.format_exc())
flush()
