#!/bin/sh
# Rust CharacterBody/PhysicsRope/PhysicsController scene dump must equal the committed golden.
# usage: scripts/h14-diff.sh <build-dir>   (build-dir holds PhysicsH14Tests.bin.aarch64)
# Record vectors for rust/tests/golden/H14: OHPL_H14_VEC=h14_vectors.txt OHPL_H14_DUMP=h14_scenes.txt PhysicsH14Tests.bin.aarch64
set -e
bin="${1:?build dir}/PhysicsH14Tests.bin.aarch64"
golden="$(dirname "$0")/../rust/tests/golden/H14/h14_scenes.txt"
out="${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/h14-diff"
mkdir -p "$out"
OHPL_H14_DUMP="$out/rust.txt" "$bin"
if cmp "$golden" "$out/rust.txt"; then
	echo "identical: $(grep -c '^end' "$golden") scenes"
else
	diff "$golden" "$out/rust.txt" | head -20
	exit 1
fi
