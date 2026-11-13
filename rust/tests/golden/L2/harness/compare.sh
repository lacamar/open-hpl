#!/bin/bash
# usage: compare.sh <out dir with l2_trace>
# Runs the Rust backend for both extension sets and diffs the glctx call trace and the caps against the committed golden (recorded from the former C++ path).
set -e
G=$(cd "$(dirname "$0")/.." && pwd)
cd "$1"
for set in A B; do
  FAKE_EXT_SET=$set L2_TRACE_OUT=$PWD/rust$set.txt L2_FFI_OUT=$PWD/rust$set.ffi timeout 120 ./l2_trace > rust$set.out 2>&1
  # golden = the calls inside the ohpl_glctx_* markers ("<" ... ">"), minus the extra extension-string read of ohpl_glctx_load
  awk '$0=="<"{i=1;next} $0==">"{i=0;next} i&&$0!="glGetString(7939)"' rust$set.txt > rust$set.cmp
  grep -E '^(CAPS |cap[0-9]+=)' rust$set.txt > rust$set.caps
  n=$(wc -l < "$G/trace_$set.txt")
  d=$(( $(diff "$G/trace_$set.txt" rust$set.cmp | grep -c '^[<>]' || true) + $(diff "$G/caps_$set.txt" rust$set.caps | grep -c '^[<>]' || true) ))
  echo "set $set: compared $n lines, $d differing"
done
