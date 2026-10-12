"""
probe_dataflow_api.py -- what can the Dataflow blueprint libraries actually do?

READ-ONLY.

probe_dataflow_access turned up three things worth chasing:

  1. ChaosClothAssetFactory ALREADY created a Dataflow graph per cloth asset --
     has_dataflow_asset is True and dataflow_instance.dataflow_asset points at
     DF_<name> sitting beside it. So the graphs exist; the question is what is
     in them.
  2. DataflowBlueprintLibrary and DataflowEditorBlueprintLibrary are exposed.
  3. DataflowVariableOverrides is exposed, and DataflowInstance is a struct
     that carries overrides.

That third one is the interesting one. The six cloth graphs are identical
except for which skeletal mesh the import node reads. If the graph can expose
that mesh as a VARIABLE, then one hand-authored graph can be shared by all six
cloth assets, with each asset overriding the variable -- no node editing at
all, and five fewer graphs to build by hand.

    python Tools/ue_remote.py Tools/probe_dataflow_api.py
"""

import os

import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "dataflow_api.txt")

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


def api(cls, label):
    log("--- %s" % label)
    names = [n for n in dir(cls)
             if not n.startswith("_") and callable(getattr(cls, n, None))]
    skip = {"cast", "get_editor_property", "set_editor_property",
            "set_editor_properties", "reset_editor_property", "get_default_object",
            "static_class", "get_class", "get_name", "get_full_name",
            "get_fname", "get_path_name", "get_outer", "get_outermost",
            "get_typed_outer", "get_world", "modify", "rename", "call_method",
            "set_editor_property_by_name"}
    names = [n for n in names if n not in skip]
    if not names:
        log("      (no callables beyond the boilerplate)")
        return
    for name in sorted(names):
        doc = (getattr(cls, name).__doc__ or "").strip().splitlines()
        first = doc[0][:96] if doc else ""
        log("      %-42s %s" % (name, first))


def main():
    log("=" * 70)
    log("probe_dataflow_api")
    log("=" * 70)

    log("")
    log("1. THE BLUEPRINT LIBRARIES")
    for name in ("DataflowBlueprintLibrary", "DataflowEditorBlueprintLibrary"):
        if hasattr(unreal, name):
            api(getattr(unreal, name), "unreal.%s" % name)
        else:
            log("--- unreal.%s NOT EXPOSED" % name)

    log("")
    log("2. DataflowInstance -- where per-asset overrides would live")
    if hasattr(unreal, "DataflowInstance"):
        inst = unreal.DataflowInstance()
        for name in sorted(n for n in dir(inst) if not n.startswith("_")):
            if callable(getattr(inst, name, None)):
                continue
            try:
                log("      attr %-30s = %r" % (name, getattr(inst, name)))
            except Exception as exc:
                log("      attr %-30s <unreadable: %s>" % (name, str(exc)[:50]))
        for prop in ("dataflow_asset", "variable_overrides", "overrides",
                     "dataflow_terminal"):
            try:
                log("      prop %-30s = %r" % (prop, inst.get_editor_property(prop)))
            except Exception as exc:
                log("      prop %-30s <unreadable: %s>" % (prop, str(exc)[:50]))

    log("")
    log("3. DataflowVariableOverrides")
    if hasattr(unreal, "DataflowVariableOverrides"):
        ov = unreal.DataflowVariableOverrides()
        for name in sorted(n for n in dir(ov) if not n.startswith("_")):
            if callable(getattr(ov, name, None)):
                continue
            try:
                log("      attr %-30s = %r" % (name, getattr(ov, name)))
            except Exception:
                continue

    log("")
    log("4. THE GRAPH THE FACTORY ALREADY MADE")
    cloth = unreal.load_asset(CLOTH)
    if cloth is None:
        log("   no cloth asset at %s" % CLOTH)
    else:
        inst = cloth.get_editor_property("dataflow_instance")
        graph = None
        try:
            graph = inst.get_editor_property("dataflow_asset")
        except Exception as exc:
            log("   could not read dataflow_asset: %s" % exc)
        log("   graph: %s" % (graph.get_path_name() if graph else "None"))
        if graph is not None:
            # Size on disk says whether the factory left it empty or seeded it
            # with a starter graph.
            rel = (graph.get_path_name().split(".")[0]
                   .replace("/Game/", "Content/") + ".uasset")
            full = os.path.join(PROJECT, rel.replace("/", os.sep))
            try:
                log("   on disk: %.1f KB" % (os.path.getsize(full) / 1024.0))
            except OSError:
                log("   on disk: not found at %s" % full)
            api(type(graph), "the Dataflow asset's own API")
            for prop in ("variables", "nodes", "graph", "terminal_nodes"):
                try:
                    log("   prop %-18s = %r" % (prop, graph.get_editor_property(prop)))
                except Exception:
                    continue

    log("")
    log("=" * 70)
    log("Looking for: a way to set a graph VARIABLE per cloth asset. That")
    log("would let one hand-authored graph serve all six garments.")
    log("=" * 70)


try:
    main()
finally:
    flush()
    unreal.log("report: %s" % OUT)
