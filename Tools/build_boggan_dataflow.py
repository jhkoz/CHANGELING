"""
build_boggan_dataflow.py -- author the cloth and outfit Dataflow graphs in script.

    python Tools/ue_remote.py Tools/build_boggan_dataflow.py

THIS REPLACES NINE GRAPHS OF HAND WORK

The earlier conclusion -- that Dataflow graphs must be authored by hand -- came
from finding node TYPES unreachable by name, which is about constructing a node
struct from nothing. That is not how the editor does it either.
UDataflowEditorBlueprintLibrary exposes the real surface:

    add_dataflow_node(dataflow, node_type_name, base_name, location) -> Name
    connect_dataflow_nodes(dataflow, from_node, out_pin, to_node, in_pin) -> bool
    set_dataflow_node_property(dataflow, node_name, property_name, value) -> bool

and UDataflowBlueprintLibrary adds

    evaluate_terminal_node_by_name(dataflow, terminal_node_name, result_asset)

which is what actually bakes the graph into the asset. So the whole chain is
scriptable after all, and the manual estimate was wrong.

THE TWO GRAPHS

Cloth, once per garment -- turns a skeletal mesh into a UChaosClothAsset:

    [Cloth Skeletal Mesh Import]  Collection -> [Cloth Terminal]
      SkeletalMesh = the garment

Outfit, once per character -- combines two cloth assets into a
UChaosOutfitAsset, which is the only thing the MetaHuman 'Outfits' slot takes:

    [Get Cloth Asset] (coat)     ClothAsset ->
    [Get Cloth Asset] (breeches) ClothAsset -> [Make Outfit] Outfit -> [Outfit Terminal]

Node type names come from each node's DATAFLOW_NODE_DEFINE_INTERNAL, and the
pin names from the UPROPERTY(Meta=(DataflowInput/DataflowOutput)) members.

PIN NAMES ARE TRIED, NOT ASSUMED
The terminal nodes exist in v1 and v2 forms whose inputs differ (CollectionLod0
versus a CollectionLods array with AddPins), and Make Outfit takes an array pin
too. Rather than guess, each connection tries the plausible names in order and
logs which one took. A connection that never lands is reported, not skipped
silently -- this project has already lost a day to scripts that reported
success while doing nothing.
"""

import os

import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "boggan_dataflow.txt")

CLOTH_DIR = "/Game/_Custom/Outfits/Cloth"
OUTFIT_DIR = "/Game/_Custom/Outfits"
GARMENTS = "/Game/_Custom/Garments"

# ONE bought garment per character now, replacing the procedural coat and
# breeches. The Fab frock coat carries the whole outfit in one mesh -- coat,
# waistcoat, trousers, scarf, cuffs and socks across 12 material slots -- so
# there is nothing left to merge and the merge node is skipped entirely.
#
# It is fitted per character rather than shared: the source is cut for a
# 175.7 cm figure and each Boggan needs its own scale (0.81 for Wilder and
# Grump, 0.67 for the Childling) and its own conform pass, because the
# correction varies by height from x0.82 to x2.07.
#
# (cloth asset, [source skeletal meshes])
CLOTH = [
    ("CA_Suit_Boggan_Childling_Male", ["Frock_boggan_childling_male"]),
    ("CA_Suit_Boggan_Grump_Male",     ["Frock_boggan_grump_male"]),
    ("CA_Suit_Boggan_Wilder_Male",    ["Frock_boggan_wilder_male"]),
    ("CA_Gown_Boggan_Wilder_Female",    ["Gown_boggan_female_wilder"]),
    ("CA_Gown_Boggan_Grump_Female",     ["Gown_boggan_female_grump"]),
    ("CA_Gown_Boggan_Childling_Female", ["Gown_boggan_female_childling"]),
]

# (outfit asset, [cloth assets it contains]) -- one each now.
OUTFITS = [
    ("OA_Boggan_Childling_Male", ["CA_Suit_Boggan_Childling_Male"]),
    ("OA_Boggan_Grump_Male",     ["CA_Suit_Boggan_Grump_Male"]),
    ("OA_Boggan_Wilder_Male",    ["CA_Suit_Boggan_Wilder_Male"]),
    ("OA_Boggan_Wilder_Female",    ["CA_Gown_Boggan_Wilder_Female"]),
    ("OA_Boggan_Grump_Female",     ["CA_Gown_Boggan_Grump_Female"]),
    ("OA_Boggan_Childling_Female", ["CA_Gown_Boggan_Childling_Female"]),
]

