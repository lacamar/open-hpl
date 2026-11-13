#!/bin/sh
# H15 (PhysicsWorld update list, SurfaceData): scene dump and the surface parse of every real materials.cfg must equal the committed goldens.
# usage: scripts/h15-diff.sh <build-dir>   (build-dir holds PhysicsH15Tests.bin.aarch64)
# Regenerate rust/tests/golden/H15: OHPL_H15_OUT=.../surface_parse.txt, OHPL_H15_DUMP=.../h15_scenes.txt.
set -e
bin="${1:?build dir}/PhysicsH15Tests.bin.aarch64"
here="$(dirname "$0")/.."
out="${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/h15-diff"
mkdir -p "$out"
steam="${STEAM_COMMON:-$HOME/.local/share/Steam/steamapps/common}"
list=""
for g in "Amnesia The Dark Descent" "Machine for Pigs" "Amnesia Rebirth" "Amnesia The Bunker" "SOMA"; do
	for f in "$steam/$g"/*materials.cfg; do
		[ -f "$f" ] && list="${list:+$list:}$f"
	done
done
OHPL_H15_SURF="$list" OHPL_H15_OUT="$out/surface.txt" "$bin"
OHPL_H15_DUMP="$out/scenes.txt" OHPL_H15_VERBOSE=1 "$bin"
cmp "$out/surface.txt" "$here/rust/tests/golden/H15/surface_parse.txt"
if cmp "$out/scenes.txt" "$here/rust/tests/golden/H15/h15_scenes.txt"; then
	echo "identical: $(grep -c '^end' "$out/scenes.txt") scenes, $(wc -l < "$out/surface.txt") surface files"
else
	diff "$here/rust/tests/golden/H15/h15_scenes.txt" "$out/scenes.txt" | head -20
	exit 1
fi
