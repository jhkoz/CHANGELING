"""Where does add_sleeves actually put its geometry?

Tracks the vertices that did not exist before the call, so the sleeve is
measured in isolation rather than inside the whole coat.
"""
import bpy, sys
sys.path.insert(0, "D:/CHANGELING/Tools")
exec(open("D:/CHANGELING/Tools/blender_metahuman_garment.py").read())

bpy.ops.wm.open_mainfile(
    filepath="D:/CHANGELING/Tools/Blender/SKM_MHC_Boggan_Male_Wilder_BodyMesh.blend")
body = _find_body(exclude=None)
arm = body.find_armature()
def bpos(n):
    b = arm.data.bones.get(n); return (arm.matrix_world @ b.head_local) if b else None
for n in ("upperarm_l","lowerarm_l","hand_l","neck_01","spine_05"):
    p = bpos(n)
    if p: print("BONE %-11s %.3f %.3f %.3f" % (n, p.x, p.y, p.z))

merge = find_leg_merge(body)
coat = make_dress_shell(top=0.93, bottom=(merge+0.03) if merge else 0.52,
                        body=body, name="Coat")
add_straps(dress=coat, body=body, width=6)

before = {tuple(round(c,6) for c in (coat.matrix_world @ v.co)) for v in coat.data.vertices}
add_sleeves(dress=coat, body=body, style="fitted")
after = [(coat.matrix_world @ v.co) for v in coat.data.vertices]
new = [p for p in after if tuple(round(c,6) for c in p) not in before]

print("NEW sleeve verts: %d" % len(new))
if new:
    xs=[p.x for p in new]; ys=[p.y for p in new]; zs=[p.z for p in new]
    print("  x %.3f..%.3f   y %.3f..%.3f   z %.3f..%.3f"
          % (min(xs),max(xs),min(ys),max(ys),min(zs),max(zs)))
    left = [p for p in new if p.x > 0]
    if left:
        left.sort(key=lambda p: -p.x)
        print("  outermost LEFT verts (x,y,z):")
        for p in left[:6]:
            print("     %.3f %.3f %.3f" % (p.x, p.y, p.z))
