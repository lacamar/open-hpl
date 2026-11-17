#!/usr/bin/env bash
# A5 prop dump (OHPL_A5_PROPDUMP): loads <map>, runs the scenario file inside the map update (OHPL_A5_SCRIPT: "<update>
# <script line>", "@INTERACT <prop>"), saves at <save-update>, loads that save, runs the "L<n>" lines and dumps again.
# Two runs of the same build agree byte for byte (libc rand reseeded per update, setarch -R, fixed 1/60 steps).
# Usage: scripts/a5-prop-scenario.sh <game> <map.map> <dump-file> <scenario-file> <save-update> [after-load-updates]
set -uo pipefail
R=$(cd "$(dirname "$0")/.." && pwd); cd "$R"
GAME=${1:?game}; MAP=${2:?map}; OUT=${3:?dump}; SCEN=${4:?scenario}; SAVE=${5:?save-update}; AFTER=${6:-300}
eval "$(scripts/soma-init.sh --env-only "$GAME" 2> /dev/null)"
export OPENHPL_HEADLESS_SLOTS=${OPENHPL_HEADLESS_SLOTS:-4}
SOCK=$XDG_RUNTIME_DIR/ohpl-a5-$$.sock
C() { scripts/hpl_control.py --socket "$SOCK" --timeout 240 "$@"; }
SCRIPT=$OUT.script
SAV=$OUT.sav
{ grep -v '^#' "$SCEN" | grep -v '^$'; echo "$SAVE @SAVE $SAV"; echo "L$AFTER END"; } > "$SCRIPT"
rm -f "$OUT" "$SAV"
export OHPL_A5_NOFLICKER=1 OHPL_A5_PROPDUMP=$OUT OHPL_A5_SCRIPT=$SCRIPT OHPL_RUN_PREFIX="setarch -R"
read -r PID _ < <(scripts/soma-run.sh "$MAP" "$SOCK")
for _ in $(seq 900); do C state > /dev/null 2>&1 && break; sleep 2; done
C start_map map="$MAP" > /dev/null
for _ in $(seq 900); do grep -q '^SAVED' "$OUT" 2> /dev/null && break; sleep 1; done
C load_game file="$SAV" > /dev/null
for _ in $(seq 900); do grep -q '^END' "$OUT" 2> /dev/null && break; sleep 1; done
C quit > /dev/null 2>&1
for _ in $(seq 30); do kill -0 "$PID" 2> /dev/null && sleep 1 || break; done
kill -9 "$PID" 2> /dev/null
grep -q '^END' "$OUT" || echo "a5 scenario did not reach END ($MAP)" >&2
sed -i '/^END/q' "$OUT"
# the updates between the save and the load depend on when the command lands
sed -i '/^SAVED/,/^LOADED/{/^SAVED/!{/^LOADED/!d}}' "$OUT"
rm -f "$SAV" "$SCRIPT"
wc -l "$OUT"
