"""probe_lod_api.py -- which call actually returns a skeletal mesh LOD count?

READ-ONLY.

check_all_kith reported -1 LODs for all 30 characters, including the ones that
were never broken, which means the read failed rather than the meshes being
empty. Rather than guess at a replacement and report another false result, try
each candidate against one mesh that is known good and print what comes back.
"""

import unreal

MESH = ("/Game/_Custom/Characters/Kith/Sidhe/MHC_Sidhe_Wilder_Female"
        "/Face/SKM_MHC_Sidhe_Wilder_Female_FaceMesh")

mesh = unreal.load_asset(MESH)
print("mesh: %s" % (mesh.get_name() if mesh else "COULD NOT LOAD"))
if mesh:
    print("class: %s" % type(mesh).__name__)

    for call in ["get_lod_num", "get_num_lods", "get_lod_count"]:
        try:
            print("  mesh.%s() -> %s" % (call, getattr(mesh, call)()))
        except Exception as exc:
            print("  mesh.%s() -> FAILED: %s" % (call, exc))

    for prop in ["lod_info", "lod_settings", "num_source_models"]:
        try:
            val = mesh.get_editor_property(prop)
            print("  property %-20s -> %s (len=%s)"
                  % (prop, type(val).__name__,
                     len(val) if hasattr(val, "__len__") else "n/a"))
        except Exception as exc:
            print("  property %-20s -> FAILED: %s" % (prop, exc))

    sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    print("  SkeletalMeshEditorSubsystem: %s" % ("yes" if sub else "no"))
    if sub:
        for call in ["get_lod_count", "get_num_lods", "get_num_verts"]:
            try:
                fn = getattr(sub, call)
                print("  sub.%s(mesh%s) -> %s"
                      % (call, ", 0" if call == "get_num_verts" else "",
                         fn(mesh, 0) if call == "get_num_verts" else fn(mesh)))
            except Exception as exc:
                print("  sub.%s -> FAILED: %s" % (call, exc))
