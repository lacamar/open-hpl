#!/usr/bin/env bash
# A4b enemy save round trip: wake and alert the enemies, save (<prefix>-A.sav) at update <save-update>, load that save through the
# headless command, save again 5 updates after the load (<prefix>-B.sav) and dump 120 updates after the load.
# <prefix>.txt holds the dump without the updates between the save and the load (their count depends on when the command lands).
# <prefix>-A.txt and -B.txt hold the enemy blocks of the two saves (a4b-sav.py); A4B_SUFFIX is appended to every output name.
# Usage: scripts/a4b-roundtrip.sh <game> <map.map> <prefix> "<enemy ...>" <save-update> <x,y,z>...   (waypoints 400 updates apart)
set -uo pipefail
R=$(cd "$(dirname "$0")/.." && pwd); cd "$R"
GAME=${1:?game}; MAP=${2:?map}; P=${3:?prefix}; ENEMIES=${4:?enemies}; SAVE=${5:?save-update}; shift 5
eval "$(scripts/soma-init.sh --env-only "$GAME" 2> /dev/null)"
export OPENHPL_HEADLESS_SLOTS=${OPENHPL_HEADLESS_SLOTS:-4}
SOCK=$XDG_RUNTIME_DIR/ohpl-a4b-$$.sock
C() { scripts/hpl_control.py --socket "$SOCK" --timeout 240 "$@"; }
SFX=${A4B_SUFFIX:-}; SCRIPT=$P$SFX.script; OUT=$P$SFX.raw
: > "$SCRIPT"
for e in $ENEMIES; do echo "20 SetEntityActive(\"$e\", true);" >> "$SCRIPT"; done
T=40
for w in "$@"; do
	IFS=, read -r X Y Z <<< "$w"
	echo "$T SetPlayerPos($X, $Y, $Z);" >> "$SCRIPT"
	for e in $ENEMIES; do echo "$T ShowEnemyPlayerPosition(\"$e\");" >> "$SCRIPT"; done
	T=$((T + 400))
done
echo "$SAVE @SAVE $P-A$SFX.sav" >> "$SCRIPT"
echo "L5 @SAVE $P-B$SFX.sav" >> "$SCRIPT"
echo "L120 END" >> "$SCRIPT"
rm -f "$OUT" "$P-A$SFX.sav" "$P-B$SFX.sav"
export OHPL_A4B_DUMP=$OUT OHPL_A4B_SCRIPT=$SCRIPT OHPL_RUN_PREFIX="setarch -R"  # fixed addresses: contact order follows allocation order
read -r PID _ < <(scripts/soma-run.sh "$MAP" "$SOCK")
for _ in $(seq 90); do C state > /dev/null 2>&1 && break; sleep 2; done
C start_map map="$MAP" > /dev/null
for _ in $(seq 600); do grep -q '^SAVED' "$OUT" 2> /dev/null && break; sleep 1; done
C load_game file="$P-A$SFX.sav" > /dev/null
for _ in $(seq 600); do grep -q '^END' "$OUT" 2> /dev/null && break; sleep 1; done
C quit > /dev/null 2>&1
for _ in $(seq 30); do kill -0 "$PID" 2> /dev/null && sleep 1 || break; done
kill -9 "$PID" 2> /dev/null
grep -q '^END' "$OUT" || echo "round trip did not reach END" >&2
sed -n '/^SAVED/q;p' "$OUT" > "$P$SFX.txt"
echo SAVED >> "$P$SFX.txt"
sed -n '/^LOADED/,/^END/p' "$OUT" >> "$P$SFX.txt"
for s in A B; do scripts/a4b-sav.py "$P-$s$SFX.sav" > "$P-$s$SFX.txt"; done
wc -l "$P$SFX.txt"
