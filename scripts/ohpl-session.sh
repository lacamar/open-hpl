#!/usr/bin/env bash
# One start/use/stop script-player session (a game started by one shell call dies with it).
# Usage: scripts/ohpl-session.sh <game> <map> "cmd;cmd;..."
#   cmd    soma-play.py subcommand            goto 1 2 3
#   !cmd   hpl_control.py command             !world_stats
#   @file  soma-play.py exec of a script file @snippet.as  (OHPL_MODULE=<module>)
set -fuo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
export OHPL_GAME=$1 OHPL_PLAY_NAME=${OHPL_PLAY_NAME:-ohpl-s$1}
P="timeout 250 scripts/soma-play.py"
$P start --map "$2" 2>&1 | head -4 | cut -c1-300
IFS=';' read -ra CMDS <<< "${3:-}"
for c in "${CMDS[@]}"; do
	echo "== $c"
	case "$c" in
		'!'*) timeout 120 scripts/hpl_control.py --socket "$XDG_RUNTIME_DIR/$OHPL_PLAY_NAME.sock" ${c#!} 2>&1 | head -c "${OHPL_BYTES:-4000}"; echo;;
		'@'*) $P exec ${OHPL_MODULE:+--module "$OHPL_MODULE"} "$(tr -d '\n' < "${c#@}")" 2>&1 | head -"${OHPL_LINES:-12}" | cut -c1-600;;
		*) $P $c 2>&1 | head -"${OHPL_LINES:-12}" | cut -c1-600;;
	esac
done
$P stop 2>&1 | head -2
