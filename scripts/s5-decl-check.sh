#!/usr/bin/env bash
# S5 gate: script-engine declaration dump (sorted, property offsets masked: they follow the C++ layout) of the deployed binary, per game, recorded from the C++ baseline and compared after the flip.
# Usage: OPENHPL_SCRATCH_ROOT=<root> scripts/s5-decl-check.sh record|compare [game...]
set -uo pipefail
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODE=${1:?record|compare}; shift
BASE="${S5_DECL_BASE:-${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/gate/baseline-s5-decl}"
OUT="${S5_TMP:-${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/s5-tmp}"
mkdir -p "$BASE" "$OUT"
rc=0
for g in "${@:-soma rebirth bunker}"; do
	for g1 in $g; do
		eval "$("$REPO/scripts/ohpl_game.py" "$g1")"
		(cd "$SCRATCH" && OHPL_S2_DECL_DUMP="$OUT/decl-$g1.txt" OPENHPL_SOMA_GAME_DIR="$SCRATCH" \
			OPENHPL_SOMA_SCRIPT_CHECK="$OUT/chk-$g1.json" OPENHPL_SOMA_SCRIPT_API="$REPO/soma/data/script_api.txt" \
			OPENHPL_SOMA_SCRIPT_CHECK_FILE=/nonexistent ./"$BIN" >/dev/null 2>"$OUT/chk-$g1.err")
		sort "$OUT/decl-$g1.txt" > "$OUT/decl-$g1.sorted"
		if [ "$MODE" = record ]; then
			cp "$OUT/decl-$g1.sorted" "$BASE/$g1.txt"; echo "$g1: recorded $(wc -l < "$BASE/$g1.txt") lines"
		elif cmp -s <(sed -E 's/@[0-9]+$/@/' "$BASE/$g1.txt" | sort) <(sed -E 's/@[0-9]+$/@/' "$OUT/decl-$g1.sorted" | sort); then
			echo "$g1: identical ($(wc -l < "$BASE/$g1.txt") lines)"
		else
			echo "$g1: DIFF"; diff <(sed -E 's/@[0-9]+$/@/' "$BASE/$g1.txt" | sort) <(sed -E 's/@[0-9]+$/@/' "$OUT/decl-$g1.sorted" | sort) | head -10; rc=1
		fi
	done
done
exit $rc
