"""Material slots on the female Boggans' assembled outfits.

The gown should read Bodice / Corset / Overskirt / Oversleeves / Sleeve_bands
/ Sleeve_tops / Chest_plate. Anything else means the slot selection did not
take -- which is how Childling Male was found still wearing casual clothes
while his file size said nothing was wrong.
"""
import unreal
K = "/Game/_Custom/Characters/Kith/Boggan"
for n in ("MHC_Boggan_Wilder_Female", "MHC_Boggan_Grump_Female",
          "MHC_Boggan_Childling_Female"):
    p = "%s/%s/Clothing/%s_Outfits" % (K, n, n)
    m = unreal.load_asset(p)
    if m is None:
        unreal.log("%-32s no outfit mesh" % n); continue
    mats = m.get_editor_property("materials")
    names = [str(x.get_editor_property("material_slot_name")) for x in mats]
    unreal.log("%-32s %2d slots: %s" % (n, len(mats), ", ".join(names)[:110]))
