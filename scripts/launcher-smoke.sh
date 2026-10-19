#!/usr/bin/env bash
# Run an installed /usr/bin/open-hpl-<game> launcher headless and check it reaches the main menu.
# Usage: scripts/launcher-smoke.sh <soma|rebirth|bunker|amnesia|machine-for-pigs>
set -euo pipefail
G="${1:?usage: launcher-smoke.sh <game>}"
L=/usr/bin/open-hpl-$G
[ -x "$L" ] || { echo "no $L" >&2; exit 1; }
H="$(dirname "${BASH_SOURCE[0]}")"
S=$(mktemp -d)
# the TDD launcher is an FLTK settings dialog; run its wrapper with the game binary it would start
[ "$G" = amnesia ] && { sed 's|/Launcher\.bin|/Amnesia.bin|' "$L" >"$S/wrapper"; L="$S/wrapper"; chmod +x "$L"; }
SOCK="${XDG_RUNTIME_DIR:-/tmp}/ohpl-smoke-$G.sock"
rm -f "$SOCK"
"$H/headless-lock-holders.sh"
flock -n "${XDG_RUNTIME_DIR:-/tmp}/open-hpl-headless.lock" true || exit 1
pactl list short sinks | grep -q $'\tohpl-ours\t' ||
	pactl load-module module-null-sink sink_name=ohpl-ours sink_properties=device.description=ohpl-ours >/dev/null
ALSOFT_DRIVERS=pulse ALSOFT_PULSE_DEFAULT=ohpl-ours OPENHPL_SOUND_DEVICE=ohpl-ours PULSE_SINK=ohpl-ours PULSE_PROP=application.name=ohpl-ours \
OPENHPL_HEADLESS_SOCKET="$SOCK" \
XDG_CONFIG_HOME="$S/config" XDG_CACHE_HOME="$S/cache" XDG_DATA_HOME="$S/data" XDG_STATE_HOME="$S/state" \
	nohup "$L" >"$S/run.out" 2>&1 </dev/null &
PID=$!
trap 'kill -9 $PID 2>/dev/null; wait $PID 2>/dev/null; rm -f "$SOCK"' EXIT
C=(timeout 60 "$H/hpl_control.py" --socket "$SOCK")
case $G in
	amnesia|machine-for-pigs) Q=(frame_stats) WANT=lum_mean ;;
	*) Q=(script_exec 'code=__print(cLux_GetCurrentMap().GetFileName());') WANT=main_menu.hpm ;;
esac
for _ in $(seq 180); do
	kill -0 $PID 2>/dev/null || { echo "FAIL $G: exited" >&2; tail -20 "$S/run.out" >&2; exit 1; }
	OUT=$("${C[@]}" "${Q[@]}" 2>/dev/null) || OUT=
	grep -q "$WANT" <<<"$OUT" && { echo "OK $G"; rm -rf "$S"; exit 0; }
	sleep 1
done
echo "FAIL $G: no $WANT after 180 s (log: $S/run.out)" >&2
exit 1
