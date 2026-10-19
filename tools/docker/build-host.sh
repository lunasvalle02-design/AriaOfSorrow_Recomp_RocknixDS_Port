#!/usr/bin/env bash
# Builds an x86_64 Linux binary inside the container, used for headless
# testing (frame dumps) on the development PC.
set -euo pipefail
B=/build/host
cmake -S /src -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$B" --target aria_recomp "$@"
