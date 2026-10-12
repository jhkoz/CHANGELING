"""What applies a constraint change to the body MESH?

set_body_constraints + commit_body_state persisted the measurements (Hip
88->78, Waist 80->66) but the rebuilt body mesh came out identical. Something
between the two must evaluate the parametric body.
"""
import unreal
sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
names = [n for n in dir(sub) if not n.startswith("_") and callable(getattr(sub, n, None))]
skip = {"cast", "get_editor_property", "set_editor_property", "get_class",
        "get_name", "get_full_name", "get_fname", "get_path_name", "get_outer",
        "get_outermost", "get_typed_outer", "get_world", "modify", "rename",
        "set_editor_properties", "reset_editor_property", "get_default_object",
        "static_class", "call_method", "get_package", "is_package_external",
        "acquire_editor_element_handle", "is_editor_property_overridden"}
want = ("body", "state", "apply", "fit", "update", "evaluate", "parametric",
        "constraint", "commit", "rebuild", "regenerate")
unreal.log("=== subsystem functions that could apply a body change ===")
for n in sorted(names):
    if n in skip:
        continue
    if not any(w in n.lower() for w in want):
        continue
    doc = (getattr(sub, n).__doc__ or "").strip().splitlines()
    unreal.log("   %-44s %s" % (n, doc[0][:92] if doc else ""))
