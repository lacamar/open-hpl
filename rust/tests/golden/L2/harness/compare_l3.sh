#!/bin/bash
# usage: compare_l3.sh <out dir with l3_trace> <script dir> [set...]
# Runs every script on both paths and diffs the full traces; <script>_<set>.{cpp,rust}.txt stay in <out dir>.
OUT=$(cd "$1" && pwd) SC=$(cd "$2" && pwd)
shift 2
SETS=${@:-A B}
tot=0 bad=0 lines=0
for s in "$SC"/*.txt; do
  n=$(basename "$s" .txt)
  own=$(sed -n 's/^# sets: //p' "$s" | head -1)
  for set in ${own:-$SETS}; do
    for m in cpp rust; do
      FAKE_EXT_SET=$set OHPL_IMPL=$m L3_SCRIPT=$s L3_TRACE_OUT=$OUT/${n}_$set.$m.txt L3_FFI_OUT=$OUT/${n}_$set.$m.ffi timeout 300 "$OUT/l3_trace" > "$OUT/${n}_$set.$m.out" 2>&1 || echo "run failed: $n $set $m"
    done
    l=$(wc -l < "$OUT/${n}_$set.cpp.txt")
    d=$(diff "$OUT/${n}_$set.cpp.txt" "$OUT/${n}_$set.rust.txt" | grep -c '^[<>]')
    tot=$((tot+1)); lines=$((lines+l))
    [ "$d" != 0 ] && { bad=$((bad+1)); echo "DIFF $n $set: $d lines"; }
  done
done
echo "scripts*sets compared: $tot, trace lines: $lines, differing runs: $bad"
