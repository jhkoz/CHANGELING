"""
rebuild_missing_bps.py

Recreate the six Kith Blueprints that were deleted from disk, and repair the one
survivor whose component references are all null.

WHAT HAPPENED
    Six Blueprints are gone from Content/_Custom/Characters/Kith/Blueprints --
    no recycle bin entry, no shadow copy, nothing anywhere on the volume:
        BP_Boggan_Childling_Female      BP_Clurichaun_Wilder_Female
        BP_Boggan_Childling_Male        BP_Clurichaun_Wilder_Male
        BP_Clurichaun_Childling_Female  BP_Eshu_Wilder_Female
    Their MetaHumanCharacter sources and every baked Face/Body mesh and groom
    survive, so the Blueprints -- which are only wrappers over those assets --
    can be rebuilt exactly.

    Separately BP_Eshu_Childling_Male still exists but has FaceMesh, Mesh, Hair,
    Eyebrows and Eyelashes all set to None. That is the "character with no head"
    the level was showing, visible at the Blueprint level. It is repointed here
    rather than recreated.

WHY NOT build_meta_human
    Because that is what caused this. The earlier rebuild regenerated 79 files
    that were not broken, inflated one face from 12 MB to 147 MB by building at
    Cinematic quality, and did not even touch the hand-made Blueprint it was
    supposed to fix -- assembly writes BP_MHC_* inside the character folder
    instead. This script never calls it. It creates a Blueprint over
    AChangelingNPC and points its components at assets that already exist on
    disk, so nothing is regenerated and nothing existing is overwritten.

THE HIERARCHY, and why it has to be this one
    ChangelingNPC.cpp does not look components up by name. RebindGrooms walks
    the body's descendants and treats every UGroomComponent it finds as a groom,
    checking that the groom's attach parent holds the same mesh the binding was
    built against. ConfigureLODSync identifies the face as "the skeletal mesh
    child that is not tagged Outfit". So:

        Mesh (native, the body)
          FaceMesh          SkeletalMeshComponent, untagged
            Hair / Eyebrows / Eyelashes / Beard / ...   GroomComponents

    Grooms hang off FaceMesh, not off the body: a MetaHuman groom binding is
    built against the head, and a binding whose target differs from the mesh it
    is attached to is refused by RebindGrooms and renders unbound -- which puts
    the hair on the floor at the character's feet.

BOGGAN NAMING
    Boggan baked assets use reversed names (MHC_Boggan_Female_Grump), and there
    are ALSO newer folders in forward order (MHC_Boggan_Childling_Female) left
    by the 2026-09-20 pass that broke these characters. All four surviving
    Boggan Blueprints point at the reversed, older folders, and the character
    that check_faces reported healthy (MHC_Boggan_Female_Wilder) is one of them.
    So the reversed folders are the good ones and this follows them.

    py "D:/CHANGELING/Tools/rebuild_missing_bps.py"

DRY_RUN is on. It resolves and reports every asset without creating anything.
Look at the report, then set DRY_RUN = False.
"""

import os
import unreal

DRY_RUN = False

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "rebuild_bps_log.txt")

KITH = "/Game/_Custom/Characters/Kith"
BP_DIR = "%s/Blueprints" % KITH
DONOR = "%s/BP_Sidhe_Wilder_Female" % BP_DIR   # read the anim class off a healthy one

# (Blueprint name, character folder relative to KITH)
TARGETS = [
    ("BP_Boggan_Childling_Female",     "Boggan/MHC_Boggan_Female_Childling"),
    ("BP_Boggan_Childling_Male",       "Boggan/MHC_Boggan_Male_Childling"),
    ("BP_Clurichaun_Childling_Female", "Clurichaun/MHC_Clurichaun_Childling_Female"),
    ("BP_Clurichaun_Wilder_Female",    "Clurichaun/MHC_Clurichaun_Wilder_Female"),
    ("BP_Clurichaun_Wilder_Male",      "Clurichaun/MHC_Clurichaun_Wilder_Male"),
    ("BP_Eshu_Wilder_Female",          "Eshu/MHC_Eshu_Wilder_Female"),
    # Exists, but every reference is None. Repointed, not recreated.
    ("BP_Eshu_Childling_Male",         "Eshu/MHC_Eshu_Childling_Male"),
]

# Groom component names, by the prefix of the groom asset that fills them.
GROOM_SLOTS = ["Hair", "Eyebrows", "Eyelashes", "Beard", "Mustache",
               "Goatee", "Peachfuzz", "Fuzz"]

_lines = []


def log(msg):
    text = str(msg)
    _lines.append(text)
    unreal.log(text)


def flush():
    out_dir = os.path.dirname(OUT)
    if not os.path.isdir(out_dir):
        os.makedirs(out_dir)
    with open(OUT, "w") as handle:
        handle.write("\n".join(_lines))


# ----------------------------------------------------------------- resolving


