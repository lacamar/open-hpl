#!/usr/bin/env bash
# A10b real-engine dump (OHPL_A10B_DUMP): `boot` runs the pre-menu to the main menu; `play` changes map (load screen)
# and runs the credits. Compare dumps with rust/tests/golden/A10b/dump-diff.py.
# Usage: scripts/a10b-scenario.sh boot|play <dump-file>   (OHPL_GAME=amnesia|amfp, scratch from soma-init.sh)
set -uo pipefail
R=$(cd "$(dirname "$0")/.." && pwd); cd "$R"
MODE=${1:?usage: a10b-scenario.sh boot|play <dump-file>}
OUT=${2:?usage: a10b-scenario.sh boot|play <dump-file>}
GAME=${OHPL_GAME:-amnesia}
# the first start of a scratch dir shows extra pre-menu sections: settle the config with one throwaway boot
if [ "$MODE" = boot ] && [ -z "${A10B_WARM:-}" ]; then A10B_WARM=1 "$0" boot "$OUT.warm"; rm -f "$OUT.warm"; fi
eval "$(scripts/soma-init.sh --env-only "$GAME" 2> /dev/null)"
SOCK=$XDG_RUNTIME_DIR/${OHPL_PLAY_NAME:-ohpl-a10b}.sock
C() { scripts/hpl_control.py --socket "$SOCK" --timeout 240 "$@"; }
rm -f "$OUT"
export OHPL_A10B_DUMP=$OUT
read -r PID _ < <(scripts/soma-run.sh 02_library.map "$SOCK")
for _ in $(seq 60); do C state > /dev/null 2>&1 && break; sleep 2; done
if [ "$MODE" = boot ]; then
	C wait_frames n=3000 max_ms=240000 > /dev/null
else
	C start_map map=02_library.map > /dev/null
	C wait_frames n=90 max_ms=60000 > /dev/null
	C run_script line='SetupLoadScreen("LoadingText", "Ch01_Beginning01", 1, "game_loading_rose.jpg");' > /dev/null
	C run_script line='ChangeMap("ch01/00_rainy_hall.map", "PlayerStartArea_1", "", "");' > /dev/null
	C wait_frames n=1800 max_ms=120000 > /dev/null
	C run_script line='StartCredits("", false, "Ending", "MainCredits", 0);' > /dev/null
	C wait_frames n=400 max_ms=120000 > /dev/null
fi
C quit > /dev/null 2>&1
for _ in $(seq 30); do kill -0 "$PID" 2> /dev/null || exit 0; sleep 1; done
kill -9 "$PID"
