"""
dress_and_build_females.py -- put a court gown on each female Kith character
and build the dressed MetaHuman, entirely from Python.

Uses the wardrobe path proven on 2026-09-21 (see the CHANGELING MetaHuman
clothing notes). Per character:

    WardrobeItem asset (PrincipalAsset = gown, Pipeline = DefaultSkeletalMeshPipeline)
    -> collection.try_add_item_from_wardrobe_item("SkeletalMesh", wi)
    -> instance.set_single_slot_selection("SkeletalMesh", key)
    -> subsystem.build_meta_human(...)

A build takes ~6 minutes, and holding one remote-execution connection open for
fifteen of them is asking for trouble, so this does ONE unbuilt character per
invocation and reports how many remain. Drive it from a shell loop; it is
idempotent and safe to re-run until it says ALL DONE.

Configured by Tools/dress_build_config.json:
    {"mode": "plan"|"run", "variant": "fitted_thin", "only": [...]}

`plan` changes nothing -- it prints the character/gown pairing so the mapping
can be checked before anything is written.
"""

import json
import os
import time
import traceback

import unreal

PROJECT = unreal.Paths.project_dir()
CONFIG = os.path.join(PROJECT, "Tools", "dress_build_config.json")
REPORT = os.path.join(PROJECT, "Tools", "Export", "dress_and_build.txt")
# A character that fails stays un-built, so without this the driving loop would
# pick the same one every time and never reach the rest.
FAILED = os.path.join(PROJECT, "Tools", "Export", "dress_build_failed.json")
# A build can take the whole process down (MetaHumanDefaultEditorPipelineBase::
# TryBakeMaterials -> TG_AsyncExportTask access-violates on some characters),
# which kills us before we can record the failure -- so the loop would pick the
# same character forever. Claim the character on disk BEFORE building; whoever
# runs next sees the orphaned claim and marks it failed.
INFLIGHT = os.path.join(PROJECT, "Tools", "Export", "dress_build_inflight.json")

GARMENT_ROOT = "/Game/_Custom/Garments"
WI_ROOT = "/Game/_Custom/Garments/WardrobeItems"
KITH_ROOT = "/Game/_Custom/Characters/Kith"
BUILD_ROOT = "/Game/_Custom/Characters/Kith/Dressed"
SLOT = "SkeletalMesh"
PIPELINE_BP = ("/MetaHumanCharacter/BuildPipeline/DefaultWardobePipeline/"
               "DefaultSkeletalMeshPipeline.DefaultSkeletalMeshPipeline_C")

LINES = []


def log(m=""):
    LINES.append(str(m))
    unreal.log(str(m))
    # Append immediately. A build can take the process down mid-flight, and a
    # report only written at the end leaves nothing behind when that happens.
    try:
        d = os.path.dirname(REPORT)
        if not os.path.isdir(d):
            os.makedirs(d)
        with open(REPORT, "a", encoding="utf-8") as fh:
            print(m, file=fh)
    except Exception:
        pass


def flush():
    d = os.path.dirname(REPORT)
    if not os.path.isdir(d):
        os.makedirs(d)
    with open(REPORT, "a", encoding="utf-8") as fh:
        fh.write("\n".join(LINES) + "\n")


def failed_list():
    if os.path.isfile(FAILED):
        try:
            with open(FAILED, encoding="utf-8") as fh:
                return list(json.load(fh))
        except Exception:
            return []
    return []


def mark_failed(char_path):
    seen = failed_list()
    if char_path not in seen:
        seen.append(char_path)
    with open(FAILED, "w", encoding="utf-8") as fh:
        json.dump(seen, fh, indent=2)


def quit_if_asked(conf):
    """Shut the editor down so the driving loop gets a fresh process.

    Builds cannot be looped in one session -- the second one dies with
    "Out of video memory" -- and they cannot run headless either, because
    material baking goes through the TextureGraph plugin, which access-violates
    in a commandlet. So each build gets its own full GUI editor, launched with
    -ExecutePythonScript, and quits itself here.
    """
    if not conf.get("quit_after"):
        return
    log("quitting the editor so the next build gets a fresh process")
    try:
        unreal.SystemLibrary.quit_editor()
    except Exception as exc:
        log("quit_editor failed (%s)" % exc)


