#!/usr/bin/env bash
# ImGui draw-list dump (OHPL_S8D_DUMP) of $OHPL_GAME (soma|rebirth|bunker) from the deterministic boot of
# s8-scenario.sh, kept running S8D_MENU_SECONDS (default 12) after the gamma screen so the start menu is in it.
# Usage: scripts/s8d-scenario.sh <dump-file>
set -uo pipefail
R=$(cd "$(dirname "$0")/.." && pwd); cd "$R"
OUT=${1:?usage: s8d-scenario.sh <dump-file>}
GAME=${OHPL_GAME:-soma}
eval "$(scripts/soma-init.sh --env-only "$GAME" 2> /dev/null)"
find "$XDG_STATE_HOME" -name gamma_screen_seen -delete 2> /dev/null
SOCK=$XDG_RUNTIME_DIR/${OHPL_PLAY_NAME:-ohpl-s8d}.sock
rm -f "$OUT"
export OHPL_S8D_DUMP=$OUT OHPL_S8_DUMP=$OUT.s8 OHPL_S8_AUTOKEY=30
read -r PID _ < <(scripts/soma-run.sh boot "$SOCK")
for _ in $(seq "${S8_WAIT:-240}"); do grep -qs '^base_gamma_done' "$OUT.s8" && break; kill -0 "$PID" 2> /dev/null || break; sleep 2; done
sleep "${S8D_MENU_SECONDS:-12}"
kill -9 "$PID" 2> /dev/null
rm -f "$OUT.s8"
find "$XDG_DATA_HOME" -name '*.sav' -delete 2> /dev/null
