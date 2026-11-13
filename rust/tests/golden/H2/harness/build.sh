#!/bin/bash
# build.sh BUILD_DIR OUT_DIR: BUILD_DIR is a configured amnesia/src build with the HPL2 target built.
# Produces OUT_DIR/{xml,cfg,lang,fs}_new and OUT_DIR/res_probe.
set -e
H=$(cd "$(dirname "$0")" && pwd)
W=$(git -C "$H" rev-parse --show-toplevel)
B=$(cd "${1:?build dir}" && pwd)
OUT=${2:?out dir}
mkdir -p "$OUT"
FLAGS="-DHPL_VULKAN -DUSE_OALWRAPPER -DUSE_SDL2 -std=gnu++20 -fno-strict-aliasing -mcpu=apple-m2+crc+aes+sha3+fp16 -O1 -g -Wno-switch -Wno-reorder -w"
INC="-I$B/core2 -I$W/HPL2/core -I$W/HPL2/core/include -I$W/HPL2/dependencies/OALWrapper/include -I$W/HPL2/dependencies/sources/binreloc -I/usr/include/AL -I$W/HPL2/dependencies/newton -isystem /usr/include/SDL2 -idirafter $W/HPL2/dependencies/include"
LIBS="$B/core2/libHPL2.a $B/cargo/release/libohpl.a $B/core2/libbinreloc.a $B/core2/libohpl_cdeps.a $B/core2/newton/libNewton.a $B/core2/OALWrapper/libOALWrapper.a -lpugixml -lz -lpthread -ldl -lrt -lm -Wl,--unresolved-symbols=ignore-all"
cc() { g++ $FLAGS "$@"; }

cc $INC -c "$H/stubs.cpp" -o "$OUT/stubs.o"
for n in xml cfg lang fs; do
	cc $INC -I$H -c "$H/${n}_probe.cpp" -o "$OUT/${n}_new.o"
	g++ "$OUT/${n}_new.o" "$OUT/stubs.o" $LIBS -o "$OUT/${n}_new"
done
cc $INC -I$H -c "$H/res_probe.cpp" -o "$OUT/res.o"
g++ "$OUT/res.o" $LIBS -o "$OUT/res_probe"
