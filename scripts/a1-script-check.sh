#!/usr/bin/env bash
# Compile every Amnesia TDD and AMFP .hps against the A1 registrations (golden decls, byte-equal to the Rust table by cargo test).
# Runs in the built Soma binary, whose script engine carries the Rust array/string add-ons.
set -euo pipefail
D="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
S="${STEAM_COMMON:-$HOME/.local/share/Steam/steamapps/common}"
BIN="${OPENHPL_BUILD_DIR:-$D/amnesia/src/build}/Soma.bin.aarch64"
OPENHPL_A1_CHECK="$D/rust/tests/golden/A1/decls.txt" OPENHPL_A1_DIRS="$S/Amnesia The Dark Descent/maps:$S/Machine for Pigs/maps" "$BIN"
