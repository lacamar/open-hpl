#!/bin/bash
# build.sh BUILD_DIR OUT_DIR: BUILD_DIR is a configured amnesia/src build with the HPL2 target built.
# Produces OUT_DIR/{xml,cfg,lang,fs}_{new,old}; the _old probes link the pre-port sources from history.
set -e
H=$(cd "$(dirname "$0")" && pwd)
W=$(git -C "$H" rev-parse --show-toplevel)
B=$(cd "${1:?build dir}" && pwd)
OUT=${2:?out dir}
mkdir -p "$OUT"
"$H/fetch-old.sh" "$OUT"
FLAGS="-DHPL_VULKAN -DUSE_OALWRAPPER -DUSE_SDL2 -std=gnu++20 -fno-strict-aliasing -mcpu=apple-m2+crc+aes+sha3+fp16 -O1 -g -Wno-switch -Wno-reorder -w"
INC="-I$B/core2 -I$W/HPL2/core -I$W/HPL2/core/include -I$W/HPL2/dependencies/OALWrapper/include -I$W/HPL2/dependencies/sources/binreloc -I/usr/include/AL -I$W/HPL2/dependencies/newton -isystem /usr/include/SDL2 -idirafter $W/HPL2/dependencies/include"
LIBS="$B/core2/libHPL2.a $B/cargo/release/libohpl.a $B/core2/libbinreloc.a $B/core2/libohpl_cdeps.a $B/core2/newton/libNewton.a $B/core2/OALWrapper/libOALWrapper.a -lpugixml -lz -lpthread -ldl -lrt -lm -Wl,--unresolved-symbols=ignore-all"
cc() { g++ $FLAGS "$@"; }
lnk() { g++ "$1" "$OUT/stubs.o" "${@:3}" $LIBS -o "$2"; }

cc $INC -c "$H/stubs.cpp" -o "$OUT/stubs.o"
cc $INC -I$H -c "$H/xml_probe.cpp" -o "$OUT/xml_new.o"
cc -I$OUT/old $INC -I$H -DOLD -c "$H/xml_probe.cpp" -o "$OUT/xml_old.o"
cc -I$OUT/old $INC -c "$OUT/old/resources/XmlDocument.cpp" -o "$OUT/old_XmlDocument.o"
cc -I$OUT/old $INC -c "$OUT/old/impl/XmlDocumentTiny.cpp" -o "$OUT/old_XmlDocumentTiny.o"
lnk "$OUT/xml_new.o" "$OUT/xml_new"
lnk "$OUT/xml_old.o" "$OUT/xml_old" "$OUT/old_XmlDocument.o" "$OUT/old_XmlDocumentTiny.o"

for p in cfg:ConfigFile lang:LanguageFile fs:FileSearcher; do
	n=${p%%:*}; f=${p##*:}
	cc $INC -I$H -c "$H/${n}_probe.cpp" -o "$OUT/${n}_new.o"
	cc -I$OUT/old $INC -I$H -c "$H/${n}_probe.cpp" -o "$OUT/${n}_old.o"
	cc -I$OUT/old $INC -c "$OUT/old/resources/$f.cpp" -o "$OUT/old_$f.o"
	lnk "$OUT/${n}_new.o" "$OUT/${n}_new"
	lnk "$OUT/${n}_old.o" "$OUT/${n}_old" "$OUT/old_$f.o"
done
