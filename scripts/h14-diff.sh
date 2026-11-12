#!/bin/sh
# Differential run of CharacterBody/PhysicsRope/PhysicsController: same scenes under OHPL_IMPL=cpp and OHPL_IMPL=rust, dumps must be identical.
# usage: scripts/h14-diff.sh <build-dir>   (build-dir holds PhysicsH14Tests.bin.aarch64)
# Record vectors for rust/tests/golden/H14: OHPL_IMPL=rust OHPL_H14_VEC=h14_vectors.txt OHPL_H14_DUMP=h14_scenes.txt PhysicsH14Tests.bin.aarch64
set -e
bin="${1:?build dir}/PhysicsH14Tests.bin.aarch64"
out="${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/h14-diff"
mkdir -p "$out"
OHPL_IMPL=cpp OHPL_H14_DUMP="$out/cpp.txt" "$bin"
OHPL_IMPL=rust OHPL_H14_DUMP="$out/rust.txt" "$bin"
if cmp "$out/cpp.txt" "$out/rust.txt" && cmp "$out/cpp.txt" "$(dirname "$0")/../rust/tests/golden/H14/h14_scenes.txt"; then
	echo "identical: $(grep -c '^end' "$out/cpp.txt") scenes"
else
	diff "$out/cpp.txt" "$out/rust.txt" | head -20
	exit 1
fi
