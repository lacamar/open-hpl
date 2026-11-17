#!/usr/bin/env bash
# Port gate: build + ctest, map sweeps per backend (cpp/rust in parallel, separate scratch), scripted play,
# world dumps compared cpp vs rust and against a recorded C++ baseline, save round trips.
# Usage: scripts/gate.sh [--quick] [--impl cpp|rust|both] [--build-dir DIR] [--out DIR] [--no-build] [--no-saves]
#                        [--changed-since REV] [--jobs N] [--baseline DIR] [--record-baseline DIR]
#   --quick            map subset, no dumps, no play (agents' pre-report check); default: every SOMA/Rebirth/Bunker map
#   --impl             OHPL_IMPL values to run (default both)
#   --build-dir        default $OPENHPL_BUILD_DIR or amnesia/src/build of this checkout
#   --out              default $XDG_CACHE_HOME/open-hpl/gate/<checkout>-<time>; summary in <out>/summary.txt
#   --changed-since    only games whose code changed since REV (amnesia/amfp-only or soma-only changes); else all
#   --jobs N           games swept in parallel per backend (default 1; more is faster but adds load-induced hangs)
#   --baseline         recorded C++ dumps to compare the rust dumps with (default $XDG_CACHE_HOME/open-hpl/gate/baseline)
#   --record-baseline  record the C++ (OHPL_IMPL=cpp) dumps into DIR and stop; do it before flipping a family
#   --games "G ..."   only these games (default: all, or those --changed-since selects)
#   --scratch DIR      scratch root prefix (<DIR>-cpp, <DIR>-rust). Default: --quick keeps a per-checkout scratch under
#                      $XDG_CACHE_HOME/open-hpl/gate (warm shader/map caches, faster boots); full gates use a fresh one in --out
# Exit 0 only when every map passes (known failures excepted), every dump agrees and the round trips match.
set -uo pipefail
REPO=$(cd "$(dirname "$0")/.." && pwd); cd "$REPO"
CACHE=${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/gate
QUICK=; IMPLS="cpp rust"; BUILD=${OPENHPL_BUILD_DIR:-$REPO/amnesia/src/build}; OUT=; DO_BUILD=1; SAVES=1
SINCE=; ONLY=; JOBS=1; BASELINE=$CACHE/baseline; RECORD=; SCRATCH=
while [ $# -gt 0 ]; do
	case $1 in
	--quick) QUICK=1 ;;
	--impl) shift; [ "$1" = both ] && IMPLS="cpp rust" || IMPLS=$1 ;;
	--build-dir) shift; BUILD=$(realpath "$1") ;;
	--out) shift; OUT=$1 ;;
	--no-build) DO_BUILD= ;;
	--no-saves) SAVES= ;;
	--changed-since) shift; SINCE=$1 ;;
	--jobs) shift; JOBS=$1 ;;
	--baseline) shift; BASELINE=$1 ;;
	--record-baseline) shift; RECORD=$1 ;;
	--games) shift; ONLY=$1 ;;
	--scratch) shift; SCRATCH=$1 ;;
	*) echo "unknown option $1" >&2; exit 2 ;;
	esac; shift
done
[ -n "$RECORD" ] && { IMPLS=cpp; QUICK=; SAVES=; }
OUT=${OUT:-$CACHE/$(basename "$REPO")-$(date +%Y%m%d-%H%M%S)}
mkdir -p "$OUT"; OUT=$(realpath "$OUT")
if [ -z "$SCRATCH" ]; then
	[ -n "$QUICK" ] && SCRATCH=$CACHE/scratch-$(basename "$REPO") || SCRATCH=$OUT/scratch
fi
export OPENHPL_BUILD_DIR=$BUILD OPENHPL_HEADLESS_SLOTS=${OPENHPL_HEADLESS_SLOTS:-4}
export CCACHE_BASEDIR=${CCACHE_BASEDIR:-$REPO} CCACHE_NOHASHDIR=1
SUM=$OUT/summary.txt; : > "$SUM"
say() { echo "$*" | tee -a "$SUM"; }

if [ -n "$QUICK" ]; then
	SOMA_MAPS="00_01_apartment 01_02_upsilon_inside 02_05_theta_inside 03_02_omicron_inside 04_02_tau_inside"
	REBIRTH_MAPS="01_01_plane_wreckage 02_02_courtyard 03_05_vitae_station"
	BUNKER_MAPS="arena trenches"
else
	SOMA_MAPS=; REBIRTH_MAPS=; BUNKER_MAPS=   # empty = every map
fi
AMNESIA_MAPS="00_rainy_hall 01_cells 02_library 03_dungeon 04_final"
AMFP_MAPS="01_mansion_01 06_factory 10_reactor"
PLAY_MAPS="00_01_apartment 01_02_upsilon_inside 02_05_theta_inside"
DUMP_AMNESIA="00_rainy_hall 02_library 04_final"
A7B_AMNESIA="05_wine_cellar 11_study 19_morgue 20_sewer"
A7B_AMFP="03_cellar 06_factory"
KNOWN='rebirth:03_06_ghouls_nest.hpm:lum_mean'   # game:map:failure-prefix, space separated

GAMES="soma rebirth bunker amnesia amfp"
if [ -n "$SINCE" ]; then
	changed=$( { git diff --name-only "$SINCE"...HEAD; git diff --name-only HEAD; } | sort -u)
	if [ -z "$changed" ]; then
		GAMES=
	elif ! grep -qvE '^(amnesia/src/game/|amfp/|rust/src/lux|rust/tests/golden/A[0-9]|scripts/(a10b-scenario|a4b-[a-z]+|a5-[a-z-]+|gate).sh|scripts/a5/)' <<<"$changed"; then
		GAMES="amnesia amfp"
	elif ! grep -qvE '^(soma/|rust/src/soma|rust/tests/golden/S[0-9]|scripts/(s8-[a-z]+|gate).sh)' <<<"$changed"; then
		GAMES="soma rebirth bunker"
	fi
	say "changed since $SINCE: games ${GAMES:-none}"
fi
[ -n "$ONLY" ] && GAMES=$ONLY
has() { [[ " $GAMES " == *" $1 "* ]]; }

if [ -n "$DO_BUILD" ]; then
	make -C "$BUILD" -j"$(nproc)" Soma Amnesia Amfp Launcher PhysicsNewtonTests CStringTests PlatformXdgPathTests \
		HpslTranspilerTests GlslToVulkanTests TraceRecorderTests > "$OUT/build.log" 2>&1 \
		|| { say "BUILD FAILED"; grep -m10 -E 'error|Error' "$OUT/build.log" | tee -a "$SUM"; exit 1; }
	[ -n "$RECORD" ] || say "ctest: $(ctest --test-dir "$BUILD" 2>&1 | grep -E 'tests passed|tests failed' | tail -1)"
fi

