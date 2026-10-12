"""
ue_remote.py -- run a Python file inside the already-running UE editor.

Uses the engine's own remote execution client (PythonScriptPlugin), which is
enabled in Config/DefaultEngine.ini via bRemoteExecution=True.

    python Tools/ue_remote.py Tools/inspect_bp_subobjects.py

Beats asking someone to paste `py "..."` into the editor console for every
iteration, and unlike a commandlet it runs in the live editor, so anything
needing a registered character or Slate works.

Prints the editor's stdout back here. A script that raises inside the editor
reports success=False with the traceback in the output.
"""

import os
import sys

ENGINE_PY = os.path.join(
    "D:", os.sep, "Program Files", "Epic Games", "UE_5.7", "Engine",
    "Plugins", "Experimental", "PythonScriptPlugin", "Content", "Python")
sys.path.insert(0, ENGINE_PY)

import remote_execution as re_mod  # noqa: E402


def run(path, timeout=600.0):
    path = os.path.abspath(path)
    if not os.path.isfile(path):
        print("no such script: %s" % path)
        return 2

    remote = re_mod.RemoteExecution()
    remote.start()
    try:
        # The editor announces itself over multicast; give it a moment.
        deadline = __import__("time").time() + 10.0
        nodes = []
        while __import__("time").time() < deadline:
            nodes = remote.remote_nodes
            if nodes:
                break
            __import__("time").sleep(0.5)
        if not nodes:
            print("no UE editor answered on the multicast group.")
            print("Editor open? Plugins > Python, 'Enable Remote Execution' ticked?")
            return 3

        node = nodes[0]
        print("editor: %s" % node.get("node_id", "?"))
        remote.open_command_connection(node)

        result = remote.run_command(
            path, exec_mode=re_mod.MODE_EXEC_FILE, unattended=True)

        out = result.get("output") or []
        for entry in out:
            print("%s: %s" % (entry.get("type", "Log"), entry.get("output", "")))
        ok = result.get("success")
        print("-" * 60)
        print("success: %s" % ok)
        if result.get("result") not in (None, "None"):
            print("result : %s" % result.get("result"))
        return 0 if ok else 1
    finally:
        try:
            remote.close_command_connection()
        except Exception:
            pass
        remote.stop()


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(2)
    sys.exit(run(sys.argv[1]))
