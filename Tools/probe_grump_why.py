import unreal
sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
for name in ("MHC_Boggan_Grump_Male", "MHC_Boggan_Wilder_Male"):
    path = "/Game/_Custom/Characters/Kith/Boggan/%s" % name
    c = unreal.load_asset(path)
    unreal.log("===== %s =====" % name)
    if c is None:
        unreal.log("   could not load")
        continue
    sub.try_add_object_to_edit(c)
    unreal.log("   can_build -> %s" % sub.can_build_meta_human(c, True))
    for prop in ("has_face_dna", "face_dna", "body_type", "preview_material_type"):
        try:
            unreal.log("   %-22s %r" % (prop, c.get_editor_property(prop)))
        except Exception:
            pass
