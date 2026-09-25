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
  -DCMAKE_PREFIX_PATH="$SDL2_DIR" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

cmake --build "$BUILD_DIR" -j"$(nproc)"

# I want to use compile_commands.json for my clang lsp but we need to do two things:
# - I don't want full rebuilds, so I can't modify anything the system depends on
# - I have to change /build to the actual directory on my machine.
# This means that, in order for your lsp to work, you need to build once.
if [ -n "$HOST_DIR" ] && [ -f "$BUILD_DIR/compile_commands.json" ]; then
  find "$BUILD_DIR" -name '*.rsp' | while read -r rsp; do
    sed "s|$PWD/|$HOST_DIR/|g" "$rsp" > "$rsp.host"
  done
  sed -i -e "s|$PWD/|$HOST_DIR/|g" \
         -e 's|\.rsp\.host|.rsp|g' \
         -e 's|\(@[^ ]*\.rsp\)|\1.host|g' "$BUILD_DIR/compile_commands.json"
fi

cp "$SDL2_DIR/bin/SDL2.dll" "$BUILD_DIR/bin/"

