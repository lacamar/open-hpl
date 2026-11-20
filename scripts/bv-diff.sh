#!/bin/sh
# Run the cBoundingVolume/cFrustum harness; the dump must equal rust/tests/golden/BV/dump.txt.
# usage: scripts/bv-diff.sh <build-dir>   (make BvFrustumTests)
# Record (C++ original): OHPL_BV_DUMP=dump.txt BvFrustumTests.bin.aarch64
set -e
bin="${1:?build dir}/BvFrustumTests.bin.aarch64"
out="${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/bv-diff"
gold="$(dirname "$0")/../rust/tests/golden/BV/dump.txt"
mkdir -p "$out"
OHPL_BV_DUMP="$out/rust.txt" "$bin"
cmp "$out/rust.txt" "$gold" && echo "equal to golden: $(wc -l < "$out/rust.txt") lines"
