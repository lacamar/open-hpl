#!/bin/sh
# Differential run of iEntity3D/cNode3D/cCamera scenes: OHPL_IMPL=cpp vs rust dumps must be identical and equal the golden file.
# usage: scripts/h10-diff.sh <build-dir>   (build-dir holds SceneH10Tests.bin.aarch64; make SceneH10Tests)
# Record rust/tests/golden/H10/h10_scenes.txt: OHPL_H10_SEEDS=30 OHPL_IMPL=cpp OHPL_H10_DUMP=h10_scenes.txt SceneH10Tests.bin.aarch64
set -e
bin="${1:?build dir}/SceneH10Tests.bin.aarch64"
out="${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/h10-diff"
mkdir -p "$out"
export OHPL_H10_SEEDS=30
OHPL_IMPL=cpp OHPL_H10_DUMP="$out/cpp.txt" "$bin"
OHPL_IMPL=rust OHPL_H10_DUMP="$out/rust.txt" "$bin"
if cmp "$out/cpp.txt" "$out/rust.txt" && cmp "$out/cpp.txt" "$(dirname "$0")/../rust/tests/golden/H10/h10_scenes.txt"; then
	echo "identical: $(grep -c '^o ' "$out/cpp.txt") ops, $(grep -c '^c\?scene' "$out/cpp.txt") scenes"
else
	diff "$out/cpp.txt" "$out/rust.txt" | head -20
	exit 1
fi
