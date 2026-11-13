#!/bin/bash
# usage: compare.sh <out dir> <seeds>   OHPL_IMPL=cpp vs rust, stdout per seed, both harnesses
OUT=$1 N=$2 bad=0
for T in h18_set h18_draw; do
  for s in $(seq 1 "$N"); do
    H18_SEED=$s OHPL_IMPL=cpp "$OUT/$T" > "$OUT/$T.$s.cpp.out" 2> "$OUT/$T.$s.cpp.err"
    H18_SEED=$s OHPL_IMPL=rust "$OUT/$T" > "$OUT/$T.$s.rust.out" 2> "$OUT/$T.$s.rust.err"
    cmp -s "$OUT/$T.$s.cpp.out" "$OUT/$T.$s.rust.out" || { echo "DIFF $T seed $s"; bad=$((bad+1)); }
  done
  echo "$T seeds=$N"
done
echo "diffs=$bad"
