#!/usr/bin/env bash
# Script-function call trace (OHPL_A1B_TRACE) of Amnesia and AMFP maps: a1b-trace.sh <outdir> [game...]
# A1B_ONLY="game/map.map ..." limits the run to those maps; A1B_SKIP_INIT=1 reuses the deployed scratch.
set -uo pipefail
cd "$(dirname "$0")/.."
OUT=$(realpath -m "$1"); shift
GAMES=${*:-amnesia amfp}
SCR=${OPENHPL_SCRATCH_ROOT:-${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/scratch-a1b-$(basename "$PWD")}
mkdir -p "$OUT"
maps() { python3 -c 'import json,sys;d=json.load(open(sys.argv[1]));print(*d.get("maps",d))' "$1/conformance/expected.json"; }
slot_free() { local f; for f in "$L" "$L.1" "$L.2" "$L.3"; do { [ ! -e "$f" ] || flock -n "$f" true; } && return 0; done; return 1; }
for g in $GAMES; do
	[ -n "${A1B_SKIP_INIT:-}" ] || OPENHPL_SCRATCH_ROOT=$SCR scripts/soma-init.sh "$g" >/dev/null 2>"$OUT/init-$g.log" || exit 1
	if [ -n "${A1B_ONLY:-}" ]; then
		list=$(for x in $A1B_ONLY; do [ "${x%%/*}" = "$g" ] && echo "${x#*/}"; done)
	else
		list=$(maps "$g")
	fi
	for m in $list; do
		L=${XDG_RUNTIME_DIR:-/run/user/$UID}/open-hpl-headless.lock
		t=0; until slot_free || [ $((t += 1)) -gt 720 ]; do sleep 5; done
		rm -f "$OUT/$g-$m.txt"
		env OHPL_A1B_TRACE="$OUT/$g-$m.txt" OPENHPL_SCRATCH_ROOT=$SCR OHPL_GAME="$g" \
			bash -c 'eval "$(scripts/soma-init.sh --env-only "$OHPL_GAME")" && exec scripts/soma-sweep.py --out "$0" --map "$1"' "$OUT/res-$g-$m.json" "$m" >>"$OUT/sweep-$g.log" 2>&1
	done
done
echo done > "$OUT/DONE"
