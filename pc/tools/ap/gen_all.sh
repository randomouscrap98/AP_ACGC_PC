#!/bin/sh
# Regenerates every generated file of the AP work (header of each says which script made it).
# usage: pc/tools/ap/gen_all.sh [--check]   (--check: report out-of-date files, write nothing, exit 1 if any)
# dump_villagers.py reads villager names from the ROM in build/windows32/bin/rom/ (read only).
cd "$(dirname "$0")" || exit 1
fails=0
for gen in dump_museum.py gen_catalog_data.py dump_villagers.py gen_sound_names.py gen_plant_marks.py; do
  python3 "$gen" "$@" || fails=$((fails + 1))
done
# needs Pillow
uv run -q --with pillow python3 gen_ap_logo.py "$@" || fails=$((fails + 1))
exit $fails
