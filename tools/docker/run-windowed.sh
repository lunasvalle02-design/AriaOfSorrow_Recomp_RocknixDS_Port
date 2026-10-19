#!/usr/bin/env bash
# Runs the host build in a virtual X display shaped like the RG DS Plus
# (two 1024x768 panels stacked) and grabs screenshots of the whole window.
# Usage: run-windowed.sh <seconds> <shots> [aria args...]
set -uo pipefail
SECS="$1"; SHOTS="$2"; shift 2
RUN=/build/run
mkdir -p "$RUN/fonts" /src/out/win
rm -f /src/out/win/*.png
cp /build/host/aria_recomp "$RUN/"
cp -n "/src/rom/Castlevania - Aria of Sorrow (USA).gba" "$RUN/" 2>/dev/null || true
cp /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf "$RUN/fonts/"
Xvfb :99 -screen 0 ${SCREEN:-1024x1536x24} -nolisten tcp >/dev/null 2>&1 &
XPID=$!
sleep 1
cd "$RUN"
DISPLAY=:99 SDL_AUDIODRIVER=dummy ARIA_DS_WINDOW=${ARIA_DS_WINDOW:-1024x1536} \
  ./aria_recomp --rom "$RUN/Castlevania - Aria of Sorrow (USA).gba" "$@" > /src/out/win/log.txt 2>&1 &
GPID=$!
for i in $(seq 1 "$SHOTS"); do
  sleep "$SECS"
  DISPLAY=:99 xwd -root -silent | convert xwd:- "/src/out/win/s_$(printf %03d $i).png"
done
kill $GPID 2>/dev/null; sleep 0.5; kill -9 $GPID 2>/dev/null
kill $XPID 2>/dev/null
tail -5 /src/out/win/log.txt
