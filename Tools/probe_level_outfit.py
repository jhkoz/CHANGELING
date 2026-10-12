"""Inspect the Outfit component on actors actually placed in the level.

The outfit ASSETS are correct -- 68,489 verts, all 12 materials assigned -- so
whatever is wrong is on the instance: visibility, leader pose, or LOD. The
outfit carries a single LOD against a 4-LOD body, and ConfigureLODSync drives
every garment passively off the body's level, which is worth checking before
anything else.
"""
import unreal
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = sub.get_all_level_actors() if sub else []
seen = 0
for a in actors:
    name = a.get_class().get_name()
    if "Boggan" not in name:
        continue
    seen += 1
    if seen > 4:
        break
    unreal.log("===== %s  (%s)" % (a.get_actor_label(), name))
    for comp in a.get_components_by_class(unreal.SkeletalMeshComponent):
        mesh = comp.get_editor_property("skeletal_mesh_asset")
        tagged = comp.component_has_tag("Outfit")
        leader = comp.get_editor_property("leader_pose_component")
        unreal.log("   %-16s mesh=%-42s tag=%-5s vis=%-5s"
                   % (comp.get_name(), mesh.get_name() if mesh else "NONE",
                      tagged, comp.get_editor_property("visible")))
        if tagged:
            unreal.log("        leader=%s" % (leader.get_name() if leader else "*** NONE ***"))
            unreal.log("        forced_lod=%s  pred_lod=%s"
                       % (comp.get_editor_property("forced_lod_model"),
                          comp.get_editor_property("predicted_lod_level")))
    lod = a.get_component_by_class(unreal.LODSyncComponent)
    if lod:
        unreal.log("   LODSync NumLODs=%s  components=%d"
                   % (lod.get_editor_property("num_lo_ds"),
                      len(lod.get_editor_property("components_to_sync"))))
if seen == 0:
    unreal.log("no Boggan actors found in the open level")
