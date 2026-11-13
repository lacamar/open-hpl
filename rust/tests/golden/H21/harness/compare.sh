#!/bin/bash
# usage: compare.sh <out dir> <seeds> [fp dir]   OHPL_IMPL=cpp vs rust, stdout per case
# Each seed runs once per popup kind (msgbox color save load kb) and once with random kinds, over the six
# installed game skins (read-only). The file picker browses <fp dir> (make-fpdir.sh, default ~/.cache/open-hpl/h21-tmp/fp).
OUT=$(cd "$1" && pwd) N=$2 FP=${3:-$HOME/.cache/open-hpl/h21-tmp/fp} bad=0 cases=0 cb=0 destroy=0
H=$(cd "$(dirname "$0")" && pwd)
"$H/make-fpdir.sh" "$FP"
cd "$FP/sub"  # relative paths resolve against the cwd, so it must be fixed
. "$H/skins.sh"
KINDS=(msgbox color save load kb mixed)
for k in "${!SKINS[@]}"; do
  [ -f "${SKINS[$k]}" ] || continue
  for s in $(seq 1 "$N"); do
    for kind in "${KINDS[@]}"; do
      K=$kind; [ "$kind" = mixed ] && K=
      H21_KIND=$K H21_SKIN="${SKINS[$k]}" H21_FPDIR="$FP" H21_SEED=$s OHPL_IMPL=cpp "$OUT/h21_popups" > "$OUT/c.out" 2> /dev/null
      H21_KIND=$K H21_SKIN="${SKINS[$k]}" H21_FPDIR="$FP" H21_SEED=$s OHPL_IMPL=rust "$OUT/h21_popups" > "$OUT/r.out" 2> /dev/null
      cases=$((cases+1))
      cb=$((cb+$(grep -c '^cb ' "$OUT/c.out")))
      destroy=$((destroy+$(grep -c '^destroycb ' "$OUT/c.out")))
      cmp -s "$OUT/c.out" "$OUT/r.out" || { echo "DIFF skin $k seed $s kind $kind"; cp "$OUT/c.out" "$OUT/diff.$k.$s.$kind.cpp"; cp "$OUT/r.out" "$OUT/diff.$k.$s.$kind.rust"; bad=$((bad+1)); }
    done
  done
done
echo "cases=$cases user_callbacks=$cb destroy_callbacks=$destroy"
echo "diffs=$bad"
