#!/usr/bin/env bash
# A4b enemy scenarios: grunts (Amnesia 15_prison_north), wretches (AMFP 06_factory), water lurkers (AMFP
# 09_bilge), each as a scripted run (a4b-scenario.sh) and as a save round trip (a4b-roundtrip.sh).
# Writes <outdir>/a4b-<name><suffix>.txt, rt-<name><suffix>.txt and rt-<name>-{A,B}<suffix>.{txt,sav} (enemy blocks, saves).
# Usage: scripts/a4b-all.sh <outdir> [suffix]   (OPENHPL_BUILD_DIR, OPENHPL_SCRATCH_ROOT from the environment; A4B_GAMES
#        limits the games, default "amnesia amfp")
set -uo pipefail
R=$(cd "$(dirname "$0")/.." && pwd); cd "$R"
OUT=${1:?outdir}; X=${2:-}; mkdir -p "$OUT"
G=${A4B_GAMES:-amnesia amfp}
S=scripts/a4b-scenario.sh; RT=scripts/a4b-roundtrip.sh; export A4B_SUFFIX=$X
has() { [[ " $G " == *" $1 "* ]]; }
if has amnesia; then
	$S amnesia 15_prison_north.map "$OUT/a4b-prison$X.txt" "servant_grunt_1 servant_grunt_10" 400 \
		38.5,0.1,74 47,0.1,58 30,0.1,66 50,-0.4,70 40,0.1,62
	# save round trips: patrolling with a path, hunting with a path
	$RT amnesia 15_prison_north.map "$OUT/rt-prison-patrol" "servant_grunt_1 servant_grunt_10" 30 38.5,0.1,74
	$RT amnesia 15_prison_north.map "$OUT/rt-prison-hunt" "servant_grunt_1 servant_grunt_10" 60 38.5,0.1,74
fi
if has amfp; then
	$S amfp 06_factory.map "$OUT/a4b-factory$X.txt" "wretch_3 wretch_7" 400 \
		-5,-11.1,5 5,-11.1,20 -8,-11.1,-3 3,-11.1,14 -1,-11.1,0
	$S amfp 09_bilge.map "$OUT/a4b-bilge$X.txt" "waterlurker_1 WaterLurkerX_1 WaterLurkerX_2" 400 \
		41,-3.9,14 36,-3.9,20 46,-3.9,22 40,-3.9,10 38,-3.9,24
	$RT amfp 06_factory.map "$OUT/rt-factory" "wretch_3 wretch_7" 900 -5,-11.1,5 5,-11.1,20 -8,-11.1,-3
	$RT amfp 09_bilge.map "$OUT/rt-bilge" "waterlurker_1 WaterLurkerX_1 WaterLurkerX_2" 150 41,-3.9,14
fi
