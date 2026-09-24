#!/bin/sh

# haloopdy - 2026
# Just to make life easier: have a shell script that builds 
# when you expect that you're inside the docker container I added
# as part of the archipelago fork. This way if I need more stuff
# for archipelago, I don't have to worry about other stuff.

set -e
 
# Some defaults (also defined in the dockerfile but the dockerfile overrides these
# if you're building through that)
SDL2_VERSION="${SDL2_VERSION:-2.30.10}"
SDL2_DIR="${SDL2_DIR:-/opt/SDL2-${SDL2_VERSION}/i686-w64-mingw32}"
BUILD_DIR="${BUILD_DIR:-pc/build32}"
 
if [ ! -d "$SDL2_DIR" ]; then
  echo "SDL2 not found at $SDL2_DIR" >&2
  echo "Set SDL2_DIR, or rebuild the docker image with --build-arg SDL2_VERSION=$SDL2_VERSION" >&2
  exit 1
fi

cmake -S pc -B "$BUILD_DIR" \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/pc/cmake/Toolchain-mingw32.cmake" \
  -DCMAKE_PREFIX_PATH="$SDL2_DIR"

cmake --build "$BUILD_DIR" -j"$(nproc)"

cp "$SDL2_DIR/bin/SDL2.dll" "$BUILD_DIR/bin/"

