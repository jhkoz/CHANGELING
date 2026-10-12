"""Signatures for dataflow authoring, and what cloth node types exist."""

import os
import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "cloth_nodes_probe.txt")

lines = []


def log(m):
    lines.append(str(m))
    unreal.log(str(m))


lib = unreal.DataflowEditorBlueprintLibrary
for fn in ("add_dataflow_node", "connect_dataflow_nodes",
           "set_dataflow_node_property", "add_dataflow_from_clipboard_content"):
    log("--- %s ---" % fn)
    try:
        log((getattr(lib, fn).__doc__ or "<no doc>").strip())
    except Exception as exc:
        log("   %s" % exc)
    log("")

log("--- unreal.* names that look like cloth dataflow NODES ---")
cands = sorted(n for n in dir(unreal)
               if "cloth" in n.lower() and ("node" in n.lower()
                                            or n.startswith("ChaosClothAsset")))
for n in cands:
    log("   %s" % n)
log("   (%d)" % len(cands))

log("")
log("--- anything ending in 'DataflowNode' ---")
nodes = sorted(n for n in dir(unreal) if n.endswith("DataflowNode"))
for n in nodes[:40]:
    log("   %s" % n)
log("   (%d)" % len(nodes))

with open(OUT, "w") as h:
    h.write("\n".join(lines))
