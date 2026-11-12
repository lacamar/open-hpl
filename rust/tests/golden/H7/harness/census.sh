#!/usr/bin/env bash
# usage: census.sh <h7_gen binary> <out dir>
# Runs `h7_gen census` with OHPL_IMPL=cpp and =rust over every .mat of the SOMA, TDD, Rebirth, Bunker and AMFP
# installs (paths relative to steamapps/common, read-only) and writes <out>/census.cpp, <out>/census.rust.
set -u
bin=$1 out=$2
mkdir -p "$out"
cd "${OHPL_STEAM_COMMON:-$HOME/.local/share/Steam/steamapps/common}" || exit 1
find SOMA "Amnesia The Dark Descent" "Amnesia Rebirth" "Amnesia The Bunker" "Machine for Pigs" -iname '*.mat' | sort > "$out/mats.txt"
for m in cpp rust; do
	OHPL_IMPL=$m "$bin" census < "$out/mats.txt" > "$out/census.$m" 2> "$out/census.$m.err"
	echo "census $m rc=$?"
done
cmp "$out/census.cpp" "$out/census.rust" && echo "$(grep -c '^M ' "$out/census.cpp") materials, $(grep -c '^U ' "$out/census.cpp") unique setups, cpp and rust match"
