#!/bin/bash
# verify.sh OUT_DIR [CORPUS_DIR]: runs the probes over the Steam game data (read-only) and compares with the
# committed goldens mgr.txt, flow.txt, ent.txt; prints "diffs=N". OUT_DIR holds the build.sh binaries.
# Rewrite the goldens: GOLDEN_WRITE=1 (then review the git diff).
set -u
OUT=$(cd "${1:?out dir}" && pwd)
C=${2:-$HOME/.local/share/Steam/steamapps/common}
G=$(cd "$(dirname "$0")/.." && pwd)
declare -A GAMES=([soma]=SOMA [amnesia]="Amnesia The Dark Descent" [rebirth]="Amnesia Rebirth" [bunker]="Amnesia The Bunker")
mkdir -p "$OUT/run" "$OUT/new"
for f in mgr flow ent; do : > "$OUT/new/$f.txt"; done
export FLOW_EXTRA_DIR="$G/harness/matcases"

for g in soma amnesia rebirth bunker; do
	R="$C/${GAMES[$g]}"
	[ -d "$R" ] || continue
	L="$OUT/run/$g"
	MOPS=6000
	"$OUT/mgr_probe" "$R" "$MOPS" "$L.mgr" > "$L.mgr.sum" 2>/dev/null
	printf '%s\t%s\t%s\n' "$g" "$MOPS" "$(sed 's/.*digest=//' "$L.mgr.sum")" >> "$OUT/new/mgr.txt"

	FOPS=20000
	"$OUT/flow_probe" "$R" "$FOPS" "$L.flow" > "$L.flow.sum" 2>/dev/null
	printf '%s\t%s\t%s\n' "$g" "$FOPS" "$(sed 's/.*digest=//' "$L.flow.sum")" >> "$OUT/new/flow.txt"

	( cd "$R" && find . -type f -name '*.ent' | sed 's|^\./||' | LC_ALL=C sort ) > "$L.ent.lst"
	"$OUT/ent_new" "$R" "$L.ent.lst" "$L.ent.full" > "$L.ent" 2>/dev/null
	awk -F'\t' -v g=$g '{ if (NF < 7) printf "%s:%s\n", g, $0; else printf "%s:%s\t%s\t%s\t%s\t%s\n", g, $1, $2, $3, $5, $6 }' "$L.ent" >> "$OUT/new/ent.txt"
done

bad=0
for f in mgr flow ent; do
	if [ -n "${GOLDEN_WRITE:-}" ]; then cp "$OUT/new/$f.txt" "$G/$f.txt"; continue; fi
	cmp -s "$OUT/new/$f.txt" "$G/$f.txt" || { echo "DIFF $f"; bad=$((bad+1)); }
done
echo "diffs=$bad"
