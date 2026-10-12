#!/bin/bash
# Drive dress_and_build_females.py: ONE FRESH, FULLY-STARTED EDITOR PER BUILD.
#
# Three things all have to be true at once, each learned the hard way:
#
#  1. A fresh process per build. Looping builds in one editor exhausts VRAM --
#     the second dies with "Out of video memory" (D3D12Util.cpp:810).
#  2. A REAL, fully-initialised editor. -run=pythonscript (commandlet) AND
#     -ExecutePythonScript both run the script too early, and the build then
#     access-violates in TryBakeMaterials -> TG_AsyncExportTask (reading 0x200,
#     a null subsystem in the constructor). Same crash either way -- it is not
#     about Slate or about which character.
#  3. Therefore: launch the editor, WAIT for it to answer remote execution,
#     then drive it the way the one successful build was driven.
#
#   bash Tools/dress_build_sweep.sh [max_iterations]

set -u
PROJECT="D:/CHANGELING"
UE="/d/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor.exe"
UPROJ="$PROJECT/CHANGELING.uproject"
SCRIPT="$PROJECT/Tools/dress_and_build_females.py"
REPORT="$PROJECT/Tools/Export/dress_and_build.txt"
FAILED="$PROJECT/Tools/Export/dress_build_failed.json"
DRESSED="$PROJECT/Content/_Custom/Characters/Kith/Dressed"
STARTUP_TIMEOUT=900
BUILD_TIMEOUT=2400
MAX=${1:-16}

progress_count() {
  # Any of these growing counts as forward progress, INCLUDING a crash that
  # only managed to blacklist a character.
  local built failed
  built=$(ls "$DRESSED" 2>/dev/null | wc -l)
  failed=$(grep -c 'MHC_' "$FAILED" 2>/dev/null || echo 0)
  echo $((built + failed))
}

# Syntax-check before launching anything: a bad script fails inside the editor
# two minutes in, and looks exactly like a build crash from out here.
if ! python -c "import ast,io,sys; ast.parse(io.open(sys.argv[1],encoding='utf-8').read())" "$SCRIPT"; then
  echo "FATAL: $SCRIPT does not parse"; exit 1
fi

for i in $(seq 1 "$MAX"); do
  echo "########## ITERATION $i  $(date +%H:%M:%S)"
  before=$(progress_count)
  mark=$(wc -l < "$REPORT" 2>/dev/null || echo 0)

  "$UE" "$UPROJ" -nosplash >/dev/null 2>&1 &
  ED=$!
  if ! python "$PROJECT/Tools/ue_wait.py" "$STARTUP_TIMEOUT"; then
    echo "  editor never came up -- aborting"; kill "$ED" 2>/dev/null; exit 1
  fi

  timeout "$BUILD_TIMEOUT" python "$PROJECT/Tools/ue_remote.py" "$SCRIPT" >/dev/null 2>&1
  [ $? = 124 ] && echo "  build exceeded ${BUILD_TIMEOUT}s"

  # The script calls quit_editor; give it a moment, then make sure.
  for _ in $(seq 1 30); do kill -0 "$ED" 2>/dev/null || break; sleep 2; done
  kill -9 "$ED" 2>/dev/null
  wait "$ED" 2>/dev/null

  tail -n +$((mark + 1)) "$REPORT" 2>/dev/null \
    | grep -aE "^  MHC|^    |^PREVIOUS|^pending|^REMAINING|^ALL DONE" | tail -8

  if tail -n +$((mark + 1)) "$REPORT" 2>/dev/null | grep -q "ALL DONE"; then
    echo "SWEEP COMPLETE"; exit 0
  fi
  after=$(progress_count)
  if [ "$after" = "$before" ]; then
    echo "  no progress this iteration (nothing built, nothing blacklisted). Aborting."
    exit 1
  fi
  echo "  progress: $before -> $after of 15"
done
echo "hit iteration cap"
