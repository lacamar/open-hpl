#!/bin/bash
# usage: build_l3.sh <repo root> <cmake build dir> <out dir>
# Builds l3_trace against the HPL2 library of the build dir (fake GL from rust/src/sys/gl.rs) and
# writes <out>/replay_gen.rs (copy to src/backend/gl_res/tests/).
set -e
ROOT=$1 B=$2 OUT=$3
H=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$OUT"
python3 "$H/gen_fakegl.py" "$ROOT/rust/src/sys/gl.rs" "$OUT/fakegl_gen.cpp" "$OUT/fakegl.rs"
INC="-I$H -I$B/core2 -I$ROOT/HPL2/core -I$ROOT/HPL2/core/include -I$ROOT/HPL2/dependencies/OALWrapper/include -I/usr/include/AL -I$ROOT/HPL2/dependencies/newton/. -isystem /usr/include/SDL2"
FL="-std=gnu++17 -fno-strict-aliasing -O1 -g -DHPL_VULKAN -DUSE_OALWRAPPER -DUSE_SDL2 -Wno-switch -Wno-reorder -idirafter $ROOT/HPL2/dependencies/include"
python3 "$H/gen_replay_l3.py" "$ROOT/HPL2/core/include/impl/RustGlRes.h" "$OUT/wrap_gen.cpp" "$OUT/replay_gen.rs" "$OUT/wrap.ldflags"
c++ $FL $INC -c "$OUT/wrap_gen.cpp" -o "$OUT/wrap_gen.o"
c++ $FL -I"$H" -c "$OUT/fakegl_gen.cpp" -o "$OUT/fakegl_gen.o"
c++ $FL -I"$H" -c "$H/fakecommon.cpp" -o "$OUT/fakecommon.o"
c++ $FL $INC -c "$H/l3_trace.cpp" -o "$OUT/l3_trace.o"
LIBS=$(sed -e 's/^.*-o ..\/..\/TraceRecorderTests.bin.aarch64//' -e 's/ libHPL2TraceRecorder.a//' "$B/core2/tests/CMakeFiles/TraceRecorderTests.dir/link.txt")
cd "$B/core2/tests"
c++ -fno-strict-aliasing -mcpu=apple-m2+crc+aes+sha3+fp16 -O2 -g "$OUT/l3_trace.o" "$OUT/fakegl_gen.o" "$OUT/fakecommon.o" "$OUT/wrap_gen.o" $(cat "$OUT/wrap.ldflags") -o "$OUT/l3_trace" $LIBS
