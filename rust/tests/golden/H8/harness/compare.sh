#!/bin/bash
# usage: compare.sh <h8_gen binary> [seeds <= 12] [record]; runs the Rust render list / clip scenarios and compares
# seeds 0-11 with the committed golden ../<scenario>_<seed>.txt.
B=$1 N=${2:-12} rec=${3:-} bad=0 n=0
g=$(cd "$(dirname "$0")/.." && pwd)
t=$(mktemp -d "${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/h8-tmp/cmp.XXXXXX")
for sc in list clip; do
	for ((s=0; s<N; s++)); do
		H8_SCENARIO=$sc H8_SEED=$s "$B" >"$t/out" 2>"$t/err"
		f="$g/${sc}_$(printf %03d "$s").txt"
		if [ "$rec" = record ]; then cp "$t/out" "$f"; continue; fi
		n=$((n+1))
		cmp -s "$t/out" "$f" || { bad=$((bad+1)); echo "DIFF $sc $s"; }
	done
done
rm -rf "$t"
echo "compared $n scenarios, $bad diffs"
[ $bad = 0 ]
