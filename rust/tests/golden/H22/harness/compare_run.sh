#!/bin/bash
# usage: compare_run.sh <out dir> <seeds>   original C++ loop vs ohpl_h22_engine_run, trace per seed
# With REC=<dir> the C++ traces of seeds 1..REC_N are copied there as run_<seed>.txt.
OUT=$1 N=$2 bad=0
for s in $(seq 1 "$N"); do
  H22_SEED=$s H22_MODE=cpp "$OUT/h22_run" > "$OUT/run.$s.cpp" 2> /dev/null
  H22_SEED=$s H22_MODE=rust "$OUT/h22_run" > "$OUT/run.$s.rust" 2> /dev/null
  cmp -s "$OUT/run.$s.cpp" "$OUT/run.$s.rust" || { echo "DIFF seed $s"; bad=$((bad+1)); }
  if [ -n "$REC" ] && [ "$s" -le "${REC_N:-24}" ]; then cp "$OUT/run.$s.cpp" "$REC/run_$s.txt"; fi
done
echo "seeds=$N diffs=$bad ops=$(cat "$OUT"/run.*.cpp | wc -l)"
