#!/bin/sh

# haloopdy 2026
# Options are passed to build_mingw.sh (e.g. --clean to remove compiled outputs first).
# --linux (first option) builds the native Linux version instead (Dockerfile.linux, build_linux.sh,
# output in build/linux32/bin). --nintendont builds the Wii loader (see below).
# THIS is the file to run if you want to build! Should work on any system with podman installed?

# Host networking avoids pasta, which fails on some hosts (e.g. Ubuntu AppArmor). The build needs
# network for apt and the CMake cacert download, and never listens on anything.
if [ "$1" = "--linux" ]; then
  shift
  podman build --network=host -f Dockerfile.linux -t ap-acgc-build-linux . || exit 1
  podman run --rm --network=host -v .:/build:Z --userns=keep-id ap-acgc-build-linux ./build_linux.sh "$@"
  exit
fi
# --nintendont builds the Wii loader fork in nintendont/ (Dockerfile.nintendont, build_nintendont.sh,
# output in build/nintendont/apps/AnimalCrossingAP)
if [ "$1" = "--nintendont" ]; then
  shift
  podman build --network=host -f Dockerfile.nintendont -t ap-acgc-build-nintendont . || exit 1
  podman run --rm --network=host -v .:/build:Z --userns=keep-id ap-acgc-build-nintendont ./build_nintendont.sh "$@"
  exit
fi
podman build --network=host -t ap-acgc-build . || exit 1
podman run --rm --network=host -v .:/build:Z --userns=keep-id -e HOST_DIR="$PWD" ap-acgc-build ./build_mingw.sh "$@"
