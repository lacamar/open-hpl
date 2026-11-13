#!/bin/bash
# build.sh BUILD_DIR OUT_DIR: BUILD_DIR is a configured amnesia/src build with the HPL2 target built.
set -e
H=$(cd "$(dirname "$0")" && pwd)
W=$(git -C "$H" rev-parse --show-toplevel)
B=$(cd "${1:?build dir}" && pwd)
OUT=${2:?out dir}
mkdir -p "$OUT"
FLAGS="-DHPL_VULKAN -DUSE_OALWRAPPER -DUSE_SDL2 -std=gnu++20 -fno-strict-aliasing -mcpu=apple-m2+crc+aes+sha3+fp16 -O2 -g -Wno-switch -Wno-reorder -w"
INC="-I$B/core2 -I$W/HPL2/core -I$W/HPL2/core/include -I$W/HPL2/dependencies/OALWrapper/include -I$W/HPL2/dependencies/sources/binreloc -I/usr/include/AL -I$W/HPL2/dependencies/newton -isystem /usr/include/SDL2 -idirafter $W/HPL2/dependencies/include"
LIBS="$B/core2/libHPL2.a $B/cargo/release/libohpl.a $B/core2/libbinreloc.a $B/core2/libohpl_cdeps.a $B/core2/newton/libNewton.a $B/core2/OALWrapper/libOALWrapper.a -lpugixml -lSDL2 -lz -lpthread -ldl -lrt -lm -Wl,--unresolved-symbols=ignore-all"
g++ $FLAGS $INC -c "$H/stubs.cpp" -o "$OUT/stubs.o"
g++ $FLAGS $INC -c "$H/h4_probe.cpp" -o "$OUT/h4_probe.o"
g++ "$OUT/h4_probe.o" "$OUT/stubs.o" $LIBS -o "$OUT/h4_probe"
