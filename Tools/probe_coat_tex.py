"""Do the frock coat materials actually resolve textures?

The viewport shows UE's missing-texture checkerboard on the chest, so this
checks the material instances' parents and texture parameters rather than
relying on referenced_textures, which reads 0 on an instance whether or not
the parent supplies maps.
"""
import unreal
BASE = "/Game/Fab/Frock_Coat/Frock_coat"
for name in ("Coat", "Waistcoat_main", "Trousers", "Socks", "Coat_buttons"):
    p = "%s/Materials/%s" % (BASE, name)
    m = unreal.load_asset(p)
    if m is None:
        unreal.log("%-18s NOT FOUND at %s" % (name, p)); continue
    parent = m.get_editor_property("parent")
    unreal.log("%-18s parent=%s" % (name, parent.get_name() if parent else "*** NONE ***"))
    try:
        params = unreal.MaterialEditingLibrary.get_texture_parameter_names(parent) if parent else []
    except Exception:
        params = []
    got = 0
    for pn in params:
        try:
            t = unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(m, pn)
            if t is not None:
                got += 1
        except Exception:
            pass
    unreal.log("      texture params=%d  set=%d" % (len(params), got))
unreal.log("=== textures actually present in the pack ===")
tex = unreal.EditorAssetLibrary.list_assets("%s/Textures" % BASE, recursive=True, include_folder=False) or []
unreal.log("   %d texture assets" % len(tex))
