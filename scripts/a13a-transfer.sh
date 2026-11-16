#!/usr/bin/env bash
# A13a map transfer: load <from>, change to <to> (fade, save of the old map, load of the new one), save, change back, save,
# load the first save, save again. Writes <prefix>-{1,2,3}<suffix>.sav, the A7 map dump <prefix>-a7<suffix>.txt and
# <prefix>-struct<suffix>.txt (saves with every val="" blanked).
# Usage: scripts/a13a-transfer.sh <game> <from.map> <to.map> <prefix>   (OPENHPL_BUILD_DIR, OPENHPL_SCRATCH_ROOT from the environment; A13A_SUFFIX)
set -uo pipefail
R=$(cd "$(dirname "$0")/.." && pwd); cd "$R"
GAME=${1:?game}; FROM=${2:?from}; TO=${3:?to}; P=${4:?prefix}
eval "$(scripts/soma-init.sh --env-only "$GAME" 2> /dev/null)"
export OPENHPL_HEADLESS_SLOTS=${OPENHPL_HEADLESS_SLOTS:-4}
SOCK=$XDG_RUNTIME_DIR/ohpl-a13a-$$.sock
C() { scripts/hpl_control.py --socket "$SOCK" --timeout 240 "$@"; }
SFX=${A13A_SUFFIX:-}
rm -f "$P"-[123]"$SFX".sav "$P-a7$SFX.txt"
export OHPL_A7_MAPDUMP=$P-a7$SFX.txt OHPL_RUN_PREFIX="setarch -R"
read -r PID _ < <(scripts/soma-run.sh "$FROM" "$SOCK")
for _ in $(seq 90); do C state > /dev/null 2>&1 && break; sleep 2; done
C start_map map="$FROM" > /dev/null
C wait_frames n=150 max_ms=120000 > /dev/null
C run_script "line=ChangeMap(\"$TO\", \"\", \"\", \"\");" > /dev/null
C wait_frames n=400 max_ms=240000 > /dev/null
C save_game file="$P-1$SFX.sav" > /dev/null; C wait_frames n=10 > /dev/null
C run_script "line=ChangeMap(\"$FROM\", \"\", \"\", \"\");" > /dev/null
C wait_frames n=400 max_ms=240000 > /dev/null
C save_game file="$P-2$SFX.sav" > /dev/null; C wait_frames n=10 > /dev/null
C load_game file="$P-1$SFX.sav" > /dev/null; C wait_frames n=150 max_ms=120000 > /dev/null
C save_game file="$P-3$SFX.sav" > /dev/null; C wait_frames n=10 > /dev/null
C quit > /dev/null 2>&1
for _ in $(seq 30); do kill -0 "$PID" 2> /dev/null && sleep 1 || break; done
kill -9 "$PID" 2> /dev/null
: > "$P-struct$SFX.txt"
for n in 1 2 3; do
	[ -s "$P-$n$SFX.sav" ] || { echo "missing $P-$n$SFX.sav" >&2; continue; }
	echo "== $n" >> "$P-struct$SFX.txt"
	sed -E 's/val="[^"]*"/val=""/g' "$P-$n$SFX.sav" >> "$P-struct$SFX.txt"
done
ls -la "$P"-[123]"$SFX".sav 2> /dev/null | awk '{print $5, $NF}'
