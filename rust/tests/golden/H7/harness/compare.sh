#!/bin/bash
# usage: compare.sh <h7_gen binary> [seeds <= 12] [record]; runs the Rust material / post effect / pass scenarios and
# compares seeds 0-11 with the committed golden ../<scenario>_<seed>.txt.
B=$1 N=${2:-12} rec=${3:-} bad=0 n=0
g=$(cd "$(dirname "$0")/.." && pwd)
t=$(mktemp -d "${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/h7-tmp/cmp.XXXXXX")
check() { # name seed mode scenario
	H7_SCENARIO=$4 H7_SEED=$2 "$B" "$3" >"$t/out" 2>"$t/err"
	f="$g/$1_$(printf %03d "$2").txt"
	if [ "$rec" = record ]; then cp "$t/out" "$f"; return; fi
	n=$((n+1))
	cmp -s "$t/out" "$f" || { bad=$((bad+1)); echo "DIFF $1 $2"; }
}
for sc in mat pe; do
	for ((s=0; s<N; s++)); do check $sc $s script $sc; done
done
for ((s=0; s<N; s++)); do check pass $s pass x; done
rm -rf "$t"
echo "compared $n scenarios, $bad diffs"
[ $bad = 0 ]
