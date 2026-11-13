#!/bin/sh
# Run particle systems (real .ps), billboard, beam, flare scenes with RecordingLowLevelGraphics trace hashes and the
# decision probes on the Rust backend; the dec lines must equal rust/tests/golden/H12/dec.txt.
# usage: scripts/h12b-diff.sh <build-dir> [record]   (build-dir holds SceneH12bTests.bin.aarch64; make SceneH12bTests)
set -e
bin="${1:?build dir}/SceneH12bTests.bin.aarch64"
out="${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/h12b-diff"
gold="$(dirname "$0")/../rust/tests/golden/H12/dec.txt"
s="$HOME/.local/share/Steam/steamapps/common"
mkdir -p "$out"
export OHPL_H12B_DIRS="$s/Amnesia The Dark Descent:$s/Amnesia Rebirth:$s/Amnesia The Bunker:$s/SOMA:$s/Machine for Pigs"
export OHPL_H12B_FRAMES="${OHPL_H12B_FRAMES:-40}" OHPL_H12B_EVERY="${OHPL_H12B_EVERY:-3}"
OHPL_H12B_DUMP="$out/rust.txt" "$bin"
echo "$(grep -c '^dec ' "$out/rust.txt") decisions, $(grep -c '^ ps ' "$out/rust.txt") particle systems, $(grep -c '^trace ' "$out/rust.txt") traces"
if [ "$2" = record ]; then
	grep '^dec ' "$out/rust.txt" >"$gold"
	echo "recorded $gold"
else
	grep '^dec ' "$out/rust.txt" | cmp - "$gold" && echo "golden equal"
fi
