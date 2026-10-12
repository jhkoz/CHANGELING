"""Are the outfit's materials actually assigned, and how big is the mesh?

The assembled meshes carry all 12 frock-coat slots, but in the level the
characters show bare arms, a grey vest and dark shorts -- so the slots exist
while most of the garment is not visible. Null materials, a collapsed LOD or a
mesh with almost no geometry would all look like that, and they are told apart
by the numbers rather than by looking.
"""
import unreal
K = "/Game/_Custom/Characters/Kith/Boggan"
sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
for n in ("MHC_Boggan_Wilder_Male", "MHC_Boggan_Wilder_Female"):
    p = "%s/%s/Clothing/%s_Outfits" % (K, n, n)
    m = unreal.load_asset(p)
    unreal.log("===== %s" % n)
    if m is None:
        unreal.log("   no outfit mesh"); continue
    try:
        lods = sub.get_lod_count(m)
    except Exception:
        lods = -1
    try:
        verts = sub.get_num_verts(m, 0)
    except Exception:
        verts = -1
    unreal.log("   LODs=%d  LOD0 verts=%d" % (lods, verts))
    for slot in m.get_editor_property("materials"):
        mi = slot.get_editor_property("material_interface")
        name = slot.get_editor_property("material_slot_name")
        unreal.log("   %-22s -> %s" % (name, mi.get_name() if mi else "*** NULL ***"))
