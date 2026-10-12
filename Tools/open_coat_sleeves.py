"""Open a generated coat's sleeves so the arms do not come through them.

    blender -b --python Tools/open_coat_sleeves.py -- \
        --suit boggan_childling_male_suit

make_coat builds a sleeve by extruding from the armhole, and the tube it
produces sits ON the arm rather than around it: the arm renders over it and
the character appears bare-armed with a gold cuff floating at the wrist.

The fix is the same one the bought frock coat needed -- measure the arm's
radius along its own bone and open the sleeve to clear it -- so it is
imported from conform_source_garment rather than written twice. Weights are
untouched: moving a vertex does not change which bone owns it, so the coat
stays bound.
"""

import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.append(HERE)

import conform_source_garment as fit


def log(msg):
    print("[sleeves] %s" % msg)


def opt(args, name, default=None):
    return args[args.index(name) + 1] if name in args else default


def main():
    argv = sys.argv
    args = argv[argv.index("--") + 1:] if "--" in argv else []
    suit = opt(args, "--suit", "boggan_childling_male_suit")
    clearance = float(opt(args, "--clearance", "0.014"))

    work = os.path.join(os.path.normpath(os.path.join(HERE, "..")),
                        "Tools", "Blender", "Work")
    path = os.path.join(work, "%s.blend" % suit)
    if not os.path.isfile(path):
        log("no suit blend at %s" % path)
        return
    bpy.ops.wm.open_mainfile(filepath=path)

    coat = body = armature = None
    for obj in bpy.context.scene.objects:
        if obj.type == "ARMATURE":
            armature = obj
        elif obj.type == "MESH":
            if obj.name.startswith("Coat"):
                coat = obj
            elif obj.name.startswith("Avatar_"):
                body = obj
    if coat is None or body is None:
        log("need a Coat and an Avatar_ body in %s" % suit)
        return

    before = [coat.matrix_world @ v.co for v in coat.data.vertices]
    moved = fit.expand_sleeves(coat, body, armature, fit.limb_lookup(body),
                               clearance=clearance)
    after = [coat.matrix_world @ v.co for v in coat.data.vertices]
    log("opened %d sleeve vertices at %.0fmm clearance"
        % (moved, clearance * 1000))
    log("width  x %.3f..%.3f  ->  %.3f..%.3f"
        % (min(p.x for p in before), max(p.x for p in before),
           min(p.x for p in after), max(p.x for p in after)))

    bpy.ops.wm.save_as_mainfile(filepath=path)
    log("saved %s -- re-run exportsuits to pick it up" % path)


main()
