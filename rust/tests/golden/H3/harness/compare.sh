#!/bin/bash
# compare.sh OUT_DIR [OPS] [CORPUS_DIR]: runs the probes over the Steam game data (read-only) and diffs
#   mgr : iResourceManager sequences, OHPL_IMPL=cpp vs OHPL_IMPL=rust (full op logs)
#   ent : .ent parse call streams, pre-port C++ parser vs current (Rust) parser
# Writes OUT_DIR/results.txt (cases compared, diffs) and OUT_DIR/golden/{mgr,ent}.txt.
set -u
OUT=$(cd "${1:?out dir}" && pwd)
OPS=${2:-30000}
C=${3:-$HOME/.local/share/Steam/steamapps/common}
declare -A GAMES=([soma]=SOMA [amnesia]="Amnesia The Dark Descent" [rebirth]="Amnesia Rebirth" [bunker]="Amnesia The Bunker")
mkdir -p "$OUT/golden" "$OUT/run"
: > "$OUT/results.txt"
: > "$OUT/golden/mgr.txt"
: > "$OUT/golden/ent.txt"
: > "$OUT/golden/flow.txt"

report() { echo "$*" | tee -a "$OUT/results.txt"; }

for g in soma amnesia rebirth bunker; do
	R="$C/${GAMES[$g]}"
	[ -d "$R" ] || continue
	L="$OUT/run/$g"
	OHPL_IMPL=cpp "$OUT/mgr_probe" "$R" "$OPS" "$L.mgr.cpp" > "$L.mgr.cpp.sum" 2>/dev/null
	OHPL_IMPL=rust "$OUT/mgr_probe" "$R" "$OPS" "$L.mgr.rust" > "$L.mgr.rust.sum" 2>/dev/null
	report "$g mgr ops=$OPS lines=$(wc -l < $L.mgr.cpp) cpp-vs-rust diff lines=$(diff $L.mgr.cpp $L.mgr.rust | grep -c '^<') digest=$(sed 's/.*digest=//' $L.mgr.cpp.sum)"
	printf '%s\t%s\t%s\n' "$g" "$OPS" "$(sed 's/.*digest=//' $L.mgr.cpp.sum)" >> "$OUT/golden/mgr.txt"

	FOPS=${FLOW_OPS:-20000}
	export FLOW_EXTRA_DIR="$(cd "$(dirname "$0")" && pwd)/matcases"
	OHPL_IMPL=cpp "$OUT/flow_probe" "$R" "$FOPS" "$L.flow.cpp" > "$L.flow.cpp.sum" 2>/dev/null
	OHPL_IMPL=rust "$OUT/flow_probe" "$R" "$FOPS" "$L.flow.rust" > "$L.flow.rust.sum" 2>/dev/null
	report "$g flow ops=$FOPS lines=$(wc -l < $L.flow.cpp) cpp-vs-rust diff lines=$(diff $L.flow.cpp $L.flow.rust | grep -c '^<') digest=$(sed 's/.*digest=//' $L.flow.cpp.sum)"
	printf '%s\t%s\t%s\n' "$g" "$FOPS" "$(sed 's/.*digest=//' $L.flow.cpp.sum)" >> "$OUT/golden/flow.txt"

	( cd "$R" && find . -type f -name '*.ent' | sed 's|^\./||' | LC_ALL=C sort ) > "$L.ent.lst"
	"$OUT/ent_old" "$R" "$L.ent.lst" "$L.ent.old.full" > "$L.ent.old" 2>/dev/null
	"$OUT/ent_new" "$R" "$L.ent.lst" "$L.ent.new.full" > "$L.ent.new" 2>/dev/null
	report "$g ent files=$(wc -l < $L.ent.lst) calls=$(awk -F'\t' '{c+=$2+$5} END {print c}' $L.ent.old) old-vs-new file diffs=$(diff $L.ent.old $L.ent.new | grep -c '^<') full-log diff lines=$(diff $L.ent.old.full $L.ent.new.full | grep -c '^<')"
	# keep calls and the NaN-bits-free digest per configuration; the raw digests are compared above
	awk -F'\t' -v g=$g '{ if (NF < 7) printf "%s:%s\n", g, $0; else printf "%s:%s\t%s\t%s\t%s\t%s\n", g, $1, $2, $3, $5, $6 }' "$L.ent.old" >> "$OUT/golden/ent.txt"
done