IMPORT_NODE = "FChaosClothAssetSkeletalMeshImportNode_v2"
# v1, deliberately. The v2 terminal declares its input as a bare
# TArray<FManagedArrayCollection> CollectionLods with NO DataflowInput meta --
# its pins are created at runtime by AddPins(), so a freshly added v2 node has
# no pin to connect to and connect_dataflow_nodes cannot make one. v1 carries a
# static CollectionLod0 input marked DataflowInput, which exists immediately.
CLOTH_TERMINAL = "FChaosClothAssetTerminalNode"
# v2 here, unlike the terminal. Its constructor calls AddPins() twice
# (NumInitialOptionalInputs = 2), so a freshly added node already has two
# input pins. The v1 merge registers only Collection and grows the rest in
# C++, which left the second garment with nowhere to connect.
MERGE_NODE = "FChaosClothAssetMergeClothCollectionsNode_v2"
GET_CLOTH_NODE = "FChaosGetClothAssetNode"
MAKE_OUTFIT_NODE = "FChaosOutfitAssetMakeOutfitNode"
OUTFIT_TERMINAL = "FChaosOutfitAssetTerminalNode"

# Terminals differ between v1 and v2; Make Outfit takes an array pin.
CLOTH_IN_PINS = ["CollectionLod0", "Collection LOD 0", "CollectionLods[0]",
                 "CollectionLods", "Collection"]


def outfit_in_pins(index):
    """Make Outfit takes an array pin, so element names carry the index."""
    return ["ClothAssets[%d]" % index, "ClothAssets %d" % index,
            "Cloth Assets[%d]" % index, "ClothAssets", "ClothAsset"]

_lines = []
DFE = unreal.DataflowEditorBlueprintLibrary
DFL = unreal.DataflowBlueprintLibrary


def log(msg=""):
    text = str(msg)
    _lines.append(text)
    unreal.log(text)


def flush():
    out_dir = os.path.dirname(OUT)
    if not os.path.isdir(out_dir):
        os.makedirs(out_dir)
    with open(OUT, "w") as handle:
        handle.write("\n".join(_lines))


def package_kb(package_path):
    rel = package_path.split(".")[0].replace("/Game/", "Content/") + ".uasset"
    try:
        return os.path.getsize(
            os.path.join(PROJECT, rel.replace("/", os.sep))) / 1024.0
    except OSError:
        return -1.0


def add_node(graph, type_name, base_name, x, y):
    try:
        return DFE.add_dataflow_node(
            graph, type_name, base_name, unreal.Vector2D(x, y))
    except Exception as exc:
        log("      add %s FAILED: %s" % (type_name, str(exc)[:90]))
        return None


def connect(graph, src, out_pin, dst, in_pins):
    """Try each plausible input pin name; report which took."""
    for pin in in_pins:
        try:
            if DFE.connect_dataflow_nodes(graph, src, out_pin, dst, pin):
                log("      connected %s.%s -> %s.%s" % (src, out_pin, dst, pin))
                return True
        except Exception:
            continue
    log("      COULD NOT CONNECT %s.%s -> %s (tried %s)"
        % (src, out_pin, dst, ", ".join(in_pins)))
    return False


def set_object_property(graph, node, prop, asset):
    """Set an object-valued node property.

    SetDataflowNodeProperty takes the value as a STRING, not an object -- it
    goes through ImportText -- so an asset has to be handed over as its path.
    Both spellings are tried because object properties accept the bare path or
    the Class'/Path' form depending on the property.
    """
    forms = [
        asset.get_path_name(),
        "%s'%s'" % (type(asset).__name__, asset.get_path_name()),
    ]
    for form in forms:
        try:
            if DFE.set_dataflow_node_property(graph, node, prop, form):
                log("      %s.%s = %s" % (node, prop, asset.get_name()))
                return True
        except Exception:
            continue
    log("      COULD NOT SET %s.%s (tried %s)" % (node, prop, " and ".join(forms)))
    return False


