#!/usr/bin/env bash
# Builds the apworld and clean game builds, and puts them in <repo>/release:
#   release/animal_crossing.apworld (with the GameCube disc patch, see below)
#   release/AnimalCrossingAP-<version>.zip
#   release/AnimalCrossingAP-<version>-linux.tar.gz
#   release/AnimalCrossingAP-<version>-wii.zip (Homebrew Channel app: apps/AnimalCrossingAP/, extract to the SD/USB root)
#   release/AnimalCrossingAP-<version>-wii-source.zip (the Nintendont fork's source: GPLv2 asks for it alongside)
# The game is built into its own dirs (release/windows32 and linux32, gitignored), never build/, so the zip
# only has build output: no saves, ROM, ap_config.ini or settings from a played copy.
# Usage: ./generate_release.sh [suffix]
#   suffix: e.g. "rc1" -> game shows v0.6.0-rc1, files AnimalCrossingAP-0.6.0-rc1.zip. Game-only (overlay, crash
#   report, log, file names); the apworld and seed matching stay x.y.z.
# The apworld is built by an Archipelago source checkout ("Build APWorlds"): $ARCHIPELAGO, default ignore/Archipelago.
# If it's missing, the script offers to clone Archipelago $AP_VERSION there. The release has its own venv in the
# checkout, .venv-release (made on first use: uv, Python 3.13, Archipelago's requirements). worlds/animal_crossing in
# the checkout is linked to this repo's apworld if missing.
# Needs: podman, ninja, python3, uv, git.
set -e
suffix="${1:-}"
repo="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
cd "$repo"
AP_VERSION=0.6.7 # the Archipelago release the apworld is tested with (tag to clone)
archipelago="${ARCHIPELAGO:-$repo/ignore/Archipelago}"
if [ ! -f "$archipelago/Launcher.py" ]; then
  if [ -e "$archipelago" ] && [ -n "$(ls -A "$archipelago")" ]; then
    echo "generate_release.sh: $archipelago isn't an Archipelago checkout (no Launcher.py)" >&2
    exit 1
  fi
  printf 'No Archipelago checkout at %s. Clone Archipelago %s there? [y/N] ' "$archipelago" "$AP_VERSION"
  read -r answer
  case "$answer" in y|Y|yes) ;; *) echo "generate_release.sh: no checkout, stopping" >&2; exit 1;; esac
  mkdir -p "$(dirname "$archipelago")"
  git -c advice.detachedHead=false clone --branch "$AP_VERSION" --depth 1 https://github.com/ArchipelagoMW/Archipelago "$archipelago"
fi
archipelago="$(cd "$archipelago" && pwd)"
# The release's own venv (Archipelago ignores .venv*). Requirements once; delete the venv to redo, e.g. after
# changing the Archipelago version.
ap_venv="$archipelago/.venv-release"
if [ ! -f "$ap_venv/.setup_done" ]; then
  rm -rf "$ap_venv"
  uv venv --python 3.13 --seed "$ap_venv"
  (
    cd "$archipelago"
    "$ap_venv/bin/python" ModuleUpdate.py -y
    # ModuleUpdate lets zillion's requirements lift typing_extensions past the core pin; back to the pin, or every
    # later Archipelago start asks to reinstall
    "$ap_venv/bin/python" -m pip install -q "$(grep -E '^typing_extensions==' requirements.txt | cut -d' ' -f1)"
  )
  touch "$ap_venv/.setup_done"
fi
ap_python="$ap_venv/bin/python"
world="$archipelago/worlds/animal_crossing"
if [ ! -e "$world" ]; then
  ln -s "$repo/apworld/animal_crossing" "$world"
elif [ "$(readlink -f "$world")" != "$(readlink -f "$repo/apworld/animal_crossing")" ]; then
  echo "generate_release.sh: $world isn't this repo's apworld/animal_crossing" >&2
  exit 1
fi
version="$(python3 -c 'import json; print(json.load(open("apworld/animal_crossing/archipelago.json"))["world_version"])')"
[ -n "$suffix" ] && version="$version-$suffix"
release="$repo/release"
mkdir -p "$release"

