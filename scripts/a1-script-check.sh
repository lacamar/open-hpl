#!/usr/bin/env bash
# Compile every Amnesia TDD and AMFP .hps against the A1 registrations (golden decls, byte-equal to the Rust table by cargo test).
set -euo pipefail
D="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
O="${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/a1-script-check"
S="${STEAM_COMMON:-$HOME/.local/share/Steam/steamapps/common}"
mkdir -p "$O"
g++ -std=c++17 -O1 -I"$D/HPL2/core/include" -o "$O/check" "$D/scripts/a1_script_check.cpp" \
	"$D/HPL2/core/sources/impl/scriptstring.cpp" "$D/HPL2/core/sources/impl/scriptarray.cpp" -langelscript -lpthread -ldl
"$O/check" "$D/rust/tests/golden/A1/decls.txt" "$S/Amnesia The Dark Descent/maps" "$S/Machine for Pigs/maps"
