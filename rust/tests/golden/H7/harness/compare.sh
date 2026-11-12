#!/bin/bash
# usage: compare.sh <h7_gen binary> [seeds] [out dir]; OHPL_IMPL=cpp vs rust per scenario and seed.
# With an out dir, also keeps the C++ outputs as <out>/<scenario>_<seed>.txt (golden candidates, seeds < 12).
B=$1 N=${2:-200} O=${3:-} bad=0 n=0
t=$(mktemp -d "${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/h7-tmp/cmp.XXXXXX")
run() { # scenario seed impl [mode]
	H7_SCENARIO=$1 H7_SEED=$2 OHPL_IMPL=$3 "$B" "${4:-script}" >"$t/$3" 2>"$t/$3.err"
	echo $? >"$t/$3.rc"
}
check() { # label seed
	n=$((n+1))
	if [ "$(cat "$t/cpp.rc")" != "$(cat "$t/rust.rc")" ] || ! cmp -s "$t/cpp" "$t/rust"; then bad=$((bad+1)); echo "DIFF $1 $2"; fi
}
keep() { # name seed
	if [ -n "$O" ] && [ "$2" -lt 12 ]; then cp "$t/cpp" "$O/$1_$(printf %03d "$2").txt"; fi
}
for sc in mat pe; do
	for ((s=0; s<N; s++)); do
		run $sc $s cpp; run $sc $s rust
		check $sc $s
		grep -q '^rs 1' "$t/rust.err" || { echo "rust path not active ($sc $s)"; bad=$((bad+1)); }
		keep $sc $s
	done
done
for ((s=0; s<N; s++)); do
	run x $s cpp pass; run x $s rust pass
	check pass $s
	keep pass $s
done
rm -rf "$t"
echo "compared $n scenarios, $bad diffs"
