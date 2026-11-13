#!/bin/sh
# Run iEntity3D/cNode3D/cCamera scenes on the Rust backend; the dump must equal rust/tests/golden/H10/h10_scenes.txt.
# usage: scripts/h10-diff.sh <build-dir>   (build-dir holds SceneH10Tests.bin.aarch64; make SceneH10Tests)
# Record: OHPL_H10_SEEDS=30 OHPL_H10_DUMP=h10_scenes.txt SceneH10Tests.bin.aarch64
set -e
bin="${1:?build dir}/SceneH10Tests.bin.aarch64"
out="${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/h10-diff"
gold="$(dirname "$0")/../rust/tests/golden/H10/h10_scenes.txt"
mkdir -p "$out"
export OHPL_H10_SEEDS=30
OHPL_H10_DUMP="$out/rust.txt" "$bin"
if cmp "$out/rust.txt" "$gold"; then
	echo "equal to golden: $(grep -c '^o ' "$out/rust.txt") ops, $(grep -c '^c\?scene' "$out/rust.txt") scenes"
else
	diff "$out/rust.txt" "$gold" | head -20
	exit 1
fi
