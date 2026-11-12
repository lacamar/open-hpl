#!/bin/bash
# usage: compare.sh <out dir with h5_trace> [seeds]; OHPL_IMPL=cpp vs rust, stdout (results) and stderr (GPU trace)
O=$1 N=${2:-30} bad=0 n=0
for k in atlas combo; do
  for ((s=0; s<N; s++)); do
    for impl in cpp rust; do
      H5_SCENARIO=$k H5_SEED=$s OHPL_IMPL=$impl "$O/h5_trace" >"$O/$k.$s.$impl.out" 2>"$O/$k.$s.$impl.err"
    done
    n=$((n+1))
    cmp -s "$O/$k.$s.cpp.out" "$O/$k.$s.rust.out" && cmp -s "$O/$k.$s.cpp.err" "$O/$k.$s.rust.err" || { bad=$((bad+1)); echo "DIFF $k $s"; }
  done
done
echo "compared $n scenarios, $bad diffs"