def graph_of(asset):
    """A FRESH Dataflow asset for this cloth/outfit asset.

    Deliberately destructive: add_dataflow_node always appends, so re-running
    against an existing graph stacks Import_1, Terminal_1 and so on until the
    terminal evaluates whichever duplicate it finds first.

    It replaces the graph the asset ALREADY points at, reusing that name,
    rather than inventing a new one. ChaosClothAssetFactory creates a graph for
    every cloth asset it makes and names it by stripping the CA_ prefix
    (CA_Suit_X -> DF_Suit_X). An earlier version of this function ignored that,
    created DF_CA_Suit_X alongside and repointed the asset -- leaving the
    factory's graph orphaned, and, across re-runs, leaving live handles to
    graphs it had deleted. That crashed the editor mid-build:

        Assertion failed: (Index >= 0) & (Index < ArrayNum)
        Array index out of bounds: -1073741821 into an array of size 0
        FDataflowInstance::OnOwnerPostEditChangeProperty()

    -1073741821 is 0xC0000003, the uninitialised-memory pattern -- a handle to
    an asset that had been deleted out from under it.
    """
    try:
        instance = asset.get_editor_property("dataflow_instance")
    except Exception as exc:
        log("      no dataflow_instance: %s" % exc)
        return None

    # Prefer the name the asset already uses; fall back to DF_<asset> only if
    # it has no graph yet (which is the case for outfit assets).
    existing = None
    try:
        existing = instance.get_editor_property("dataflow_asset")
    except Exception:
        pass

    if existing is not None:
        path = existing.get_path_name().split(".")[0]
        name = path.rsplit("/", 1)[-1]
        folder = path.rsplit("/", 1)[0]
    else:
        name = "DF_%s" % asset.get_name()
        folder = asset.get_path_name().rsplit("/", 1)[0]
        path = "%s/%s" % (folder, name)

    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    graph = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, folder, unreal.Dataflow, unreal.DataflowAssetFactory())
    if graph is None:
        log("      could not create a Dataflow asset")
        return None
    try:
        instance.set_editor_property("dataflow_asset", graph)
        # The instance is a struct returned BY VALUE -- without writing it
        # back the assignment is lost and the asset still has no graph.
        asset.set_editor_property("dataflow_instance", instance)
        log("      graph %s (%s)"
            % (name, "replaced" if existing is not None else "created"))
    except Exception as exc:
        log("      could not attach the graph: %s" % exc)
        return None
    return graph


def build_cloth(cloth_name, mesh_names):
    """[Import]xN -> [Merge Collection] -> [Cloth Terminal].

    The v1 merge node is used because its inputs are static and named
    (Collection, Collection1, Collection2); the v2 one keeps them in a
    TArray whose pins only exist once AddPins() has run in C++.
    """
    path = "%s/%s" % (CLOTH_DIR, cloth_name)
    asset = unreal.load_asset(path)
    if asset is None:
        log("   SKIPPED: no cloth asset at %s" % path)
        return False

    meshes = []
    for mesh_name in mesh_names:
        mesh = unreal.load_asset("%s/%s" % (GARMENTS, mesh_name))
        if mesh is None:
            log("   SKIPPED: missing %s" % mesh_name)
            return False
        meshes.append((mesh_name, mesh))

    graph = graph_of(asset)
    if graph is None:
        return False
    log("      graph: %s" % graph.get_name())

    term = add_node(graph, CLOTH_TERMINAL, "Terminal", 400.0, 0.0)
    if term is None:
        return False

    imports = []
    for index, (mesh_name, mesh) in enumerate(meshes):
        node = add_node(graph, IMPORT_NODE, "Import%d" % index,
                        -400.0, index * 250.0)
        if node is None:
            return False
        if not set_object_property(graph, node, "SkeletalMesh", mesh):
            return False
        log("      %s <- %s" % (node, mesh_name))
        imports.append(node)

    if len(imports) == 1:
        connect(graph, imports[0], "Collection", term, CLOTH_IN_PINS)
    else:
        merge = add_node(graph, MERGE_NODE, "Merge", 0.0, 0.0)
        if merge is None:
            return False
        # Array connections are named <Property>[<index>].
        for index, node in enumerate(imports):
            connect(graph, node, "Collection", merge,
                    ["Collections[%d]" % index, "Collection %d" % index])
        connect(graph, merge, "Collection", term, CLOTH_IN_PINS)

    try:
        DFL.evaluate_terminal_node_by_name(graph, term, asset)
        log("      evaluated terminal")
    except Exception as exc:
        log("      evaluate FAILED: %s" % exc)
        return False

    unreal.EditorAssetLibrary.save_asset(graph.get_path_name().split(".")[0])
    unreal.EditorAssetLibrary.save_asset(path)
    log("      cloth asset now %.1f KB" % package_kb(path))
    return True


