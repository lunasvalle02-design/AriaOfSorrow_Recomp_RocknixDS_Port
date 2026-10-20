#!/usr/bin/env bash
# Dumps a fixed set of frames with the binary in /build/run into /src/out/cmp/$1
# (faithful 240x160 and handheld 240x180) for byte-exact before/after checks.
set -euo pipefail
OUT=/src/out/cmp/$1
rm -rf "$OUT"; mkdir -p "$OUT/native" "$OUT/hh"
cd /build/run
for mode in native hh; do
  extra=""; [ "$mode" = hh ] && extra="--handheld"
  GBARECOMP_INPUT_REPLAY=/src/out/replay.txt GBARECOMP_FRAMEDUMP_DIR="$OUT/$mode" \
  GBARECOMP_FRAMEDUMP_START=5070 GBARECOMP_FRAMEDUMP_STRIDE=7 GBARECOMP_FRAMEDUMP_COUNT=100000 \
    ./aria_recomp --rom "/build/run/Castlevania - Aria of Sorrow (USA).gba" --no-window \
      --frames 700 --load-state /src/out/corridor.state $extra >/dev/null 2>&1
done
ls "$OUT/native" | wc -l; ls "$OUT/hh" | wc -l
