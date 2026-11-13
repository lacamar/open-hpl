#!/bin/bash
# compare.sh [out dir]: cpp (pre-A5 shell composition) vs rust (ohpl_a5_*) traces, both against small_props.txt.
set -e
W=$(git rev-parse --show-toplevel)
OUT=${1:-$HOME/.cache/open-hpl/a5-tmp}
mkdir -p $OUT
(cd $W/rust && cargo build --offline -q)
g++ -std=gnu++20 -O1 -ffp-contract=off -I$W/amnesia/src/game -I$W/HPL2/core/include $W/rust/tests/golden/A5/harness/a5_harness.cpp $W/rust/target/debug/libohpl.a -lz -lpthread -ldl -lm -Wl,--unresolved-symbols=ignore-all -o $OUT/a5_harness
$OUT/a5_harness cpp > $OUT/cpp.txt
$OUT/a5_harness rust > $OUT/rust.txt
G=$W/rust/tests/golden/A5/small_props.txt
wc -l < $G
cmp $OUT/cpp.txt $OUT/rust.txt && cmp $OUT/cpp.txt $G && echo "A5 cpp == rust == golden"
