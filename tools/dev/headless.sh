#!/usr/bin/env bash
# Usage: tools/dev/headless.sh <script-file|-> <frames> <dump-start> <stride> [extra aria args...]
# Runs the x86_64 host build headless in Docker with an input replay built from
# the script, dumping every <stride>th frame from <dump-start> to out/frames/.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
WIN_ROOT="$(cygpath -w "$ROOT")"
SCRIPT="$1"; FRAMES="$2"; START="$3"; STRIDE="$4"; shift 4
mkdir -p "$ROOT/out/frames"
rm -f "$ROOT/out/frames"/*
if [ "$SCRIPT" = "-" ]; then cat > "$ROOT/out/replay.txt.in"; else cp "$SCRIPT" "$ROOT/out/replay.txt.in"; fi
python "$ROOT/tools/dev/mkreplay.py" "$ROOT/out/replay.txt.in" > "$ROOT/out/replay.txt"
MSYS_NO_PATHCONV=1 docker run --rm -v "$WIN_ROOT:/src" -v aria-build:/build \
  -e GBARECOMP_INPUT_REPLAY=/src/out/replay.txt \
  -e GBARECOMP_FRAMEDUMP_DIR=/src/out/frames \
  -e GBARECOMP_FRAMEDUMP_START="$START" \
  -e GBARECOMP_FRAMEDUMP_STRIDE="$STRIDE" \
  -e GBARECOMP_FRAMEDUMP_COUNT=100000 \
  -e GBARECOMP_ASSIST_SCRIPT="${ASSIST:-}" \
  -e GBARECOMP_SAVE_STATE_AT="${SAVE_AT:-}" \
  ${DUMP_IO:+-e GBARECOMP_DUMP_IO=1} \
  ${DUMP_MEM:+-e GBARECOMP_DUMP_MEM=1} \
  ${SPLIT:+-e ARIA_DEBUG_SPLIT_TOP=$SPLIT} \
  aria-rocknix-build bash /src/tools/docker/run-host.sh --no-window --frames "$FRAMES" "$@" 2>&1 | grep -v "^\s*bridged\|self_heal\|jump-table\|interpreter bridged" | tail -${TAILN:-6}
ls "$ROOT/out/frames" | wc -l
