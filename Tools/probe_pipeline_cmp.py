"""Compare the real pipeline class behind each Boggan male character.

probe_grump_slot printed type(pipeline).__name__, which is the nearest
Python-exposed base (MetaHumanDefaultPipeline) and identical for all three.
get_class().get_name() gives the actual UObject class, which is where a
difference would show.
"""
import unreal

KITH = "/Game/_Custom/Characters/Kith/Boggan"
for name in ("MHC_Boggan_Grump_Male", "MHC_Boggan_Wilder_Male",
             "MHC_Boggan_Childling_Male", "MHC_Boggan_Grump_Female"):
    c = unreal.load_asset("%s/%s" % (KITH, name))
    if c is None:
        unreal.log("%-30s could not load" % name)
        continue
    coll = c.get_editor_property("internal_collection")
    p = coll.get_editor_property("pipeline") if coll else None
    cls = p.get_class().get_name() if p else "None"
    ed = "?"
    try:
        ep = p.get_editor_property("editor_pipeline")
        ed = ep.get_class().get_name() if ep else "None"
    except Exception as exc:
        ed = "<%s>" % str(exc)[:40]
    unreal.log("%-30s pipeline=%-28s editor=%s" % (name, cls, ed))
