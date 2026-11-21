#!/bin/sh
# Builds the Nintendont fork (nintendont/) out of tree. Runs inside the Dockerfile.nintendont image
# (build_podman.sh --nintendont), or anywhere DEVKITPRO / DEVKITPPC / DEVKITARM point at the pinned toolchain.
# The source is copied to build/nintendont/src first: Nintendont builds in place and rewrites tracked files
# (loader/loader.dol, loader/data/kernel.zip). Output: build/nintendont/apps/AnimalCrossingAP (Homebrew Channel app).
# --clean: start from a fresh copy (otherwise only changed files are copied and rebuilt).
set -e
cd "$(dirname "$0")"
out="${NIN_BUILD_DIR:-build/nintendont}"
[ "$1" = "--clean" ] && rm -rf "$out/src"
mkdir -p "$out/src"
# cp -u keeps unchanged files' timestamps older than their objects, so make only rebuilds what changed
cp -Ru nintendont/. "$out/src/"
# Serial on purpose: with -j the loader can start before kernel.zip exists (seen 2026-11-21)
make -C "$out/src"
app="$out/apps/AnimalCrossingAP"
mkdir -p "$app"
cp "$out/src/loader/loader.dol" "$app/boot.dol"
cp "$out/src/nintendont/meta.xml" "$out/src/nintendont/icon.png" "$app/"
echo "Done: $app"
