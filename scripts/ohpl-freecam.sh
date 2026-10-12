#!/usr/bin/env bash
# One start/use/stop freecam session.
# Usage: scripts/ohpl-freecam.sh <game> <map.hpm> "cmd;cmd"
cd "$(dirname "${BASH_SOURCE[0]}")/.."
export OHPL_GAME=$1
S=$XDG_RUNTIME_DIR/ohpl-fc.sock
read pid sock < <(scripts/soma-run.sh "$2" "$S")
H="timeout 120 scripts/hpl_control.py --socket $S"
for i in $(seq 90); do $H camera_state >/dev/null 2>&1 && break; sleep 2; done
$H wait_frames n=30 >/dev/null 2>&1
IFS=';' read -ra CMDS <<< "$3"
for c in "${CMDS[@]}"; do echo "== $c"; $H $c 2>&1 | head -c "${OHPL_BYTES:-600}"; echo; done
$H quit >/dev/null 2>&1; sleep 2; kill -TERM $pid 2>/dev/null
