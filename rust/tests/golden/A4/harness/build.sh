#!/bin/bash
# build.sh [orig|new|both]: differential harness for the A4 enemy logic. orig = the C++ at A4_BASE (default
# rust-port-batch3), new = this tree. Needs the release libohpl.a (CARGO_TARGET_DIR=$A4/cargo cargo build --release in rust/).
set -e
R=$(cd "$(dirname "$0")" && pwd)
W=$(cd "$R/../../../../.." && pwd)
A4=${A4:-$HOME/.cache/open-hpl/port-a4}
H=$A4/harness
LIB=${LIB:-$A4/cargo/release/libohpl.a}
mkdir -p "$H/obj"
cd "$H"
FLAGS="-std=gnu++20 -fno-strict-aliasing -mcpu=apple-m2+crc+aes+sha3+fp16 -O2 -g -DNDEBUG -Wno-switch -Wno-reorder -Wno-unused-variable -Wno-unused-function -Wno-narrowing -fpermissive -DLINUX -w"
COMMON="-I$H -I$R -I$W/amnesia/src/game -I$W/HPL2/core/include"
which=${1:-both}
[ $which = new ] || python3 -I "$R/gen.py" orig "$H"
[ $which = orig ] || python3 -I "$R/gen.py" new "$H"
{
for f in math/Math:Math math/MathTypes:MathTypes system/String:String math/BoundingVolume:BV math/Quaternion:Quat graphics/Color:Color; do
  src=${f%%:*}; o=${f##*:}
  [ -f obj/$o.o ] || echo "g++ $FLAGS $COMMON -c $W/HPL2/core/sources/$src.cpp -o obj/$o.o"
done
python3 -I "$R/genrec.py" "$H" "$W" > /dev/null
echo "g++ $FLAGS $COMMON -DNEWSIDE -DRECORD -c $R/main_rec.cpp -o obj/main_rec.o"
[ $which = new ] || echo "g++ $FLAGS $COMMON -DNEWSIDE -c $R/main_orig.cpp -o obj/main_orig.o"
[ $which = orig ] || echo "g++ $FLAGS $COMMON -DNEWSIDE -c $R/main_new.cpp -o obj/main_new.o"
} | xargs -P3 -I{} sh -c "{}"
C="obj/Math.o obj/MathTypes.o obj/String.o obj/BV.o obj/Quat.o obj/Color.o"
[ $which = new ] || g++ $FLAGS obj/main_orig.o $C $LIB -lSDL2 -Wl,--gc-sections -Wl,--unresolved-symbols=ignore-all -lpthread -ldl -lm -lz -o harness_orig
[ $which = orig ] || g++ $FLAGS obj/main_new.o $C $LIB -lSDL2 -Wl,--gc-sections -Wl,--unresolved-symbols=ignore-all -lpthread -ldl -lm -lz -o harness_new
g++ $FLAGS obj/main_rec.o $C $LIB $(cat wrap.txt) -lSDL2 -Wl,--gc-sections -Wl,--unresolved-symbols=ignore-all -lpthread -ldl -lm -lz -o harness_rec
echo built
