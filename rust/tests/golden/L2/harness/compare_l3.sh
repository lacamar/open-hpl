#!/bin/bash
# usage: compare_l3.sh <out dir with l3_trace> <script dir> [set...]
# Runs every script on the Rust backend and diffs the GL call lines against the committed golden trace_<script>_<set>.txt.
OUT=$(cd "$1" && pwd) SC=$(cd "$2" && pwd)
G=$(cd "$(dirname "$0")/../../L3" && pwd)
shift 2
SETS=${@:-A B}
tot=0 bad=0 lines=0
for s in "$SC"/*.txt; do
  n=$(basename "$s" .txt)
  own=$(sed -n 's/^# sets: //p' "$s" | head -1)
  for set in ${own:-$SETS}; do
    FAKE_EXT_SET=$set L3_SCRIPT=$s L3_TRACE_OUT=$OUT/${n}_$set.rust.txt L3_FFI_OUT=$OUT/${n}_$set.rust.ffi timeout 300 "$OUT/l3_trace" > "$OUT/${n}_$set.rust.out" 2>&1 || echo "run failed: $n $set"
    l=$(wc -l < "$G/trace_${n}_$set.txt")
    d=$(diff "$G/trace_${n}_$set.txt" <(grep -E '^glu?[A-Z]\w*\(' "$OUT/${n}_$set.rust.txt") | grep -c '^[<>]')
    tot=$((tot+1)); lines=$((lines+l))
    [ "$d" != 0 ] && { bad=$((bad+1)); echo "DIFF $n $set: $d lines"; }
  done
done
echo "scripts*sets compared: $tot, trace lines: $lines, differing runs: $bad"
