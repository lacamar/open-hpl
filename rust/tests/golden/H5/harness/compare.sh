#!/bin/bash
# usage: compare.sh <out dir with h5_trace> [seeds]; runs the Rust paths and compares stdout (results) with the committed
# ../atlas_<seed>.txt / ../combo_<seed>.txt (seed numbers are zero-padded to 3 digits) and the CreateFromBitmap lines of
# stderr (GPU trace) with ../trace_atlas_<seed>.txt, for the seeds that have golden files.
O=$1 N=${2:-200} bad=0 n=0
g=$(cd "$(dirname "$0")/.." && pwd)
for k in atlas combo; do
  for ((s=0; s<N; s++)); do
    f="$g/${k}_$(printf %03d "$s").txt"
    [ -f "$f" ] || continue
    H5_SCENARIO=$k H5_SEED=$s "$O/h5_trace" >"$O/$k.$s.out" 2>"$O/$k.$s.err"
    n=$((n+1))
    cmp -s "$O/$k.$s.out" "$f" || { bad=$((bad+1)); echo "DIFF $k $s"; }
    t="$g/trace_${k}_$(printf %03d "$s").txt"
    [ -f "$t" ] && { grep CreateFromBitmap "$O/$k.$s.err" | cmp -s - "$t" || { bad=$((bad+1)); echo "DIFF trace $k $s"; }; }
  done
done
echo "compared $n scenarios, $bad diffs"
[ $bad = 0 ]
