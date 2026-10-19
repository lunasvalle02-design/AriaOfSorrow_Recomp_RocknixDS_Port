#!/usr/bin/env bash
# Usage: tools/dev/windowed.sh <script|-> <seconds-between-shots> <shots> [aria args...]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
WIN_ROOT="$(cygpath -w "$ROOT")"
SCRIPT="$1"; SECS="$2"; SHOTS="$3"; shift 3
if [ "$SCRIPT" = "-" ]; then cat > "$ROOT/out/replay.txt.in"; else cp "$SCRIPT" "$ROOT/out/replay.txt.in"; fi
python "$ROOT/tools/dev/mkreplay.py" "$ROOT/out/replay.txt.in" > "$ROOT/out/replay.txt"
MSYS_NO_PATHCONV=1 docker run --rm -v "$WIN_ROOT:/src" -v aria-build:/build \
  -e GBARECOMP_INPUT_REPLAY=/src/out/replay.txt \
  ${SCREEN:+-e SCREEN=$SCREEN} ${TAB:+-e ARIA_DEBUG_AUX_TAB=$TAB} ${ARIA_DS_WINDOW:+-e ARIA_DS_WINDOW=$ARIA_DS_WINDOW} \
  aria-rocknix-build bash /src/tools/docker/run-windowed.sh "$SECS" "$SHOTS" "$@"
