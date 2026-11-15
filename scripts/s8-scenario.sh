#!/usr/bin/env bash
# S8 real-engine dump (OHPL_S8_DUMP): boots $OHPL_GAME (soma|rebirth|bunker) through the splash and the gamma screen
# (Return forced after 30 updates) with a fixed step and clock. Usage: scripts/s8-scenario.sh <dump-file>
set -uo pipefail
R=$(cd "$(dirname "$0")/.." && pwd); cd "$R"
OUT=${1:?usage: s8-scenario.sh <dump-file>}
GAME=${OHPL_GAME:-soma}
eval "$(scripts/soma-init.sh --env-only "$GAME" 2> /dev/null)"
find "$XDG_STATE_HOME" -name gamma_screen_seen -delete 2> /dev/null
SOCK=$XDG_RUNTIME_DIR/${OHPL_PLAY_NAME:-ohpl-s8}.sock
rm -f "$OUT"
export OHPL_S8_DUMP=$OUT OHPL_S8_AUTOKEY=30
read -r PID _ < <(scripts/soma-run.sh boot "$SOCK")
for _ in $(seq 240); do grep -qs '^base_gamma_done' "$OUT" && break; kill -0 "$PID" 2> /dev/null || break; sleep 2; done
sleep 1
kill -9 "$PID" 2> /dev/null
find "$XDG_DATA_HOME" -name '*.sav' -delete 2> /dev/null
