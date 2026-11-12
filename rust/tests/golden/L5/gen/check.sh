#!/usr/bin/env bash
# Usage: check.sh <build-dir>   (built `HPL2`; see gen.sh)
# Runs the C++ and Rust paths of the harness and compares both with the golden vectors.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
g=$(dirname "$here")
tmp=$(mktemp -d "${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/l5-tmp/check.XXXXXX")
export OPENHPL_HEADLESS_SOCKET=x
"$here/gen.sh" "$1" "$tmp/gen"
for m in cpp rust; do
	OHPL_IMPL=$m "$tmp/gen" oal >"$tmp/oal_$m.txt"
	(cd "$g/fixtures" && OHPL_IMPL=$m "$tmp/gen" video a64x48.ogv 90 b50x38.ogv 120 >"$tmp/video_$m.txt")
	(cd "$g" && OHPL_IMPL=$m "$tmp/gen" env fixtures/env_cave.xml fixtures/env_fmod.xml fixtures/missing.xml >"$tmp/env_$m.txt")
	(cd "$g" && OHPL_IMPL=$m "$tmp/gen" font fixtures/font_sample.fnt fixtures/font_lacks.fnt fixtures/font_big.fnt fixtures/font_missing.fnt >"$tmp/font_$m.txt")
	diff "$tmp/oal_$m.txt" "$g/oal_trace.txt"
	diff "$tmp/video_$m.txt" "$g/video_ref.txt"
	diff "$tmp/env_$m.txt" "$g/env.txt"
	diff "$tmp/font_$m.txt" "$g/font.txt"
done
rm -rf "$tmp"
echo "L5 harness: cpp and rust match the golden vectors"
