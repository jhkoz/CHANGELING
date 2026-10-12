"""Is the assembled coat still a coat, or has it become a skin?

The conform moved 55,299 of 59,351 vertices -- 93% -- onto the body surface
plus 12 mm. If the result now has essentially the body's own silhouette, then
it is painted on rather than worn, which is what the level is showing.

Also checks whether the coat materials actually have textures, since the
checkerboard in the viewport is UE's missing-texture pattern.
"""
import unreal
K = "/Game/_Custom/Characters/Kith/Boggan"
sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)

for n in ("MHC_Boggan_Wilder_Male",):
    outfit = unreal.load_asset("%s/%s/Clothing/%s_Outfits" % (K, n, n))
    body = unreal.load_asset("%s/%s/Body/SKM_%s_BodyMesh" % (K, n, n))
    for label, m in (("outfit", outfit), ("body  ", body)):
        if m is None:
            unreal.log("%s MISSING" % label); continue
        b = m.get_bounds()
        e = b.box_extent
        unreal.log("%s  size x %.1f y %.1f z %.1f (cm)"
                   % (label, e.x * 2, e.y * 2, e.z * 2))

unreal.log("=== coat material textures ===")
for name in ("Coat", "Waistcoat_main", "Trousers", "Socks"):
    m = unreal.load_asset("/Game/Fab/Frock_Coat/Frock_coat/Materials/%s" % name)
    if m is None:
        unreal.log("   %-18s MATERIAL NOT FOUND" % name); continue
    try:
        texs = m.get_editor_property("referenced_textures")
    except Exception:
        texs = None
    n_tex = len(texs) if texs else 0
    unreal.log("   %-18s %s  textures=%d" % (name, type(m).__name__, n_tex))