def claim(char_path):
    with open(INFLIGHT, "w", encoding="utf-8") as fh:
        json.dump({"character": char_path, "at": time.time()}, fh)


def release():
    if os.path.isfile(INFLIGHT):
        os.remove(INFLIGHT)


def reap_crashed():
    """Did a previous run die mid-build? Then that character is poison."""
    if not os.path.isfile(INFLIGHT):
        return None
    try:
        with open(INFLIGHT, encoding="utf-8") as fh:
            stale = json.load(fh).get("character")
    except Exception:
        release()
        return None
    release()
    if not stale:
        return None
    built, _ = already_built(stale)
    if built:
        return None
    mark_failed(stale)
    return stale


def cfg():
    if os.path.isfile(CONFIG):
        with open(CONFIG, encoding="utf-8") as fh:
            return json.load(fh)
    return {}


def tokens(name):
    """Character identity as an unordered token set.

    The gown tags and the MHC asset names do not agree on order or case:
    `boggan_female_grump` (a leftover from the stale duplicate naming) has to
    match `MHC_Boggan_Grump_Female`, and `sidheautumn` has to match
    `SidheAutumn`. Comparing sets of lowercased tokens handles both, and the
    kith names stay distinct under it (sidhe vs sidheautumn).
    """
    name = name.lower()
    for prefix in ("mhc_", "dress_"):
        if name.startswith(prefix):
            name = name[len(prefix):]
    return frozenset(t for t in name.split("_") if t)


def find_assets(class_name, module, root):
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    flt = unreal.ARFilter(
        class_paths=[unreal.TopLevelAssetPath(module, class_name)],
        package_paths=[root],
        recursive_paths=True)
    return ar.get_assets(flt)


def build_pairs(variant):
    """[(character asset, gown asset path)] for every female with a gown."""
    suffix = "_" + variant
    gowns = {}
    for a in find_assets("SkeletalMesh", "/Script/Engine", GARMENT_ROOT):
        name = str(a.asset_name)
        if not name.lower().endswith(suffix):
            continue
        tag = name[: -len(suffix)]
        gowns[tokens(tag)] = str(a.package_name)

    chars = {}
    for a in find_assets("MetaHumanCharacter", "/Script/MetaHumanCharacter",
                         KITH_ROOT):
        chars[tokens(str(a.asset_name))] = str(a.package_name)

    pairs, orphans = [], []
    for key, gown in sorted(gowns.items(), key=lambda kv: kv[1]):
        if key in chars:
            pairs.append((chars[key], gown))
        else:
            orphans.append(gown)
    return pairs, orphans, len(gowns), len(chars)


def already_built(char_path):
    name = char_path.rsplit("/", 1)[-1]
    out = "%s/%s" % (BUILD_ROOT, name.replace("MHC_", ""))
    return bool(unreal.EditorAssetLibrary.list_assets(out, recursive=True)), out


def make_wardrobe_item(gown_path):
    eal = unreal.EditorAssetLibrary
    gown = unreal.load_asset(gown_path)
    wi_name = "WI_%s" % gown.get_name()
    wi_path = "%s/%s" % (WI_ROOT, wi_name)

    if eal.does_asset_exist(wi_path):
        wi = unreal.load_asset(wi_path)
    else:
        wi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            wi_name, WI_ROOT, unreal.MetaHumanWardrobeItem,
            unreal.MetaHumanWardrobeItemFactory())
    wi.set_editor_property("principal_asset", gown)

    # Without a Pipeline the build drops the item with only a log error.
    # The shipped default is a Blueprint, and the property is Instanced, so
    # the object has to be outered to the wardrobe item.
    if not wi.get_editor_property("pipeline"):
        cls = unreal.load_object(None, PIPELINE_BP)
        wi.set_editor_property("pipeline", unreal.new_object(cls, outer=wi))
    return wi


