#!/usr/bin/env bash
# S7 scenario: run rust/tests/golden/S7/scenario-<game>.txt (script_exec snippets over the sound, music, voice and
# dialog natives) on one map with OHPL_S7_DUMP, headless. Prints the scenario output file.
# Usage: scripts/s7-scenario.sh <game> <out-prefix>   (OPENHPL_BUILD_DIR, OPENHPL_SCRATCH_ROOT from the environment)
set -uo pipefail
R=$(cd "$(dirname "$0")/.." && pwd); cd "$R"
G=${1:?usage: s7-scenario.sh <game> <out-prefix>}; P=${2:?}
case $G in soma) M=00_01_apartment ;; rebirth) M=02_02_courtyard ;; bunker) M=arena ;; esac
rm -f "$P-$G.out" "$P-$G.dump"
eval "$(scripts/soma-init.sh --env-only "$G")"
# another instance holds the headless lock for minutes at a time; the sweep calls a silent wait a hang
export OPENHPL_HEADLESS_SLOTS=${OPENHPL_HEADLESS_SLOTS:-4}
slot_free() { local L=${XDG_RUNTIME_DIR:-/run/user/$UID}/open-hpl-headless.lock f; for f in "$L" "$L.1" "$L.2" "$L.3"; do { [ ! -e "$f" ] || flock -n "$f" true; } && return 0; done; return 1; }
until slot_free; do sleep 5; done
OHPL_GAME=$G OHPL_S7_DUMP="$P-$G.dump" OHPL_S7_SCENARIO="$R/rust/tests/golden/S7/scenario-$G.txt" OHPL_S7_SCENARIO_OUT="$P-$G.out" \
	scripts/soma-sweep.py --boot-timeout 3000 --out "$P-$G.json" --map "$M" > "$P-$G.log" 2>&1
cat "$P-$G.out"
