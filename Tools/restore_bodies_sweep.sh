#!/bin/bash
# One fresh, fully-started editor per MetaHuman build -- see
# Tools/dress_build_sweep.sh for why all three conditions are required.
set -u
P="D:/CHANGELING"
UE="/d/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor.exe"
SCRIPT="$P/Tools/restore_full_bodies.py"
REPORT="$P/Tools/Export/restore_full_bodies.txt"
MAX=${1:-8}

python -c "import ast,io,sys; ast.parse(io.open(sys.argv[1],encoding='utf-8').read())" "$SCRIPT" || { echo "FATAL: script does not parse"; exit 1; }

for i in $(seq 1 "$MAX"); do
  echo "########## ITERATION $i  $(date +%H:%M:%S)"
  mark=$(wc -l < "$REPORT" 2>/dev/null || echo 0)
  "$UE" "$P/CHANGELING.uproject" -nosplash >/dev/null 2>&1 &
  ED=$!
  if ! python "$P/Tools/ue_wait.py" 900; then
    echo "  editor never came up"; kill "$ED" 2>/dev/null; exit 1
  fi
  timeout 2400 python "$P/Tools/ue_remote.py" "$SCRIPT" >/dev/null 2>&1
  for _ in $(seq 1 30); do kill -0 "$ED" 2>/dev/null || break; sleep 2; done
  kill -9 "$ED" 2>/dev/null; wait "$ED" 2>/dev/null

  tail -n +$((mark + 1)) "$REPORT" 2>/dev/null | grep -aE "harvest|rebuild|OK:|FAILED|REMAINING|ALL DONE"
  if tail -n +$((mark + 1)) "$REPORT" 2>/dev/null | grep -q "ALL DONE"; then
    echo "SWEEP COMPLETE"; exit 0
  fi
  if [ "$mark" = "$(wc -l < "$REPORT" 2>/dev/null || echo 0)" ]; then
    echo "  report did not grow -- aborting"; exit 1
  fi
done
echo "hit iteration cap"
