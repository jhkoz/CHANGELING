"""Measure the Witch Dresses mesh before fitting it to the female Boggans."""
import unreal
P = "/Game/Fab/Witch_Dresses/Witch_dresses/StaticMeshes/Witch_dresses"
m = unreal.load_asset(P)
unreal.log("asset: %s" % (type(m).__name__ if m else "NOT FOUND"))
if m:
    b = m.get_bounding_box(); mn, mx = b.min, b.max
    unreal.log("  size x %.1f  y %.1f  z %.1f (cm)" % (mx.x-mn.x, mx.y-mn.y, mx.z-mn.z))
    unreal.log("  z range %.1f .. %.1f" % (mn.z, mx.z))
    unreal.log("  verts %d  tris %d" % (m.get_num_vertices(0), m.get_num_triangles(0)))
    mats = m.get_editor_property("static_materials")
    unreal.log("  %d materials: %s" % (len(mats),
        ", ".join(str(s.get_editor_property("material_slot_name")) for s in mats)))
