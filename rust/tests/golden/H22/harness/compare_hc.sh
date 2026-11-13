#!/bin/bash
# usage: compare_hc.sh <out dir> <script dir>   OHPL_IMPL=cpp vs rust transcript per script (hc_*.script)
# Writes <out>/<name>.{cpp,rust}.out; with REC=<dir> the cpp transcripts are copied there as <name>.txt.
OUT=$1 SD=$2 bad=0 n=0
mkdir -p "$OUT/xdg"
for s in "$SD"/hc_*.script; do
  b=$(basename "$s" .script)
  for i in cpp rust; do
    H22_SCRIPT=$s H22_XDG=$OUT/xdg OHPL_IMPL=$i timeout -s KILL 60 "$OUT/h22_hc" > "$OUT/$b.$i.out" 2> "$OUT/$b.$i.err"
  done
  n=$((n+1))
  cmp -s "$OUT/$b.cpp.out" "$OUT/$b.rust.out" || { echo "DIFF $b"; bad=$((bad+1)); }
  [ -n "$REC" ] && cp "$OUT/$b.cpp.out" "$REC/$b.txt"
done
echo "scripts=$n diffs=$bad"
