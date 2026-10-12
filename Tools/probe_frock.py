"""Measure the downloaded frock coat so it can be fitted to a Boggan."""
import unreal
P = "/Game/Fab/Frock_Coat/Frock_coat/StaticMeshes/Frock_coat"
m = unreal.load_asset(P)
unreal.log("asset: %s  class=%s" % (P, type(m).__name__ if m else "NOT FOUND"))
if m:
    b = m.get_bounding_box()
    mn, mx = b.min, b.max
    unreal.log("  bounds min %.1f %.1f %.1f" % (mn.x, mn.y, mn.z))
    unreal.log("  bounds max %.1f %.1f %.1f" % (mx.x, mx.y, mx.z))
    unreal.log("  size  x %.1f  y %.1f  z %.1f (cm)"
               % (mx.x-mn.x, mx.y-mn.y, mx.z-mn.z))
    unreal.log("  LODs: %d" % m.get_num_lods())
    unreal.log("  tris LOD0: %d" % m.get_num_triangles(0))
    unreal.log("  verts LOD0: %d" % m.get_num_vertices(0))
    mats = m.get_editor_property("static_materials")
    unreal.log("  materials: %d" % len(mats))
    for s in mats:
        mi = s.get_editor_property("material_interface")
        unreal.log("     %s" % (mi.get_name() if mi else "None"))
