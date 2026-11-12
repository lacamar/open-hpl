#!/bin/bash
# compare.sh OUT_DIR: save bytes must match between OHPL_IMPL=cpp and rust, and the golden
set -e
OUT=${1:?out dir}
OHPL_IMPL=cpp $OUT/ser_probe $OUT/cpp.xml
OHPL_IMPL=rust $OUT/ser_probe $OUT/rust.xml
cmp $OUT/cpp.xml $OUT/rust.xml && echo "save bytes identical"
cmp $OUT/cpp.xml "$(dirname $0)/../ser_probe.xml" && echo "matches golden"
