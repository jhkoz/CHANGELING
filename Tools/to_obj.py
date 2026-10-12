"""Convert anything assimp can read into an OBJ that Blender can import.

    python Tools/to_obj.py <in.fbx> [--out <out.obj>] [--no-uvs] [--no-normals]

WHY NOT pyassimp. assimp can write OBJ, but only through pyassimp, a ctypes
wrapper that needs an assimp.dll this machine does not have -- the single DLL
on it belongs to Gaea and was built for another application. assimp_py, which
does have working wheels for both this Python and Blender's, is IMPORT ONLY:
53 symbols, and import_file is the only entry point.

So the read comes from assimp_py, whose FBX importer is the mature half of
assimp, and the write happens here. OBJ is text; there is nothing to link.

WHAT OBJ CANNOT CARRY: bones, skin weights, or the armature. A garment taken
out through OBJ returns as static geometry and has to be re-bound. Use this
for shapes. The FBX path through export_garment is what carries a skinned
mesh, and it is the one Unreal accepts.
"""

import os
import sys

try:
    import assimp_py
except ImportError:
    sys.exit("assimp_py is not installed for %s\n"
             "    python -m pip install assimp-py" % sys.executable)

NL = chr(10)


def convert(src, dst, uvs=True, normals=True, y_up=True):
    """Read src with assimp, write dst as OBJ.

    assimp_py hands back MEMORYVIEWS OF FLAT FLOATS, not sequences of
    vectors: a mesh of 89840 verts has len(vertices) == 269520. Indexing it
    as though it held tuples raises "'float' object is not subscriptable",
    and reading len() as a vertex count overstates it threefold -- which is
    also why a mesh reads 317225 verts through assimp and about 70k in
    Blender. Converted once with .tolist(), then walked in strides.
    """
    # WRITE Y-UP, BECAUSE THAT IS WHAT OBJ MEANS.
    #
    # assimp hands the data back on the axes it was authored on -- Z-up, for
    # anything out of Blender -- and Blender's OBJ importer assumes Y-up and
    # rotates. Round-tripped raw, the source's Y range (-0.198..0.082) came
    # back as the imported Z range: the boot arrived lying on its side, with
    # everything else about it correct, which is the kind of fault that
    # survives a glance at the vertex count.
    #
    # Converting here means the default import is right. --keep-axes leaves
    # the coordinates alone for a consumer that wants Z-up, which then has to
    # be imported with up_axis='Z', forward_axis='Y'.
    flags = (assimp_py.Process_Triangulate
             | assimp_py.Process_JoinIdenticalVertices)
    scene = assimp_py.import_file(src, flags)

    meshes = scene.meshes
    print("read %s: %d mesh(es), %d material(s)"
          % (os.path.basename(src), len(meshes), len(scene.materials)))

    lines = ["# " + os.path.basename(src) + " -> obj via assimp_py"]
    v_base = uv_base = n_base = 1

    for index, mesh in enumerate(meshes):
        name = (mesh.name or ("mesh_%d" % index)).replace(" ", "_")
        lines.append("o " + name)

        count = mesh.num_vertices
        verts = mesh.vertices.tolist()
        for i in range(count):
            j = i * 3
            x, y, z = verts[j], verts[j + 1], verts[j + 2]
            if y_up:
                x, y, z = x, z, -y
            lines.append("v %.6f %.6f %.6f" % (x, y, z))

        layer = None
        if uvs and mesh.texcoords:
            comps = mesh.num_uv_components[0] if mesh.num_uv_components else 2
            layer = mesh.texcoords[0].tolist()
            for i in range(count):
                j = i * comps
                lines.append("vt %.6f %.6f" % (layer[j], layer[j + 1]))

        norms = None
        if normals and mesh.normals is not None and len(mesh.normals):
            norms = mesh.normals.tolist()
            for i in range(count):
                j = i * 3
                nx, ny, nz = norms[j], norms[j + 1], norms[j + 2]
                if y_up:
                    nx, ny, nz = nx, nz, -ny
                lines.append("vn %.6f %.6f %.6f" % (nx, ny, nz))

        idx = mesh.indices.tolist()
        tris = 0
        for i in range(0, len(idx) - 2, 3):
            parts = []
            for corner in (idx[i], idx[i + 1], idx[i + 2]):
                v = v_base + corner
                if layer is not None and norms is not None:
                    parts.append("%d/%d/%d"
                                 % (v, uv_base + corner, n_base + corner))
                elif layer is not None:
                    parts.append("%d/%d" % (v, uv_base + corner))
                elif norms is not None:
                    parts.append("%d//%d" % (v, n_base + corner))
                else:
                    parts.append("%d" % v)
            lines.append("f " + " ".join(parts))
            tris += 1

        v_base += count
        if layer is not None:
            uv_base += count
        if norms is not None:
            n_base += count
        print("  %-26s %7d verts  %7d tris" % (name[:26], count, tris))

    with open(dst, "w", encoding="utf-8") as out:
        out.write(NL.join(lines))
        out.write(NL)
    print("wrote %s (%.1f MB)" % (dst, os.path.getsize(dst) / 1048576.0))
    return dst


if __name__ == "__main__":
    args = sys.argv[1:]
    if not args:
        sys.exit(__doc__)
    source = args[0]
    target = (args[args.index("--out") + 1] if "--out" in args
              else os.path.splitext(source)[0] + ".obj")
    convert(source, target,
            uvs="--no-uvs" not in args,
            normals="--no-normals" not in args,
            y_up="--keep-axes" not in args)
