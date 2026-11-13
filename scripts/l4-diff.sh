#!/bin/sh
# Newton backend regression: PhysicsNewtonTests dump must equal the committed golden (recorded from the former C++ path).
# usage: scripts/l4-diff.sh <build-dir>   (build-dir holds PhysicsNewtonTests.bin.aarch64)
set -e
bin="${1:?build dir}/PhysicsNewtonTests.bin.aarch64"
golden="$(dirname "$0")/../rust/tests/golden/L4/newton_scenes.txt"
out="${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/l4-diff"
mkdir -p "$out"
OHPL_L4_DUMP="$out/rust.txt" OHPL_L4_VERBOSE=1 "$bin"
if cmp "$golden" "$out/rust.txt"; then
	echo "identical: $(grep -c '^end' "$golden") scenes"
else
	diff "$golden" "$out/rust.txt" | head -20
	exit 1
fi
