#!/bin/bash
# usage: compare.sh <out dir with l2_trace>
# Runs both paths for both extension sets and diffs the traces.
set -e
cd "$1"
for set in A B; do
  for m in cpp rust; do
    FAKE_EXT_SET=$set OHPL_IMPL=$m L2_TRACE_OUT=$PWD/$m$set.txt L2_FFI_OUT=$PWD/$m$set.ffi timeout 120 ./l2_trace > $m$set.out 2>&1
  done
  # markers are Rust-path only; the Rust path reads the extension string once more in ohpl_glctx_load
  grep -v -x -e '<' -e '>' rust$set.txt | grep -v -x 'glGetString(7939)' > rust$set.cmp
  n=$(wc -l < cpp$set.txt)
  d=$(diff cpp$set.txt rust$set.cmp | grep -c '^[<>]' || true)
  echo "set $set: compared $n lines, $d differing"
done
