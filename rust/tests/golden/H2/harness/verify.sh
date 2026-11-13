#!/bin/bash
# verify.sh OUT_DIR [CORPUS_DIR]: runs the probes over the Steam game data (read-only) and compares with the
# committed goldens (xml_raw, cfg, lang, fs, res); prints "diffs=N". OUT_DIR holds the build.sh binaries.
# Rewrite the goldens: GOLDEN_WRITE=1 (then review the git diff).
set -u
OUT=$(cd "${1:?out dir}" && pwd)
C=${2:-$HOME/.local/share/Steam/steamapps/common}
G=$(cd "$(dirname "$0")/.." && pwd)
declare -A GAMES=([soma]=SOMA [amnesia]="Amnesia The Dark Descent" [rebirth]="Amnesia Rebirth" [bunker]="Amnesia The Bunker")
mkdir -p "$OUT/run" "$OUT/xdg" "$OUT/new"
for f in xml_raw cfg lang fs res; do : > "$OUT/new/$f.txt"; done

for g in soma amnesia rebirth bunker; do
	R="$C/${GAMES[$g]}"
	[ -d "$R" ] || continue
	L="$OUT/run/$g"
	( cd "$R" && find . -type f \( -name '*.cfg' -o -name '*.fnt' -o -name '*.gui' -o -name '*.xml' -o -name '*.snt' -o -name '*.ps' -o -name '*.mat' -o -name '*.ent' -o -name '*.lang' \) | sed 's|^\./||' | LC_ALL=C sort ) > "$L.xml.lst"
	( cd "$R" && find . -type f -name '*.dae' | sed 's|^\./||' | LC_ALL=C sort | awk 'NR%10==0' ) >> "$L.xml.lst"
	grep '\.cfg$' "$L.xml.lst" > "$L.cfg.lst"
	grep '\.lang$' "$L.xml.lst" > "$L.lang.lst"
	echo "$R" > "$L.fs.lst"

	"$OUT/xml_new" tiny "$R" "$L.xml.lst" > "$L.xml.tiny" 2>/dev/null
	"$OUT/xml_new" rust "$R" "$L.xml.lst" > "$L.xml.rs" 2>/dev/null
	cmp -s "$L.xml.tiny" "$L.xml.rs" || echo "xml tinyxml-vs-rust differs: $g"
	paste "$L.xml.lst" <(cut -f2 "$L.xml.rs") | awk -v g=$g -F'\t' '$1 ~ /\.dae$/ { next } $1 ~ /\.(mat|ent)$/ { if (++c % 8) next } { printf "%s:%s\t%s\n", g, $1, $2 }' >> "$OUT/new/xml_raw.txt"

	"$OUT/cfg_new" "$R" "$L.cfg.lst" "$L.scr" > "$L.cfg.out" 2>/dev/null
	printf '%s\t%s\t%s\n' "$g" "$(wc -l < "$L.cfg.lst")" "$(cat "$L.cfg.out" "$L.scr.plain" "$L.scr.crc" | sha256sum | cut -c1-16)" >> "$OUT/new/cfg.txt"

	"$OUT/lang_new" "$R" "$L.lang.lst" > "$L.lang.out" 2>/dev/null
	printf '%s\t%s\t%s\n' "$g" "$(wc -l < "$L.lang.lst")" "$(sha256sum < "$L.lang.out" | cut -c1-16)" >> "$OUT/new/lang.txt"

	"$OUT/fs_new" "$L.fs.lst" "$L.fs.full" > "$L.fs.sum" 2>/dev/null
	awk -v g=$g '{ f=$1; q=$2; d=$3; sub(/files=/,"",f); sub(/queries=/,"",q); sub(/digest=/,"",d); printf "%s\t%s\t%s\t%s\n", g, f, q, d }' "$L.fs.sum" >> "$OUT/new/fs.txt"

	XDG_CACHE_HOME="$OUT/xdg" "$OUT/res_probe" "$R" 8000 "$L.res" > "$L.res.sum" 2>/dev/null
	printf '%s\t%s\t%s\n' "$g" 8000 "$(sed 's/.*digest=//' "$L.res.sum")" >> "$OUT/new/res.txt"
done

bad=0
for f in xml_raw cfg lang fs res; do
	if [ -n "${GOLDEN_WRITE:-}" ]; then cp "$OUT/new/$f.txt" "$G/$f.txt"; continue; fi
	[ -s "$G/$f.txt" ] || { echo "missing golden $f.txt"; bad=$((bad+1)); continue; }
	cmp -s "$OUT/new/$f.txt" "$G/$f.txt" || { echo "DIFF $f"; bad=$((bad+1)); }
done
echo "diffs=$bad"
