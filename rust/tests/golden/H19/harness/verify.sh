#!/bin/bash
# usage: verify.sh <out dir>   h19_widgets against the committed widgets_*.txt (trace as line count + FNV-1a)
OUT=$1 G=$(cd "$(dirname "$0")/.." && pwd) bad=0
S=${STEAM_COMMON:-$HOME/.local/share/Steam/steamapps/common}
k=0
for e in "Amnesia The Dark Descent/gui/default/gui_default.skin:tdd_default" "Amnesia The Dark Descent/gui/main_menu/gui_main_menu.skin:tdd_main_menu" \
         "Machine for Pigs/gui/default/gui_default.skin:mfp_default" "SOMA/gui/gui_default.skin:soma_default" \
         "Amnesia The Bunker/gui/gui_default.skin:bunker_default" "Amnesia Rebirth/gui/gui_default.skin:rebirth_default"; do
  k=$((k+1))
  H19_DIGEST=1 H19_SKIN="$S/${e%%:*}" H19_SEED=$k "$OUT/h19_widgets" 2> /dev/null | cmp -s - "$G/widgets_${e##*:}.txt" || { echo "DIFF ${e##*:}"; bad=$((bad+1)); }
done
for f in "$G"/base_*.txt; do
  n=$(basename "$f" .txt); n=${n#base_}
  H19_SEED=$((10#$n)) "$OUT/h19_base" | cmp -s - "$f" || { echo "DIFF $f"; bad=$((bad+1)); }
done
echo "diffs=$bad"
