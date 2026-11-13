#!/bin/sh
# A/B of the world a .map load produces: OHPL_IMPL=cpp vs rust, entity counts, names, ids, transforms
# in list (creation) order, from cWorld::DumpH13. Run per map through a headless engine launch.
# usage: scripts/h13-mapdump.sh <map-name> [game]   (soma-play.py start --map; set OHPL_GAME)
set -e
map="${1:?map}"
out="${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/h13-diff"
mkdir -p "$out"
for impl in cpp rust; do
	rm -f "$out/world_$impl.txt"
	OHPL_IMPL=$impl OHPL_H13_MAPDUMP="$out/world_$impl.txt" "$(dirname "$0")/soma-sweep.py" --map "$map" >/dev/null
done
cmp "$out/world_cpp.txt" "$out/world_rust.txt"
