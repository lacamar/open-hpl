#!/bin/bash
# usage: compare.sh <h9_gen binary> [out dir]; OHPL_IMPL=cpp vs rust dumps; with an out dir keeps the C++ output as <out>/combos.txt (golden).
B=$1 O=${2:-}
mkdir -p "${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/h9-tmp"
t=$(mktemp -d "${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/h9-tmp/cmp.XXXXXX")
for m in cpp rust; do OHPL_IMPL=$m "$B" >"$t/$m" 2>"$t/$m.err"; done
bad=0
cmp -s "$t/cpp" "$t/rust" || { bad=1; echo "DIFF: $(diff "$t/cpp" "$t/rust" | head -5)"; }
grep -q '^rs 1' "$t/rust.err" || { echo "rust path not active"; bad=1; }
n=$(grep -c '^light\|^fog' "$t/cpp")
[ -n "$O" ] && cp "$t/cpp" "$O/combos.txt"
rm -rf "$t"
echo "compared $n configurations, $bad diffs"
