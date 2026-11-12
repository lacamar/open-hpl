#!/bin/sh
# Differential run of the Newton backend: same scenes under OHPL_IMPL=cpp and OHPL_IMPL=rust, dumps must be identical.
# usage: scripts/l4-diff.sh <build-dir>   (build-dir holds PhysicsNewtonTests.bin.aarch64)
set -e
bin="${1:?build dir}/PhysicsNewtonTests.bin.aarch64"
out="${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/l4-diff"
mkdir -p "$out"
OHPL_IMPL=cpp OHPL_L4_DUMP="$out/cpp.txt" OHPL_L4_VERBOSE=1 "$bin"
OHPL_IMPL=rust OHPL_L4_DUMP="$out/rust.txt" OHPL_L4_VERBOSE=1 "$bin"
if cmp "$out/cpp.txt" "$out/rust.txt"; then
	echo "identical: $(grep -c '^end' "$out/cpp.txt") scenes"
else
	diff "$out/cpp.txt" "$out/rust.txt" | head -20
	exit 1
fi
