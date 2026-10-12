"""Has the body actually slimmed, and what is the outfit now?"""
import unreal
C = "/Game/_Custom/Characters/Kith/Boggan/MHC_Boggan_Childling_Male"
sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
c = unreal.load_asset(C)
try: sub.remove_object_to_edit(c)
except Exception: pass
sub.try_add_object_to_edit(c)
d = {str(k.get_editor_property("name")): k.get_editor_property("target_measurement")
     for k in sub.get_body_constraints(c)}
unreal.log("constraints: Hip=%.0f HighHip=%.0f Waist=%.0f Underbust=%.0f Chest=%.0f Height=%.0f"
           % (d.get("Hip",-1), d.get("High Hip",-1), d.get("Waist",-1),
              d.get("Underbust",-1), d.get("Chest",-1), d.get("Height",-1)))
try: sub.remove_object_to_edit(c)
except Exception: pass
for label, p in (("body  ", "%s/Body/SKM_MHC_Boggan_Childling_Male_BodyMesh" % C),
                 ("outfit", "%s/Clothing/MHC_Boggan_Childling_Male_Outfits" % C)):
    m = unreal.load_asset(p)
    if m is None:
        unreal.log("%s MISSING" % label); continue
    e = m.get_bounds().box_extent
    mats = [str(s.get_editor_property("material_slot_name"))
            for s in m.get_editor_property("materials")]
    unreal.log("%s size x %.1f y %.1f z %.1f cm   %d mats: %s"
               % (label, e.x*2, e.y*2, e.z*2, len(mats), ", ".join(mats)[:80]))
