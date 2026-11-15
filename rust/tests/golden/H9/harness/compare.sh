#!/bin/bash
# usage: compare.sh <h9_gen binary> [record]; runs the Rust combo setup and compares the dump with ../combos.txt (golden).
B=$1
g=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/h9-tmp"
t=$(mktemp -d "${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/h9-tmp/cmp.XXXXXX")
"$B" >"$t/out" 2>"$t/err"
n=$(grep -c '^light\|^fog' "$t/out")
if [ "${2:-}" = record ]; then cp "$t/out" "$g/combos.txt"; echo "recorded $n configurations"; rm -rf "$t"; exit 0; fi
bad=0
cmp -s "$t/out" "$g/combos.txt" || { bad=1; echo "DIFF: $(diff "$t/out" "$g/combos.txt" | head -5)"; }
rm -rf "$t"
echo "compared $n configurations, $bad diffs"
[ $bad = 0 ]
