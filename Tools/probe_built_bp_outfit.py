"""What does the assembly's own BP_MHC_* carry for clothing?

The hand-made BP_Boggan_* Blueprints have no outfit component -- strip_bp_clothing
removed them, and ConfigureLODSync warns about a second skeletal child. But the
MetaHuman build produces its own Blueprint alongside the character, and whatever
IT uses to show the outfit is the pattern the hand-made one has to match.
"""
import unreal

KITH = "/Game/_Custom/Characters/Kith/Boggan"
sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary

for name in ("MHC_Boggan_Wilder_Male", "MHC_Boggan_Childling_Male"):
    path = "%s/%s/BP_%s" % (KITH, name, name)
    bp = unreal.load_asset(path)
    unreal.log("===== %s =====" % path.rsplit("/", 1)[-1])
    if bp is None:
        unreal.log("   not found")
        continue
    try:
        parent = bp.get_editor_property("parent_class")
        unreal.log("   parent: %s" % (parent.get_name() if parent else "?"))
    except Exception:
        pass
    seen = set()
    for h in sub.k2_gather_subobject_data_for_blueprint(bp):
        d = lib.get_data(h)
        o = lib.get_object(d)
        if o is None or not isinstance(o, unreal.ActorComponent):
            continue
        if o.get_path_name() in seen:
            continue
        seen.add(o.get_path_name())
        try:
            var = str(lib.get_variable_name(d))
        except Exception:
            var = o.get_name()
        detail = ""
        if isinstance(o, unreal.SkeletalMeshComponent):
            m = o.get_editor_property("skeletal_mesh_asset")
            detail = m.get_name() if m else "EMPTY"
        elif "Cloth" in type(o).__name__:
            for p in ("cloth_asset", "asset", "outfit_asset"):
                try:
                    v = o.get_editor_property(p)
                    if v is not None:
                        detail = "%s=%s" % (p, v.get_name())
                        break
                except Exception:
                    continue
        elif "Groom" in type(o).__name__:
            g = o.get_editor_property("groom_asset")
            detail = g.get_name() if g else "EMPTY"
        unreal.log("   %-22s %-30s %s" % (var, type(o).__name__, detail))
