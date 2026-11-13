#!/usr/bin/env bash
# A/B the A7 map/area ownership on real maps: OHPL_IMPL=cpp vs rust, OHPL_A7_MAPDUMP text compared.
# usage: mapdump-compare.sh <game: amnesia|amfp> <build-dir> <work-dir> <map>...
# <work-dir> holds scratch dirs (setup-test-scratch.sh) and the dumps; never under steamapps/common.
set -uo pipefail
GAME="${1:?game}"; BUILD="${2:?build dir}"; WORK="${3:?work dir}"; shift 3
REPO="$(cd "$(dirname "$0")/../../../.." && pwd)"
case "$GAME" in
	amnesia) ROOT="$HOME/.local/share/Steam/steamapps/common/Amnesia The Dark Descent" ;;
	amfp) ROOT="$HOME/.local/share/Steam/steamapps/common/Machine for Pigs" ;;
	*) echo "bad game" >&2; exit 2 ;;
esac
SCRATCH="$WORK/scratch-$GAME"
[ -d "$SCRATCH" ] || "$REPO/scripts/setup-test-scratch.sh" "$ROOT" "$SCRATCH" >/dev/null
"$REPO/scripts/deploy-test-binary.sh" "$BUILD/Amnesia.bin.aarch64" "$SCRATCH" >/dev/null
# private control socket: the stock sweep's ohpl-sweep.sock is shared with other agents' runs
sed 's/ohpl-sweep\.sock/ohpl-a7sweep.sock/' "$REPO/scripts/soma-sweep.py" >"$WORK/a7-sweep.py"
ln -sf "$REPO/scripts/headless-lock-holders.sh" "$WORK/headless-lock-holders.sh"
export PYTHONPATH="$REPO/scripts"
export OPENHPL_HEADLESS_SLOTS=4
export OHPL_GAME="$GAME"
VAR="OPENHPL_$(echo "$GAME" | tr a-z A-Z)_SCRATCH"; export "$VAR=$SCRATCH"
fail=0
for map in "$@"; do
	for impl in cpp rust; do
		out="$WORK/dump-$GAME-$map-$impl.txt"; rm -f "$out"
		OHPL_IMPL=$impl OHPL_A7_MAPDUMP="$out" python3 "$WORK/a7-sweep.py" --map "$map" --out "$WORK/res-$GAME-$map-$impl.json" >"$WORK/log-$GAME-$map-$impl.txt" 2>&1
	done
	for impl in cpp rust; do
		lg=$(python3 -I -c 'import json,sys;print(list(json.load(open(sys.argv[1]))["maps"].values())[0].get("log",""))' "$WORK/res-$GAME-$map-$impl.json" 2>/dev/null)
		grep -qa "dump '$map' load backend=$impl" "$lg" 2>/dev/null || { echo "BACKEND? $GAME $map $impl not confirmed in log"; fail=1; }
	done
	a="$WORK/dump-$GAME-$map-cpp.txt"; b="$WORK/dump-$GAME-$map-rust.txt"
	if [ -s "$a" ] && python3 -I "$REPO/rust/tests/golden/A7/mapdump-diff.py" "$a" "$b" >/dev/null; then
		echo "SAME $GAME $map lines=$(wc -l <"$a") areas=$(grep -c '|area' "$a")"
	else
		echo "DIFF $GAME $map cpp=$(wc -l <"$a" 2>/dev/null) rust=$(wc -l <"$b" 2>/dev/null)"; fail=1
	fi
done
exit $fail
