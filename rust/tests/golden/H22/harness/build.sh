#!/bin/bash
# usage: build.sh <repo root> <cmake build dir> <out dir>
# Builds h22_hc against the HPL2 library of the build dir (needs `make TraceRecorderTests` first).
set -e
ROOT=$1 B=$2 OUT=$3
H=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$OUT"
INC="-I$B/core2 -I$ROOT/HPL2/core -I$ROOT/HPL2/core/include -I$ROOT/HPL2/tests -I$ROOT/HPL2/tests/trace -I$ROOT/HPL2/dependencies/OALWrapper/include -I/usr/include/AL -I$ROOT/HPL2/dependencies/newton/. -isystem /usr/include/SDL2"
FL="-std=gnu++17 -fno-strict-aliasing -O1 -g -DHPL_VULKAN -DUSE_OALWRAPPER -DUSE_SDL2 -Wno-switch -Wno-reorder -idirafter $ROOT/HPL2/dependencies/include"
LIBS=$(sed -e 's/^.*-o ..\/..\/TraceRecorderTests.bin.aarch64//' "$B/core2/tests/CMakeFiles/TraceRecorderTests.dir/link.txt")
for T in h22_hc; do
  c++ $FL -I"$OUT" $INC -c "$H/$T.cpp" -o "$OUT/$T.o"
  (cd "$B/core2/tests" && c++ -fno-strict-aliasing -mcpu=apple-m2+crc+aes+sha3+fp16 -O2 -g "$OUT/$T.o" -o "$OUT/$T" $LIBS)
done
