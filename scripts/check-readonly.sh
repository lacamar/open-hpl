#!/usr/bin/env bash
# Usage: scripts/check-readonly.sh <binary> <game dir> ['<control cmd> k=v' ...]
# Runs a build on a real install with fresh XDG dirs; fails on EROFS, writes outside XDG/TMPDIR, or any change there.
# OHPL_RO_WRAP prefixes the command (e.g. "xvfb-run -a timeout 20" for the FLTK launcher).
set -euo pipefail
BIN=$(realpath "$1"); GAME=$(realpath "$2"); shift 2
T=$(mktemp -d "${TMPDIR:-/tmp}/ohpl-ro.XXXXXX")
SOCK=${XDG_RUNTIME_DIR:-/tmp}/ohpl-ro-$$.sock
C=(python3 "$(dirname "$(realpath "$0")")/hpl_control.py" --socket "$SOCK" --timeout 300)
manifest() { find "$GAME" -printf '%P\t%y\t%s\t%T@\t%C@\t%m\n' | sort; }

manifest > "$T/before"
pactl list short sinks | grep -q $'\tohpl-ours\t' ||
	pactl load-module module-null-sink sink_name=ohpl-ours sink_properties=device.description=ohpl-ours >/dev/null
env ALSOFT_DRIVERS=pulse ALSOFT_PULSE_DEFAULT=ohpl-ours OPENHPL_SOUND_DEVICE=ohpl-ours PULSE_SINK=ohpl-ours \
	OPENHPL_HEADLESS_SOCKET="$SOCK" XDG_CONFIG_HOME="$T/config" XDG_CACHE_HOME="$T/cache" \
	XDG_DATA_HOME="$T/data" XDG_STATE_HOME="$T/state" \
	${OHPL_RO_WRAP:-} strace -ff -qq -I1 --kill-on-exit -e trace=%file -o "$T/st" "$BIN" "$GAME" >"$T/out" 2>&1 </dev/null &
PID=$!
if [ $# -gt 0 ]; then
	until "${C[@]}" ping >/dev/null 2>&1; do kill -0 $PID 2>/dev/null || break; sleep 1; done
	for c in "$@"; do read -ra a <<<"$c"; "${C[@]}" "${a[@]}" | head -c 300; echo; done
	"${C[@]}" quit >/dev/null 2>&1 || true
fi
wait $PID || echo "exit status $?"
manifest > "$T/after"

fail=0
grep -ahE '^(openat\(.*O_(WRONLY|RDWR|CREAT|TRUNC)|(mkdirat|unlinkat|renameat2?|linkat|symlinkat|truncate|fchmodat2?|fchownat|utimensat|l?setxattr|l?removexattr|mknodat)\()' "$T"/st.* | grep -aE 'EROFS|\(AT_FDCWD, "/' |
	grep -avE "\(AT_FDCWD, \"($T|${TMPDIR:-/tmp}/|/proc/|/dev/|${XDG_RUNTIME_DIR:-/run/user/})|= -1 EEXIST" > "$T/writes" &&
	{ echo "writes outside the XDG dirs:"; head -20 "$T/writes"; fail=1; }
diff "$T/before" "$T/after" > "$T/changed" || { echo "changed in $GAME:"; head -20 "$T/changed"; fail=1; }
grep -a "open-hpl: could not\|refusing to open" "$T/out" && fail=1
echo "opens: $(cat "$T"/st.* | grep -c 'openat('), xdg: $(du -sh "$T" | cut -f1)"
[ $fail = 0 ] && { echo "OK: $GAME untouched"; rm -rf "${T:?}"; } || echo "FAIL: logs in $T"
exit $fail
