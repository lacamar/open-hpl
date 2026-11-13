#!/bin/sh
# Run .ps emitter loading and emitter frames on the Rust backend; the sampled dump must equal rust/tests/golden/H12/ps_emitters.txt.
# usage: scripts/h12-diff.sh <build-dir> [record]   (build-dir holds SceneH12Tests.bin.aarch64; make SceneH12Tests)
set -e
bin="${1:?build dir}/SceneH12Tests.bin.aarch64"
out="${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/h12-diff"
gold="$(dirname "$0")/../rust/tests/golden/H12/ps_emitters.txt"
s="$HOME/.local/share/Steam/steamapps/common"
mkdir -p "$out"
export OHPL_H12_DIRS="$s/Amnesia The Dark Descent:$s/Amnesia Rebirth:$s/Amnesia The Bunker:$s/SOMA:$s/Machine for Pigs"
export OHPL_H12_FRAMES=24
export OHPL_H12_EVERY=22 OHPL_H12_SIM_EVERY=6 OHPL_H12_ATTRS=1
OHPL_H12_DUMP="$out/rust.txt" "$bin"
strip() { sed 's|^emitter \([0-9]*\) .*|emitter \1|' "$1"; }
if [ "$2" = record ]; then
	mkdir -p "$(dirname "$gold")"
	strip "$out/rust.txt" >"$gold"
	echo "recorded $gold"
else
	strip "$out/rust.txt" | cmp - "$gold" && echo "golden equal"
fi
