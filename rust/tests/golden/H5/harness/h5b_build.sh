#!/bin/bash
# usage: h5b_build.sh <repo root> <cmake build dir> <rust libohpl.a> <out dir> [facade] [h5b_run|h5b_imm|h5b_font] [extra .cpp sources of the tree the archive predates]
# Builds h5b_run against the HPL2 library of the build dir (needs `make TraceRecorderTests` first).
# Pass "facade" when the build dir's libHPL2.a predates RustGfxFacade.cpp.
set -e
ROOT=$1 B=$2 OHPL=$3 OUT=$4 FACADE=$5 SRC=${6:-h5b_run} EXTRA=$7
H=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$OUT"
INC="-I$B/core2 -I$ROOT/HPL2/core -I$ROOT/HPL2/core/include -I$ROOT/HPL2/core/sources -I$ROOT/HPL2/tests -I$ROOT/HPL2/dependencies/OALWrapper/include -I/usr/include/AL -I$ROOT/HPL2/dependencies/newton/. -isystem /usr/include/SDL2"
FL="-std=gnu++17 -fno-strict-aliasing -mcpu=apple-m2+crc+aes+sha3+fp16 -O2 -g -DNDEBUG -DHPL_VULKAN -DUSE_OALWRAPPER -DUSE_SDL2 -Wno-switch -Wno-reorder -idirafter $ROOT/HPL2/dependencies/include"
OBJS="$OUT/$SRC.o"
c++ $FL $INC -c "$H/$SRC.cpp" -o "$OUT/$SRC.o"
if [ "$FACADE" = facade ]; then
	c++ $FL $INC -c "$ROOT/HPL2/core/sources/graphics/RustGfxFacade.cpp" -o "$OUT/facade.o"
	OBJS="$OBJS $OUT/facade.o"
fi
for x in $EXTRA; do
	c++ $FL $INC -c "$ROOT/$x" -o "$OUT/$(basename $x).o"
	OBJS="$OBJS $OUT/$(basename $x).o"
done
LIBS=$(sed -e 's/^.*-o ..\/..\/TraceRecorderTests.bin.aarch64//' -e "s#../../cargo/release/libohpl.a#$OHPL#" "$B/core2/tests/CMakeFiles/TraceRecorderTests.dir/link.txt")
cd "$B/core2/tests"
W1=_ZN3hpl17cGpuShaderManager12CreateShaderERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEENS_14eGpuShaderTypeEPNS_19cParserVarContainerE
c++ -fno-strict-aliasing -mcpu=apple-m2+crc+aes+sha3+fp16 -O2 -g $OBJS -Wl,--wrap=$W1 -o "$OUT/$SRC" $LIBS
