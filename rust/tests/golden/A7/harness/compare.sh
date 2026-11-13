#!/bin/bash
# compare.sh [out dir]: std-container originals (cpp) vs Rust-owned arena (rust), both against ../a7_ops.txt.
# A7_RECORD=1 rewrites the golden from the cpp side.
set -e
W=$(git rev-parse --show-toplevel)
OUT=${1:-$HOME/.cache/open-hpl/a7-tmp}
mkdir -p $OUT
(cd $W/rust && CARGO_TARGET_DIR=${CARGO_TARGET_DIR:-$W/rust/target} cargo build --offline -q)
LIB=${CARGO_TARGET_DIR:-$W/rust/target}/debug/libohpl.a
g++ -std=gnu++20 -O1 -ffp-contract=off -I$W/amnesia/src/game -I$W/HPL2/core/include $W/rust/tests/golden/A7/harness/a7_harness.cpp $LIB -lz -lpthread -ldl -lm -Wl,--unresolved-symbols=ignore-all -o $OUT/a7_harness
$OUT/a7_harness cpp > $OUT/cpp.txt
$OUT/a7_harness rust > $OUT/rust.txt
G=$W/rust/tests/golden/A7/a7_ops.txt
[ -n "$A7_RECORD" ] && cp $OUT/cpp.txt $G
wc -l < $G
cmp $OUT/cpp.txt $OUT/rust.txt && cmp $OUT/cpp.txt $G && echo "A7 cpp == rust == golden"
