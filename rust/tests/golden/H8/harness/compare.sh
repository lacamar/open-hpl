#!/bin/bash
# usage: compare.sh <h8_gen binary> [seeds] [out dir]; OHPL_IMPL=cpp vs rust per scenario and seed.
# With an out dir, keeps the C++ outputs as <out>/<scenario>_<seed>.txt (golden, seeds < 12).
B=$1 N=${2:-200} O=${3:-} bad=0 n=0
t=$(mktemp -d "${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/h8-tmp/cmp.XXXXXX")
for sc in list clip; do
	for ((s=0; s<N; s++)); do
		for m in cpp rust; do H8_SCENARIO=$sc H8_SEED=$s OHPL_IMPL=$m "$B" >"$t/$m" 2>"$t/$m.err"; echo $? >"$t/$m.rc"; done
		n=$((n+1))
		if [ "$(cat "$t/cpp.rc")" != "$(cat "$t/rust.rc")" ] || ! cmp -s "$t/cpp" "$t/rust"; then bad=$((bad+1)); echo "DIFF $sc $s"; fi
		grep -q '^rs 1' "$t/rust.err" || { echo "rust path not active ($sc $s)"; bad=$((bad+1)); }
		if [ -n "$O" ] && [ "$s" -lt 12 ]; then cp "$t/cpp" "$O/${sc}_$(printf %03d "$s").txt"; fi
	done
done
rm -rf "$t"
echo "compared $n scenarios, $bad diffs"
