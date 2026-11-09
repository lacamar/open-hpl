#!/usr/bin/env bash
# Per-merge check for the Rust port: build, tests, deploy, full sweeps.
set -uo pipefail
cd "$(dirname "$0")/.."
log=$(mktemp)
trap 'command rm -f "$log"' EXIT
make -C amnesia/src/build -j8 Soma Amnesia Amfp Launcher PhysicsNewtonTests CStringTests \
	PlatformXdgPathTests HpslTranspilerTests GlslToVulkanTests >"$log" 2>&1 || { grep -m5 'error' "$log"; exit 1; }
ctest --test-dir amnesia/src/build 2>&1 | grep -E 'tests passed|Failed'
for g in soma amnesia rebirth bunker; do eval "$(scripts/soma-init.sh "$g" 2>/dev/null)"; done
for g in soma rebirth bunker; do
	eval "$(scripts/soma-init.sh --env-only "$g")"
	OHPL_GAME=$g scripts/soma-sweep.py >"$log" 2>&1
	echo "$g: $(tail -1 "$log")"
	grep ' FAIL ' "$log"
done
eval "$(scripts/soma-init.sh --env-only amnesia)"
for m in 00_rainy_hall 01_cells 02_library 03_dungeon 04_final; do
	OHPL_GAME=amnesia scripts/soma-sweep.py --map "$m" 2>&1 | grep -E ' (PASS|FAIL) ' | sed 's/^/amnesia /'
done
git checkout -q -- soma/conformance/results.json