for impl in $IMPLS; do
	for g in $GAMES; do
		OPENHPL_SCRATCH_ROOT=$SCRATCH-$impl scripts/soma-init.sh "$g" > /dev/null 2> "$OUT/init-$impl-$g.log" \
			|| { say "init $impl $g failed (see init-$impl-$g.log)"; exit 1; }
		# static body combining sorts by pointer: a map cache made by another run changes contact order, so every scratch starts from one cache set
		if [ -d "$CACHE/map-cache-shared/$g" ]; then
			mkdir -p "$SCRATCH-$impl/$g-scratch/.xdg/cache/open-hpl" && cp -a "$CACHE/map-cache-shared/$g/." "$SCRATCH-$impl/$g-scratch/.xdg/cache/open-hpl/"
		fi
	done
done

slot_free() { local L=${XDG_RUNTIME_DIR:-/run/user/$UID}/open-hpl-headless.lock f; for f in "$L" "$L.1" "$L.2" "$L.3"; do { [ ! -e "$f" ] || flock -n "$f" true; } && return 0; done; return 1; }
# sweep <impl> <game> <out.json> [soma-sweep args...]; extra env via ENVX
sweep() {
	local impl=$1 g=$2 json=$3; shift 3
	# a waiting instance looks hung to the sweep: queue here while another agent's instance holds the lock
	local t=0; until slot_free || [ $((t += 1)) -gt 360 ]; do sleep 5; done
	env ${ENVX:-} OPENHPL_SCRATCH_ROOT="${SCR:-$SCRATCH-$impl}" OHPL_IMPL="$impl" OHPL_GAME="$g" \
		bash -c 'eval "$(scripts/soma-init.sh --env-only "$OHPL_GAME")" && exec scripts/soma-sweep.py --out "$0" "$@"' "$json" "$@" \
		>> "$OUT/sweep-$impl-$g.log" 2>&1
}
mapargs() { local a=(); for m in "$@"; do a+=(--map "$m"); done; echo "${a[@]}"; }
failing() {  # maps in <json> that are not ok
	python3 -c 'import json,sys
d=json.load(open(sys.argv[1])); m=d.get("maps",d)
for k,v in m.items():
    if isinstance(v,dict) and (v.get("status")!="ok" or v.get("failures")): print(k, v.get("status"), ",".join(v.get("failures") or []))' "$1" 2>/dev/null
}

game() {  # <impl> <game>: sweep, then one retry of failing maps (recorded, reported)
	local impl=$1 g=$2 extra= maps=
	[ "$impl" = rust ] && extra=OHPL_NO_GLEW=1
	case $g in soma) maps=$SOMA_MAPS ;; rebirth) maps=$REBIRTH_MAPS ;; bunker) maps=$BUNKER_MAPS ;;
		amnesia) maps=$AMNESIA_MAPS ;; amfp) maps=$AMFP_MAPS ;; esac
	local json=$OUT/res-$impl-$g.json
	# shellcheck disable=SC2046
	ENVX=$extra sweep "$impl" "$g" "$json" $(mapargs $maps)
	failing "$json" > "$OUT/first-fail-$impl-$g.txt"
	if [ -s "$OUT/first-fail-$impl-$g.txt" ]; then
		# --only-failed counts maps missing from --out as failed, so keep the same map filter
		# shellcheck disable=SC2046
		ENVX=$extra sweep "$impl" "$g" "$json" --only-failed $(mapargs $maps)
	fi
}

