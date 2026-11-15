#!/usr/bin/env bash
# usage: census.sh <h7_gen binary> <out dir>
# Runs `h7_gen census` over every .mat of the SOMA, TDD, Rebirth, Bunker and AMFP
# installs (paths relative to steamapps/common, read-only), writes <out>/census.rust and compares it with ../census.txt.
set -u
bin=$1 out=$2
mkdir -p "$out"
cd "${OHPL_STEAM_COMMON:-$HOME/.local/share/Steam/steamapps/common}" || exit 1
find SOMA "Amnesia The Dark Descent" "Amnesia Rebirth" "Amnesia The Bunker" "Machine for Pigs" -iname '*.mat' | sort > "$out/mats.txt"
"$bin" census < "$out/mats.txt" > "$out/census.rust" 2> "$out/census.rust.err"
echo "census rc=$?"
echo "$(grep -c '^M ' "$out/census.rust") materials, $(grep -c '^U ' "$out/census.rust") unique setups"
if [ -f "$(dirname "$0")/../census.txt" ]; then cmp "$out/census.rust" "$(dirname "$0")/../census.txt" && echo equal to golden; fi
