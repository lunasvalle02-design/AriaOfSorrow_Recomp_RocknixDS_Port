#!/usr/bin/env bash
# Cross-compiles aria_recomp for ROCKNIX (aarch64) inside the build container.
set -euo pipefail
B=/build/rocknix
# Debian keeps SDL2's arch-specific config header under the multiarch include
# dir, which the cross compiler does not search; expose just that header.
SHIM=/build/rocknix-shim
mkdir -p "$SHIM/SDL2"
cp /usr/include/aarch64-linux-gnu/SDL2/_real_SDL_config.h "$SHIM/SDL2/"
cmake -S /src -B "$B" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=/src/cmake/aarch64-linux-gnu.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS="-isystem $SHIM" -DCMAKE_CXX_FLAGS="-isystem $SHIM" \
  -DCMAKE_EXE_LINKER_FLAGS="-static-libstdc++ -static-libgcc" \
  -DGBARECOMP_SELFHEAL_RECOMPILE_DEFAULT=OFF \
  -DRECOMP_UI_GLES2=ON \
  -DGBARECOMP_COMPILER_CACHE=OFF >/dev/null
cmake --build "$B" --target aria_recomp "$@"
aarch64-linux-gnu-strip -o "$B/aria_recomp.stripped" "$B/aria_recomp"
file "$B/aria_recomp.stripped"
