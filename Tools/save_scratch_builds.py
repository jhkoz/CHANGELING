"""Flush the in-memory assets created by build_meta_human to disk.

build_meta_human() creates its output as dirty in-memory packages; nothing
reaches disk until saved. That is why a build can report "assembly succeeded"
while the output folder appears empty.
"""

import unreal

SCRATCH = "/Game/_Scratch"

if not unreal.EditorAssetLibrary.does_directory_exist(SCRATCH):
    unreal.log("no %s directory in memory or on disk" % SCRATCH)
else:
    assets = unreal.EditorAssetLibrary.list_assets(SCRATCH, recursive=True,
                                                   include_folder=False)
    unreal.log("assets under %s: %d" % (SCRATCH, len(assets)))
    saved, failed = 0, 0
    for path in assets:
        pkg = path.split(".")[0]
        try:
            if unreal.EditorAssetLibrary.save_asset(pkg, only_if_is_dirty=False):
                saved += 1
            else:
                failed += 1
        except Exception as exc:
            unreal.log("   FAILED %s: %s" % (pkg, exc))
            failed += 1
    unreal.log("saved %d, failed %d" % (saved, failed))

# Also flush any other dirty packages the build touched.
try:
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.log("save_dirty_packages done")
except Exception as exc:
    unreal.log("save_dirty_packages failed: %s" % exc)
