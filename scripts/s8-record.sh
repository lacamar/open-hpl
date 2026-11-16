#!/usr/bin/env bash
# Run scripts/s8-scenario.sh for soma, rebirth and bunker into <dir>/<game>.txt.
# Usage: scripts/s8-record.sh <dir> [games...]   (OPENHPL_SCRATCH_ROOT, OPENHPL_BUILD_DIR as for soma-init.sh)
set -uo pipefail
R=$(cd "$(dirname "$0")/.." && pwd); cd "$R"
DIR=${1:?usage: s8-record.sh <dir> [games...]}; shift
mkdir -p "$DIR"
for g in ${*:-soma rebirth bunker}; do
	scripts/soma-init.sh "$g" > /dev/null 2>&1 || { echo "init $g failed"; exit 1; }
	OHPL_GAME=$g OHPL_PLAY_NAME=ohpl-s8-$g-$$ scripts/s8-scenario.sh "$DIR/$g.txt" > "$DIR/$g.log" 2>&1
done
echo done > "$DIR/done"
