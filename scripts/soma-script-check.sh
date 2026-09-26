#!/usr/bin/env bash
# Compile every SOMA game script against the recovered API (no engine start).
# Usage: scripts/soma-script-check.sh [report.json]
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPORT="${1:-$ROOT/soma/conformance/script_check.json}"
GAME="${OPENHPL_SOMA_SCRATCH:-${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/soma-scratch}"
env OPENHPL_SOMA_GAME_DIR="$GAME" OPENHPL_SOMA_SCRIPT_CHECK="$REPORT" \
	OPENHPL_SOMA_SCRIPT_API="$ROOT/soma/data/script_api.txt" "$ROOT/amnesia/src/build/Soma.bin.aarch64"
