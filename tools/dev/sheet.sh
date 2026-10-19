#!/usr/bin/env bash
# Builds out/sheet.png from out/frames/*.png (optionally only a subset glob).
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
WIN_ROOT="$(cygpath -w "$ROOT")"
MSYS_NO_PATHCONV=1 docker run --rm -e COLS="${COLS:-5}" -v "$WIN_ROOT:/src" aria-rocknix-build \
  bash -c "python3 /src/tools/dev/montage.py /src/out/sheet.png /src/out/frames/${1:-*}.png"
