"""
probe_wardrobe_build.py -- the decisive test: register a garment from Python,
then actually BUILD the MetaHuman and see whether the dress comes out the
other end as part of the assembled character.

Everything lands in /Game/_Scratch/WardrobeProbe. The real Kith character is
loaded read-only and never saved.

    python Tools/ue_remote.py Tools/probe_wardrobe_build.py

Takes ~6 minutes (a MetaHuman build is not fast).
"""

import os
import time
import traceback

import unreal

SOURCE_CHARACTER = "/Game/_Custom/Characters/Kith/Sidhe/MHC_Sidhe_Grump_Female"
GARMENT = "/Game/_Scratch/RootFixTest/Dress_sidhe_grump_female_rootfix"
SCRATCH = "/Game/_Scratch/WardrobeProbe"
BUILD_OUT = "/Game/_Scratch/WardrobeProbe/Built"
SLOT = "SkeletalMesh"

# A character left registered with the editor subsystem by a previous run
# keeps its package in memory, so delete-then-duplicate returns None. Give
# every run its own names instead of fighting that.
RUN = time.strftime("%H%M%S")

REPORT = os.path.join(
    unreal.Paths.project_dir(), "Tools", "Export", "probe_wardrobe_build.txt")
LINES = []


def log(m=""):
    LINES.append(str(m))
    unreal.log(str(m))


def flush():
    d = os.path.dirname(REPORT)
    if not os.path.isdir(d):
        os.makedirs(d)
    with open(REPORT, "w", encoding="utf-8") as fh:
        fh.write("\n".join(LINES) + "\n")


def head(t):
    log()
    log("=" * 70)
    log(t)
    log("=" * 70)


def main():
    eal = unreal.EditorAssetLibrary
    t0 = time.time()

    head("1. FRESH DUPLICATE")
    dup_path = "%s/MHC_Probe_%s" % (SCRATCH, RUN)
    dup = eal.duplicate_asset(SOURCE_CHARACTER, dup_path)
    if not dup:
        log("  duplicate FAILED: %s" % dup_path)
        flush()
        return
    log("  %s" % dup.get_path_name())
    coll = dup.get_editor_property("internal_collection")

    head("2. WARDROBE ITEM")
    garment = unreal.load_asset(GARMENT)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    wi_name = "WI_%s_%s" % (garment.get_name(), RUN)
    wi_path = "%s/%s" % (SCRATCH, wi_name)
    if eal.does_asset_exist(wi_path):
        eal.delete_asset(wi_path)
    wi = tools.create_asset(wi_name, SCRATCH,
                            unreal.MetaHumanWardrobeItem,
                            unreal.MetaHumanWardrobeItemFactory())
    wi.set_editor_property("principal_asset", garment)
    log("  %s -> %s" % (wi.get_name(), garment.get_name()))

    # A wardrobe item with a null Pipeline fails build validation:
    #   "should use a pipeline derived from UMetaHumanSkeletalMeshPipeline".
    # The shipped default is a BLUEPRINT, so instantiate its generated class
    # outered to the wardrobe item (the property is EditAnywhere, Instanced).
    pipe_bp = "/MetaHumanCharacter/BuildPipeline/DefaultWardobePipeline/" \
              "DefaultSkeletalMeshPipeline"
    pipe_cls = unreal.load_object(None, pipe_bp + ".DefaultSkeletalMeshPipeline_C")
    if pipe_cls is None:
        log("  could not load %s_C -- falling back to the C++ class" % pipe_bp)
        pipe_cls = unreal.MetaHumanDefaultSkeletalMeshPipeline
    pipe = unreal.new_object(pipe_cls, outer=wi)
    wi.set_editor_property("pipeline", pipe)
    log("  pipeline: %s" % wi.get_editor_property("pipeline"))

    head("3. REGISTER + WEAR")
    key = coll.try_add_item_from_wardrobe_item(SLOT, wi)
    log("  key: %r" % (key,))
    log("  variation: %s" % key.get_editor_property("variation"))
    # NB: this is an exposed attribute, not a method -- calling it raises.
    if not key.references_external_wardrobe_item:
        log("  REJECTED -- null key back, aborting")
        flush()
        return
    inst = coll.get_editor_property("default_instance")
    inst.set_single_slot_selection(SLOT, key)
    log("  slot '%s' selected on %s" % (SLOT, inst.get_name()))
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(False, True)
    log("  scratch saved")

    head("4. BUILD (~6 min)")
    sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
    if not sub.try_add_object_to_edit(dup):
        log("  try_add_object_to_edit FAILED -- build will not work")
        flush()
        return
    log("  added to edit")
    log("  can_build: %s" % sub.can_build_meta_human(dup, True))

    params = unreal.MetaHumanCharacterEditorBuildParameters()
    params.set_editor_property("absolute_build_path", BUILD_OUT)
    params.set_editor_property("name_override", "Probe_%s" % RUN)
    log("  building into %s ..." % BUILD_OUT)
    sub.build_meta_human(dup, params)

    # build_meta_human leaves output as dirty in-memory packages; without this
    # the folder stays empty on disk and the log still claims success.
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log("  build returned + packages saved  (%.1f min)" % ((time.time() - t0) / 60.0))

    head("5. WHAT CAME OUT")
    out_dir = "%s/Probe_%s" % (BUILD_OUT, RUN)
    assets = eal.list_assets(out_dir, recursive=True) or []
    log("  %d assets under %s" % (len(assets), out_dir))
    dress_hits = []
    for a in sorted(assets):
        obj = unreal.load_asset(a)
        cls = obj.get_class().get_name() if obj else "?"
        log("    %-14s %s" % (cls, a))
        name = a.lower()
        if "dress" in name or "outfit" in name or "cloth" in name:
            dress_hits.append(a)

    head("6. VERDICT")
    if dress_hits:
        log("  GARMENT PRESENT in the build output:")
        for h in dress_hits:
            log("    %s" % h)
    else:
        log("  No obviously garment-named asset in the output.")
        log("  Check the BP/skeletal meshes above -- the dress may be merged")
        log("  into the assembled character rather than kept as its own asset.")

    # Look inside the generated Blueprint for a component carrying the dress.
    for a in assets:
        if not a.endswith("_C") and "BP_" not in a and "Blueprint" not in a:
            continue
    log()
    log("  report: %s" % REPORT)
    flush()


try:
    main()
except Exception:
    head("FATAL")
    log(traceback.format_exc())
    flush()