half() {
	local impl=$1 extra=
	[ "$impl" = rust ] && extra=OHPL_NO_GLEW=1
	if [ -z "$RECORD" ]; then
		local running=0
		for g in $GAMES; do
			game "$impl" "$g" &
			running=$((running + 1))
			[ "$running" -ge "$JOBS" ] && { wait -n; running=$((running - 1)); }
		done
		wait
	fi
	[ -n "$QUICK" ] && return
	for gm in soma:00_01_apartment rebirth:02_02_courtyard bunker:arena; do
		local G=${gm%%:*} M=${gm#*:}
		has "$G" || continue
		rm -f "$OUT/h4seq-$G-$impl.txt" "$OUT/l9-$G-$impl.txt" "$OUT/s7sc-$G-$impl.txt" "$OUT/s7-$G-$impl.txt" "$OUT/s7p-$G-$impl.txt"
		ENVX="$extra OHPL_H4_SEQDUMP=$OUT/h4seq-$G-$impl.txt OHPL_L9_XMLDUMP=$OUT/l9-$G-$impl.txt" sweep "$impl" "$G" "$OUT/dump-$impl-$G.json" --map "$M"
		ENVX="$extra OHPL_S7_DUMP=$OUT/s7-$G-$impl.txt OHPL_S7_SCENARIO=$REPO/rust/tests/golden/S7/scenario-$G.txt OHPL_S7_SCENARIO_OUT=$OUT/s7sc-$G-$impl.txt" sweep "$impl" "$G" "$OUT/dump-s7-$impl-$G.json" --map "$M"
		ENVX="$extra OHPL_S7_DUMP=$OUT/s7p-$G-$impl.txt" sweep "$impl" "$G" "$OUT/dump-s7p-$impl-$G.json" --play 10 --map "$M"
	done
	for gm in soma:00_01_apartment rebirth:02_02_courtyard bunker:arena amnesia:00_rainy_hall amfp:06_factory; do
		has "${gm%%:*}" || continue
		rm -f "$OUT/esnd-${gm%%:*}-$impl.txt"
		ENVX="$extra OHPL_ESND_DUMP=$OUT/esnd-${gm%%:*}-$impl.txt" sweep "$impl" "${gm%%:*}" "$OUT/dump-esnd-$impl-${gm%%:*}.json" --map "${gm#*:}"
	done
	if has soma; then
		rm -f "$OUT/s7v-soma-$impl.txt" "$OUT/s5bv-soma-$impl.txt"
		ENVX="$extra OHPL_S7_DUMP=$OUT/s7v-soma-$impl.txt OHPL_S7_SCENARIO=$REPO/rust/tests/golden/S5b/voice-soma.txt OHPL_S7_SCENARIO_OUT=$OUT/s5bv-soma-$impl.txt" sweep "$impl" soma "$OUT/dump-s7v-$impl-soma.json" --map 00_01_apartment
	fi
	for g in soma rebirth bunker; do
		has "$g" || continue
		rm -f "$OUT/s8-$g-$impl.txt"
		env $extra OPENHPL_SCRATCH_ROOT="$SCRATCH-$impl" OHPL_IMPL="$impl" OHPL_PLAY_NAME="ohpl-s8-$impl-$g-$$" OHPL_GAME="$g" S8_WAIT=1500 \
			scripts/s8-scenario.sh "$OUT/s8-$g-$impl.txt" >> "$OUT/s8-$impl-$g.log" 2>&1
	done
	for g in soma rebirth bunker; do
		has "$g" || continue
		# the draw lists vary with timing: the C++ baseline is two runs, compared as a union
		for n in "" 2; do
			[ -n "$n" ] && [ -z "$RECORD" ] && continue
			rm -f "$OUT/s8d$n-$g-$impl.txt"
			env $extra OPENHPL_SCRATCH_ROOT="$SCRATCH-$impl" OHPL_IMPL="$impl" OHPL_PLAY_NAME="ohpl-s8d$n-$impl-$g-$$" OHPL_GAME="$g" S8_WAIT=1500 \
				scripts/s8d-scenario.sh "$OUT/s8d$n-$g-$impl.txt" >> "$OUT/s8d-$impl-$g.log" 2>&1
		done
	done
	for G in soma rebirth bunker; do
		has "$G" || continue
		case $G in
		soma) S5M="00_01_apartment 01_02_upsilon_inside 02_05_theta_inside 03_01_omicron_outside 04_03_tau_escape" ;;
		rebirth) S5M="02_02_courtyard" ;;
		*) S5M="arena" ;;
		esac
		rm -f "$OUT/s5-$G-$impl.txt"
		# shellcheck disable=SC2046
		ENVX="$extra OHPL_S5_DUMP=$OUT/s5-$G-$impl.txt" sweep "$impl" "$G" "$OUT/dump-s5-$impl-$G.json" --play 10 $(mapargs $S5M)
	done
	for G in soma rebirth bunker; do
		has "$G" || continue
		case $G in
		soma) S8BM="00_01_apartment 01_02_upsilon_inside 02_05_theta_inside" ;;
		rebirth) S8BM="02_02_courtyard 01_01_plane_wreckage" ;;
		*) S8BM="arena" ;;
		esac
		rm -f "$OUT/s8b-$G-$impl.txt" "$OUT/s8dm-$G-$impl.txt"
		# shellcheck disable=SC2046
		ENVX="$extra OHPL_S8D_DUMP=$OUT/s8dm-$G-$impl.txt OHPL_S8B_DUMP=$OUT/s8b-$G-$impl.txt" sweep "$impl" "$G" "$OUT/dump-s8b-$impl-$G.json" $(mapargs $S8BM)
	done
	for g in soma rebirth bunker; do
		has "$g" || continue
		rm -f "$OUT/s8c-$g-$impl.txt"
		env $extra OPENHPL_SCRATCH_ROOT="$SCRATCH-$impl" OHPL_IMPL="$impl" OHPL_PLAY_NAME="ohpl-s8c-$impl-$g-$$" OHPL_GAME="$g" \
			scripts/s8c-scenario.sh "$OUT/s8c-$g-$impl.txt" >> "$OUT/s8c-$impl-$g.log" 2>&1
	done
	if has amnesia; then
		for M in $DUMP_AMNESIA; do
			rm -f "$OUT/h13-$M-$impl.txt" "$OUT/a7-$M-$impl.txt" "$OUT/a10a-$M-$impl.txt" "$OUT/a11a-$M-$impl.txt" "$OUT/a11b-$M-$impl.txt" "$OUT/l9-$M-$impl.txt" "$OUT/a12a-$M-$impl.txt" "$OUT/a12b-$M-$impl.txt" "$OUT/a12c-$M-$impl.txt" "$OUT/a2b-$M-$impl.txt"
			ENVX="$extra OHPL_L9_XMLDUMP=$OUT/l9-$M-$impl.txt OHPL_H13_MAPDUMP=$OUT/h13-$M-$impl.txt OHPL_A7_MAPDUMP=$OUT/a7-$M-$impl.txt OHPL_A11A_INVDUMP=$OUT/a11a-$M-$impl.txt" \
				sweep "$impl" amnesia "$OUT/dump-$impl-amnesia.json" --map "$M"
			# the options menu shows cLuxPlayer's crosshair settings, uninitialised until the profile loads: pin the heap
			ENVX="$extra MALLOC_PERTURB_=85 OHPL_A10A_DUMP=$OUT/a10a-$M-$impl.txt" sweep "$impl" amnesia "$OUT/dump-a10a-$impl-amnesia.json" --map "$M"
			# the a11b driver gives items and hints: run it alone so it neither sees nor changes other dumps
			ENVX="$extra OHPL_A11B_DUMP=$OUT/a11b-$M-$impl.txt" sweep "$impl" amnesia "$OUT/dump-a11b-$impl-amnesia.json" --map "$M"
			ENVX="$extra OHPL_A12A_DUMP=$OUT/a12a-$M-$impl.txt OHPL_A12A_SCRIPT=$REPO/rust/tests/golden/A12a/script.txt" sweep "$impl" amnesia "$OUT/dump-a12a-$impl-amnesia.json" --map "$M"
			ENVX="$extra OHPL_A12B_DUMP=$OUT/a12b-$M-$impl.txt" sweep "$impl" amnesia "$OUT/dump-a12b-$impl-amnesia.json" --map "$M"
			ENVX="$extra OHPL_A12C_DUMP=$OUT/a12c-$M-$impl.txt" sweep "$impl" amnesia "$OUT/dump-a12c-$impl-amnesia.json" --map "$M"
			# the a2b driver replaces the player update with a scripted 1/60 step: own run, long enough for the death sequence
			ENVX="$extra OHPL_A2B_DUMP=$OUT/a2b-$M-$impl.txt" sweep "$impl" amnesia "$OUT/dump-a2b-$impl-amnesia.json" --map "$M" --frames 1100
			# every script-function call with arguments and results (A1b)
			rm -f "$OUT/a1b-$M-$impl.txt"
			ENVX="$extra OHPL_A1B_TRACE=$OUT/a1b-$M-$impl.txt" sweep "$impl" amnesia "$OUT/dump-a1b-$impl-amnesia.json" --map "$M"
		done
	fi
	if has amfp; then
		for M in $AMFP_MAPS; do
			rm -f "$OUT/a1b-amfp-$M-$impl.txt"
			ENVX="$extra OHPL_A1B_TRACE=$OUT/a1b-amfp-$M-$impl.txt" sweep "$impl" amfp "$OUT/dump-a1b-$impl-amfp.json" --map "$M"
		done
	fi
	if has amnesia; then
		for M in $DUMP_AMNESIA; do
			rm -f "$OUT/a13a-$M-$impl.txt"
			ENVX="$extra OHPL_A13A_DUMP=$OUT/a13a-$M-$impl.txt" sweep "$impl" amnesia "$OUT/dump-a13a-$impl-amnesia.json" --map "$M"
		done
	fi
	if has amfp; then
		for M in 01_mansion_01 06_factory; do
			rm -f "$OUT/a13a-$M-$impl.txt"
			ENVX="$extra OHPL_A13A_DUMP=$OUT/a13a-$M-$impl.txt" sweep "$impl" amfp "$OUT/dump-a13a-$impl-amfp.json" --map "$M"
		done
	fi
	if has amnesia; then
		for M in $A7B_AMNESIA; do
			rm -f "$OUT/a7b-$M-$impl.txt"
			ENVX="$extra OHPL_A7B_DUMP=$OUT/a7b-$M-$impl.txt" sweep "$impl" amnesia "$OUT/dump-a7b-$impl-amnesia.json" --map "$M"
		done
	fi
	if has amfp; then
		for M in $A7B_AMFP; do
			rm -f "$OUT/a7b-amfp-$M-$impl.txt"
			ENVX="$extra OHPL_A7B_DUMP=$OUT/a7b-amfp-$M-$impl.txt" sweep "$impl" amfp "$OUT/dump-a7b-$impl-amfp.json" --map "$M"
		done
	fi
	if has amnesia; then
		# the debug-window driver reloads maps and toggles engine state: own run
		for M in $DUMP_AMNESIA; do
			rm -f "$OUT/a12d-$M-$impl.txt" "$OUT/a12d-$M-$impl.txt.batch"
			ENVX="$extra OHPL_A12D_DUMP=$OUT/a12d-$M-$impl.txt" sweep "$impl" amnesia "$OUT/dump-a12d-$impl-amnesia.json" --map "$M"
		done
	fi
	if has amnesia; then
		# the journal driver opens the journal container, which stops the default container's modules: own run
		for M in $DUMP_AMNESIA; do
			rm -f "$OUT/a11c-$M-$impl.txt"
			ENVX="$extra OHPL_A11C_DUMP=$OUT/a11c-$M-$impl.txt" sweep "$impl" amnesia "$OUT/dump-a11c-$impl-amnesia.json" --map "$M"
		done
	fi
	if has amnesia; then
		# the menu dump lists the profiles on disk, so it gets a scratch no sweep has touched
		rm -f "$OUT/a9-$impl.txt"; rm -rf "$SCRATCH-a9-$impl"
		OPENHPL_SCRATCH_ROOT=$SCRATCH-a9-$impl scripts/soma-init.sh amnesia > /dev/null 2> "$OUT/init-a9-$impl.log"
		SCR=$SCRATCH-a9-$impl ENVX="$extra MALLOC_PERTURB_=85 OHPL_A9_MENUDUMP=$OUT/a9-$impl.txt" sweep "$impl" amnesia "$OUT/dump-a9-$impl.json" --map 00_rainy_hall
	fi
	for g in amnesia amfp; do
		has "$g" || continue
		for mode in boot play; do
			[ "$g$mode" = amfpplay ] && continue
			rm -f "$OUT/a10b-$mode-$g-$impl.txt"
			env $extra OPENHPL_SCRATCH_ROOT="$SCRATCH-$impl" OHPL_IMPL="$impl" OHPL_PLAY_NAME="ohpl-a10b-$impl-$g-$$" OHPL_GAME="$g" \
				scripts/a10b-scenario.sh "$mode" "$OUT/a10b-$mode-$g-$impl.txt" >> "$OUT/a10b-$impl-$g.log" 2>&1
		done
	done
	if has amnesia || has amfp; then
		# A4b: scripted enemy runs and save round trips, one dump line per enemy update
		rm -f "$OUT"/a4b-*-"$impl".txt "$OUT"/rt-*-"$impl".txt
		A4B_GAMES="$(has amnesia && echo amnesia) $(has amfp && echo amfp)" OPENHPL_SCRATCH_ROOT="$SCRATCH-$impl" OHPL_IMPL="$impl" \
			scripts/a4b-all.sh "$OUT" "-$impl" >> "$OUT/a4b-$impl.log" 2>&1
	fi
	if has amnesia || has amfp; then
		# A2c: scripted player inputs in the real update, one state line per update
		rm -f "$OUT"/a2c-*-"$impl".txt
		for gm in $(has amnesia && for M in $DUMP_AMNESIA; do echo "amnesia:$M"; done) $(has amfp && echo amfp:06_factory amfp:09_bilge); do
			OPENHPL_SCRATCH_ROOT="$SCRATCH-$impl" OHPL_IMPL="$impl" \
				scripts/a2c-scenario.sh "${gm%%:*}" "${gm##*:}.map" "$OUT/a2c-${gm##*:}-$impl.txt" >> "$OUT/a2c-$impl.log" 2>&1
		done
	fi
	if has amnesia; then
		# A13a: map transfer (change map, change back, load, save at every step)
		rm -f "$OUT"/a13atr-*-"$impl".*
		A13A_SUFFIX="-$impl" OPENHPL_SCRATCH_ROOT="$SCRATCH-$impl" OHPL_IMPL="$impl" \
			scripts/a13a-transfer.sh amnesia 02_library.map 03_dungeon.map "$OUT/a13atr" >> "$OUT/a13atr-$impl.log" 2>&1
	fi
	if [ -z "$RECORD" ] && has soma; then
		# shellcheck disable=SC2046
		ENVX=$extra sweep "$impl" soma "$OUT/play-$impl-soma.json" --play 10 $(mapargs $PLAY_MAPS)
	fi
}
for impl in $IMPLS; do half "$impl" & done; wait

