"""What body controls does the MetaHuman Character editor expose?

Needed to name real sliders rather than guess at UI labels.
"""
import unreal
C = "/Game/_Custom/Characters/Kith/Boggan/MHC_Boggan_Childling_Male"
c = unreal.load_asset(C)
unreal.log("character: %s" % (type(c).__name__ if c else "NOT FOUND"))
if c:
    for p in ("body_type", "fixed_body_type", "body_settings",
              "body_constraints", "body_measurements", "parametric_body",
              "skin_settings", "head_model_settings"):
        try:
            v = c.get_editor_property(p)
            unreal.log("  prop %-26s = %s" % (p, str(v)[:90]))
        except Exception as exc:
            unreal.log("  prop %-26s <%s>" % (p, str(exc)[:60]))

sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
if sub:
    names = [n for n in dir(sub) if not n.startswith("_") and "body" in n.lower()]
    unreal.log("=== subsystem body functions ===")
    for n in sorted(names):
        doc = (getattr(sub, n).__doc__ or "").strip().splitlines()
        unreal.log("   %-46s %s" % (n, doc[0][:80] if doc else ""))

unreal.log("=== body-related unreal classes ===")
for n in sorted(x for x in dir(unreal) if "MetaHumanCharacterBody" in x or
                ("Body" in x and "MetaHuman" in x)):
    unreal.log("   %s" % n)
