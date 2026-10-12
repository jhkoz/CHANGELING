"""Can a Chaos Cloth dataflow graph be authored from Python, or only opened?"""

import os
import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "cloth_api_probe2.txt")

lines = []


def log(m):
    lines.append(str(m))
    unreal.log(str(m))


for name in ("ChaosClothAsset", "ChaosClothAssetBase", "DataflowBlueprintLibrary",
             "DataflowEditorBlueprintLibrary", "Dataflow", "DataflowAssetFactory",
             "ChaosClothComponent"):
    if not hasattr(unreal, name):
        log("%s: NOT PRESENT" % name)
        continue
    cls = getattr(unreal, name)
    members = sorted(m for m in dir(cls) if not m.startswith("_"))
    log("")
    log("--- %s (%d members) ---" % (name, len(members)))
    for m in members:
        log("   %s" % m)

log("")
log("--- can an empty cloth asset be created? ---")
try:
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.ChaosClothAssetFactory()
    asset = tools.create_asset("TEST_ClothProbe", "/Game/_Scratch",
                               unreal.ChaosClothAsset, factory)
    log("   created: %s" % (asset is not None))
    if asset is not None:
        log("   class: %s" % type(asset).__name__)
        props = [p for p in dir(asset) if not p.startswith("_")
                 and any(k in p.lower() for k in ("dataflow", "collection",
                                                  "lod", "mesh"))]
        log("   relevant properties: %s" % props)
        try:
            df = asset.get_editor_property("dataflow_instance")
            log("   dataflow_instance: %s" % df)
        except Exception as exc:
            log("   dataflow_instance unreadable: %s" % exc)
        unreal.EditorAssetLibrary.delete_asset("/Game/_Scratch/TEST_ClothProbe")
        log("   probe asset deleted")
except Exception as exc:
    log("   FAILED: %s" % exc)

with open(OUT, "w") as h:
    h.write("\n".join(lines))
