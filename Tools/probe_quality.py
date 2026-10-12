"""Which build parameter actually controls face size?

Every build today produced a 167-193 MB face despite pipeline_quality = HIGH,
against 11.8 MB originals. build_params_probe showed TWO properties --
pipeline_quality AND pipeline_type -- and only the first was set.
"""
import unreal

p = unreal.MetaHumanCharacterEditorBuildParameters()
unreal.log("=== MetaHumanCharacterEditorBuildParameters defaults ===")
for name in sorted(n for n in dir(p) if not n.startswith("_") and not callable(getattr(p, n, None))):
    try:
        unreal.log("   %-28s = %r" % (name, getattr(p, name)))
    except Exception as exc:
        unreal.log("   %-28s <%s>" % (name, str(exc)[:50]))

for enum_name in ("MetaHumanQualityLevel", "MetaHumanDefaultPipelineType",
                  "MetaHumanCharacterPaletteBuildQuality"):
    if hasattr(unreal, enum_name):
        e = getattr(unreal, enum_name)
        vals = [n for n in dir(e) if n.isupper()]
        unreal.log("=== %s: %s" % (enum_name, ", ".join(vals)))
