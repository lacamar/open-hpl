#!/usr/bin/env bash
#
# Formalizes the ad-hoc headless boot-liveness check documented in
# PORTING_NOTES.md ("Headless testing pattern established this session"):
# launch a deployed binary backgrounded, then verify it actually booted
# (or diagnose why not) without depending on a screenshot or an unlocked
# desktop session.
#
# Usage:
#   scripts/headless-check.sh <path-to-deployed-binary> [wait-seconds] [socket-path]
#
# <path-to-deployed-binary> must already be deployed inside its real game
# directory (Amnesia/AMFP/SOMA all resolve config/maps/etc relative to the
# binary's own location via binreloc - see PORTING_NOTES.md).
#
# If [socket-path] is given, OPENHPL_HEADLESS_SOCKET is set to it before
# launching and a final "ping" is sent over it via hpl_control.py once the
# process-liveness checks below pass - the log/proc-stat checks are
# independent evidence the engine is doing real work even without the
# control socket at all.
#
# Exit status: 0 if the process is alive and CPU-active (or, when a socket
# is given, answered ping) after the wait; 1 otherwise. Diagnostics for a
# hang (gdb backtrace) or crash (coredumpctl) are printed either way.

set -uo pipefail

BIN_PATH="${1:?usage: headless-check.sh <path-to-deployed-binary> [wait-seconds] [socket-path]}"
WAIT_SECONDS="${2:-12}"
SOCKET_PATH="${3:-}"

if [ ! -x "$BIN_PATH" ]; then
	echo "error: '$BIN_PATH' is not an executable file" >&2
	exit 1
fi

BIN_DIR="$(cd "$(dirname "$BIN_PATH")" && pwd)"
BIN_NAME="$(basename "$BIN_PATH")"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

LOG_FILE="$BIN_DIR/hpl.log"
RUN_LOG="$(mktemp -t headless-check-XXXXXX.log)"

rm -f "$LOG_FILE"

echo "Launching '$BIN_NAME' in $BIN_DIR (wait ${WAIT_SECONDS}s)..."

PID_FILE="$(mktemp -t headless-check-pid-XXXXXX)"

(
	cd "$BIN_DIR"
	if [ -n "$SOCKET_PATH" ]; then
		export OPENHPL_HEADLESS_SOCKET="$SOCKET_PATH"
		rm -f "$SOCKET_PATH"
	fi
	setsid nohup "./$BIN_NAME" >"$RUN_LOG" 2>&1 </dev/null &
	echo $! > "$PID_FILE"
)

sleep 1
# Read the PID directly from the launcher subshell rather than pgrep -f'ing
# the binary name - that pattern can (and did, in testing) match this very
# script's own argv, which contains the same path as a plain string.
PID="$(cat "$PID_FILE" 2>/dev/null)"
rm -f "$PID_FILE"

if [ -z "$PID" ] || ! kill -0 "$PID" 2>/dev/null; then
	echo "FAIL: process did not start (or exited immediately)"
	echo "--- launch output ---"
	cat "$RUN_LOG" 2>/dev/null
	exit 1
fi

echo "PID $PID started, sampling CPU ticks..."

read_ticks() {
	awk '{print $14+$15}' "/proc/$1/stat" 2>/dev/null
}

TICKS_BEFORE="$(read_ticks "$PID")"
sleep "$WAIT_SECONDS"

if ! kill -0 "$PID" 2>/dev/null; then
	echo "FAIL: process $PID died during the ${WAIT_SECONDS}s wait"
	echo "--- launch output ---"
	cat "$RUN_LOG" 2>/dev/null
	if command -v coredumpctl >/dev/null 2>&1; then
		echo "--- recent coredumps ---"
		coredumpctl list --no-legend 2>/dev/null | tail -5
	fi
	exit 1
fi

TICKS_AFTER="$(read_ticks "$PID")"

echo "--- hpl.log ---"
if [ -f "$LOG_FILE" ]; then
	grep -E "ERROR|Game Running|Total:" "$LOG_FILE" || true
else
	echo "(no hpl.log written - only appears on warning/error, or this game doesn't write one at this path)"
fi

STATUS=0

if [ -n "$TICKS_BEFORE" ] && [ -n "$TICKS_AFTER" ] && [ "$TICKS_AFTER" -gt "$TICKS_BEFORE" ]; then
	echo "OK: PID $PID alive, CPU ticks climbing ($TICKS_BEFORE -> $TICKS_AFTER over ${WAIT_SECONDS}s) - not hung"
else
	echo "WARN: PID $PID alive but CPU ticks flat ($TICKS_BEFORE -> $TICKS_AFTER) - possibly hung"
	if command -v gdb >/dev/null 2>&1; then
		echo "--- gdb backtrace ---"
		gdb -p "$PID" -batch -ex bt -ex detach 2>&1 | tail -40
	fi
	STATUS=1
fi

if [ -n "$SOCKET_PATH" ]; then
	echo "--- control socket ping ---"
	if python3 "$SCRIPT_DIR/hpl_control.py" --socket "$SOCKET_PATH" ping; then
		echo "OK: control socket responded"
	else
		echo "FAIL: control socket did not respond"
		STATUS=1
	fi
fi

echo "(process left running as PID $PID - 'kill $PID' or send {\"cmd\":\"quit\"} to stop it)"
exit "$STATUS"
