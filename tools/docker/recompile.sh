#!/usr/bin/env bash
# Generates recomp_out/ (the native C++ translation of YOUR ROM) inside the
# build container. recomp_out/ is gitignored and must never be committed.
set -euo pipefail
ROM="${1:-/src/rom/Castlevania - Aria of Sorrow (USA).gba}"
B=/build/tool
cmake -S /src -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$B" --target gba_recompile
cd /src
"$B/third_party/gbarecomp/gba_recompile" --rom "$ROM" --config game.toml \
    --entry 0x080000C0 --codegen-shards 16 --out recomp_out
