#!/bin/sh
# Flatpak launcher: the game folder in /app is read-only, so saves, settings, ap_config.ini
# and the ROM live in the data dir (~/.var/app/<app id>/data) and the game runs with --data-dir.
game=/app/lib/AnimalCrossingAP
data="${XDG_DATA_HOME:-$HOME/.var/app/$FLATPAK_ID/data}"

# The game is 32-bit: without the Compat.i386 extension its loader is missing
if [ ! -e /lib/ld-linux.so.2 ]; then
  echo "Animal Crossing AP needs the 32-bit Flatpak libraries. Install them once with:" >&2
  echo "  flatpak install flathub org.freedesktop.Platform.Compat.i386//26.08 org.freedesktop.Platform.GL32.default//26.08" >&2
  echo "(NVIDIA driver: also the matching org.freedesktop.Platform.GL32.nvidia-* )" >&2
  exit 1
fi

mkdir -p "$data/rom"
# Read-only game files the game opens by relative path; links follow app updates
for f in shaders cacert.pem icon.png; do
  ln -sfn "$game/$f" "$data/$f"
done
# A copy, so it can be edited
[ -e "$data/offline_example.json" ] || cp "$game/offline_example.json" "$data/"

# Wayland app id / X11 window class = the .desktop file name (default in pc_main.c is AnimalCrossingAP)
export SDL_VIDEO_X11_WMCLASS="$FLATPAK_ID"
exec "$game/AnimalCrossing" --data-dir "$data" "$@"
