#!/bin/sh
# Differential run of .ps emitter loading and emitter frames: OHPL_IMPL=cpp vs rust dumps must be identical;
# the sampled cpp dump must equal rust/tests/golden/H12/ps_emitters.txt.
# usage: scripts/h12-diff.sh <build-dir> [record]   (build-dir holds SceneH12Tests.bin.aarch64; make SceneH12Tests)
set -e
bin="${1:?build dir}/SceneH12Tests.bin.aarch64"
out="${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/h12-diff"
gold="$(dirname "$0")/../rust/tests/golden/H12/ps_emitters.txt"
s="$HOME/.local/share/Steam/steamapps/common"
mkdir -p "$out"
export OHPL_H12_DIRS="$s/Amnesia The Dark Descent:$s/Amnesia Rebirth:$s/Amnesia The Bunker:$s/SOMA:$s/Machine for Pigs"
export OHPL_H12_FRAMES=24
OHPL_H12_SIM_EVERY=7 OHPL_IMPL=cpp OHPL_H12_DUMP="$out/cpp.txt" "$bin"
OHPL_H12_SIM_EVERY=7 OHPL_IMPL=rust OHPL_H12_DUMP="$out/rust.txt" "$bin"
cmp "$out/cpp.txt" "$out/rust.txt" || { diff "$out/cpp.txt" "$out/rust.txt" | head -20; exit 1; }
echo "identical: $(grep -c '^d ' "$out/cpp.txt") emitters, $(grep -c '^sim ' "$out/cpp.txt") simulated"
export OHPL_H12_EVERY=22 OHPL_H12_SIM_EVERY=6 OHPL_H12_ATTRS=1
OHPL_IMPL=cpp OHPL_H12_DUMP="$out/golden_cpp.txt" "$bin"
OHPL_IMPL=rust OHPL_H12_DUMP="$out/golden_rust.txt" "$bin"
cmp "$out/golden_cpp.txt" "$out/golden_rust.txt"
strip() { sed 's|^emitter \([0-9]*\) .*|emitter \1|' "$1"; }
if [ "$2" = record ]; then
	mkdir -p "$(dirname "$gold")"
	strip "$out/golden_cpp.txt" >"$gold"
	echo "recorded $gold"
else
	strip "$out/golden_cpp.txt" | cmp - "$gold" && echo "golden equal"
fi
