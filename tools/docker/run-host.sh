#!/usr/bin/env bash
# Runs the host build headless inside the container. Extra args pass through.
set -euo pipefail
RUN=/build/run
mkdir -p "$RUN"
cp /build/host/aria_recomp "$RUN/"
cp -n "/src/rom/Castlevania - Aria of Sorrow (USA).gba" "$RUN/" 2>/dev/null || true
cd "$RUN"
exec ./aria_recomp --rom "$RUN/Castlevania - Aria of Sorrow (USA).gba" "$@"
