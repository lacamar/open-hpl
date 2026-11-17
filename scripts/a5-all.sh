#!/usr/bin/env bash
# A5 prop scenarios (scripts/a5/*.scn through a5-prop-scenario.sh): Amnesia 13_machine_room, 17_control_room, 19_morgue, 29_orb_chamber,
# 05_wine_cellar and AMFP 03_cellar, 05_church, 11_engine_room. Writes <outdir>/a5-<map><suffix>.txt and a5-amfp-<map><suffix>.txt.
# Usage: scripts/a5-all.sh <outdir> [suffix]   (A5_GAMES limits the games, default "amnesia amfp")
set -uo pipefail
R=$(cd "$(dirname "$0")/.." && pwd); cd "$R"
OUT=${1:?outdir}; X=${2:-}; mkdir -p "$OUT"
G=${A5_GAMES:-amnesia amfp}
has() { [[ " $G " == *" $1 "* ]]; }
S=scripts/a5-prop-scenario.sh
if has amnesia; then
	for m in 13_machine_room 17_control_room 05_wine_cellar 19_morgue 29_orb_chamber; do
		$S amnesia "$m.map" "$OUT/a5-$m$X.txt" "scripts/a5/$m.scn" 280 300
	done
fi
if has amfp; then
	for m in 03_cellar 05_church 11_engine_room; do
		$S amfp "$m.map" "$OUT/a5-amfp-$m$X.txt" "scripts/a5/amfp_$m.scn" 280 300
	done
fi
