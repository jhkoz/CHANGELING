"""Block until a running UE editor answers remote execution (or time out).

Launching the editor and immediately firing a script at it does not work: the
MetaHuman build crashes in TryBakeMaterials -> TG_AsyncExportTask when the
script runs before the editor has finished initialising, which is exactly what
-ExecutePythonScript and -run=pythonscript both do. Waiting for the editor to
announce itself on the multicast group is the signal that it is really up.

    python Tools/ue_wait.py [timeout_seconds]
"""
import os
import sys
import time

ENGINE_PY = os.path.join(
    "D:", os.sep, "Program Files", "Epic Games", "UE_5.7", "Engine",
    "Plugins", "Experimental", "PythonScriptPlugin", "Content", "Python")
sys.path.insert(0, ENGINE_PY)
import remote_execution as re_mod  # noqa: E402

timeout = float(sys.argv[1]) if len(sys.argv) > 1 else 600.0
remote = re_mod.RemoteExecution()
remote.start()
try:
    deadline = time.time() + timeout
    while time.time() < deadline:
        if remote.remote_nodes:
            print("editor ready after %.0fs" % (timeout - (deadline - time.time())))
            sys.exit(0)
        time.sleep(2.0)
    print("editor did not become ready within %.0fs" % timeout)
    sys.exit(1)
finally:
    remote.stop()
