# Cross toolchain for ROCKNIX (aarch64, glibc) using Debian's multiarch
# packages: g++-aarch64-linux-gnu + libsdl2-dev:arm64.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_LIBRARY_ARCHITECTURE aarch64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# Cortex-A55 (RK3568). Plain armv8.2-a keeps the binary portable to other
# ROCKNIX aarch64 devices.
set(CMAKE_C_FLAGS_INIT "-march=armv8.2-a -mtune=cortex-a55")
set(CMAKE_CXX_FLAGS_INIT "-march=armv8.2-a -mtune=cortex-a55")
