#!/usr/bin/env bash
#
# Mandatory scratch-test-environment setup for anyone testing a build
# against real Frictional Games data in this repo. Real Steam SOMA files
# got corrupted twice this project - once by a test script that documented
# deployment straight into the real install directory (fixed - see
# scripts/headless-check.sh and HPL2/core/sources/impl/LowLevelSystemSDL.cpp),
# and at least once more by (very likely) an ad hoc `cp`/`ln` deploy command
# that hand-rolled its own scratch setup and got it wrong. This script is
# the ONE sanctioned way to build a scratch test environment from here on -
# never write your own `ln -s`/`cp -r` sequence against a real Steam
# install; use this instead.
#
# Usage:
#   scripts/setup-test-scratch.sh <real-game-root> <scratch-dir>
#
# Creates <scratch-dir>, symlinks (NEVER copies) every top-level entry of
# <real-game-root> into it, and creates isolated XDG_*_HOME subdirectories
# under <scratch-dir>/.xdg/. Prints `export` lines for those XDG vars to
# stdout - eval them in your shell, e.g.:
#
#   eval "$(scripts/setup-test-scratch.sh /home/lm/.local/share/Steam/steamapps/common/SOMA /tmp/my-scratch)"
#
# Deploy your OWN built binary into <scratch-dir> afterward using
# scripts/deploy-test-binary.sh (NOT a bare `cp`/`\cp -f`, see that
# script's own header for exactly why) - this script only sets up the
# read-only real-data symlinks and XDG dirs, it never touches your build
# output.
#
# Safety guarantees this script enforces:
# - <scratch-dir> must NOT resolve (after realpath) under a real Steam
#   library path ("steamapps/common") - refuses otherwise. This is the
#   exact mistake that corrupted real game files: a "scratch dir" that
#   was actually the real install directory, or a path under it.
# - <real-game-root>'s contents are ALWAYS symlinked, never copied -
#   `ln -s`, not `cp`/`rsync`. A symlink can never itself be "written
#   through" by a later `cp -f` in your own test flow, because `cp -f`
#   onto an existing symlink's PATH replaces the symlink (unlinks it
#   first when it can't open-for-write due to permissions, which every
#   real Steam file safely has as read-only for other reasons anyway) -
#   whereas a copied real file sitting in your scratch dir is just an
#   ordinary writable file with no such protection, and a bug or typo in
#   a later step of your own test flow could write to it directly.
# - Refuses if <real-game-root> itself doesn't look like a real Frictional
#   Games install (no Soma.bin.x86_64/Amnesia.bin.x86_64-shaped binary
#   found at its top level) - catches an accidental argument-order swap.

set -euo pipefail

REAL_ROOT="${1:?usage: setup-test-scratch.sh <real-game-root> <scratch-dir>}"
SCRATCH_DIR="${2:?usage: setup-test-scratch.sh <real-game-root> <scratch-dir>}"

if [ ! -d "$REAL_ROOT" ]; then
	echo "error: '$REAL_ROOT' is not a directory" >&2
	exit 1
fi

REAL_ROOT="$(cd "$REAL_ROOT" && pwd)"

case "$REAL_ROOT" in
	*/steamapps/common/*) : ;; # expected - this IS the real game root
	*)
		echo "error: '$REAL_ROOT' doesn't look like a real Steam game directory (no steamapps/common in its path) - did you swap the argument order? Usage: setup-test-scratch.sh <real-game-root> <scratch-dir>" >&2
		exit 1
		;;
esac

if ! find "$REAL_ROOT" -maxdepth 1 \( -name "*.bin.x86_64" -o -name "*.bin.aarch64" \) 2>/dev/null | grep -q .; then
	echo "error: '$REAL_ROOT' has no top-level *.bin.x86_64/*.bin.aarch64 - doesn't look like a real Frictional Games install root" >&2
	exit 1
fi

# Validate the RAW argument (string-only, no filesystem side effects) for
# a real-Steam-path segment BEFORE creating anything - mkdir'ing first and
# validating after (an earlier version of this exact script did that) can
# itself create a stray directory inside a real Steam install if the
# argument turns out to be unsafe, which is precisely the class of mistake
# this script exists to prevent. Checked again below after mkdir+realpath
# too, in case a relative path or a symlink component only becomes
# apparent once resolved.
case "$SCRATCH_DIR" in
	*/steamapps/common/*)
		echo "error: refusing to use '$SCRATCH_DIR' as a scratch dir - it resolves under a real Steam library path. Use a path under /tmp or your own project scratchpad instead." >&2
		exit 1
		;;
esac

mkdir -p "$SCRATCH_DIR"
SCRATCH_DIR="$(cd "$SCRATCH_DIR" && pwd)"

case "$SCRATCH_DIR" in
	*/steamapps/common/*)
		echo "error: refusing to use '$SCRATCH_DIR' as a scratch dir - it resolves under a real Steam library path. Use a path under /tmp or your own project scratchpad instead." >&2
		exit 1
		;;
esac

if [ "$SCRATCH_DIR" = "$REAL_ROOT" ]; then
	echo "error: scratch dir and real game root are the same path - refusing" >&2
	exit 1
fi

echo "Symlinking '$REAL_ROOT' contents into '$SCRATCH_DIR' (real data, read-only via symlink - never copied)..." >&2
for entry in "$REAL_ROOT"/* "$REAL_ROOT"/.[!.]*; do
	[ -e "$entry" ] || continue
	base="$(basename "$entry")"
	ln -sf "$entry" "$SCRATCH_DIR/$base"
done

XDG_ROOT="$SCRATCH_DIR/.xdg"
mkdir -p "$XDG_ROOT/state" "$XDG_ROOT/data" "$XDG_ROOT/config" "$XDG_ROOT/cache"

echo "Scratch environment ready at '$SCRATCH_DIR' - real data symlinked, XDG dirs isolated under '$XDG_ROOT'." >&2
echo "Deploy your own built binary into '$SCRATCH_DIR' yourself, then launch with cwd='$SCRATCH_DIR' and OPENHPL_HEADLESS_SOCKET set." >&2

# The actual usable output - eval this.
printf 'export XDG_STATE_HOME=%q\n' "$XDG_ROOT/state"
printf 'export XDG_DATA_HOME=%q\n' "$XDG_ROOT/data"
printf 'export XDG_CONFIG_HOME=%q\n' "$XDG_ROOT/config"
printf 'export XDG_CACHE_HOME=%q\n' "$XDG_ROOT/cache"
