#!/bin/bash
# verify.sh OUT_DIR: save bytes of the probe must equal the golden
set -e
OUT=${1:?out dir}
$OUT/ser_probe $OUT/rust.xml
cmp $OUT/rust.xml "$(dirname $0)/../ser_probe.xml" && echo "matches golden"
