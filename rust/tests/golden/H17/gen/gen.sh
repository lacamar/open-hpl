#!/usr/bin/env bash
# Usage: gen.sh <libohpl.a> <outbin>   (cargo build --release in rust/; no engine build needed)
# Compiles the real cInput/cAction sources against fake devices that wrap the Rust device state.
# Run: OHPL_IMPL=cpp <outbin> script.txt > cpp.txt; OHPL_IMPL=rust <outbin> script.txt > rust.txt; diff both with trace.txt
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
root=${H17_ROOT:-$(cd "$here/../../../../.." && pwd)}
c=$root/HPL2/core
c++ -std=c++17 -O1 -g -fno-strict-aliasing -DUSE_SDL2 -Wno-switch -Wno-reorder -I"$c/include" -I"$c" -isystem /usr/include/SDL2 \
	"$here/gen.cpp" "$c"/sources/input/{Input,Action,ActionKeyboard,ActionMouseButton,ActionGamepadButton,ActionGamepadAxis,ActionGamepadHat,InputDevice,Keyboard,Mouse,Gamepad}.cpp \
	-o "$2" -Wl,--gc-sections "$1" -lpthread -ldl -lm
