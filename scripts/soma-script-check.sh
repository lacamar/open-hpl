#!/usr/bin/env bash
# Compile every game script of $OHPL_GAME (soma, rebirth, bunker) against its recovered API (no engine start).
# Usage: scripts/soma-script-check.sh [report.json]
set -euo pipefail
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
eval "$("$REPO/scripts/ohpl_game.py")"
REPORT="${1:-$CONF/script_check.json}"
mkdir -p "$(dirname "$REPORT")"
env OPENHPL_SOMA_GAME_DIR="$SCRATCH" OPENHPL_SOMA_SCRIPT_CHECK="$REPORT" \
	OPENHPL_SOMA_SCRIPT_API="$REPO/$GAME/data/script_api.txt" "$REPO/amnesia/src/build/Soma.bin.aarch64"
