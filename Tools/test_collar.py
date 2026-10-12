"""Does raising the collar above the shoulder give a real armhole?

collar=0.93 puts the shell top at z 1.218, below upperarm_l at 1.252, so the
shell has no shoulder and the 'armholes' are side openings at chest height.
The sleeve then extrudes from there, backwards and short of the arm.
"""
import bpy, sys
sys.path.insert(0, "D:/CHANGELING/Tools")
exec(open("D:/CHANGELING/Tools/blender_metahuman_garment.py").read())

for collar in (0.93, 0.97, 0.99):
    bpy.ops.wm.open_mainfile(
        filepath="D:/CHANGELING/Tools/Blender/SKM_MHC_Boggan_Male_Wilder_BodyMesh.blend")
    body = _find_body(exclude=None)
    arm = body.find_armature()
    def bpos(n):
        b = arm.data.bones.get(n); return (arm.matrix_world @ b.head_local) if b else None
    zs = [(body.matrix_world @ v.co).z for v in body.data.vertices]
    zmin, span = min(zs), max(zs)-min(zs)
    print("=== collar %.2f  -> shell top z %.3f   (upperarm_l z %.3f)"
          % (collar, zmin + span*collar, bpos("upperarm_l").z))
    merge = find_leg_merge(body)
    try:
        coat = make_dress_shell(top=collar, bottom=(merge+0.03) if merge else 0.52,
                                body=body, name="Coat")
        add_straps(dress=coat, body=body, width=6)
        before = {tuple(round(c,6) for c in (coat.matrix_world @ v.co))
                  for v in coat.data.vertices}
        add_sleeves(dress=coat, body=body, style="fitted")
        new = [p for p in ((coat.matrix_world @ v.co) for v in coat.data.vertices)
               if tuple(round(c,6) for c in p) not in before]
        if not new:
            print("   NO sleeve geometry"); continue
        ys=[p.y for p in new]; zz=[p.z for p in new]
        print("   sleeve y %.3f..%.3f   z %.3f..%.3f" % (min(ys),max(ys),min(zz),max(zz)))
        for n in ("upperarm_l","lowerarm_l","hand_l"):
            p = bpos(n)
            print("   within 9cm of %-11s %4d"
                  % (n, sum(1 for q in new if (q-p).length < 0.09)))
    except Exception as exc:
        print("   FAILED: %s" % exc)