if [ -z "$QUICK" ] && { has amnesia || has amfp; }; then
	# A5: scripted prop scenarios (grab/throw, doors, levers, wheels, lamps, items) with a save/load in the middle. Newton's contact
	# order follows the scratch path, so both paths and the recorded baseline run in one fixed scratch dir, one at a time
	A5SCR=$CACHE/a5-scratch; mkdir -p "$CACHE"
	for impl in $IMPLS; do
		rm -f "$OUT"/a5-*-"$impl".txt
		(
			flock 9
			for g in amnesia amfp; do has "$g" && OPENHPL_SCRATCH_ROOT=$A5SCR scripts/soma-init.sh "$g" > /dev/null 2>> "$OUT/a5-$impl.log"; done
			A5_GAMES="$(has amnesia && echo amnesia) $(has amfp && echo amfp)" OPENHPL_SCRATCH_ROOT=$A5SCR OHPL_IMPL="$impl" \
				scripts/a5-all.sh "$OUT" "-$impl" >> "$OUT/a5-$impl.log" 2>&1
		) 9> "$CACHE/a5.lock"
	done
fi


if [ -n "$RECORD" ]; then
	mkdir -p "$RECORD"
	for f in "$OUT"/h4seq-*-cpp.txt "$OUT"/h13-*-cpp.txt "$OUT"/a7-*-cpp.txt "$OUT"/a10a-*-cpp.txt "$OUT"/a10b-*-cpp.txt "$OUT"/a11a-*-cpp.txt "$OUT"/a11b-*-cpp.txt "$OUT"/a9-cpp.txt "$OUT"/l9-*-cpp.txt "$OUT"/a12a-*-cpp.txt "$OUT"/a12b-*-cpp.txt "$OUT"/a11c-*-cpp.txt "$OUT"/a12c-*-cpp.txt "$OUT"/a12d-*-cpp.txt "$OUT"/s7-*-cpp.txt "$OUT"/s7sc-*-cpp.txt "$OUT"/s7p-*-cpp.txt "$OUT"/s8-*-cpp.txt "$OUT"/s5-*-cpp.txt "$OUT"/a2b-*-cpp.txt "$OUT"/s8b-*-cpp.txt "$OUT"/s7v-*-cpp.txt "$OUT"/s5bv-*-cpp.txt "$OUT"/a4b-*-cpp.txt "$OUT"/rt-*-cpp.txt "$OUT"/s8c-*-cpp.txt "$OUT"/s8d-*-cpp.txt "$OUT"/s8d2-*-cpp.txt "$OUT"/s8dm-*-cpp.txt "$OUT"/a1b-*-cpp.txt "$OUT"/a13a-*-cpp.txt "$OUT"/a13atr-*-cpp.txt "$OUT"/a13b-*-cpp.txt "$OUT"/a7b-*-cpp.txt "$OUT"/a5-*-cpp.txt "$OUT"/a2c-*-cpp.txt "$OUT"/esnd-*-cpp.txt; do
		[ -s "$f" ] && cp "$f" "$RECORD/$(basename "$f" | sed 's/-cpp\.txt$/.txt/')"
	done
	{ echo "commit $(git rev-parse HEAD)"; echo "recorded $(date -Iseconds)"; ls "$RECORD"; } > "$RECORD/BASELINE"
	say "baseline recorded in $RECORD: $(ls "$RECORD" | grep -c '\.txt$') dumps"
	exit 0
