#!/usr/bin/env bash
# Usage: gen.sh <outdir>  (regenerates the L1 golden vectors from the pre-L1 C++ at the base commit)
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
root=$(git -C "$here" rev-parse --show-toplevel)
base=${L1_BASE:-bf394c5}
out=$(realpath "$1")
tmp=$(mktemp -d "${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/l1-tmp/gen.XXXXXX")
mkdir -p "$tmp/impl"
git -C "$root" show "$base:HPL2/core/include/impl/RustInputSDL.h" >"$tmp/impl/RustInputSDL.h"
g++ -std=c++17 -I"$tmp" -I"$root/HPL2/core/include" -o "$tmp/gen" "$here/gen.cpp"
"$tmp/gen" "$out"
rm -rf "$tmp"
