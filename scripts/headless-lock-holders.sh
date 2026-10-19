#!/usr/bin/env bash
# Report who holds the headless instance lock; a new instance waits at 0% CPU until they exit.
LOCK="${XDG_RUNTIME_DIR:-/tmp}/open-hpl-headless.lock"
[ -e "$LOCK" ] && ! flock -n "$LOCK" true || exit 0
PIDS=$(lslocks -rn -o PID,PATH | awk -v l="$LOCK" '$2 == l {print $1}' | sort -u | paste -sd,)
echo "headless lock $LOCK is held; the new instance waits until these exit (kill -9 <pid> if orphaned):" >&2
[ -n "$PIDS" ] && ps -o pid,etime,args -p "$PIDS" >&2
