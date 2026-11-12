#!/bin/bash
# build.sh OUT_DIR: builds the probe against the worktree sources and rust/target/debug/libohpl.a
set -e
W=$(git rev-parse --show-toplevel)
OUT=${1:?out dir}
mkdir -p $OUT
INC="-I$W/HPL2/core -I$W/HPL2/core/include -isystem /usr/include/SDL2 -idirafter $W/HPL2/dependencies/include -DUSE_SDL2 -std=gnu++20 -fno-strict-aliasing -O1 -g -Wno-switch -Wno-reorder"
g++ $INC -c $W/HPL2/core/sources/system/SerializeClass.cpp -o $OUT/ser.o
g++ $INC -c $W/HPL2/core/sources/system/String.cpp -o $OUT/str.o
for f in math/Math math/MathTypes graphics/Color; do g++ $INC -c $W/HPL2/core/sources/$f.cpp -o $OUT/$(basename $f).o; done
g++ $INC -c $(dirname $0)/ser_probe.cpp -o $OUT/probe.o
g++ $OUT/*.o $W/rust/target/debug/libohpl.a -lz -lpthread -ldl -lm -Wl,--unresolved-symbols=ignore-all -o $OUT/ser_probe
