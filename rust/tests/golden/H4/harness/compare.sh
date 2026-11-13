#!/bin/bash
# compare.sh OUT_DIR [CORPUS_DIR]: runs h4_probe over every .hpm of the Steam data (read-only) and writes
# OUT_DIR/h4.txt (the golden file). Rust side: cargo test h4_golden.
set -u
OUT=$(cd "${1:?out dir}" && pwd)
C=${2:-$HOME/.local/share/Steam/steamapps/common}
declare -A GAMES=([soma]=SOMA [rebirth]="Amnesia Rebirth" [bunker]="Amnesia The Bunker")
"$OUT/h4_probe" synth > "$OUT/h4.txt"
for g in soma rebirth bunker; do
	R="$C/${GAMES[$g]}"
	[ -d "$R" ] || continue
	( cd "$R" && find . -type f -name '*.hpm' | sed 's|^\./||' | LC_ALL=C sort ) > "$OUT/$g.lst"
	"$OUT/h4_probe" maps "$R" "$OUT/$g.lst" | sed "s|^|$g:|" >> "$OUT/h4.txt"
done
wc -l "$OUT/h4.txt"
