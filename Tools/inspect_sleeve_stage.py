"""Is the sleeve wrong when created, or wrecked afterwards?

Builds a coat on one avatar and measures the arm region after each stage.
A correct fitted sleeve should have geometry spread along the arm -- near the
upper arm (z~1.25), the elbow (z~1.06) AND the hand (z~0.93). The finished
coat has it only at hand height, so one of these stages is collapsing it.
"""
import bpy, sys
sys.path.insert(0, "D:/CHANGELING/Tools")
exec(open("D:/CHANGELING/Tools/blender_metahuman_garment.py").read())

bpy.ops.wm.open_mainfile(
    filepath="D:/CHANGELING/Tools/Blender/SKM_MHC_Boggan_Male_Wilder_BodyMesh.blend")

body = _find_body(exclude=None)
arm = body.find_armature()

def bpos(n):
    b = arm.data.bones.get(n)
    return (arm.matrix_world @ b.head_local) if b else None

marks = {n: bpos(n) for n in ("upperarm_l", "lowerarm_l", "hand_l")}

def report(obj, label):
    mw = obj.matrix_world
    pts = [mw @ v.co for v in obj.data.vertices]
    print("--- %s: %d verts" % (label, len(pts)))
    for n, p in marks.items():
        near = sum(1 for q in pts if (q - p).length < 0.09)
        print("      within 9cm of %-11s %5d" % (n, near))
    xs = [q.x for q in pts]
    print("      x %.3f..%.3f (w %.3f)" % (min(xs), max(xs), max(xs)-min(xs)))

merge = find_leg_merge(body)
shell_bottom = (merge + 0.03) if merge is not None else 0.52
coat = make_dress_shell(top=0.93, bottom=shell_bottom, body=body, name="Coat")
report(coat, "after shell")

add_straps(dress=coat, body=body, width=STRAP_WIDTHS.get("broad", 6))
report(coat, "after straps")

add_sleeves(dress=coat, body=body, style="fitted")
report(coat, "after sleeves")

split_z = min((coat.matrix_world @ v.co).z for v in coat.data.vertices)
add_thickness(dress=coat, body=body, offset=0.010, thickness=0.007,
              bodice_above_z=split_z)
report(coat, "after thickness (shrinkwrap+solidify)")
