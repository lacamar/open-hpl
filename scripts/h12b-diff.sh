#!/bin/sh
# H12 pass 2 differential: OHPL_IMPL=cpp vs rust dumps of particle systems (real .ps), billboard, beam, flare scenes
# with RecordingLowLevelGraphics trace hashes, plus the decision probes; the cpp dec lines must equal rust/tests/golden/H12/dec.txt.
# usage: scripts/h12b-diff.sh <build-dir> [record]   (build-dir holds SceneH12bTests.bin.aarch64; make SceneH12bTests)
set -e
bin="${1:?build dir}/SceneH12bTests.bin.aarch64"
out="${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/h12b-diff"
gold="$(dirname "$0")/../rust/tests/golden/H12/dec.txt"
s="$HOME/.local/share/Steam/steamapps/common"
mkdir -p "$out"
export OHPL_H12B_DIRS="$s/Amnesia The Dark Descent:$s/Amnesia Rebirth:$s/Amnesia The Bunker:$s/SOMA:$s/Machine for Pigs"
export OHPL_H12B_FRAMES="${OHPL_H12B_FRAMES:-40}" OHPL_H12B_EVERY="${OHPL_H12B_EVERY:-3}"
OHPL_IMPL=cpp OHPL_H12B_DUMP="$out/cpp.txt" "$bin"
OHPL_IMPL=rust OHPL_H12B_DUMP="$out/rust.txt" "$bin"
cmp "$out/cpp.txt" "$out/rust.txt" || { diff "$out/cpp.txt" "$out/rust.txt" | head -20; exit 1; }
echo "identical: $(grep -c '^dec ' "$out/cpp.txt") decisions, $(grep -c '^ ps ' "$out/cpp.txt") particle systems, $(grep -c '^trace ' "$out/cpp.txt") traces"
if [ "$2" = record ]; then
	grep '^dec ' "$out/cpp.txt" >"$gold"
	echo "recorded $gold"
else
	grep '^dec ' "$out/cpp.txt" | cmp - "$gold" && echo "golden equal"
fi
