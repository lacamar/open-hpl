#!/usr/bin/env bash
# S4b agent dumps: one scripted run per map listed in <scenario dir>, each writes <outdir>/s4b-<game>-<map>.txt.
# Usage: scripts/s4b-all.sh <game> <scenario-dir> <outdir> [map ...]   (OPENHPL_BUILD_DIR, OPENHPL_SCRATCH_ROOT from the environment)
set -uo pipefail
R=$(cd "$(dirname "$0")/.." && pwd)
cd "$R"
GAME=${1:?game}
SC=${2:?scenarios}
OUT=${3:?outdir}
shift 3
mkdir -p "$OUT"
export OHPL_GAME=$GAME
eval "$(scripts/soma-init.sh --env-only "$GAME" 2> /dev/null)"
for f in "$SC/$GAME-"*.script; do
	M=$(basename "$f" .script)
	M=${M#"$GAME-"}
	[ $# -gt 0 ] && [[ " $* " != *" $M "* ]] && continue
	rm -f "$OUT/s4b-$GAME-$M${S4B_SUFFIX:-}.txt"
	OHPL_S4B_DUMP=$OUT/s4b-$GAME-$M${S4B_SUFFIX:-}.txt OHPL_S4B_SCRIPT=$f OHPL_RUN_PREFIX="setarch -R" \
		scripts/soma-sweep.py --game "$GAME" --map "$M" --out "$OUT/s4b-$GAME-$M${S4B_SUFFIX:-}.json" > "$OUT/s4b-$GAME-$M${S4B_SUFFIX:-}.log" 2>&1
	grep -q '^END' "$OUT/s4b-$GAME-$M${S4B_SUFFIX:-}.txt" || echo "$GAME $M: no END" >&2
done
