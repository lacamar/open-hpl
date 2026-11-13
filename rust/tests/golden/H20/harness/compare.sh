#!/bin/bash
# usage: compare.sh <out dir> <seeds>   OHPL_IMPL=cpp vs rust, stdout per seed
# h20_widgets: the H20 widgets over each installed game skin (read-only).
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
cases=0
for k in "${!SKINS[@]}"; do
  [ -f "${SKINS[$k]}" ] || continue
  for s in $(seq 1 "$N"); do
    H20_SKIN="${SKINS[$k]}" H20_SEED=$s OHPL_IMPL=cpp "$OUT/h20_widgets" > "$OUT/w.$k.$s.cpp.out" 2> /dev/null
    H20_SKIN="${SKINS[$k]}" H20_SEED=$s OHPL_IMPL=rust "$OUT/h20_widgets" > "$OUT/w.$k.$s.rust.out" 2> /dev/null
    cases=$((cases+1))
    cmp -s "$OUT/w.$k.$s.cpp.out" "$OUT/w.$k.$s.rust.out" || { echo "DIFF h20_widgets skin $k seed $s"; bad=$((bad+1)); }
  done
done
echo "h20_widgets cases=$cases"
echo "diffs=$bad"
