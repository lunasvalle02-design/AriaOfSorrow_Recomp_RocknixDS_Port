# Build environment for the ROCKNIX (aarch64) port and for host-side
# (x86_64 Linux) testing. Debian bookworm's glibc (2.36) is older than
# ROCKNIX's, so binaries built here run on the device.
FROM debian:bookworm

RUN dpkg --add-architecture arm64 && apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build git ca-certificates pkg-config \
        python3 python3-pil ccache file patchelf \
        libsdl2-dev \
        g++-aarch64-linux-gnu gcc-aarch64-linux-gnu \
        libsdl2-dev:arm64 libgles-dev:arm64 libegl-dev:arm64 \
        fonts-dejavu-core \
        xvfb x11-apps imagemagick && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /src
