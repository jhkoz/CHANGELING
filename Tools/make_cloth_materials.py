"""
make_cloth_materials.py

Builds the fabric and trim materials for the court gowns and assigns them.

The fabric uses Unreal's CLOTH shading model, which is the difference between a
gown reading as fabric and reading as painted plastic. Cloth adds two inputs
over the standard lit model:

  Fuzz Color -- the colour of light scattering through the surface fibres, which
                is what gives velvet and brocade their sheen at grazing angles.
  Cloth      -- how much of that fuzz contributes, 0 to 1.

Both are exposed as parameters, along with base colour and roughness, so every
gown can be re-coloured per character from a material instance without touching
the graph.

Run headless with the editor CLOSED:
    UnrealEditor-Cmd.exe <project> -run=pythonscript
        -script="D:/CHANGELING/Tools/make_cloth_materials.py"
        -unattended -nosplash -AllowCommandletRendering
        -EnablePlugins=PythonScriptPlugin

No texture import here, so this does not hit the Slate assertion that forced the
body masks into their own editor-only script.
"""

import os
import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
LOG_PATH = os.path.join(PROJECT, "Tools", "Export", "cloth_materials_log.txt")

MAT_DIR = "/Game/_Custom/Materials/Garments"
GARMENT_DIR = "/Game/_Custom/Garments"

_lines = []


def log(msg):
    text = str(msg)
    _lines.append(text)
    unreal.log(text)


def probe():
    """Report the enum members this build actually has, before relying on them."""
    log("--- available MaterialProperty members ---")
    names = [n for n in dir(unreal.MaterialProperty) if n.startswith("MP_")]
    for n in sorted(names):
        log("   %s" % n)
    log("")
    log("--- available shading models ---")
    for n in sorted(n for n in dir(unreal.MaterialShadingModel) if n.startswith("MSM_")):
        log("   %s" % n)
    log("")


def _param(mat, cls, name, x, y, value=None):
    """Add a named parameter expression, or None if the class is unavailable."""
    try:
        node = unreal.MaterialEditingLibrary.create_material_expression(mat, cls, x, y)
    except Exception as exc:
        log("   could not create %s (%s): %s" % (name, cls, exc))
        return None
    try:
        node.set_editor_property("parameter_name", name)
        if value is not None:
            if isinstance(value, unreal.LinearColor):
                node.set_editor_property("default_value", value)
            else:
                node.set_editor_property("default_value", float(value))
    except Exception as exc:
        log("   could not configure %s: %s" % (name, exc))
    return node


def _connect(node, mat, prop, label):
    if node is None:
        return False
    try:
        ok = unreal.MaterialEditingLibrary.connect_material_property(node, "", prop)
        log("   %-12s -> %-24s %s" % (label, str(prop).split(".")[-1],
                                      "ok" if ok else "REFUSED"))
        return bool(ok)
    except Exception as exc:
        log("   %-12s -> %-24s FAILED: %s" % (label, prop, exc))
        return False


def make_fabric():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    name = "M_CourtFabric"
    path = "%s/%s" % (MAT_DIR, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    mat = tools.create_asset(name, MAT_DIR, unreal.Material,
                             unreal.MaterialFactoryNew())
    if mat is None:
        log("FATAL: could not create %s" % path)
        return None

    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_CLOTH)
    log("%s: shading model set to CLOTH" % name)

    base = _param(mat, unreal.MaterialExpressionVectorParameter, "BaseColor",
                  -400, -200, unreal.LinearColor(0.18, 0.05, 0.12, 1.0))
    rough = _param(mat, unreal.MaterialExpressionScalarParameter, "Roughness",
                   -400, 0, 0.82)
    fuzz = _param(mat, unreal.MaterialExpressionVectorParameter, "FuzzColor",
                  -400, 200, unreal.LinearColor(0.55, 0.42, 0.50, 1.0))
    cloth = _param(mat, unreal.MaterialExpressionScalarParameter, "Cloth",
                   -400, 400, 0.85)
    spec = _param(mat, unreal.MaterialExpressionScalarParameter, "Specular",
                  -400, 600, 0.25)

    # The Cloth AMOUNT lives on CustomData0, and Python's MaterialProperty enum
    # has no entry for it -- only 16 members, none custom-data. Leaving it
    # unconnected is not harmless: CustomData0 defaults to 0, which blends the
    # fuzz lobe entirely out and makes the Cloth shading model behave exactly
    # like Default Lit. The whole point would be lost silently.
    #
    # Route everything through MakeMaterialAttributes instead, whose ClearCoat
    # input IS CustomData0 -- the pin is shared, and which name the editor shows
    # depends on the shading model. MP_MATERIAL_ATTRIBUTES is scriptable, so
    # this reaches what the direct property cannot.
    mat.set_editor_property("use_material_attributes", True)
    attrs = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionMakeMaterialAttributes, -100, 0)

    def wire(node, input_name, label):
        if node is None:
            return
        try:
            ok = unreal.MaterialEditingLibrary.connect_material_expressions(
                node, "", attrs, input_name)
            log("   %-12s -> attributes.%-18s %s"
                % (label, input_name, "ok" if ok else "REFUSED"))
        except Exception as exc:
            log("   %-12s -> attributes.%-18s FAILED: %s"
                % (label, input_name, exc))

    wire(base, "BaseColor", "BaseColor")
    wire(rough, "Roughness", "Roughness")
    wire(spec, "Specular", "Specular")
    wire(fuzz, "SubsurfaceColor", "FuzzColor")
    wire(cloth, "ClearCoat", "Cloth")

    MP = unreal.MaterialProperty
    _connect(attrs, mat, MP.MP_MATERIAL_ATTRIBUTES, "Attributes")

    unreal.MaterialEditingLibrary.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(path)
    return mat


