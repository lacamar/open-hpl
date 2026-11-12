#!/bin/bash
# compare.sh OUT_DIR [CORPUS_DIR]: runs every probe over the Steam game data (read-only) and diffs
#   xml : pre-port cXmlDocumentTiny vs current (cpp and rust), raw tinyXML vs Rust
#   cfg, lang, fs : pre-port C++ vs current OHPL_IMPL=cpp vs OHPL_IMPL=rust
# Writes OUT_DIR/results.txt (cases compared, diffs) and OUT_DIR/golden/{xml_raw,fs}.txt.
set -u
OUT=$(cd "${1:?out dir}" && pwd)
C=${2:-$HOME/.local/share/Steam/steamapps/common}
declare -A GAMES=([soma]=SOMA [amnesia]="Amnesia The Dark Descent" [rebirth]="Amnesia Rebirth" [bunker]="Amnesia The Bunker")
mkdir -p "$OUT/golden" "$OUT/run"
: > "$OUT/results.txt"
: > "$OUT/golden/xml_raw.txt"
: > "$OUT/golden/fs.txt"

diffcount() { diff <(cat "$1") <(cat "$2") | grep -c '^<'; }
report() { echo "$*" | tee -a "$OUT/results.txt"; }

for g in soma amnesia rebirth bunker; do
	R="$C/${GAMES[$g]}"
	[ -d "$R" ] || continue
	L="$OUT/run/$g"
	( cd "$R" && find . -type f \( -name '*.cfg' -o -name '*.fnt' -o -name '*.gui' -o -name '*.xml' -o -name '*.snt' -o -name '*.ps' -o -name '*.mat' -o -name '*.ent' -o -name '*.lang' \) | sed 's|^\./||' | LC_ALL=C sort ) > "$L.xml.lst"
	( cd "$R" && find . -type f -name '*.dae' | sed 's|^\./||' | LC_ALL=C sort | awk 'NR%10==0' ) >> "$L.xml.lst"
	grep '\.cfg$' "$L.xml.lst" > "$L.cfg.lst"
	grep '\.lang$' "$L.xml.lst" > "$L.lang.lst"
	echo "$R" > "$L.fs.lst"
	N=$(wc -l < "$L.xml.lst")

	"$OUT/xml_old" api "$R" "$L.xml.lst" > "$L.xml.old" 2>/dev/null
	OHPL_IMPL=cpp "$OUT/xml_new" api "$R" "$L.xml.lst" > "$L.xml.new_cpp" 2>/dev/null
	OHPL_IMPL=rust "$OUT/xml_new" api "$R" "$L.xml.lst" > "$L.xml.new_rust" 2>/dev/null
	"$OUT/xml_new" tiny "$R" "$L.xml.lst" > "$L.xml.tiny" 2>/dev/null
	"$OUT/xml_new" rust "$R" "$L.xml.lst" > "$L.xml.rs" 2>/dev/null
	report "$g xml api   files=$N old-vs-cpp diffs=$(diffcount $L.xml.old $L.xml.new_cpp) old-vs-rust diffs=$(diffcount $L.xml.old $L.xml.new_rust) lines=$(wc -l < $L.xml.old)"
	report "$g xml raw   files=$N tinyxml-vs-rust diffs=$(diffcount $L.xml.tiny $L.xml.rs) lines=$(wc -l < $L.xml.tiny)"
	paste "$L.xml.lst" <(cut -f2 "$L.xml.tiny") | awk -v g=$g -F'\t' '$1 ~ /\.dae$/ { next } $1 ~ /\.(mat|ent)$/ { if (++c % 8) next } { printf "%s:%s\t%s\n", g, $1, $2 }' >> "$OUT/golden/xml_raw.txt"

	"$OUT/cfg_old" "$R" "$L.cfg.lst" "$L.scr_old" > "$L.cfg.old" 2>/dev/null
	OHPL_IMPL=cpp "$OUT/cfg_new" "$R" "$L.cfg.lst" "$L.scr_cpp" > "$L.cfg.new_cpp" 2>/dev/null
	OHPL_IMPL=rust "$OUT/cfg_new" "$R" "$L.cfg.lst" "$L.scr_rust" > "$L.cfg.new_rust" 2>/dev/null
	report "$g cfg       files=$(wc -l < $L.cfg.lst) old-vs-cpp diffs=$(diffcount $L.cfg.old $L.cfg.new_cpp) old-vs-rust diffs=$(diffcount $L.cfg.old $L.cfg.new_rust)"

	"$OUT/lang_old" "$R" "$L.lang.lst" > "$L.lang.old" 2>/dev/null
	OHPL_IMPL=cpp "$OUT/lang_new" "$R" "$L.lang.lst" > "$L.lang.new_cpp" 2>/dev/null
	OHPL_IMPL=rust "$OUT/lang_new" "$R" "$L.lang.lst" > "$L.lang.new_rust" 2>/dev/null
	report "$g lang      files=$(wc -l < $L.lang.lst) old-vs-cpp diffs=$(diffcount $L.lang.old $L.lang.new_cpp) old-vs-rust diffs=$(diffcount $L.lang.old $L.lang.new_rust)"

	"$OUT/fs_old" "$L.fs.lst" "$L.fs.old.full" > "$L.fs.old" 2>/dev/null
	OHPL_IMPL=cpp "$OUT/fs_new" "$L.fs.lst" "$L.fs.cpp.full" > "$L.fs.new_cpp" 2>/dev/null
	OHPL_IMPL=rust "$OUT/fs_new" "$L.fs.lst" "$L.fs.rust.full" > "$L.fs.new_rust" 2>/dev/null
	report "$g fs        $(cat $L.fs.old) | old-vs-cpp diffs=$(diffcount $L.fs.old.full $L.fs.cpp.full) old-vs-rust diffs=$(diffcount $L.fs.old.full $L.fs.rust.full)"
	awk -v g=$g '{ split($1,a,/ /); f=$1; q=$2; d=$3; sub(/files=/,"",f); sub(/queries=/,"",q); sub(/digest=/,"",d); printf "%s\t%s\t%s\t%s\n", g, f, q, d }' "$L.fs.old" >> "$OUT/golden/fs.txt"
done
