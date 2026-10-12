"""Find a reliable way to tell a commandlet from the running editor.

sys.argv does not work: Python inside a UE commandlet does not receive the
engine's command line. Texture import asserts on Slate in a commandlet, so the
orchestrator needs to know which it is in BEFORE it tries.
"""

import os
import unreal

PROJECT = os.path.normpath(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = os.path.join(PROJECT, "Tools", "Export", "headless_probe.txt")

lines = []


def log(m):
    lines.append(str(m))
    unreal.log(str(m))


import sys
log("sys.argv = %r" % (sys.argv,))

tests = {}


def attempt(label, fn):
    try:
        tests[label] = repr(fn())
    except Exception as exc:
        tests[label] = "EXC: %s" % str(exc).split("\n")[0][:70]


attempt("EditorLevelLibrary.get_editor_world",
        lambda: unreal.EditorLevelLibrary.get_editor_world())
attempt("UnrealEditorSubsystem.get_editor_world",
        lambda: unreal.get_editor_subsystem(
            unreal.UnrealEditorSubsystem).get_editor_world())
attempt("EditorUtilitySubsystem present",
        lambda: unreal.get_editor_subsystem(unreal.EditorUtilitySubsystem) is not None)
attempt("SystemLibrary.get_game_name",
        lambda: unreal.SystemLibrary.get_game_name())
attempt("has LevelEditorSubsystem",
        lambda: unreal.get_editor_subsystem(unreal.LevelEditorSubsystem) is not None)
attempt("Paths.is_project_file_path_set",
        lambda: unreal.Paths.is_project_file_path_set())
attempt("SystemLibrary.is_server", lambda: None)

for k in sorted(tests):
    log("%-42s = %s" % (k, tests[k]))

log("")
log("--- unreal.* names mentioning commandlet/headless/editor-mode ---")
for n in sorted(unreal.__dict__):
    low = n.lower()
    if "commandlet" in low or "headless" in low:
        log("   %s" % n)

with open(OUT, "w") as h:
    h.write("\n".join(lines))
