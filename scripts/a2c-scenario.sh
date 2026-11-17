#!/usr/bin/env bash
# A2c player dump (OHPL_A2C_DUMP): loads <map>, scripted walk/look/run/crouch/jump/lean/lantern/item/damage/freecam inside cLuxPlayer::Update,
# one line per update, libc rand reseeded per update, run under setarch -R.
# Usage: scripts/a2c-scenario.sh <game> <map.map> <dump-file>
set -uo pipefail
R=$(cd "$(dirname "$0")/.." && pwd); cd "$R"
GAME=${1:?game}; MAP=${2:?map}; OUT=${3:?dump}
eval "$(scripts/soma-init.sh --env-only "$GAME" 2> /dev/null)"
export OPENHPL_HEADLESS_SLOTS=${OPENHPL_HEADLESS_SLOTS:-4}
# map caches (static body combining sorts by pointer, so a cache made by another run gives other contact order): one shared set
SHARED=${A2C_MAP_CACHE:-$HOME/.cache/open-hpl/gate/map-cache-shared}/$GAME
if [ -d "$SHARED" ]; then mkdir -p "$XDG_CACHE_HOME/open-hpl" && cp -a "$SHARED/." "$XDG_CACHE_HOME/open-hpl/"; fi
SOCK=$XDG_RUNTIME_DIR/ohpl-a2c-$$.sock
C() { scripts/hpl_control.py --socket "$SOCK" --timeout 240 "$@"; }
rm -f "$OUT"
export OHPL_A2C_DUMP=$OUT OHPL_RUN_PREFIX="setarch -R"
read -r PID _ < <(scripts/soma-run.sh "$MAP" "$SOCK")
for _ in $(seq 90); do C state > /dev/null 2>&1 && break; sleep 2; done
C start_map map="$MAP" > /dev/null
for _ in $(seq 600); do grep -q '^END' "$OUT" 2> /dev/null && break; sleep 1; done
C quit > /dev/null 2>&1
for _ in $(seq 30); do kill -0 "$PID" 2> /dev/null && sleep 1 || break; done
kill -9 "$PID" 2> /dev/null
grep -q "^END" "$OUT" || echo "scenario did not reach END" >&2
sed -i "/^END/q" "$OUT"
[ -d "$SHARED" ] || { mkdir -p "$SHARED" && cp -a "$XDG_CACHE_HOME/open-hpl/." "$SHARED/"; }
wc -l "$OUT"
