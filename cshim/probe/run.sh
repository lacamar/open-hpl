#!/usr/bin/env bash
# AngelScript calling-convention probe: Rust extern "C" fns registered as CDECL / CDECL_OBJFIRST.
set -euo pipefail
D="$(cd "$(dirname "$0")/../.." && pwd)"
O="${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/as-probe"
mkdir -p "$O"
rustc --edition 2024 -O --crate-type staticlib -C panic=abort -o "$O/libas_probe.a" "$D/cshim/probe/as_probe.rs"
g++ -O1 -I"$D/HPL2/core/include" -o "$O/as_probe" "$D/cshim/probe/as_probe.cpp" "$D/HPL2/core/sources/impl/scriptstring.cpp" "$O/libas_probe.a" -langelscript -lpthread -ldl
"$O/as_probe"
