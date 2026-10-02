#!/usr/bin/env bash
# Usage: eval "$(scripts/setup-test-scratch.sh <real-game-root> <scratch-dir>)"
# Symlinks (never copies) the real install into <scratch-dir>, isolates XDG dirs under
# <scratch-dir>/.xdg and prints their exports. Deploy with scripts/deploy-test-binary.sh.
# Real Steam data was corrupted twice by hand-rolled scratch setups: never write under steamapps/common.
set -euo pipefail

REAL_ROOT="${1:?usage: setup-test-scratch.sh <real-game-root> <scratch-dir>}"
SCRATCH_DIR="${2:?usage: setup-test-scratch.sh <real-game-root> <scratch-dir>}"
die() { echo "error: $*" >&2; exit 1; }

[ -d "$REAL_ROOT" ] || die "'$REAL_ROOT' is not a directory"
REAL_ROOT="$(cd "$REAL_ROOT" && pwd)"
case "$REAL_ROOT" in */steamapps/common/*) ;; *) die "'$REAL_ROOT' is not under steamapps/common - argument order swapped?" ;; esac
find "$REAL_ROOT" -maxdepth 1 \( -name "*.bin.x86_64" -o -name "*.bin.aarch64" \) | grep -q . ||
	die "'$REAL_ROOT' has no top-level *.bin.x86_64/*.bin.aarch64"

# checked before mkdir too, so an unsafe argument never creates a directory
case "$SCRATCH_DIR" in */steamapps/common/*) die "refusing scratch dir '$SCRATCH_DIR' under steamapps/common" ;; esac
mkdir -p "$SCRATCH_DIR"
SCRATCH_DIR="$(cd "$SCRATCH_DIR" && pwd)"
case "$SCRATCH_DIR" in */steamapps/common/*) die "refusing scratch dir '$SCRATCH_DIR' under steamapps/common" ;; esac
[ "$SCRATCH_DIR" != "$REAL_ROOT" ] || die "scratch dir is the real game root"

for entry in "$REAL_ROOT"/* "$REAL_ROOT"/.[!.]*; do
	[ -e "$entry" ] || continue
	base="$(basename "$entry")"
	# a shared real hpl.log deadlocked concurrent engines; stray aarch64 binaries aren't game data
	case "$base" in
		hpl*.log) continue ;;
		OpenHpl*.bin.aarch64) ;;
		*.bin.aarch64) continue ;;
	esac
	ln -sf "$entry" "$SCRATCH_DIR/$base"
done

XDG_ROOT="$SCRATCH_DIR/.xdg"
mkdir -p "$XDG_ROOT/state" "$XDG_ROOT/data" "$XDG_ROOT/config" "$XDG_ROOT/cache"
for v in STATE DATA CONFIG CACHE; do
	printf 'export XDG_%s_HOME=%q\n' "$v" "$XDG_ROOT/${v,,}"
done