def dress_and_build(char_path, gown_path, out_dir):
    t0 = time.time()
    char = unreal.load_asset(char_path)
    coll = char.get_editor_property("internal_collection")

    wi = make_wardrobe_item(gown_path)
    log("    wardrobe item: %s" % wi.get_name())

    key = coll.try_add_item_from_wardrobe_item(SLOT, wi)
    if not key.references_external_wardrobe_item:
        log("    REJECTED by slot '%s' -- skipping" % SLOT)
        return False
    log("    registered, variation=%s" % key.get_editor_property("variation"))

    coll.get_editor_property("default_instance").set_single_slot_selection(
        SLOT, key)
    log("    slot selected")
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(False, True)

    sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
    if not sub.try_add_object_to_edit(char):
        log("    try_add_object_to_edit FAILED")
        return False
    if not sub.can_build_meta_human(char, True):
        log("    can_build_meta_human said no")
        return False

    params = unreal.MetaHumanCharacterEditorBuildParameters()
    params.set_editor_property("absolute_build_path", BUILD_ROOT)
    params.set_editor_property(
        "name_override", char.get_name().replace("MHC_", ""))
    log("    building -> %s" % out_dir)
    claim(char_path)
    sub.build_meta_human(char, params)
    release()

    # build_meta_human leaves its output as dirty in-memory packages; without
    # this the folder stays empty and the log still says it succeeded.
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)

    made = unreal.EditorAssetLibrary.list_assets(out_dir, recursive=True) or []
    log("    built %d assets in %.1f min" % (len(made), (time.time() - t0) / 60.0))
    return bool(made)


def main():
    conf = cfg()
    mode = conf.get("mode", "plan")
    variant = conf.get("variant", "fitted_thin")
    only = [o.lower() for o in conf.get("only", [])]

    log("")
    log("=" * 70)
    log("dress_and_build_females  mode=%s  variant=%s  %s"
        % (mode, variant, time.strftime("%Y-%m-%d %H:%M:%S")))
    log("=" * 70)

    crashed = reap_crashed() if mode == "run" else None
    if crashed:
        log("PREVIOUS RUN DIED BUILDING %s -- marked failed, moving on"
            % crashed.rsplit("/", 1)[-1])

    pairs, orphans, n_gowns, n_chars = build_pairs(variant)
    if only:
        pairs = [p for p in pairs
                 if any(o in p[0].lower() for o in only)]
    log("gowns matching '%s': %d   characters under Kith: %d"
        % (variant, n_gowns, n_chars))
    log("paired: %d   unmatched gowns: %d" % (len(pairs), len(orphans)))
    for g in orphans:
        log("   NO CHARACTER FOR %s" % g)

    known_bad = failed_list()
    pending = []
    for char_path, gown_path in pairs:
        done, out_dir = already_built(char_path)
        state = "built" if done else ("FAILED EARLIER" if char_path in known_bad
                                      else "PENDING")
        log("  %-58s %s" % (char_path.rsplit("/", 1)[-1], state))
        log("      gown: %s" % gown_path.rsplit("/", 1)[-1])
        if not done and char_path not in known_bad:
            pending.append((char_path, gown_path, out_dir))

    log("")
    log("pending: %d of %d   (skipping %d that failed earlier)"
        % (len(pending), len(pairs), len(known_bad)))

    if mode != "run":
        log("plan mode -- nothing written")
        flush()
        return

    if not pending:
        log("ALL DONE")
        flush()
        quit_if_asked(conf)
        return

    char_path, gown_path, out_dir = pending[0]
    log("")
    log("--- %s" % char_path)
    try:
        ok = dress_and_build(char_path, gown_path, out_dir)
        log("    %s" % ("OK" if ok else "FAILED"))
        if not ok:
            mark_failed(char_path)
    except Exception:
        log(traceback.format_exc())
        mark_failed(char_path)
    log("")
    log("REMAINING AFTER THIS RUN: %d" % (len(pending) - 1))
    flush()
    quit_if_asked(conf)


try:
    main()
except Exception:
    log(traceback.format_exc())
    flush()
