"""
probe_grump_log.py -- what does the EDITOR say when Grump refuses a wardrobe item?

READ-ONLY on the real characters. Nothing is selected, saved or built.

WHAT HAS ALREADY BEEN RULED OUT

MHC_Boggan_Grump_Male refuses every slot offered to it -- Outfits,
SkeletalMesh, Character, Hair -- and so does MHC_Boggan_Grump_Female, while
every other character tried accepts, including ones that have never worn
anything. Established so far:

  * a fresh DUPLICATE of Grump refuses too, so it is in the asset, not in
    wedged in-memory state
  * crossing characters against outfits puts it on the character:
        Grump  + Grump outfit    refused
        Grump  + Wilder outfit   refused
        Wilder + Grump outfit    ACCEPTED
  * a full re-assembly did not change it
  * the collection's pipeline class is BP_ChangelingPipeline_C on Grump and on
    every character that works
  * can_build_meta_human is True

So the return value has been examined from every angle and says only "no".
This has never looked at what the editor itself LOGGED while saying it --
try_add_item_from_wardrobe_item almost certainly reports a reason, and that
reason has been sitting in the log all along.

It also dumps every readable property of a refusing character beside a working
one, since whatever differs has to be visible somewhere.
"""

import os

import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "grump_log.txt")
LOG = os.path.join(PROJECT, "Saved", "Logs", "CHANGELING.log")

KITH = "/Game/_Custom/Characters/Kith/Boggan"
REFUSES = "%s/MHC_Boggan_Grump_Male" % KITH
WORKS = "%s/MHC_Boggan_Wilder_Male" % KITH
OUTFIT = "/Game/_Custom/Outfits/OA_Boggan_Grump_Male"
SCRATCH = "/Game/_Scratch/GrumpProbe"

_lines = []


def log(msg=""):
    text = str(msg)
    _lines.append(text)
    unreal.log(text)


def flush():
    out_dir = os.path.dirname(OUT)
    if not os.path.isdir(out_dir):
        os.makedirs(out_dir)
    with open(OUT, "w") as handle:
        handle.write("\n".join(_lines))


def log_size():
    try:
        return os.path.getsize(LOG)
    except OSError:
        return 0


def log_tail(since):
    """Whatever the editor wrote to its own log after `since` bytes."""
    try:
        with open(LOG, "r", errors="replace") as handle:
            handle.seek(since)
            return handle.read()
    except OSError:
        return ""


def make_item(tag):
    path = "%s/WI_Log_%s" % (SCRATCH, tag)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    item = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "WI_Log_%s" % tag, SCRATCH, unreal.MetaHumanWardrobeItem,
        unreal.MetaHumanWardrobeItemFactory())
    if item is None:
        return None
    item.set_editor_property("principal_asset", unreal.load_asset(OUTFIT))
    cls = unreal.load_object(
        None,
        "/MetaHumanCharacter/BuildPipeline/DefaultWardobePipeline/"
        "DefaultOutfitPipeline.DefaultOutfitPipeline_C")
    if cls is not None:
        item.set_editor_property("pipeline", unreal.new_object(cls, outer=item))
    return item


def attempt(label, character_path):
    log("")
    log("=" * 70)
    log("%s  %s" % (label, character_path.rsplit("/", 1)[-1]))
    log("=" * 70)

    character = unreal.load_asset(character_path)
    if character is None:
        log("   could not load")
        return
    collection = character.get_editor_property("internal_collection")
    item = make_item(label)
    if collection is None or item is None:
        log("   missing collection or item")
        return

    mark = log_size()
    try:
        key = collection.try_add_item_from_wardrobe_item("Outfits", item)
        if isinstance(key, tuple):
            key = key[1] if len(key) > 1 else None
        ok = key is not None and key.references_external_wardrobe_item
    except Exception as exc:
        ok, key = False, str(exc)[:80]
    log("   result: %s" % ("ACCEPTED" if ok else "refused"))

    fresh = log_tail(mark)
    interesting = [line for line in fresh.splitlines()
                   if any(word in line for word in
                          ("MetaHuman", "Wardrobe", "Palette", "Outfit",
                           "Warning", "Error", "Slot", "Item"))]
    if interesting:
        log("   editor said:")
        for line in interesting[:30]:
            log("      %s" % line.strip()[:170])
    else:
        log("   editor logged nothing about it (%d bytes written)" % len(fresh))


def compare():
    log("")
    log("=" * 70)
    log("PROPERTY COMPARISON  (refusing | working)")
    log("=" * 70)
    bad = unreal.load_asset(REFUSES)
    good = unreal.load_asset(WORKS)
    if bad is None or good is None:
        log("   could not load both")
        return

    names = sorted(n for n in dir(bad)
                   if not n.startswith("_")
                   and not callable(getattr(bad, n, None)))
    for name in names:
        try:
            a = getattr(bad, name)
            b = getattr(good, name)
        except Exception:
            continue
        sa, sb = str(a)[:60], str(b)[:60]
        flag = "  <-- DIFFERS" if sa != sb else ""
        log("   %-34s %-30s %-30s%s" % (name, sa, sb, flag))


def main():
    log("=" * 70)
    log("probe_grump_log")
    log("=" * 70)
    log("log file: %s (%d bytes)" % (LOG, log_size()))

    attempt("works  ", WORKS)
    attempt("REFUSES", REFUSES)
    compare()


try:
    main()
finally:
    flush()
    unreal.log("report: %s" % OUT)
