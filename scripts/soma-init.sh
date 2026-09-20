#!/usr/bin/env bash
# Build Soma, (re)create the scratch test dir, deploy the binary.
# Usage: eval "$(scripts/soma-init.sh)"   - prints export lines on stdout.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${OPENHPL_BUILD_DIR:-$REPO/amnesia/src/build}"
SOMA_ROOT="${OPENHPL_SOMA_ROOT:-$HOME/.local/share/Steam/steamapps/common/SOMA}"
SCRATCH="${OPENHPL_SOMA_SCRATCH:-${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/soma-scratch}"

make -C "$BUILD_DIR" -j"$(nproc)" Soma >&2

if [ ! -d "$SCRATCH" ]; then
	"$REPO/scripts/setup-test-scratch.sh" "$SOMA_ROOT" "$SCRATCH" >/dev/null
fi
"$REPO/scripts/deploy-test-binary.sh" "$BUILD_DIR/Soma.bin.aarch64" "$SCRATCH" >&2

echo "export OPENHPL_SOMA_SCRATCH='$SCRATCH'"
echo "export XDG_CONFIG_HOME='$SCRATCH/.xdg/config'"
echo "export XDG_CACHE_HOME='$SCRATCH/.xdg/cache'"
echo "export XDG_DATA_HOME='$SCRATCH/.xdg/data'"
echo "export XDG_STATE_HOME='$SCRATCH/.xdg/state'"
