#!/bin/bash
# usage: compare.sh <h6_script binary> [seeds] [out dir]; OHPL_IMPL=cpp vs rust per scenario and seed.
# With an out dir, also keeps the C++ outputs as <out>/<scenario>_<seed>.txt (golden candidates).
B=$1 N=${2:-200} O=${3:-} bad=0 n=0
t=$(mktemp -d "${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/h6-tmp/cmp.XXXXXX")
for sc in skel anim mesh; do
  for ((s=0; s<N; s++)); do
    H6_SCENARIO=$sc H6_SEED=$s OHPL_IMPL=cpp "$B" >"$t/cpp" 2>"$t/cpp.err"; c=$?
    H6_SCENARIO=$sc H6_SEED=$s OHPL_IMPL=rust "$B" >"$t/rust" 2>"$t/rust.err"; r=$?
    n=$((n+1))
    if [ $c != $r ] || ! cmp -s "$t/cpp" "$t/rust"; then bad=$((bad+1)); echo "DIFF $sc $s rc $c $r"; fi
    if [ -n "$O" ] && [ "$s" -lt 12 ]; then cp "$t/cpp" "$O/${sc}_$(printf %03d "$s").txt"; fi
  done
done
rm -rf "$t"
echo "compared $n scenarios, $bad diffs"
