"""Export one Synty Sidekick character's parts for assembly in Blender.

Sidekick splits a character into 25 numbered slots -- head, nose, ears, eyes,
brows, hair, facial hair, torso, hips, arms, hands, legs, feet -- all on the
single SKEL_Default_Sidekick skeleton. Mixing slots across the ten base
variants is what gives NPC variety without any sculpting.

This pulls a Boggan childling's set: base 03 for the body and face, with hair
and ears taken from other bases so he is not simply "base 03".
"""
import os, unreal
PROJECT = os.path.normpath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT_DIR = os.path.join(PROJECT, "Tools", "Export", "SidekickParts")
if not os.path.isdir(OUT_DIR): os.makedirs(OUT_DIR)
SPECIES = "/Game/Synty/SidekickCharacters/Resources/Meshes/Species/Humans"

# slot -> which base variant to take it from.
#
# The library is not ten whole characters. There is ONE body -- torso, hips,
# arms, hands, legs, feet, eyes, tongue all exist only on BASE_01 -- two heads,
# and ten or eleven variants of each facial feature. So variety is a face
# question, not a body one:
#
#   2 heads x 10 hair x 10 beards x 11 noses x 10 ears x 10 brows
#
# which is over two million faces before outfits or colour. Body differences
# between kith come from proportion scaling in Blender instead.
PICK = {
    # face: mixed, which is where a character's identity comes from
    "01HEAD": "02", "35NOSE": "07", "36TETH": "03",
    "03EBRL": "05", "04EBRR": "05",
    "07EARL": "09", "08EARR": "09",      # bigger ears suit a Boggan
    "02HAIR": "06",
    "09FCHR": "01",                       # childling: minimal facial hair
    # body: BASE_01 is the only one that exists
    "05EYEL": "01", "06EYER": "01", "37TONG": "01",
    "10TORS": "01", "17HIPS": "01",
    "11AUPL": "01", "12AUPR": "01", "13ALWL": "01", "14ALWR": "01",
    "15HNDL": "01", "16HNDR": "01",
    "18LEGL": "01", "19LEGR": "01", "20FOTL": "01", "21FOTR": "01",
}

made = missing = 0
for slot, base in sorted(PICK.items()):
    name = "SK_HUMN_BASE_%s_%s_HU01" % (base, slot)
    m = unreal.load_asset("%s/%s" % (SPECIES, name))
    if m is None:
        unreal.log("MISSING %s" % name); missing += 1; continue
    out = os.path.join(OUT_DIR, "%s.fbx" % slot)
    t = unreal.AssetExportTask()
    t.set_editor_property("object", m); t.set_editor_property("filename", out)
    t.set_editor_property("automated", True); t.set_editor_property("prompt", False)
    t.set_editor_property("exporter", unreal.SkeletalMeshExporterFBX())
    if unreal.Exporter.run_asset_export_task(t):
        made += 1
    else:
        unreal.log("FAILED %s" % name); missing += 1
unreal.log("=== exported %d slot(s), %d missing -> %s" % (made, missing, OUT_DIR))
