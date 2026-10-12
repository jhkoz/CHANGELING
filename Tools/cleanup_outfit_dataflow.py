"""
cleanup_outfit_dataflow.py -- remove the orphaned Dataflow and cloth assets.

    python Tools/ue_remote.py Tools/cleanup_outfit_dataflow.py

WHAT WENT WRONG

Building a character crashed the editor:

    Assertion failed: (Index >= 0) & (Index < ArrayNum)
    Array index out of bounds: -1073741821 into an array of size 0
    FDataflowInstance::OnOwnerPostEditChangeProperty()
      <- FTextureCompilingManager::PostCompilation()

-1073741821 is 0xC0000003, the uninitialised-memory pattern: a
FDataflowInstance resolved a handle to an asset that no longer exists.

The cause is mine. ChaosClothAssetFactory ALREADY creates a Dataflow graph for
every cloth asset it makes, naming it by stripping the CA_ prefix -- so
CA_Suit_X gets DF_Suit_X. build_boggan_dataflow's graph_of() did not know
that: it created its own DF_CA_Suit_X, pointed the asset at that instead, and
left the factory's graph orphaned. Worse, it deletes and recreates its graph on
every run for idempotency, so earlier generations were deleted while other
things still referenced them.

Four generations are now sitting in Outfits/Cloth for three characters.

WHAT THIS DOES

Nothing is deleted on the strength of its name. For every candidate it asks
the asset registry who references it, and only an asset with no referencers
and no live Dataflow link is removed. Anything still referenced is reported
and kept.

DRY_RUN is on. Read the report, then set DRY_RUN = False.
"""

import os

import unreal

DRY_RUN = False

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "cleanup_dataflow.txt")

ROOTS = ["/Game/_Custom/Outfits"]

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


def package_of(path):
    return path.split(".")[0]


def referencers(package_path):
    """Who points at this package, excluding itself."""
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    try:
        options = unreal.AssetRegistryDependencyOptions(
            include_soft_package_references=True,
            include_hard_package_references=True,
            include_searchable_names=False,
            include_soft_management_references=False,
            include_hard_management_references=False)
        found = registry.get_referencers(package_path, options) or []
    except Exception as exc:
        log("      (referencer lookup failed: %s)" % str(exc)[:60])
        return None
    return [str(f) for f in found if str(f) != package_path]


def live_graph(asset):
    """The Dataflow asset this cloth/outfit asset actually points at."""
    try:
        instance = asset.get_editor_property("dataflow_instance")
        graph = instance.get_editor_property("dataflow_asset")
        return package_of(graph.get_path_name()) if graph else None
    except Exception:
        return None


def main():
    log("=" * 74)
    log("cleanup_outfit_dataflow   DRY_RUN=%s" % DRY_RUN)
    log("=" * 74)

    assets = []
    for root in ROOTS:
        assets.extend(package_of(p) for p in
                      unreal.EditorAssetLibrary.list_assets(
                          root, recursive=True, include_folder=False))
    assets = sorted(set(assets))

    cloth_like, graphs, others = [], [], []
    for path in assets:
        asset = unreal.load_asset(path)
        if asset is None:
            continue
        kind = type(asset).__name__
        if kind in ("ChaosClothAsset", "ChaosOutfitAsset"):
            cloth_like.append((path, asset, kind))
        elif kind == "Dataflow":
            graphs.append(path)
        else:
            others.append((path, kind))

    log("")
    log("1. LIVE DATAFLOW LINKS")
    live = set()
    for path, asset, kind in cloth_like:
        graph = live_graph(asset)
        if graph:
            live.add(graph)
        log("   %-46s %-18s -> %s"
            % (path.rsplit("/", 1)[-1], kind,
               graph.rsplit("/", 1)[-1] if graph else "NONE"))

    log("")
    log("2. DATAFLOW ASSETS (%d found, %d of them live)"
        % (len(graphs), len(live)))
    orphan_graphs = []
    for path in graphs:
        if path in live:
            log("   KEEP    %-56s (live)" % path.rsplit("/", 1)[-1])
            continue
        refs = referencers(path)
        if refs is None:
            log("   KEEP    %-56s (could not check)" % path.rsplit("/", 1)[-1])
        elif refs:
            log("   KEEP    %-56s (%d referencer(s))"
                % (path.rsplit("/", 1)[-1], len(refs)))
            for ref in refs[:3]:
                log("              <- %s" % ref)
        else:
            log("   ORPHAN  %s" % path.rsplit("/", 1)[-1])
            orphan_graphs.append(path)

    log("")
    log("3. SUPERSEDED CLOTH ASSETS")
    log("   (CA_Suit_* replaced the per-garment CA_Coat_*/CA_Breeches_* when")
    log("    both garments moved into one cloth asset per character)")
    orphan_cloth = []
    for path, asset, kind in cloth_like:
        name = path.rsplit("/", 1)[-1]
        if not (name.startswith("CA_Coat_") or name.startswith("CA_Breeches_")):
            continue
        refs = referencers(path)
        if refs is None:
            log("   KEEP    %-56s (could not check)" % name)
        elif refs:
            log("   KEEP    %-56s (%d referencer(s))" % (name, len(refs)))
            for ref in refs[:3]:
                log("              <- %s" % ref)
        else:
            log("   ORPHAN  %s" % name)
            orphan_cloth.append(path)

    log("")
    log("4. EVERYTHING ELSE HERE (untouched)")
    for path, kind in others:
        log("   %-46s %s" % (path.rsplit("/", 1)[-1], kind))

    victims = orphan_graphs + orphan_cloth
    log("")
    log("=" * 74)
    log("%d asset(s) to remove" % len(victims))
    for path in victims:
        log("   %s" % path)

    if DRY_RUN:
        log("")
        log("DRY RUN -- nothing deleted. Set DRY_RUN = False to apply.")
        return

    log("")
    log("DELETING")
    gone, failed = 0, 0
    for path in victims:
        try:
            if unreal.EditorAssetLibrary.delete_asset(path):
                log("   deleted %s" % path.rsplit("/", 1)[-1])
                gone += 1
            else:
                log("   FAILED  %s" % path.rsplit("/", 1)[-1])
                failed += 1
        except Exception as exc:
            log("   FAILED  %s: %s" % (path.rsplit("/", 1)[-1], str(exc)[:60]))
            failed += 1

    log("")
    log("=" * 74)
    log("VERIFY -- what is left, and does every live link still resolve?")
    log("=" * 74)
    remaining = []
    for root in ROOTS:
        remaining.extend(package_of(p) for p in
                         unreal.EditorAssetLibrary.list_assets(
                             root, recursive=True, include_folder=False))
    for path in sorted(set(remaining)):
        asset = unreal.load_asset(path)
        kind = type(asset).__name__ if asset else "UNLOADABLE"
        extra = ""
        if kind in ("ChaosClothAsset", "ChaosOutfitAsset"):
            graph = live_graph(asset)
            if graph is None:
                extra = "  -> NO GRAPH"
            elif unreal.EditorAssetLibrary.does_asset_exist(graph):
                extra = "  -> %s ok" % graph.rsplit("/", 1)[-1]
            else:
                extra = "  -> DANGLING %s" % graph.rsplit("/", 1)[-1]
        log("   %-46s %-18s%s" % (path.rsplit("/", 1)[-1], kind, extra))

    log("")
    log("deleted %d, failed %d" % (gone, failed))
    log("=" * 74)


try:
    main()
finally:
    flush()
    unreal.log("report: %s" % OUT)
