#!/bin/sh
# Dump the world a .map load produces (entity counts, names, ids, transforms in list order, from cWorld::DumpH13)
# through a headless engine launch; with a second argument the dump is compared to that file.
# usage: scripts/h13-mapdump.sh <map-name> [golden-file]   (set OHPL_GAME)
set -e
map="${1:?map}"
out="${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/h13-diff"
mkdir -p "$out"
rm -f "$out/world_rust.txt"
OHPL_H13_MAPDUMP="$out/world_rust.txt" "$(dirname "$0")/soma-sweep.py" --map "$map" >/dev/null
[ -z "$2" ] || cmp "$out/world_rust.txt" "$2"
