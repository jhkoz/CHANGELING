"""
probe_dataflow_access.py -- can an existing Dataflow graph be cloned and
repointed from Python?

READ-MOSTLY. Anything it creates goes to /Game/_Scratch/DataflowProbe.

WHY THIS IS WORTH ASKING

The plan needs nine Dataflow graphs by hand: six cloth assets (Skeletal Mesh
Import -> Terminal) and three outfits (Make Outfit -> Outfit Terminal). The six
cloth graphs are IDENTICAL except for which skeletal mesh the import node
points at.

So if a UDataflow asset can be duplicated, and the import node's mesh property
reached, the manual work collapses from six graphs to one: author it once, then
clone and repoint five times from here.

An earlier finding in this project was that Dataflow node TYPES are C++ structs
unreachable by name -- that is about CREATING nodes from nothing. Reading and
editing nodes in a graph that already exists is a different question, and this
is the one that matters, because the user is about to author one anyway.

What each answer means:
  * nodes enumerable AND the mesh property settable -> author 1, script 5
  * asset duplicable but nodes opaque               -> author 6, no shortcut
  * nothing reachable                               -> author 6, no shortcut

    python Tools/ue_remote.py Tools/probe_dataflow_access.py
"""

import os
import traceback

import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "dataflow_access.txt")

SCRATCH = "/Game/_Scratch/DataflowProbe"
CLOTH = "/Game/_Custom/Outfits/Cloth/CA_Coat_Boggan_Wilder_Male"

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


def surface(thing, label, keep=None):
    log("--- %s" % label)
    names = [n for n in dir(thing) if not n.startswith("_")]
    if keep:
        names = [n for n in names if any(k in n.lower() for k in keep)]
    if not names:
        log("      (nothing matching)")
        return
    for name in sorted(names):
        try:
            kind = "call" if callable(getattr(thing, name)) else "attr"
        except Exception:
            kind = "?"
        log("      %-4s %s" % (kind, name))


def main():
    log("=" * 70)
    log("probe_dataflow_access")
    log("=" * 70)

    # ------------------------------------------------ 1. what reaches Python
    log("")
    log("1. DATAFLOW CLASSES EXPOSED TO PYTHON")
    names = sorted(n for n in dir(unreal) if "dataflow" in n.lower())
    if names:
        for name in names:
            log("   %s" % name)
    else:
        log("   NONE -- the Dataflow module is not exposed at all")

    # --------------------------------- 2. can we reach a cloth asset's graph
    log("")
    log("2. CAN A CLOTH ASSET'S DATAFLOW BE READ?")
    cloth = unreal.load_asset(CLOTH)
    if cloth is None:
        log("   no cloth asset at %s -- run make_boggan_cloth.py first" % CLOTH)
    else:
        log("   cloth asset: %s (%s)"
            % (cloth.get_name(), type(cloth).__name__))
        surface(cloth, "cloth asset surface",
                keep=["dataflow", "graph", "collection", "terminal"])
        for prop in ("dataflow_instance", "dataflow", "dataflow_asset",
                     "data_flow_instance", "has_dataflow_asset"):
            try:
                value = cloth.get_editor_property(prop)
                log("   property %-24s = %r" % (prop, value))
                # The instance is a struct that should name the asset.
                if value is not None and not isinstance(value, bool):
                    for inner in ("dataflow_asset", "data_flow_asset",
                                  "dataflow", "asset"):
                        try:
                            got = value.get_editor_property(inner)
                            log("      .%-20s = %r" % (inner, got))
                        except Exception:
                            continue
            except Exception as exc:
                log("   property %-24s <unreadable: %s>"
                    % (prop, str(exc)[:60]))

    # ------------------------------------------- 3. can a Dataflow be made
    log("")
    log("3. CAN A DATAFLOW ASSET BE CREATED / DUPLICATED?")
    factory_name = None
    for name in ("DataflowAssetFactory", "DataflowFactory"):
        if hasattr(unreal, name):
            factory_name = name
            break
    log("   factory: %s" % (factory_name or "NONE exposed"))

    graph_cls = None
    for name in ("Dataflow", "DataflowAsset", "DataflowObject"):
        if hasattr(unreal, name):
            graph_cls = getattr(unreal, name)
            log("   graph class: unreal.%s" % name)
            break
    if graph_cls is None:
        log("   no Dataflow asset class exposed -- cloning is impossible")

    if graph_cls is not None and factory_name is not None:
        try:
            path = "%s/DF_Probe" % SCRATCH
            if unreal.EditorAssetLibrary.does_asset_exist(path):
                unreal.EditorAssetLibrary.delete_asset(path)
            made = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                "DF_Probe", SCRATCH, graph_cls,
                getattr(unreal, factory_name)())
            log("   created: %s" % (made.get_path_name() if made else "FAILED"))
            if made is not None:
                surface(made, "dataflow asset surface",
                        keep=["node", "graph", "terminal", "variable"])
                # Nodes are the whole question.
                for prop in ("dataflow", "nodes", "graph", "variables"):
                    try:
                        value = made.get_editor_property(prop)
                        log("   property %-16s = %r" % (prop, value))
                    except Exception:
                        continue
        except Exception as exc:
            log("   create FAILED: %s" % exc)
            for line in traceback.format_exc().splitlines()[-3:]:
                log("      %s" % line)

    log("")
    log("=" * 70)
    log("If section 2 named a Dataflow asset and section 3 could read its")
    log("nodes, then one hand-authored cloth graph can be cloned and")
    log("repointed five times from Python. Otherwise all six are manual.")
    log("=" * 70)


try:
    main()
except Exception as exc:
    log("FATAL: %s" % exc)
    for line in traceback.format_exc().splitlines():
        log("   %s" % line)
finally:
    flush()
    unreal.log("report: %s" % OUT)
