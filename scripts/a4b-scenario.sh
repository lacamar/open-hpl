#!/usr/bin/env bash
# A4b enemy dump (OHPL_A4B_DUMP): loads <map>, wakes the listed enemies, then for each waypoint puts the player there and
# alerts the enemies, <updates> map updates apart. The script runs inside the map update (OHPL_A4B_SCRIPT), so two runs
# of the same build agree line for line; one dump line per enemy update (state, position, velocity, mover, pathfinder,
# animation).
# Usage: scripts/a4b-scenario.sh <game> <map.map> <dump-file> "<enemy ...>" <updates-per-waypoint> <x,y,z>...
set -uo pipefail
R=$(cd "$(dirname "$0")/.." && pwd); cd "$R"
GAME=${1:?game}; MAP=${2:?map}; OUT=${3:?dump}; ENEMIES=${4:?enemies}; STEP=${5:?updates}; shift 5
eval "$(scripts/soma-init.sh --env-only "$GAME" 2> /dev/null)"
export OPENHPL_HEADLESS_SLOTS=${OPENHPL_HEADLESS_SLOTS:-4}
SOCK=$XDG_RUNTIME_DIR/ohpl-a4b-$$.sock
C() { scripts/hpl_control.py --socket "$SOCK" --timeout 240 "$@"; }
SCRIPT=$OUT.script
: > "$SCRIPT"
for e in $ENEMIES; do echo "20 SetEntityActive(\"$e\", true);" >> "$SCRIPT"; done
T=40
for w in "$@"; do
	IFS=, read -r X Y Z <<< "$w"
	echo "$T SetPlayerPos($X, $Y, $Z);" >> "$SCRIPT"
	for e in $ENEMIES; do echo "$T ShowEnemyPlayerPosition(\"$e\");" >> "$SCRIPT"; done
	T=$((T + STEP))
done
echo "$T END" >> "$SCRIPT"
rm -f "$OUT"
export OHPL_A4B_DUMP=$OUT OHPL_A4B_SCRIPT=$SCRIPT OHPL_RUN_PREFIX="setarch -R"  # fixed addresses: contact order follows allocation order
read -r PID _ < <(scripts/soma-run.sh "$MAP" "$SOCK")
for _ in $(seq 90); do C state > /dev/null 2>&1 && break; sleep 2; done
C start_map map="$MAP" > /dev/null
for _ in $(seq 600); do grep -q '^END' "$OUT" 2> /dev/null && break; sleep 1; done
C quit > /dev/null 2>&1
for _ in $(seq 30); do kill -0 "$PID" 2> /dev/null && sleep 1 || break; done
kill -9 "$PID" 2> /dev/null
grep -q "^END" "$OUT" || echo "scenario did not reach END" >&2
sed -i "/^END/q" "$OUT"  # updates after the marker depend on when the run shuts down
wc -l "$OUT"
