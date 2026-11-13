#!/bin/sh
# H13 differential: world registry sequences (std::list oracle vs Rust), static-combine plans and
# .map parse traces. The harness exits non-zero on any registry/plan diff; its dump must equal
# rust/tests/golden/H13.
# usage: scripts/h13-diff.sh <build-dir> [record]   (build-dir holds SceneH13Tests.bin.aarch64; make SceneH13Tests)
set -e
bin="${1:?build dir}/SceneH13Tests.bin.aarch64"
out="${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/h13-diff"
gold="$(dirname "$0")/../rust/tests/golden/H13"
s="$HOME/.local/share/Steam/steamapps/common"
mkdir -p "$out"
export OHPL_H13_DIRS="$s/Amnesia The Dark Descent:$s/Machine for Pigs"
OHPL_H13_DUMP="$out/dump.txt" "$bin"
command grep -a -v '^map ' "$out/dump.txt" > "$out/reg_plan.txt"
command grep -a '^map ' "$out/dump.txt" > "$out/maps.txt"
if [ "$2" = record ]; then
	cp "$out/reg_plan.txt" "$out/maps.txt" "$gold/"
	exit 0
fi
cmp "$out/reg_plan.txt" "$gold/reg_plan.txt"
cmp "$out/maps.txt" "$gold/maps.txt"
