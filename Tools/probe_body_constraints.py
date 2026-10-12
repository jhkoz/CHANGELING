"""The parametric body measurements on the Childling male, as the editor sees them."""
import unreal
C = "/Game/_Custom/Characters/Kith/Boggan/MHC_Boggan_Childling_Male"
c = unreal.load_asset(C)
sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
cons = sub.get_body_constraints(c)
unreal.log("=== %d body constraints ===" % len(cons))
for k in cons:
    fields = {}
    for f in dir(k):
        if f.startswith("_") or callable(getattr(k, f, None)):
            continue
        try:
            fields[f] = getattr(k, f)
        except Exception:
            pass
    unreal.log("  %s" % ("  ".join("%s=%s" % (a, b) for a, b in sorted(fields.items()))[:170]))
