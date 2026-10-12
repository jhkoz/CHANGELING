"""What of Chaos Cloth is reachable from Python in 5.7?"""

import os
import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "cloth_api_probe.txt")

lines = []


def log(m):
    lines.append(str(m))
    unreal.log(str(m))


log("--- unreal.* names mentioning cloth ---")
hits = sorted(n for n in dir(unreal) if "cloth" in n.lower())
for n in hits:
    log("   %s" % n)
log("   (%d)" % len(hits))

log("")
log("--- names mentioning chaos ---")
ch = sorted(n for n in dir(unreal) if "chaos" in n.lower())
for n in ch:
    log("   %s" % n)
log("   (%d)" % len(ch))

log("")
log("--- names mentioning dataflow ---")
df = sorted(n for n in dir(unreal) if "dataflow" in n.lower())
for n in df[:40]:
    log("   %s" % n)
log("   (%d)" % len(df))

for cls_name in ("ClothingAssetFactory", "ChaosClothAssetFactory",
                 "ChaosClothAsset", "ClothingAssetCommon",
                 "ChaosClothComponent", "SkeletalMesh"):
    if not hasattr(unreal, cls_name):
        log("")
        log("%s: NOT PRESENT" % cls_name)
        continue
    cls = getattr(unreal, cls_name)
    members = [m for m in dir(cls)
               if not m.startswith("_")
               and any(k in m.lower() for k in
                       ("cloth", "create", "import", "bind", "section", "add"))]
    log("")
    log("--- %s (cloth/create/bind members) ---" % cls_name)
    for m in sorted(members):
        log("   %s" % m)
    if not members:
        log("   (none)")

with open(OUT, "w") as h:
    h.write("\n".join(lines))
log("")
log("written: %s" % OUT)
