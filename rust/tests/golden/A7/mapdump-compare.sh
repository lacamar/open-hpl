#!/usr/bin/env bash
# Check the A7 map/area ownership on real maps: OHPL_A7_MAPDUMP text against mapdumps/<game>-<map>.txt.gz.
# usage: mapdump-compare.sh <game: amnesia|amfp> <build-dir> <work-dir> <map>...
# MAPDUMP_RECORD=1 REF_BIN=<pre-flip Amnesia.bin.aarch64> rewrites the goldens from that binary with OHPL_IMPL=cpp.
# <work-dir> holds scratch dirs (setup-test-scratch.sh) and the dumps; never under steamapps/common.
set -uo pipefail
GAME="${1:?game}"; BUILD="${2:?build dir}"; WORK="${3:?work dir}"; shift 3
REPO="$(cd "$(dirname "$0")/../../../.." && pwd)"
GOLD="$REPO/rust/tests/golden/A7/mapdumps"
case "$GAME" in
	amnesia) ROOT="$HOME/.local/share/Steam/steamapps/common/Amnesia The Dark Descent" ;;
	amfp) ROOT="$HOME/.local/share/Steam/steamapps/common/Machine for Pigs" ;;
	*) echo "bad game" >&2; exit 2 ;;
esac
SCRATCH="$WORK/scratch-$GAME"
[ -d "$SCRATCH" ] || "$REPO/scripts/setup-test-scratch.sh" "$ROOT" "$SCRATCH" >/dev/null
BIN="$BUILD/Amnesia.bin.aarch64"
[ -n "${MAPDUMP_RECORD:-}" ] && BIN="${REF_BIN:?REF_BIN with MAPDUMP_RECORD}"
"$REPO/scripts/deploy-test-binary.sh" "$BIN" "$SCRATCH" >/dev/null
# private control socket: the stock sweep's ohpl-sweep.sock is shared with other agents' runs
sed 's/ohpl-sweep\.sock/ohpl-a7sweep.sock/' "$REPO/scripts/soma-sweep.py" >"$WORK/a7-sweep.py"
ln -sf "$REPO/scripts/headless-lock-holders.sh" "$WORK/headless-lock-holders.sh"
export PYTHONPATH="$REPO/scripts"
export OPENHPL_HEADLESS_SLOTS=4
export OHPL_GAME="$GAME"
VAR="OPENHPL_$(echo "$GAME" | tr a-z A-Z)_SCRATCH"; export "$VAR=$SCRATCH"
mkdir -p "$GOLD"
fail=0
for map in "$@"; do
	out="$WORK/dump-$GAME-$map.txt"; rm -f "$out"
	env ${MAPDUMP_RECORD:+OHPL_IMPL=cpp} OHPL_A7_MAPDUMP="$out" python3 "$WORK/a7-sweep.py" --map "$map" --out "$WORK/res-$GAME-$map.json" >"$WORK/log-$GAME-$map.txt" 2>&1
	g="$GOLD/$GAME-$map.txt.gz"; ref="$WORK/golden-$GAME-$map.txt"
	if [ -n "${MAPDUMP_RECORD:-}" ]; then
		[ -s "$out" ] && gzip -9 -c "$out" >"$g" && echo "RECORDED $GAME $map lines=$(wc -l <"$out")" || { echo "NODUMP $GAME $map"; fail=1; }
	elif [ -s "$out" ] && [ -s "$g" ] && zcat "$g" >"$ref" && python3 -I "$REPO/rust/tests/golden/A7/mapdump-diff.py" "$ref" "$out" >/dev/null; then
		echo "SAME $GAME $map lines=$(wc -l <"$out") areas=$(grep -c '|area' "$out")"
	else
		echo "DIFF $GAME $map golden=$(zcat "$g" 2>/dev/null | wc -l) now=$(wc -l <"$out" 2>/dev/null)"; fail=1
	fi
done
exit $fail
