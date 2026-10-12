#!/usr/bin/env bash
# Build the game binary, (re)create the scratch test dir, deploy the binary.
# Usage: eval "$(scripts/soma-init.sh [soma|rebirth|bunker|amnesia|amfp])"   - prints export lines on stdout.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${OPENHPL_BUILD_DIR:-$REPO/amnesia/src/build}"
eval "$("$REPO/scripts/ohpl_game.py" "${1:-${OHPL_GAME:-}}")"

make -C "$BUILD_DIR" -j"$(nproc)" "${BIN%.bin.aarch64}" >&2

if [ ! -d "$SCRATCH" ]; then
	"$REPO/scripts/setup-test-scratch.sh" "$ROOT" "$SCRATCH" >/dev/null
fi
"$REPO/scripts/deploy-test-binary.sh" "$BUILD_DIR/$BIN" "$SCRATCH" >&2

echo "export OHPL_GAME='$GAME'"
echo "export OPENHPL_SCRATCH_ROOT='$(dirname "$SCRATCH")'"
echo "export OPENHPL_${GAME^^}_SCRATCH='$SCRATCH'"
echo "export XDG_CONFIG_HOME='$SCRATCH/.xdg/config'"
echo "export XDG_CACHE_HOME='$SCRATCH/.xdg/cache'"
echo "export XDG_DATA_HOME='$SCRATCH/.xdg/data'"
echo "export XDG_STATE_HOME='$SCRATCH/.xdg/state'"
