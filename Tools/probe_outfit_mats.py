"""Did Childling's baked outfit actually become the frock coat?

The frock coat source has 12 material slots (coat, trim, buttons, waistcoat,
trousers, scarf, cuffs, socks). The procedural coat+breeches had far fewer.
Material count tells them apart regardless of file size.
"""
import unreal
K = "/Game/_Custom/Characters/Kith/Boggan"
for n in ("MHC_Boggan_Wilder_Male", "MHC_Boggan_Childling_Male", "MHC_Boggan_Grump_Male"):
    p = "%s/%s/Clothing/%s_Outfits" % (K, n, n)
    m = unreal.load_asset(p)
    if m is None:
        unreal.log("%-30s MISSING" % n); continue
    mats = m.get_editor_property("materials")
    names = [x.get_editor_property("material_slot_name") for x in mats]
    unreal.log("%-30s %2d slots: %s" % (n, len(mats), ", ".join(str(x) for x in names)[:110]))
