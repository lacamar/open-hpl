#!/bin/sh
# Differential run of H15 (PhysicsWorld update list, SurfaceData): OHPL_IMPL=cpp vs rust dumps must be identical,
# and the surface parse (legacy TinyXML loader vs Rust) must agree on every real materials.cfg.
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
OHPL_IMPL=cpp OHPL_H15_SURF="$list" OHPL_H15_OUT="$out/surface_cpp.txt" "$bin"
OHPL_IMPL=rust OHPL_H15_SURF="$list" OHPL_H15_OUT="$out/surface_rust.txt" "$bin"
OHPL_IMPL=cpp OHPL_H15_DUMP="$out/cpp.txt" OHPL_H15_VERBOSE=1 "$bin"
OHPL_IMPL=rust OHPL_H15_DUMP="$out/rust.txt" OHPL_H15_VERBOSE=1 "$bin"
cmp "$out/surface_cpp.txt" "$out/surface_rust.txt"
cmp "$out/surface_cpp.txt" "$here/rust/tests/golden/H15/surface_parse.txt"
if cmp "$out/cpp.txt" "$out/rust.txt" && cmp "$out/cpp.txt" "$here/rust/tests/golden/H15/h15_scenes.txt"; then
	echo "identical: $(grep -c '^end' "$out/cpp.txt") scenes, $(wc -l < "$out/surface_cpp.txt") surface files"
else
	diff "$out/cpp.txt" "$out/rust.txt" | head -20
	exit 1
fi
