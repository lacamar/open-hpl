#!/bin/bash
# usage: verify_hc.sh <out dir> <script dir>   transcript of cHeadlessControlServer per script (hc_*.script, gen_hc.py) against ../hc_*.txt
OUT=$1 SD=$2 G=$(cd "$(dirname "$0")/.." && pwd) bad=0 n=0
mkdir -p "$OUT/xdg"
for s in "$SD"/hc_*.script; do
  b=$(basename "$s" .script)
  H22_SCRIPT=$s H22_XDG=$OUT/xdg timeout -s KILL 60 "$OUT/h22_hc" 2> /dev/null | cmp -s - "$G/$b.txt" || { echo "DIFF $b"; bad=$((bad+1)); }
  n=$((n+1))
done
echo "scripts=$n diffs=$bad"