def build_outfit(outfit_name, cloth_names):
    path = "%s/%s" % (OUTFIT_DIR, outfit_name)
    asset = unreal.load_asset(path)
    if asset is None:
        log("   SKIPPED: no outfit asset")
        return False

    graph = graph_of(asset)
    if graph is None:
        return False
    log("      graph: %s" % graph.get_name())

    make = add_node(graph, MAKE_OUTFIT_NODE, "MakeOutfit", -200.0, 0.0)
    term = add_node(graph, OUTFIT_TERMINAL, "Terminal", 200.0, 0.0)
    if make is None or term is None:
        return False

    for index, cloth_name in enumerate(cloth_names):
        cloth = unreal.load_asset("%s/%s" % (CLOTH_DIR, cloth_name))
        if cloth is None:
            log("      missing %s" % cloth_name)
            continue
        getter = add_node(graph, GET_CLOTH_NODE, "Get%d" % index,
                          -600.0, index * 200.0)
        if getter is None:
            continue
        if not set_object_property(graph, getter, "ClothAsset", cloth):
            continue
        connect(graph, getter, "ClothAsset", make, outfit_in_pins(index))

    connect(graph, make, "Outfit", term, ["Outfit"])

    try:
        DFL.evaluate_terminal_node_by_name(graph, term, asset)
        log("      evaluated terminal")
    except Exception as exc:
        log("      evaluate FAILED: %s" % exc)
        return False

    unreal.EditorAssetLibrary.save_asset(graph.get_path_name().split(".")[0])
    unreal.EditorAssetLibrary.save_asset(path)
    log("      outfit asset now %.1f KB" % package_kb(path))
    return True


def main():
    log("=" * 70)
    log("build_boggan_dataflow")
    log("=" * 70)

    log("")
    log("CLOTH ASSETS")
    cloth_ok = 0
    for cloth_name, mesh_names in CLOTH:
        log("")
        log("   === %s  <- %s" % (cloth_name, ", ".join(mesh_names)))
        try:
            if build_cloth(cloth_name, mesh_names):
                cloth_ok += 1
        except Exception as exc:
            import traceback
            log("      EXCEPTION: %s" % exc)
            for line in traceback.format_exc().splitlines()[-4:]:
                log("         %s" % line)

    log("")
    log("OUTFIT ASSETS")
    outfit_ok = 0
    for outfit_name, cloth_names in OUTFITS:
        log("")
        log("   === %s" % outfit_name)
        try:
            if build_outfit(outfit_name, cloth_names):
                outfit_ok += 1
        except Exception as exc:
            import traceback
            log("      EXCEPTION: %s" % exc)
            for line in traceback.format_exc().splitlines()[-4:]:
                log("         %s" % line)

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)

    log("")
    log("=" * 70)
    log("VERIFY -- sizes on disk (empty cloth ~8 KB, empty outfit ~4.7 KB)")
    log("=" * 70)
    for cloth_name, _ in CLOTH:
        log("   %-38s %8.1f KB"
            % (cloth_name, package_kb("%s/%s" % (CLOTH_DIR, cloth_name))))
    for outfit_name, _ in OUTFITS:
        log("   %-38s %8.1f KB"
            % (outfit_name, package_kb("%s/%s" % (OUTFIT_DIR, outfit_name))))

    log("")
    log("cloth built %d/%d, outfits built %d/%d"
        % (cloth_ok, len(CLOTH), outfit_ok, len(OUTFITS)))
    log("=" * 70)


try:
    main()
finally:
    flush()
    unreal.log("report: %s" % OUT)
