"""Render an actor from a few angles and write PNGs to disk.

    SHOT_ACTOR=SK_Boggan_Childling_Male python Tools/ue_remote.py Tools/shot_actor.py

Uses a SceneCapture2D into a render target rather than
take_high_res_screenshot, which is queued for a frame the editor never
delivers when the call arrives over remote execution -- it reports success and
writes nothing.

Checking a character by reading numbers back has been wrong twice today: a
coat that measured correctly turned out to be a flat decal, and a Childling
that passed two size checks was wearing somebody else's clothes. Looking is
cheaper than either.
"""
import os, math, unreal

PROJECT = os.path.normpath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
SHOT_DIR = os.path.join(PROJECT, "Tools", "Export", "Shots")
if not os.path.isdir(SHOT_DIR):
    os.makedirs(SHOT_DIR)
# Read the target from a sidecar file, NOT the environment: this runs inside
# the editor process, which has its own environment, so anything exported by
# the calling shell is invisible here and the default is silently used.
#
#     echo "CTRL_10TORS control" > Tools/Export/Shots/_target.txt
#
ARGS = os.path.join(SHOT_DIR, "_target.txt")
LABEL, NAME = "SK_Boggan_Childling_Male", "sidekick"
if os.path.exists(ARGS):
    parts = open(ARGS).read().split()
    if parts:
        LABEL = parts[0]
        NAME = parts[1] if len(parts) > 1 else LABEL.lower()
# Four quarters. Three left a 90-degree gap, and with the mesh
# yawed inside the Blueprint the coat's actual front kept landing
# in it -- every angle showed a back or a profile.
ANGLES = (("front", 0.0), ("side", 90.0), ("back", 180.0),
          ("other", 270.0))

# Lit shots show the look; the albedo pass shows what the MATERIAL is doing
# with no lighting in the way. Worth having both: a shadow side in a Lumen
# capture is indistinguishable from a black texture by eye, and reading it
# wrong sent this chase off after Synty's palette for no reason.
SOURCES = (("", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR),
           ("albedo_", unreal.SceneCaptureSource.SCS_BASE_COLOR))

# No flip here. The render target is written the right way up; earlier shots
# only looked upside down because the camera rotation was built as
# Rotator(0, yaw + 180, 0) -- Rotator is (ROLL, PITCH, YAW), so that was a
# 180-degree PITCH and the camera really was upside down.


sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
target = None
for actor in sub.get_all_level_actors() or []:
    if actor.get_actor_label() == LABEL:
        target = actor
        break

if target is None:
    unreal.log("SHOT: no actor labelled %s" % LABEL)
else:
    origin, extent = target.get_actor_bounds(False)
    cam = sub.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(0, 0, 0))
    # The render target needs a world, and the capture actor is the one to
    # hand it -- create_render_target2d takes a world context, not a size pair.
    #
    # RGBA8 matters: the default float format makes export_render_target write
    # EXR bytes regardless of the .png name asked for, and nothing downstream
    # can open the result.
    rt = unreal.RenderingLibrary.create_render_target2d(
        cam, 900, 1300, unreal.TextureRenderTargetFormat.RTF_RGBA8)
    comp = cam.capture_component2d
    comp.set_editor_property("texture_target", rt)
    comp.set_editor_property("capture_source",
                             unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    comp.set_editor_property("fov_angle", 40.0)
    comp.set_editor_property("capture_every_frame", False)
    # Lumen is temporal: a capture with no persistent state starts from
    # nothing every time, so indirect light never accumulates and everything
    # the sun does not hit directly comes out near black. That looked exactly
    # like a broken material -- Synty's own untouched mesh rendered the same
    # way -- when the palette has no black texel in it at all.
    comp.set_editor_property("always_persist_rendering_state", True)

    try:
        # Frame from the LARGEST dimension, not the height. A T-posed
        # character is wider than it is tall, and a distance chosen from
        # height alone puts the side camera inside the outstretched arm.
        # Turn the SUBJECT, not the camera. Orbiting walks the camera into
        # whatever the character is standing near -- a flight of steps, in
        # this level -- and the shot comes back as a wall of stone. One fixed
        # camera position that is known to be clear, and the actor yaws.
        dist = max(extent.x, extent.y, extent.z) * 3.0 + 120.0
        rest = target.get_actor_rotation()
        cam.set_actor_location_and_rotation(
            unreal.Vector(origin.x + dist, origin.y, origin.z + extent.z * 0.15),
            unreal.Rotator(0.0, 0.0, 180.0), False, False)
        try:
            for prefix, source in SOURCES:
                comp.set_editor_property("capture_source", source)
                for tag, yaw in ANGLES:
                    # ABSOLUTE yaw, not an offset from however the actor was
                    # placed. Adding to the placed rotation meant "front" was
                    # whatever direction the character happened to be facing
                    # in the level -- every "front" shot of this character was
                    # actually a profile, which is why its sleeves kept
                    # reading as stubs: an arm pointing at the camera is a
                    # stub. The camera sits at +X, so yaw 180 faces it.
                    target.set_actor_rotation(
                        unreal.Rotator(rest.roll, rest.pitch, 180.0 + yaw),
                        False)
                    # Warm up, then capture: Lumen is temporal and the first
                    # frame has nothing to go on, which reads as black.
                    for _ in range(6):
                        comp.capture_scene()
                    unreal.RenderingLibrary.export_render_target(
                        cam, rt, SHOT_DIR, "%s%s_%s.png" % (prefix, NAME, tag))
                    path = os.path.join(SHOT_DIR,
                                        "%s%s_%s.png" % (prefix, NAME, tag))
                    unreal.log("SHOT: %s (%s)"
                               % (path, "%d bytes" % os.path.getsize(path)
                                  if os.path.exists(path) else "NOT WRITTEN"))
        finally:
            target.set_actor_rotation(rest, False)
    finally:
        sub.destroy_actor(cam)