# GameCube build (0.7.0) and its patch: bsdiff4 deltas vs the vanilla USA disc in apworld/animal_crossing/data/gc
# (gitignored), so the apworld below carries them (gc_patch.py; launcher "Animal Crossing GameCube Patcher")
python3 configure.py --non-matching
ninja
rm -rf apworld/animal_crossing/data/gc
uv run -q --with bsdiff4 python apworld/animal_crossing/gc_patch.py make build/windows32/bin/rom/*.ciso \
  build/GAFE01_00/static.dol build/GAFE01_00/foresta/foresta.rel.szs "$version" apworld/animal_crossing/data/gc

# Apworld, via Archipelago's "Build APWorlds" (it also opens the output folder)
(
  cd "$archipelago"
  "$ap_python" Launcher.py "Build APWorlds" -- "Animal Crossing"
  cp build/apworlds/animal_crossing.apworld "$release/"
)

# Game build (same as build_podman.sh, with a separate build dir)
podman build -t ap-acgc-build .
podman run --rm -v .:/build:Z --userns=keep-id -e BUILD_DIR=release/windows32 -e PC_VERSION_SUFFIX="$suffix" ap-acgc-build ./build_mingw.sh

# Leftovers from older release builds that the build no longer makes (bin/ isn't cleaned between releases).
# SDL2 -> SDL3 in 0.6.0. Fails on any other SDL2 file so a new leftover gets noticed
stage_bin() {
  rm -rf "$stage/AnimalCrossingAP"
  cp -r "$1" "$stage/AnimalCrossingAP"
  rm -f "$stage/AnimalCrossingAP/SDL2.dll" "$stage/AnimalCrossingAP/licenses/SDL2-LICENSE.txt"
  local left
  left="$(find "$stage/AnimalCrossingAP" -iname '*sdl2*')"
  if [ -n "$left" ]; then
    echo "generate_release.sh: old SDL2 files in $1, remove them:" >&2
    echo "$left" >&2
    exit 1
  fi
}

# Zip bin/ under a named top folder
stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT
stage_bin release/windows32/bin
zip="$release/AnimalCrossingAP-$version.zip"
rm -f "$zip"
python3 -m zipfile -c "$zip" "$stage/AnimalCrossingAP"

# Native Linux build (same as build_podman.sh --linux, separate build dir), as a tarball
podman build --network=host -f Dockerfile.linux -t ap-acgc-build-linux .
podman run --rm --network=host -v .:/build:Z --userns=keep-id -e BUILD_DIR=release/linux32 -e PC_VERSION_SUFFIX="$suffix" \
  ap-acgc-build-linux ./build_linux.sh
stage_bin release/linux32/bin
tarball="$release/AnimalCrossingAP-$version-linux.tar.gz"
rm -f "$tarball"
tar -czf "$tarball" -C "$stage" AnimalCrossingAP

# Wii loader (Nintendont fork; same as build_podman.sh --nintendont, separate build dir, clean). The zip has
# apps/AnimalCrossingAP/ at the top, the layout Homebrew Channel apps ship in: extract it to the SD card or USB root.
# GPLv2: the licence goes into the app folder and the fork's source into its own zip next to it.
podman build --network=host -f Dockerfile.nintendont -t ap-acgc-build-nintendont .
podman run --rm --network=host -v .:/build:Z --userns=keep-id -e NIN_BUILD_DIR=release/nintendont \
  ap-acgc-build-nintendont ./build_nintendont.sh --clean
rm -rf "$stage/apps"
mkdir -p "$stage/apps"
cp -r release/nintendont/apps/AnimalCrossingAP "$stage/apps/"
cp nintendont/COPYING "$stage/apps/AnimalCrossingAP/COPYING.txt"
wii="$release/AnimalCrossingAP-$version-wii.zip"
rm -f "$wii"
python3 -m zipfile -c "$wii" "$stage/apps"
wii_src="$release/AnimalCrossingAP-$version-wii-source.zip"
rm -f "$wii_src"
python3 -m zipfile -c "$wii_src" nintendont

echo "Release files in $release:"
ls -l "$release"