fi

if [ -n "$SAVES" ]; then
	for impl in $IMPLS; do
		if has soma; then
			env OPENHPL_SCRATCH_ROOT="$SCRATCH-$impl" OHPL_IMPL="$impl" OHPL_PLAY_NAME="ohpl-gate-$$" \
				bash -c 'eval "$(scripts/soma-init.sh --env-only soma)" && timeout 900 scripts/soma-save-roundtrip.py 01_02_upsilon_inside' \
				> "$OUT/rt-soma-$impl.log" 2>&1
			sleep 15
		fi
		if has amnesia; then
			env OPENHPL_SCRATCH_ROOT="$SCRATCH-$impl" OHPL_IMPL="$impl" OHPL_PLAY_NAME="ohpl-gate-$$" \
				timeout 900 scripts/amnesia-save-roundtrip.sh > "$OUT/rt-amnesia-$impl.log" 2>&1
			sleep 5
		fi
	done
fi

if [ -n "$SAVES" ] && has amnesia; then
	INVIMPL=${IMPLS##* }
	env OPENHPL_SCRATCH_ROOT="$SCRATCH-$INVIMPL" OHPL_IMPL="$INVIMPL" OHPL_PLAY_NAME="ohpl-gate-inv-$$" \
		timeout 900 scripts/a11a-inv-roundtrip.sh "$BUILD/Amnesia.bin.aarch64" "$BUILD/Amnesia.bin.aarch64" "$OUT/inv-rt" > "$OUT/inv-rt.log" 2>&1
	JRNSAVE=$BUILD/Amnesia.bin.aarch64; [ -x "$BASELINE/Amnesia.cpp.bin.aarch64" ] && JRNSAVE=$BASELINE/Amnesia.cpp.bin.aarch64
	env OPENHPL_SCRATCH_ROOT="$SCRATCH-$INVIMPL" OHPL_IMPL="$INVIMPL" OHPL_PLAY_NAME="ohpl-gate-jrn-$$" \
		timeout 900 scripts/a11c-journal-roundtrip.sh "$JRNSAVE" "$BUILD/Amnesia.bin.aarch64" "$OUT/jrn-rt" > "$OUT/jrn-rt.log" 2>&1
	if [ -d "$BASELINE/gui" ]; then
		env OPENHPL_SCRATCH_ROOT="$SCRATCH-$INVIMPL" OHPL_IMPL="$INVIMPL" OHPL_PLAY_NAME="ohpl-gate-inv-$$" \
			timeout 900 scripts/a11a-inv-gui.sh "$BUILD/Amnesia.bin.aarch64" "$OUT/inv-gui" > "$OUT/inv-gui.log" 2>&1
		scripts/a11a-inv-compare.py "$BASELINE/gui" "$OUT/inv-gui" 0.02 > "$OUT/inv-gui-cmp.log" 2>&1
	fi
fi

python3 - "$OUT" "$KNOWN" "$IMPLS" "${QUICK:-0}" "${SAVES:-0}" "$BASELINE" "$CACHE/flaky.tsv" "$GAMES" "$REPO" <<'EOF' | tee -a "$SUM"
import datetime, filecmp, json, os, re, struct, subprocess, sys
out, known, impls, quick, saves, base, flaky, games, repo = sys.argv[1:]
known, impls, quick, saves, games = known.split(), impls.split(), quick == "1", saves == "1", games.split()
bad = 0

def rows(p):
    d = json.load(open(p)); m = d.get("maps", d)
    return {k: v for k, v in m.items() if isinstance(v, dict)}

def known_fail(g, m, fails):
    return bool(fails) and all(any(k == f"{g}:{m}:{f.split(':')[0]}" for k in known) for f in fails)

def judge(label, g, p):
    global bad
    if not os.path.exists(p):
        print(f"{label}: NO RESULTS"); bad += 1; return
    r = rows(p)
    fails = [(k, v.get("status"), v.get("failures") or []) for k, v in r.items() if v.get("status") != "ok" or v.get("failures")]
    unexpected = [f for f in fails if f[1] != "ok" or not known_fail(g, f[0], f[2])]
    bad += len(unexpected)
    line = f"{label}: {len(r) - len(fails)}/{len(r)} ok"
    line += "".join(f"\n  FAIL {k} {s} {','.join(fl)}" for k, s, fl in unexpected)
    if len(fails) > len(unexpected):
        line += f"  (known: {', '.join(f[0] for f in fails if f not in unexpected)})"
    print(line)

for impl in impls:
    for g in games:
        judge(f"{impl} {g}", g, os.path.join(out, f"res-{impl}-{g}.json"))
    if not quick and "soma" in games:
        judge(f"{impl} soma --play", "soma", os.path.join(out, f"play-{impl}-soma.json"))

# retries: a map that only passed on the second try is reported and logged; repeat offenders want a bead
history = []
if os.path.exists(flaky):
    history = [l.rstrip("\n").split("\t") for l in open(flaky) if l.strip()]
new = []
for impl in impls:
    for g in games:
        p, res = (os.path.join(out, f"{n}-{impl}-{g}{e}") for n, e in (("first-fail", ".txt"), ("res", ".json")))
        if not os.path.exists(p) or not os.path.exists(res):
            continue
        final = rows(res)
        for l in open(p):
            m, status, fl = (l.rstrip("\n").split(" ", 2) + ["", ""])[:3]
            first_fails = [f for f in fl.split(",") if f]
            if status == "ok" and known_fail(g, m, first_fails):
                continue
            v = final.get(m, {})
            if v.get("status") == "ok" and (not v.get("failures") or known_fail(g, m, v["failures"])):
                seen = 1 + sum(1 for h in history if h[1:4] == [impl, g, m])
                print(f"RETRIED {impl} {g} {m}: first {status} {fl} -> ok on retry (flaky {seen}x"
                      + (", file a bead)" if seen >= 2 else ")"))
                new.append([datetime.date.today().isoformat(), impl, g, m, f"{status} {fl}".strip()])
if new:
    os.makedirs(os.path.dirname(flaky), exist_ok=True)
    with open(flaky, "a") as f:
        for n in new:
            f.write("\t".join(n) + "\n")

def multiset(p):
    return sorted(re.sub(r"CombinedMesh\d+", "", l) for l in open(p, errors="replace"))

def same(name, a, b):
    if name.startswith("a10b-"):
        tool = os.path.join(repo, "rust/tests/golden/A10b/dump-diff.py")
        r = subprocess.run([sys.executable, tool, name.split("-")[1], a, b], capture_output=True, text=True)
        last = (r.stdout.strip().splitlines() or [""])[0]
        return (r.returncode == 0, last if r.returncode == 0 else f"DIFFER {last}")
    if name.startswith("a7b-"):
        tool = os.path.join(repo, "rust/tests/golden/A7b/dump-diff.py")
        r = subprocess.run([sys.executable, tool, a, b], capture_output=True, text=True)
        last = (r.stdout.strip().splitlines() or [""])[-1]
        return (r.returncode == 0, last if r.returncode == 0 else f"DIFFER {last}")
    if name.startswith("a13a-"):
        tool = os.path.join(repo, "rust/tests/golden/A13a/dump-diff.py")
        r = subprocess.run([sys.executable, tool, a, b], capture_output=True, text=True)
        last = (r.stdout.strip().splitlines() or [""])[-1]
        return (r.returncode == 0, last if r.returncode == 0 else f"DIFFER {last}")
    if name.startswith(("a7-", "a13atr-a7")):
        tool = os.path.join(repo, "rust/tests/golden/A7/mapdump-diff.py")
        r = subprocess.run([sys.executable, tool, a, b], capture_output=True, text=True)
        last = (r.stdout.strip().splitlines() or [""])[-1]
        return (r.returncode == 0, "equal after masking" if r.returncode == 0 else f"DIFFER {last}")
    if name.startswith("s5-"):
        # one section per map process; timer lengths and fire order follow script random draws, play length varies
        def sections(p):
            out, cur = [], None
            for l in open(p, errors="replace").read().splitlines():
                if l == "begin":
                    cur = []
                    out.append(cur)
                elif cur is not None:
                    cur.append(re.sub(r"t\[(\d+) [0-9a-f]{8} ", r"t[\1 X ", l))
            return out
        sa, sb = sections(a), sections(b)
        if len(sa) != len(sb):
            return (False, f"DIFFER ({len(sa)} vs {len(sb)} map sections)")
        for i, (x, y) in enumerate(zip(sa, sb)):
            st_x, st_y = [l for l in x if l.startswith("st ")], [l for l in y if l.startswith("st ")]
            cfg_x, cfg_y = [l for l in x if l.startswith("in ")], [l for l in y if l.startswith("in ")]
            if st_x != st_y or cfg_x != cfg_y:
                return (False, f"DIFFER (section {i}: self-test or handler state)")
            # frame counts vary: mouse samples must agree over the shorter run, key events as runs of equal lines
            def events(sec):
                ev = [l for l in sec if l.startswith(("act ", "ana "))]
                keys = []
                for l in ev:
                    if not l.startswith("ana 0 ") and (not keys or keys[-1] != l):
                        keys.append(l)
                return [l for l in ev if l.startswith("ana 0 ")], keys
            (mx, kx), (my, ky) = events(x), events(y)
            n = min(len(mx), len(my))
            if mx[:n] != my[:n] or kx != ky or abs(len(mx) - len(my)) > 10:
                return (False, f"DIFFER (section {i}: input events)")
            # script timers repeat as often as the run is long: only the set of distinct lines must agree; the self-test (no script name) exactly
            def timers(sec):
                ls = [l for l in sec if l.startswith(("ts ", "fire "))]
                own = sorted(l for l in ls if l.startswith(("ts  ", "fire  ")))
                return own, {l for l in ls if l not in own}
            # player: key timing and physics settling differ between runs, so velocities are not compared; step, camera, health and
            # state transitions must agree, positions within 3 m, over the shorter run (at least 600 ticks; run length follows load, crashes show in the sweep status);
            # handler/module/effect lists exactly
            def ticks(sec):
                return [l.split()[2:] for l in sec if l.startswith("pl ")]
            def tol(u, v):
                return all(p == q or (j in (1, 2, 3, 4, 5, 6, 10) and (j > 3 or abs(struct.unpack(">f", bytes.fromhex(p))[0] - struct.unpack(">f", bytes.fromhex(q))[0]) <= 3.0))
                           for j, (p, q) in enumerate(zip(u, v)))
            def trans(tk):
                o = []
                for t in tk:
                    if not o or o[-1] != (t[11], t[12]):
                        o.append((t[11], t[12]))
                return o
            plx, ply = ticks(x), ticks(y)
            k = min(len(plx), len(ply))
            tx, ty = trans(plx[:k]), trans(ply[:k])
            if (not all(tol(u, v) for u, v in zip(plx, ply)) or tx != ty or min(len(plx), len(ply)) < 600
                    or [l for l in x if re.match(r"g[hems] ", l)] != [l for l in y if re.match(r"g[hems] ", l)]):
                return (False, f"DIFFER (section {i}: player ticks or game lists)")
            (ox, sx), (oy, sy) = timers(x), timers(y)
            if ox != oy or sx != sy:
                return (False, f"DIFFER (section {i}: timers)")
            # map load / timer / collide-flag / light-level lines (S9): distinct lines, order follows timer draws; the shorter run's set
            # must be contained in the longer run's (late timer events follow run length)
            # entity and dynamic mesh counts follow the scratch's history (steps of 4 between runs of one binary in one scratch)
            s9n = lambda l: re.sub(r"( ents| dmesh) \d+", r"\1 N", l)
            s9x, s9y = {s9n(l) for l in x if l.startswith("s9 ")}, {s9n(l) for l in y if l.startswith("s9 ")}
            if not s9x or not s9y or not (s9x <= s9y if len(plx) <= len(ply) else s9y <= s9x):
                return (False, f"DIFFER (section {i}: s9 map lines)")
        return (True, f"equal over {len(sa)} maps")
    if name.startswith("a12a-"):
        # frames past the script depend on when the run shuts down
        def head(p):
            out = []
            for l in open(p, errors="replace").read().splitlines():
                if l == "tick 300":
                    return out
                out.append(l)
        ha, hb = head(a), head(b)
        ok_ = ha is not None and hb is not None and ha == hb
        return (ok_, f"equal over {len(ha)} lines" if ok_ else "DIFFER")
    if name.startswith("s7") and not name.startswith("s7sc"):
        # physics sounds and the bunker bridge follow real time: compare what lives long, the bank digests and the music/voice/dialog lines
        def frames(p):
            return [int(m.group(1)) for m in re.finditer(r"^F(\d+) ", open(p, errors="replace").read(), re.M)]
        # runs last as long as the wall clock allows: compare up to the shorter run's last frame
        last = min(max(frames(a), default=0), max(frames(b), default=0))
        def load(p):
            life, fsb, other = {}, [], []
            for l in open(p, errors="replace").read().splitlines():
                fm = re.match(r"F(\d+) ", l)
                if fm and int(fm.group(1)) > last:
                    break
                if l.startswith("MS ") and l.split()[2] == "6":
                    continue  # the scenario's music_c.ogg dynamic track (priority 6) uses an uninitialised tID
                m = re.match(r"F\d+ p=\d id=(\d+) (.*?) type=(\S+) cat=(\S+) vol=(\S+) vm=\S+ sm=(\S+) ", l)
                if m:
                    life.setdefault((m.group(2), m.group(1)), []).append(m.group(3, 4, 5, 6))
                elif l.startswith("FSB "):
                    fsb.append(l)
                elif "music_c.ogg" not in l:  # the scenario's dynamic track uses an uninitialised tID
                    if not (name.startswith("s7v") and other and other[-1] == l):  # subtitles repeat per frame
                        other.append(l)
            keep = lambda n: not n.startswith(("physics/", "DW_tunnel/bridge/"))
            sig = {(n, tuple(sorted(set(r)))) for (n, _), r in life.items() if len(r) >= 250 and keep(n)}
            return sorted(fsb), sig, other
        fa, sa, oa = load(a); fb, sb, ob = load(b)
        bad = [w for w, x, y in (("fsb", fa, fb), ("long-lived events", sa, sb), ("music/voice/dialog lines", oa, ob)) if x != y]
        return (not bad and len(fa) > 0, f"equal ({len(fa)} fsb digests, {len(sa)} long-lived events, {len(oa)} lines)" if not bad else "DIFFER in " + ", ".join(bad))
    if name.startswith("a1b-"):
        # the engine seeds its random numbers from the clock: draws are masked and a run that branches on one
        # is compared up to it; AmfpStub_ calls were not traced by the C++-era recording
        rx = re.compile(r"^(Rand(?:Float|Int)\(.*\))=.*$")
        load = lambda p: [rx.sub(r"\1=?", l.rstrip("\n")) for l in open(p, errors="replace") if not l.startswith("AmfpStub_")]
        la, lb = load(a), load(b)
        if la == lb:
            return (len(la) > 20, f"identical call trace ({len(la)} calls, random draws masked)")
        k = next((i for i in range(min(len(la), len(lb))) if la[i] != lb[i]), min(len(la), len(lb)))
        if k == min(len(la), len(lb)):
            return (k > 20, f"equal over {k} calls (runs stopped at different times: {len(la)}/{len(lb)})")
        ok_ = k > 20 and any(l.startswith("Rand") for l in la[:k])
        return (ok_, f"equal over {k} calls up to a branch on a random draw" if ok_ else f"DIFFER at call {k}")
    if name.startswith("a12d-"):
        # sound rows and the music line follow what happens to be playing
        mask = lambda p: [re.sub(r"^l lines=\d+ nodes=\d+ objs=(\d+) other=\d+", r"l objs=\1", l) for l in open(p, errors="replace") if not l.startswith("dn ")]
        return (mask(a) == mask(b), "equal (playing-sound rows masked)" if mask(a) == mask(b) else "DIFFER")
    if name.startswith("s8c-"):
        # per effect type, up to the shorter run (time-driven effects render a jittering number of frames); the game's
        # scripts set the tentacle rotations from a per-frame random draw
        def load(p):
            d = {}
            for l in open(p, errors="replace"):
                d.setdefault(l.split(" ", 1)[0], []).append(re.sub(r"a_mtxModel=[0-9a-f,]*", "a_mtxModel=rnd", l))
            return d
        da, db = load(a), load(b)
        n = sum(min(len(da.get(k, [])), len(db.get(k, []))) for k in da)
        bad = [k for k in set(da) | set(db) if da.get(k, [])[:len(db.get(k, []))] != db.get(k, [])[:len(da.get(k, []))]
               or (bool(da.get(k)) != bool(db.get(k)))
               or (k not in ("fx=ImageFadeFX", "fx=FearTentacles") and len(da.get(k, [])) != len(db.get(k, [])))]
        return (not bad and n > 50, f"equal ({n} effect passes)" if not bad else "DIFFER in " + ", ".join(sorted(bad)))
    if name.startswith("a12b-"):
        # camera positions follow the player body, which settles with real time
        mask = lambda p: [l for l in open(p, errors="replace") if not l.startswith(("i ps pos", "i step_pos"))]
        return (mask(a) == mask(b), "equal (camera positions masked)" if mask(a) == mask(b) else "DIFFER")
    if name.startswith("esnd-"):
        # runs stop at different frames: compare the frames both runs completed
        def frames(p):
            fr, cur = [], None
            for l in open(p, errors="replace"):
                if l.startswith("f "):
                    cur = [l]; fr.append(cur)
                elif cur is not None:
                    cur.append(l)
            return fr[:-1]
        fa, fb = frames(a), frames(b)
        k = min(len(fa), len(fb))
        ok_ = k > 100 and fa[:k] == fb[:k] and any(len(f) > 1 for f in fa[:k])
        return (ok_, f"equal over {k} frames (run lengths {len(fa)}/{len(fb)})" if ok_ else "DIFFER")
    if name.startswith(("a11b-", "a11c-", "a2b-")):
        # runs stop at different frames: compare up to the last whole frame of the shorter one
        la, lb = (open(x, errors="replace").read().splitlines() for x in (a, b))
        n = min(len(la), len(lb))
        k = max((i for i in range(n) if la[i].startswith(("tick ", "f "))), default=0)
        ok_ = k > 100 and la[:k] == lb[:k]
        return (ok_, f"equal over {k} lines (run lengths {len(la)}/{len(lb)})" if ok_ else "DIFFER")
    if name.startswith(("s8d-", "s8dm-")):
        # ImGui draw lists run on the frame clock: every frame of b must be one a C++ run drew
        stem = os.path.basename(a).replace("-cpp", "")
        extra = [a.replace("/s8d-", "/s8d2-")] + [os.path.join(base, stem), os.path.join(base, stem.replace("s8d-", "s8d2-"))]
        refs = [a] + [r for r in extra if r != a and os.path.exists(r)]
        r = subprocess.run([sys.executable, os.path.join(repo, "scripts/s8d-compare.py"), b] + refs, capture_output=True, text=True)
        return (r.returncode == 0, (r.stdout.splitlines() or ["no output"])[0])
    if filecmp.cmp(a, b, shallow=False):
        return (True, "identical")
    if name.startswith("a10a-"):
        mask = lambda p: [re.sub(r"combo=-?\d+\[.*\]|text=\"[^\"]*\"", "", l) if "size=400,18" in l else l for l in open(p, errors="replace")]
        if mask(a) == mask(b):
            return (True, "identical (sound device names masked)")
    if name.startswith("h4seq") and multiset(a) == multiset(b):
        return (True, "same multiset (combine order is pointer-ordered in C++)")
    return (False, "DIFFER")

def ok(p):
    return os.path.exists(p) and os.path.getsize(p) > 0

if not quick:
    names = [f"h4seq-{g}" for g in ("soma", "rebirth", "bunker") if g in games]
    names += [f"l9-{g}" for g in ("soma", "rebirth", "bunker") if g in games]
    names += [f"{k}-{g}" for k in ("s7", "s7sc", "s7p") for g in ("soma", "rebirth", "bunker") if g in games]
    names += ["s7v-soma", "s5bv-soma"] if "soma" in games else []
    names += [f"s8-{g}" for g in ("soma", "rebirth", "bunker") if g in games]
    names += [f"esnd-{g}" for g in ("soma", "rebirth", "bunker", "amnesia", "amfp") if g in games]
    names += [f"s8b-{g}" for g in ("soma", "rebirth", "bunker") if g in games]
    names += [f"s8c-{g}" for g in ("soma", "rebirth", "bunker") if g in games]
    names += [f"{k}-{g}" for k in ("s8d", "s8dm") for g in ("soma", "rebirth", "bunker") if g in games]
    names += [f"s5-{g}" for g in ("soma", "rebirth", "bunker") if g in games]
    if "amnesia" in games:
        names += [f"{k}-{m}" for m in ("00_rainy_hall", "02_library", "04_final") for k in ("h13", "a7", "a10a", "a11a", "a11b", "l9", "a12b", "a11c", "a12c", "a12d", "a2b", "a1b", "a13a", "a2c")] + ["a12a-02_library"] + ["a9"]
    if "amnesia" in games:
        names += [f"a7b-{m}" for m in ("05_wine_cellar", "11_study", "19_morgue", "20_sewer")]
    names += [f"a10b-boot-{g}" for g in ("amnesia", "amfp") if g in games]
    names += [f"a13b-boot-{g}" for g in ("amnesia", "amfp") if g in games]
    if "amnesia" in games:
        names += [f"{k}{e}" for k in ("rt-prison-patrol", "rt-prison-hunt") for e in ("", "-A", "-B")] + ["a4b-prison"]
        names += [f"a5-{m}" for m in ("13_machine_room", "17_control_room", "05_wine_cellar", "19_morgue", "29_orb_chamber")]
    if "amfp" in games:
        names += [f"a1b-amfp-{m}" for m in ("01_mansion_01", "06_factory", "10_reactor")]
        names += [f"a13a-{m}" for m in ("01_mansion_01", "06_factory")]
        names += [f"a7b-amfp-{m}" for m in ("03_cellar", "06_factory")]
        names += [f"a13a-{m}" for m in ("01_mansion_01", "06_factory")] + ["a2c-06_factory", "a2c-09_bilge"]
        names += [f"{k}{e}" for k in ("rt-factory", "rt-bilge") for e in ("", "-A", "-B")] + ["a4b-factory", "a4b-bilge"]
        names += [f"a5-amfp-{m}" for m in ("03_cellar", "05_church", "11_engine_room")]
    if "amnesia" in games:
        names += ["a10b-play-amnesia", "a13atr-a7", "a13atr-struct"]
        names += ["a10b-play-amnesia", "a13b-play-amnesia"]
    has_base = os.path.isdir(base)
    if not has_base:
        print(f"no C++ baseline at {base} (record one with --record-baseline before flipping a family)")
    for name in names:
        if len(impls) == 2:
            a, b = (os.path.join(out, f"{name}-{i}.txt") for i in impls)
            if ok(a) and ok(b):
                s, why = same(name, a, b)
                print(f"{name} cpp vs rust: {why}"); bad += not s
            else:
                print(f"{name} cpp vs rust: MISSING DUMP"); bad += 1
        if has_base and "rust" in impls:
            ref, got = os.path.join(base, f"{name}.txt"), os.path.join(out, f"{name}-rust.txt")
            if not ok(ref):
                print(f"{name} rust vs baseline: no baseline dump")
            elif ok(got):
                s, why = same(name, ref, got)
                print(f"{name} rust vs baseline: {why}"); bad += not s
            else:
                print(f"{name} rust vs baseline: MISSING DUMP"); bad += 1

if saves:
    soma = {}
    for impl in impls:
        if "soma" in games:
            t = open(os.path.join(out, f"rt-soma-{impl}.log"), errors="replace").read()
            m = re.search(r"round-trip diffs: (\d+)", t)
            soma[impl] = sorted(l.split("|")[0] for l in t.splitlines() if l[:1] in "+-" and "|" in l)
            print(f"soma save round trip {impl}: {m.group(1) + ' diffs' if m else 'NO RESULT'}")
            bad += not m
        if "amnesia" in games:
            a = open(os.path.join(out, f"rt-amnesia-{impl}.log"), errors="replace").read()
            m = re.search(r"structure diffs A-B: (\d+)\s+B-C: (\d+)", a)
            print(f"amnesia save round trip {impl}: " + (f"A-B {m.group(1)} B-C {m.group(2)}" if m else "NO RESULT"))
            bad += not m or m.group(1) != "0" or m.group(2) != "0"
    if "amnesia" in games:
        for label, f, rx in (("inventory save round trip", "inv-rt.log", r"A==B: True\s+B==C: True"),
                             ("journal save round trip (notes, diaries, quests)", "jrn-rt.log", r"A==B: True\s+B==C: True"),
                             ("inventory GUI vs C++ baseline", "inv-gui-cmp.log", r"steps \d+ failed 0")):
            p = os.path.join(out, f)
            if f == "inv-gui-cmp.log" and not os.path.isdir(os.path.join(base, "gui")):
                print(f"{label}: no baseline gui dir in {base}")
                continue
            good = os.path.exists(p) and re.search(rx, open(p, errors="replace").read())
            print(f"{label}: " + ("ok" if good else "FAIL"))
            bad += not good
    if len(soma) == 2:
        same_e = soma[impls[0]] == soma[impls[1]]
        print("soma round-trip entities: " + ("same on both paths" if same_e else "DIFFER between paths"))
        bad += not same_e
print("GATE " + ("PASS" if bad == 0 else f"FAIL ({bad})"))
sys.exit(1 if bad else 0)
EOF
status=${PIPESTATUS[0]}
echo "summary: $SUM"
exit "$status"
