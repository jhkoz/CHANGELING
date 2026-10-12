"""Which body mesh does each Kith Blueprint actually render?

In the component-based approach the garment is fitted to the body the
Blueprint uses, so this is the authoritative list -- not the forward/reversed
folder naming, which differs per character and has misled this work twice.
"""
import os, unreal
PROJECT = os.path.normpath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "bp_bodies.txt")
BP_DIR = "/Game/_Custom/Characters/Kith/Blueprints"
sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
lines = []
for a in sorted(unreal.EditorAssetLibrary.list_assets(BP_DIR, recursive=False, include_folder=False) or []):
    path = a.split(".")[0]
    bp = unreal.load_asset(path)
    if bp is None:
        continue
    body = None
    for h in sub.k2_gather_subobject_data_for_blueprint(bp):
        d = lib.get_data(h); o = lib.get_object(d)
        if not isinstance(o, unreal.SkeletalMeshComponent):
            continue
        try: var = str(lib.get_variable_name(d))
        except Exception: var = o.get_name()
        if var in ("Mesh", "CharacterMesh0"):
            m = o.get_editor_property("skeletal_mesh_asset")
            body = m.get_name() if m else None
    line = "%-34s %s" % (path.rsplit("/",1)[-1], body or "*** NO BODY ***")
    lines.append(line); unreal.log(line)
with open(OUT, "w") as h:
    h.write("\n".join(lines))
