#!/bin/sh

# haloopdy - 2026
# Native Linux (i686) build. Expects to run inside the Dockerfile.linux container
# (see build_podman.sh --linux), same as build_mingw.sh does for Windows.

set -e

# --clean: remove compiled outputs before building (never deletes BUILD_DIR itself)
CLEAN=0
for arg in "$@"; do
  case "$arg" in
    --clean) CLEAN=1 ;;
    *) echo "Unknown option: $arg (supported: --clean)" >&2; exit 1 ;;
  esac
done

SDL3_VERSION="${SDL3_VERSION:-3.4.20}"
SDL3_DIR="${SDL3_DIR:-/opt/SDL3-${SDL3_VERSION}/i686-linux-gnu}"
OPENSSL_VERSION="${OPENSSL_VERSION:-3.5.8}"
OPENSSL_DIR="${OPENSSL_DIR:-/opt/openssl-${OPENSSL_VERSION}/i686-linux-gnu}"
BUILD_DIR="${BUILD_DIR:-build/linux32}"

for dir in "$SDL3_DIR" "$OPENSSL_DIR"; do
  if [ ! -d "$dir" ]; then
    echo "$dir not found; build inside the Dockerfile.linux image (build_podman.sh --linux)" >&2
    exit 1
  fi
done

# Only the i386 pkg-config files, never the build machine's
export PKG_CONFIG_LIBDIR=/usr/lib/i386-linux-gnu/pkgconfig:/usr/share/pkgconfig

cmake -S pc -B "$BUILD_DIR" \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/pc/cmake/Toolchain-linux32.cmake" \
  -DCMAKE_PREFIX_PATH="$SDL3_DIR" \
  -DOPENSSL_ROOT_DIR="$OPENSSL_DIR"

if [ "$CLEAN" = 1 ]; then
  cmake --build "$BUILD_DIR" --target clean
fi

cmake --build "$BUILD_DIR" -j"$(nproc)"

# Third-party license texts for the statically linked libraries
mkdir -p "$BUILD_DIR/bin/licenses"
cp "$OPENSSL_DIR/LICENSE.txt" "$BUILD_DIR/bin/licenses/OpenSSL-LICENSE.txt"
cp "$SDL3_DIR/LICENSE.txt" "$BUILD_DIR/bin/licenses/SDL3-LICENSE.txt"
