#!/bin/bash
# usage: compare.sh <out dir> <seeds>   OHPL_IMPL=cpp vs rust, stdout per seed
# h19_base: probe widgets; h19_widgets: the nine simple widgets over each installed game skin (read-only).
OUT=$1 N=$2 bad=0
S=${STEAM_COMMON:-$HOME/.local/share/Steam/steamapps/common}
SKINS=(
  "$S/Amnesia The Dark Descent/gui/default/gui_default.skin"
  "$S/Amnesia The Dark Descent/gui/main_menu/gui_main_menu.skin"
  "$S/Machine for Pigs/gui/default/gui_default.skin"
  "$S/SOMA/gui/gui_default.skin"
  "$S/Amnesia The Bunker/gui/gui_default.skin"
  "$S/Amnesia Rebirth/gui/gui_default.skin"
)
for s in $(seq 1 "$N"); do
  H19_SEED=$s OHPL_IMPL=cpp "$OUT/h19_base" > "$OUT/h19_base.$s.cpp.out" 2> /dev/null
  H19_SEED=$s OHPL_IMPL=rust "$OUT/h19_base" > "$OUT/h19_base.$s.rust.out" 2> /dev/null
  cmp -s "$OUT/h19_base.$s.cpp.out" "$OUT/h19_base.$s.rust.out" || { echo "DIFF h19_base seed $s"; bad=$((bad+1)); }
done
echo "h19_base seeds=$N"
cases=0
for k in "${!SKINS[@]}"; do
  [ -f "${SKINS[$k]}" ] || continue
  for s in $(seq 1 "$N"); do
    H19_SKIN="${SKINS[$k]}" H19_SEED=$s OHPL_IMPL=cpp "$OUT/h19_widgets" > "$OUT/w.$k.$s.cpp.out" 2> /dev/null
    H19_SKIN="${SKINS[$k]}" H19_SEED=$s OHPL_IMPL=rust "$OUT/h19_widgets" > "$OUT/w.$k.$s.rust.out" 2> /dev/null
    cases=$((cases+1))
    cmp -s "$OUT/w.$k.$s.cpp.out" "$OUT/w.$k.$s.rust.out" || { echo "DIFF h19_widgets skin $k seed $s"; bad=$((bad+1)); }
  done
done
echo "h19_widgets cases=$cases"
echo "diffs=$bad"
