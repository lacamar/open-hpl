#!/bin/bash
# usage: compare_hud.sh <out dir> <seeds>   original C++ DevHud::Render vs ohpl_h22_hud_render, trace per seed
# With REC=<dir> the C++ traces of seeds 1..REC_N are copied there as hud_<seed>.txt.
OUT=$1 N=$2 bad=0
for s in $(seq 1 "$N"); do
  H22_SEED=$s H22_MODE=cpp "$OUT/h22_hud" > "$OUT/hud.$s.cpp" 2> /dev/null
  H22_SEED=$s H22_MODE=rust "$OUT/h22_hud" > "$OUT/hud.$s.rust" 2> /dev/null
  cmp -s "$OUT/hud.$s.cpp" "$OUT/hud.$s.rust" || { echo "DIFF seed $s"; bad=$((bad+1)); }
  if [ -n "$REC" ] && [ "$s" -le "${REC_N:-60}" ]; then cp "$OUT/hud.$s.cpp" "$REC/hud_$s.txt"; fi
done
echo "seeds=$N diffs=$bad lines=$(cat "$OUT"/hud.*.cpp | wc -l)"