def resolve(character_folder):
    """Find the face, body and grooms for one character.

    Returns (face, body, grooms, problems) where grooms is a list of
    (slot_name, groom_asset, binding_asset_or_None).
    """
    base = character_folder.rsplit("/", 1)[-1]
    root = "%s/%s" % (KITH, character_folder)
    problems = []

    face = unreal.load_asset("%s/Face/SKM_%s_FaceMesh" % (root, base))
    if face is None:
        problems.append("no face mesh")
    body = unreal.load_asset("%s/Body/SKM_%s_BodyMesh" % (root, base))
    if body is None:
        problems.append("no body mesh")

    grooms = []
    groom_dir = "%s/Grooms" % root
    if unreal.EditorAssetLibrary.does_directory_exist(groom_dir):
        paths = sorted(
            p.split(".")[0]
            for p in unreal.EditorAssetLibrary.list_assets(
                groom_dir, recursive=False, include_folder=False))

        # Sort by CLASS, not by name. Assembly does not always name these
        # consistently: both Eshu characters carry a 199 MB GroomAsset called
        # "GroomAsset_0" whose binding is called "Hair_M_TwistedBraids" with no
        # _Binding suffix at all. Trusting the names wires the hair to nothing.
        found_grooms, bindings = [], {}
        for path in paths:
            name = path.rsplit("/", 1)[-1]
            # Card and helmet LOD meshes and material instances live here too,
            # and there are dozens of them per character.
            if "_CardsMesh" in name or "_Helmet" in name \
                    or name.startswith("MI_"):
                continue
            asset = unreal.load_asset(path)
            if isinstance(asset, unreal.GroomBindingAsset):
                bindings[name] = asset
            elif isinstance(asset, unreal.GroomAsset):
                found_grooms.append((name, asset))

        def slot_for(text):
            for candidate in GROOM_SLOTS:
                if text.startswith(candidate + "_"):
                    return candidate
            return None

        claimed = set()
        for name, asset in found_grooms:
            # Preferred: the binding named after the groom.
            binding_name = None
            if "%s_Binding" % name in bindings:
                binding_name = "%s_Binding" % name

            slot = slot_for(name)

            if binding_name is None:
                # Otherwise match on the face mesh the binding was built
                # against -- a binding aimed at another head is refused by
                # RebindGrooms and renders the hair unbound at the feet.
                for candidate, binding in sorted(bindings.items()):
                    if candidate in claimed:
                        continue
                    try:
                        target = binding.get_target_skeletal_mesh()
                    except Exception:
                        target = None
                    if target is not None and face is not None \
                            and target != face:
                        continue
                    if slot is None and slot_for(candidate) is None:
                        continue
                    binding_name = candidate
                    break

            if binding_name is not None:
                claimed.add(binding_name)
                # An unnamed groom takes its slot from its binding, which is
                # where the real name survived.
                if slot is None:
                    slot = slot_for(binding_name)
                if slot_for(name) is None:
                    problems.append(
                        "groom '%s' is unnamed; paired with binding '%s' -> %s"
                        % (name, binding_name, slot))

            if slot is None:
                problems.append("groom '%s' has no known slot prefix" % name)
                continue
            if binding_name is None:
                problems.append("groom '%s' has no binding asset" % name)
            grooms.append((slot, asset, bindings.get(binding_name)))
        grooms.sort()

    if not grooms:
        problems.append("no grooms found under %s" % groom_dir)

    return face, body, grooms, problems


def donor_anim_class():
    """The anim class every healthy Kith body uses (ABP_Unarmed_C)."""
    bp = unreal.load_asset(DONOR)
    if bp is None:
        return None
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    for handle in sub.k2_gather_subobject_data_for_blueprint(bp):
        obj = lib.get_object(lib.get_data(handle))
        if isinstance(obj, unreal.SkeletalMeshComponent):
            anim = obj.get_editor_property("anim_class")
            if anim is not None:
                return anim
    return None


# ------------------------------------------------------------------ building


def components_of(bp):
    """(root_handle, {variable name: (handle, component)}) for one Blueprint."""
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    found = {}
    root_handle = None
    for handle in sub.k2_gather_subobject_data_for_blueprint(bp):
        data = lib.get_data(handle)
        obj = lib.get_object(data)
        if obj is None:
            continue
        if not isinstance(obj, unreal.ActorComponent):
            if root_handle is None:
                root_handle = handle
            continue
        try:
            var = str(lib.get_variable_name(data))
        except Exception:
            var = obj.get_name()
        found[var] = (handle, obj)
    return root_handle, found


def add_component(bp, parent_handle, klass, name):
    """Add one component under parent_handle and rename it."""
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    params = unreal.AddNewSubobjectParams()
    params.set_editor_property("parent_handle", parent_handle)
    params.set_editor_property("new_class", klass)
    params.set_editor_property("blueprint_context", bp)
    handle, fail = sub.add_new_subobject(params)
    if fail and str(fail):
        raise RuntimeError("add %s: %s" % (name, fail))
    sub.rename_subobject(handle, name)
    return handle


