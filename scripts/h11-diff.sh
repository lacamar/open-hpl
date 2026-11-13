#!/bin/sh
# Differential run of cMeshEntity/cSubMeshEntity/cAnimationState, lights and cFogArea over real game meshes and animations:
# OHPL_IMPL=cpp vs rust dumps must be identical and equal rust/tests/golden/H11 (meshes need the Steam installs, read-only).
# usage: scripts/h11-diff.sh <build-dir>   (make HPL2 HPL2TraceRecorder in it)
set -e
here=$(cd "$(dirname "$0")" && pwd)
h=$here/../rust/tests/golden/H11
out="${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/h11-diff"
mkdir -p "$out"
"$h/harness/gen.sh" "$1" h11_script.cpp "$out/h11_script"
"$h/harness/run.sh" "$out/h11_script" "$out/run" both
for f in "$out"/run/cpp/*.txt; do
	cmp -s "$f" "$h/$(basename "$f")" || { echo "differs from golden: $(basename "$f")"; exit 1; }
done
echo "equal to golden"
