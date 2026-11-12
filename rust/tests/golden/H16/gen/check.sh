#!/usr/bin/env bash
# Usage: check.sh <build-dir>   (built `HPL2`; see gen.sh)
# Runs the C++ and Rust paths of the harness and compares both with the golden vectors.
# AI "ranks" lines (heap order of cAINode, the A* tie key) depend on the allocation pattern and are skipped.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
g=$(dirname "$here")
tmp=$(mktemp -d "${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/h16-tmp/check.XXXXXX")
export OPENHPL_HEADLESS_SOCKET=x
"$here/gen.sh" "$1" "$tmp/gen"
for m in cpp rust; do
	OHPL_IMPL=$m "$tmp/gen" sound 12 80 >"$tmp/sound_$m.txt"
	OHPL_IMPL=$m "$tmp/gen" ai 12 "$tmp/edges.xml" >"$tmp/ai_$m.txt"
	diff "$tmp/sound_$m.txt" "$g/sound.txt"
	diff <(grep -v '^ranks' "$tmp/ai_$m.txt") <(grep -v '^ranks' "$g/ai.txt")
done
rm -rf "$tmp"
echo "H16 harness: cpp and rust match the golden vectors"