def build_one(bp_name, character_folder, anim_class):
    bp_path = "%s/%s" % (BP_DIR, bp_name)
    face, body, grooms, problems = resolve(character_folder)

    log("   face   : %s" % (face.get_name() if face else "*** MISSING ***"))
    log("   body   : %s" % (body.get_name() if body else "*** MISSING ***"))
    for slot, groom, binding in grooms:
        log("   groom  : %-10s %-28s binding=%s"
            % (slot, groom.get_name(),
               binding.get_name() if binding else "*** MISSING ***"))
    for problem in problems:
        log("   PROBLEM: %s" % problem)

    if face is None or body is None:
        log("   SKIPPED: cannot build without both meshes")
        return False

    if DRY_RUN:
        log("   dry run -- nothing created")
        return True

    existing = unreal.load_asset(bp_path)
    if existing is None:
        factory = unreal.BlueprintFactory()
        parent = unreal.load_class(None, "/Script/CHANGELING.ChangelingNPC")
        if parent is None:
            log("   FAILED: could not load AChangelingNPC")
            return False
        factory.set_editor_property("parent_class", parent)
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            bp_name, BP_DIR, unreal.Blueprint, factory)
        if bp is None:
            log("   FAILED: could not create %s" % bp_path)
            return False
        log("   created %s" % bp_path)
    else:
        bp = existing
        log("   exists -- repointing in place")

    root_handle, found = components_of(bp)

    # The body: native, inherited from ACharacter.
    if "Mesh" not in found:
        log("   FAILED: no inherited Mesh component")
        return False
    mesh_handle, mesh = found["Mesh"]
    mesh.set_editor_property("skeletal_mesh_asset", body)
    if anim_class is not None:
        mesh.set_editor_property("anim_class", anim_class)

    # The face: Blueprint-added, attached to the body.
    if "FaceMesh" not in found:
        add_component(bp, mesh_handle, unreal.SkeletalMeshComponent, "FaceMesh")
        _, found = components_of(bp)
        if "FaceMesh" not in found:
            log("   FAILED: could not add FaceMesh")
            return False
    face_handle, face_comp = found["FaceMesh"]
    face_comp.set_editor_property("skeletal_mesh_asset", face)

    # The grooms: attached to the face, because that is what their bindings
    # were built against.
    for slot, groom, binding in grooms:
        if slot not in found:
            add_component(bp, face_handle, unreal.GroomComponent, slot)
            _, found = components_of(bp)
            if slot not in found:
                log("   FAILED: could not add groom slot %s" % slot)
                continue
        _, groom_comp = found[slot]
        groom_comp.set_editor_property("groom_asset", groom)
        if binding is not None:
            groom_comp.set_editor_property("binding_asset", binding)

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    if not unreal.EditorAssetLibrary.save_asset(bp_path):
        log("   FAILED: could not save %s" % bp_path)
        return False
    log("   saved")
    return True


def verify(bp_name):
    """Read the saved asset back.

    Reporting success without this is how four earlier scripts in this project
    reported success while doing nothing at all.
    """
    bp_path = "%s/%s" % (BP_DIR, bp_name)
    bp = unreal.load_asset(bp_path)
    if bp is None:
        return "NOT ON DISK"
    _, found = components_of(bp)
    bits = []
    for var, (_, comp) in sorted(found.items()):
        if isinstance(comp, unreal.GroomComponent):
            asset = comp.get_editor_property("groom_asset")
            bind = comp.get_editor_property("binding_asset")
            bits.append("%s=%s%s" % (var, "OK" if asset else "EMPTY",
                                     "" if bind else "/NOBIND"))
        elif isinstance(comp, unreal.SkeletalMeshComponent) and var != "Outfit":
            asset = comp.get_editor_property("skeletal_mesh_asset")
            bits.append("%s=%s" % (var, "OK" if asset else "EMPTY"))
    return ", ".join(bits) if bits else "no components"


def main():
    log("=" * 70)
    log("rebuild_missing_bps  --  DRY_RUN=%s" % DRY_RUN)
    log("=" * 70)

    anim = donor_anim_class()
    log("anim class from %s: %s"
        % (DONOR.rsplit("/", 1)[-1], anim.get_name() if anim else "NOT FOUND"))

    done, failed = 0, 0
    for bp_name, folder in TARGETS:
        log("")
        log("=== %s   <- %s" % (bp_name, folder))
        try:
            if build_one(bp_name, folder, anim):
                done += 1
            else:
                failed += 1
        except Exception as exc:
            import traceback
            log("   EXCEPTION: %s" % exc)
            for line in traceback.format_exc().splitlines()[-5:]:
                log("      %s" % line)
            failed += 1

    if not DRY_RUN:
        log("")
        log("=" * 70)
        log("VERIFY (read back from disk)")
        log("=" * 70)
        for bp_name, _ in TARGETS:
            log("   %-32s %s" % (bp_name, verify(bp_name)))

    log("")
    log("=" * 70)
    log("ok %d, failed %d" % (done, failed))
    log("=" * 70)


try:
    main()
finally:
    flush()
    unreal.log("log: %s" % OUT)
