"""Are the body constraints gone, or just unavailable unregistered?"""
import unreal
C = "/Game/_Custom/Characters/Kith/Boggan/MHC_Boggan_Childling_Male"
W = "/Game/_Custom/Characters/Kith/Boggan/MHC_Boggan_Wilder_Male"
sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
for label, path in (("slimmed ", C), ("untouched", W)):
    c = unreal.load_asset(path)
    n_plain = len(sub.get_body_constraints(c) or [])
    try:
        sub.remove_object_to_edit(c)
    except Exception:
        pass
    reg = sub.try_add_object_to_edit(c)
    n_reg = len(sub.get_body_constraints(c) or [])
    vals = ""
    if n_reg:
        d = {str(k.get_editor_property("name")): k.get_editor_property("target_measurement")
             for k in sub.get_body_constraints(c)}
        vals = "  Hip=%.0f Waist=%.0f Chest=%.0f Height=%.0f" % (
            d.get("Hip", -1), d.get("Waist", -1), d.get("Chest", -1), d.get("Height", -1))
    unreal.log("%s  unregistered=%d  registered(%s)=%d%s"
               % (label, n_plain, reg, n_reg, vals))
    try:
        sub.remove_object_to_edit(c)
    except Exception:
        pass
