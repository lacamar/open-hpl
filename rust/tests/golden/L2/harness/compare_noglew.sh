#!/bin/bash
# usage: compare_noglew.sh <out dir after compare_l3.sh> <script dir>
# Re-runs the Rust path with OHPL_NO_GLEW=1 (glewInit skipped, every GLEW pointer null) and diffs
# against the C++ trace: a GLEW call left on the Rust path crashes the run.
OUT=$(cd "$1" && pwd) SC=$(cd "$2" && pwd)
tot=0 bad=0
for s in "$SC"/*.txt; do
  n=$(basename "$s" .txt)
  own=$(sed -n 's/^# sets: //p' "$s" | head -1)
  for set in ${own:-A B}; do
    FAKE_EXT_SET=$set OHPL_IMPL=rust OHPL_NO_GLEW=1 L3_SCRIPT=$s L3_TRACE_OUT=$OUT/${n}_$set.noglew.txt timeout 300 "$OUT/l3_trace" > "$OUT/${n}_$set.noglew.out" 2>&1 || echo "run failed: $n $set"
    d=$(diff "$OUT/${n}_$set.cpp.txt" "$OUT/${n}_$set.noglew.txt" 2>&1 | grep -c '^[<>]')
    tot=$((tot+1))
    [ "$d" != 0 ] && { bad=$((bad+1)); echo "DIFF $n $set: $d lines"; }
  done
done
echo "no-GLEW runs: $tot, differing: $bad"
