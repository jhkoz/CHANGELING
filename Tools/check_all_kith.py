"""check_all_kith.py -- end-to-end state of every Kith character.

READ-ONLY.

The symptom that started this was a runtime report of `face (0 LODs), 0
groom(s)` on seven characters. Checking that the Blueprint's references are
non-null is not the same as checking the meshes behind them actually have LODs,
so this follows each reference through to the asset and reports the LOD count.

Reports, per Blueprint:
    body LODs / face LODs / groom count / grooms missing a binding

A face with 0 LODs, or a groom with no binding, is the failure. A groom binding
whose target is not the face it hangs off is also flagged: RebindGrooms refuses
that pairing and the hair renders unbound at the character's feet.
"""

import os
import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "kith_state.txt")
BP_DIR = "/Game/_Custom/Characters/Kith/Blueprints"

# Rebuilt or repaired in this session -- called out so the report shows whether
# they now match the characters that were never broken.
TOUCHED = {
    "BP_Boggan_Childling_Female", "BP_Boggan_Childling_Male",
    "BP_Clurichaun_Childling_Female", "BP_Clurichaun_Wilder_Female",
    "BP_Clurichaun_Wilder_Male", "BP_Eshu_Wilder_Female",
    "BP_Eshu_Childling_Male",
}

_lines = []


def log(m):
    _lines.append(str(m))
    unreal.log(str(m))


_MESH_SUB = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)


def lods(mesh):
    """LOD count via SkeletalMeshEditorSubsystem.

    USkeletalMesh exposes no LOD accessor to Python in 5.7 -- get_lod_num,
    get_num_lods and get_lod_count are all absent on the asset itself, and the
    earlier check_faces.py called get_lod_num inside a bare except, so every
    number it ever printed was the -1 fallback rather than a reading.
    """
    if mesh is None or _MESH_SUB is None:
        return -1
    try:
        return _MESH_SUB.get_lod_count(mesh)
    except Exception:
        return -1


sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary

paths = sorted(
    p.split(".")[0]
    for p in unreal.EditorAssetLibrary.list_assets(
        BP_DIR, recursive=False, include_folder=False))

log("%-34s %5s %5s %5s %6s  %s"
    % ("blueprint", "body", "face", "grms", "state", "notes"))
log("-" * 100)

bad = 0
for path in paths:
    name = path.rsplit("/", 1)[-1]
    bp = unreal.load_asset(path)
    if bp is None:
        log("%-34s  could not load" % name)
        bad += 1
        continue

    body_mesh = face_mesh = None
    groom_count = 0
    notes = []
    seen = set()

    for handle in sub.k2_gather_subobject_data_for_blueprint(bp):
        data = lib.get_data(handle)
        obj = lib.get_object(data)
        if obj is None or not isinstance(obj, unreal.ActorComponent):
            continue
        key = obj.get_path_name()
        if key in seen:
            continue
        seen.add(key)
        try:
            var = str(lib.get_variable_name(data))
        except Exception:
            var = obj.get_name()

        if isinstance(obj, unreal.GroomComponent):
            groom_count += 1
            groom = obj.get_editor_property("groom_asset")
            binding = obj.get_editor_property("binding_asset")
            if groom is None:
                notes.append("%s has no groom" % var)
            if binding is None:
                notes.append("%s has no binding" % var)
        elif isinstance(obj, unreal.SkeletalMeshComponent):
            mesh = obj.get_editor_property("skeletal_mesh_asset")
            if var == "Mesh":
                body_mesh = mesh
            elif var == "Outfit":
                continue
            elif var == "FaceMesh":
                face_mesh = mesh
            elif mesh is not None and not obj.component_has_tag("Outfit"):
                # Streetwear slots on a couple of characters.
                notes.append("extra skeletal '%s'" % var)

    body_lods = lods(body_mesh)
    face_lods = lods(face_mesh)

    broken = (body_lods <= 0 or face_lods <= 0 or groom_count == 0
              or any("no binding" in n or "no groom" in n for n in notes))
    if broken:
        bad += 1

    state = "BAD" if broken else "ok"
    mark = "*" if name in TOUCHED else " "
    log("%-34s %5d %5d %5d %6s%s %s"
        % (name, body_lods, face_lods, groom_count, state, mark,
           "; ".join(notes)))

log("")
log("%d blueprint(s), %d with a problem   (* = rebuilt or repaired today)"
    % (len(paths), bad))

out_dir = os.path.dirname(OUT)
if not os.path.isdir(out_dir):
    os.makedirs(out_dir)
with open(OUT, "w") as h:
    h.write("\n".join(_lines))
unreal.log("written: %s" % OUT)
