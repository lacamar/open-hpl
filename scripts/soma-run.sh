#!/usr/bin/env bash
# Launch one headless instance of $OHPL_GAME (default soma) on a map from the scratch dir.
# amnesia/amfp ignore the map: send start_map map=<file.map> once the socket answers.
# Usage: scripts/soma-run.sh <map.hpm|boot> [socket]   -> prints "<pid> <socket>"
set -euo pipefail
MAP="${1:?usage: soma-run.sh <map.hpm> [socket]}"
SOCK="${2:-${XDG_RUNTIME_DIR:-/tmp}/ohpl-run.sock}"
eval "$("$(dirname "${BASH_SOURCE[0]}")/ohpl_game.py")"
[ -x "$SCRATCH/$BIN" ] || { echo "no scratch dir - run: eval \"\$(scripts/soma-init.sh $GAME)\"" >&2; exit 1; }

pactl list short sinks | grep -q $'\tohpl-ours\t' ||
	pactl load-module module-null-sink sink_name=ohpl-ours sink_properties=device.description=ohpl-ours >/dev/null
python3 -c 'import socket,sys; socket.socket(socket.AF_UNIX).connect(sys.argv[1])' "$SOCK" 2>/dev/null &&
	{ echo "$SOCK is in use by a live instance" >&2; exit 1; }
rm -f "$SOCK"
"$(dirname "${BASH_SOURCE[0]}")/headless-lock-holders.sh"
cd "$SCRATCH"
BOOT=(OPENHPL_SOMA_MAP="$MAP" OPENHPL_SOMA_SKIP_BOOT=1)
[ "$MAP" = boot ] || [ -z "$HPL3" ] && BOOT=()
ALSOFT_DRIVERS=pulse ALSOFT_PULSE_DEFAULT=ohpl-ours OPENHPL_SOUND_DEVICE=ohpl-ours PULSE_SINK=ohpl-ours PULSE_PROP=application.name=ohpl-ours \
OPENHPL_HEADLESS_SOCKET="$SOCK" \
OPENHPL_SOMA_FREECAM="${OPENHPL_SOMA_FREECAM-1}" \
XDG_CONFIG_HOME="$SCRATCH/.xdg/config" XDG_CACHE_HOME="$SCRATCH/.xdg/cache" \
XDG_DATA_HOME="$SCRATCH/.xdg/data" XDG_STATE_HOME="$SCRATCH/.xdg/state" \
	env "${BOOT[@]}" setsid nohup "./$BIN" >run.out 2>&1 </dev/null &
echo "$! $SOCK"
