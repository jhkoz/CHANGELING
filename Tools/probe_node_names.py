"""Can cloth dataflow node type names be discovered by trying them?

add_dataflow_node takes a registered FName. The node types are C++ structs, not
UObjects, so they never appear in dir(unreal). If plausible names register, the
graph can be authored from script; if none do, this is editor work.
"""

import os
import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "node_names_probe.txt")
SCRATCH = "/Game/_Scratch"

lines = []


def log(m):
    lines.append(str(m))
    unreal.log(str(m))


CANDIDATES = [
    "ChaosClothAssetSkeletalMeshImportNode",
    "ChaosClothAssetSkeletalMeshImportNode_v2",
    "SkeletalMeshImportNode",
    "SkeletalMeshImportNode_v2",
    "ChaosClothAssetTerminalNode",
    "ClothAssetTerminalNode",
    "ChaosClothAssetSimulationDefaultConfigNode",
    "SimulationDefaultConfigNode",
    "ChaosClothAssetRemeshNode",
    "RemeshNode",
    "ChaosClothAssetTransferSkinWeightsNode",
    "TransferSkinWeightsNode",
    "ChaosClothAssetSelectionNode",
    "SelectionNode",
    "ChaosClothAssetStaticMeshImportNode",
]

tools = unreal.AssetToolsHelpers.get_asset_tools()
df_path = "%s/TEST_ClothGraph" % SCRATCH
if unreal.EditorAssetLibrary.does_asset_exist(df_path):
    unreal.EditorAssetLibrary.delete_asset(df_path)

dataflow = tools.create_asset("TEST_ClothGraph", SCRATCH, unreal.Dataflow,
                              unreal.DataflowAssetFactory())
log("dataflow asset created: %s" % (dataflow is not None))
if dataflow is None:
    log("cannot probe node names without a dataflow asset")
else:
    lib = unreal.DataflowEditorBlueprintLibrary
    ok, bad = [], []
    for name in CANDIDATES:
        try:
            result = lib.add_dataflow_node(dataflow, name, name,
                                           unreal.Vector2D(0.0, 0.0))
            text = str(result)
            if text and text.lower() not in ("none", ""):
                ok.append((name, text))
            else:
                bad.append(name)
        except Exception as exc:
            bad.append("%s (%s)" % (name, str(exc).split("\n")[0][:60]))

    log("")
    log("REGISTERED (%d):" % len(ok))
    for name, result in ok:
        log("   %-46s -> %s" % (name, result))
    log("")
    log("not registered (%d):" % len(bad))
    for name in bad:
        log("   %s" % name)

    unreal.EditorAssetLibrary.delete_asset(df_path)
    log("")
    log("probe asset deleted")

with open(OUT, "w") as h:
    h.write("\n".join(lines))
