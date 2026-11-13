#!/bin/bash
# usage: digests.sh <out dir>
# Checks the stdout (state, callbacks, RecordingLowLevelGraphics trace) of every (kind, skin, seed) against ../digests.txt
OUT=$(cd "$1" && pwd)
H=$(cd "$(dirname "$0")" && pwd)
G=$(cd "$H/.." && pwd)
FP=${FP:-$HOME/.cache/open-hpl/h21-tmp/fp}
"$H/make-fpdir.sh" "$FP"
cd "$FP/sub"
. "$H/skins.sh"
bad=0 n=0
for k in "${!SKINS[@]}"; do
  [ -f "${SKINS[$k]}" ] || continue
  for kind in msgbox color save load kb mixed; do
    for s in 1 2 3; do
      K=$kind; [ "$kind" = mixed ] && K=
      d=$(H21_KIND=$K H21_SKIN="${SKINS[$k]}" H21_FPDIR="$FP" H21_SEED=$s "$OUT/h21_popups" 2> /dev/null | md5sum | cut -d' ' -f1)
      line="$kind $k $s $d"
      n=$((n+1))
      grep -qxF "$line" "$G/digests.txt" || { echo "DIFF $line"; bad=$((bad+1)); }
    done
  done
done
echo "cases=$n diffs=$bad"