def make_trim():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    name = "M_CourtTrim"
    path = "%s/%s" % (MAT_DIR, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    mat = tools.create_asset(name, MAT_DIR, unreal.Material,
                             unreal.MaterialFactoryNew())
    if mat is None:
        return None

    # Default lit, metallic -- braid is metal thread, not fabric, so it keeps
    # the standard model rather than Cloth.
    base = _param(mat, unreal.MaterialExpressionVectorParameter, "BaseColor",
                  -400, -200, unreal.LinearColor(0.72, 0.55, 0.20, 1.0))
    metal = _param(mat, unreal.MaterialExpressionScalarParameter, "Metallic",
                   -400, 0, 1.0)
    rough = _param(mat, unreal.MaterialExpressionScalarParameter, "Roughness",
                   -400, 200, 0.32)

    MP = unreal.MaterialProperty
    _connect(base, mat, MP.MP_BASE_COLOR, "BaseColor")
    _connect(metal, mat, MP.MP_METALLIC, "Metallic")
    _connect(rough, mat, MP.MP_ROUGHNESS, "Roughness")

    unreal.MaterialEditingLibrary.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(path)
    log("%s: default lit, metallic braid" % name)
    return mat


def assign(fabric, trim):
    """Put fabric in slot 0 and trim in slot 1 on every garment."""
    paths = [p.split(".")[0] for p in
             unreal.EditorAssetLibrary.list_assets(GARMENT_DIR, recursive=True,
                                                   include_folder=False)]
    done, skipped = 0, 0
    for path in sorted(paths):
        mesh = unreal.load_asset(path)
        if not isinstance(mesh, unreal.SkeletalMesh):
            continue
        try:
            mats = mesh.get_editor_property("materials")
            if len(mats) < 1:
                skipped += 1
                continue

            # REBUILD the array. get_editor_property returns struct entries BY
            # VALUE, so assigning to mats[0].material_interface edits a copy and
            # writing the list back changes nothing -- silently. That is how all
            # 60 garments ended up reporting success with <none> in every slot.
            rebuilt = []
            for index, slot in enumerate(mats):
                entry = unreal.SkeletalMaterial()
                entry.set_editor_property(
                    "material_interface",
                    trim if (index == 1 and trim is not None) else fabric)
                try:
                    entry.set_editor_property("material_slot_name",
                                              slot.get_editor_property("material_slot_name"))
                except Exception:
                    pass          # keep whatever default the struct carries
                rebuilt.append(entry)

            mesh.set_editor_property("materials", rebuilt)
            unreal.EditorAssetLibrary.save_asset(path)

            # Read it back. An assignment that reports success and does nothing
            # is the failure mode this whole function already fell into once.
            check = unreal.load_asset(path).get_editor_property("materials")
            if not check or check[0].material_interface is None:
                log("   DID NOT STICK: %s" % path.rsplit("/", 1)[-1])
                skipped += 1
                continue

            done += 1
            if done == 1:
                log("   slots on the first garment: %d, slot0 now %s"
                    % (len(mats), check[0].material_interface.get_name()))
        except Exception as exc:
            log("   FAILED %s: %s" % (path.rsplit("/", 1)[-1], exc))
            skipped += 1
    log("assigned materials on %d garment(s), skipped %d" % (done, skipped))
    return done


def main():
    probe()
    fabric = make_fabric()
    log("")
    trim = make_trim()
    log("")
    if fabric is not None:
        assign(fabric, trim)
    log("")
    log("=" * 62)
    log("Fabric: %s/M_CourtFabric (Cloth shading model)" % MAT_DIR)
    log("Trim:   %s/M_CourtTrim" % MAT_DIR)
    log("Make a material instance per character to vary colour and fuzz.")
    log("=" * 62)

    out_dir = os.path.dirname(LOG_PATH)
    if not os.path.isdir(out_dir):
        os.makedirs(out_dir)
    with open(LOG_PATH, "w") as handle:
        handle.write("\n".join(_lines))


main()
