#!/bin/bash
# usage: verify.sh <out dir>   h18_set / h18_draw stdout against the committed set_*.txt / draw_*.txt
OUT=$1 G=$(cd "$(dirname "$0")/.." && pwd) bad=0
for T in set draw; do
  for f in "$G"/${T}_*.txt; do
    n=$(basename "$f" .txt); n=${n#${T}_}
    H18_SEED=$((10#$n)) "$OUT/h18_$T" 2> /dev/null | cmp -s - "$f" || { echo "DIFF $f"; bad=$((bad+1)); }
  done
done
echo "diffs=$bad"
