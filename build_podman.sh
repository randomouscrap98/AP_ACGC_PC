#!/bin/sh

# haloopdy 2026
# Options are passed to build_mingw.sh (e.g. --clean to remove compiled outputs first).
# THIS is the file to run if you want to build! Should work on any system with podman installed?

podman build -t ap-acgc-build .
podman run --rm -v .:/build:Z --userns=keep-id -e HOST_DIR="$PWD" ap-acgc-build ./build_mingw.sh "$@"
