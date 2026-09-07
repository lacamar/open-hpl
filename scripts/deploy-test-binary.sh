#!/usr/bin/env bash
#
# Mandatory way to deploy a freshly-built test binary into a scratch dir
# created by scripts/setup-test-scratch.sh. NEVER use a bare `cp`/`\cp -f`
# for this - a real, confirmed incident (see PORTING_NOTES.md/TASKS.md)
# happened because a scratch dir's symlink farm can point a filename (e.g.
# "Soma.bin.aarch64") at a REAL file that turns out to be owner-writable
# (this happens whenever a *prior* stray build artifact already got left
# in the real install directory under that name - which had itself
# already happened once before this incident). `cp -f` onto an existing
# writable destination opens-and-truncates it IN PLACE rather than
# replacing the destination path - so if that destination is a symlink,
# `cp -f` writes straight through the symlink into whatever it points at.
# For a stray artifact this silently "self-heals" by overwriting garbage
# with your build - but the exact same code path, hitting the real Steam
# install for the first time (e.g. a topical name collision with a real
# shipped file, or simply before anyone notices day one's stray file), is
# how the real Steam game files get corrupted.
#
# This script's whole job is one line of defense: `rm -f` the destination
# (which only ever removes a symlink or a plain file AT that path, never
# touches whatever a symlink might have pointed at) immediately before
# `cp`, so `cp` always creates a brand new regular file in the scratch
# dir - it can never write through a stale symlink again, no matter what
# that symlink resolves to or how its target's permissions are set.
#
# Usage:
#   scripts/deploy-test-binary.sh <built-binary> <scratch-dir>
#
# Deploys <built-binary> into <scratch-dir> under its own basename.
# Refuses (same check as setup-test-scratch.sh) if <scratch-dir> resolves
# under a real Steam library path.

set -euo pipefail

SRC="${1:?usage: deploy-test-binary.sh <built-binary> <scratch-dir>}"
SCRATCH_DIR="${2:?usage: deploy-test-binary.sh <built-binary> <scratch-dir>}"

if [ ! -f "$SRC" ]; then
	echo "error: '$SRC' is not a file" >&2
	exit 1
fi

case "$SCRATCH_DIR" in
	*/steamapps/common/*)
		echo "error: refusing to deploy into '$SCRATCH_DIR' - it resolves under a real Steam library path." >&2
		exit 1
		;;
esac

if [ ! -d "$SCRATCH_DIR" ]; then
	echo "error: '$SCRATCH_DIR' is not a directory - run setup-test-scratch.sh first" >&2
	exit 1
fi

SCRATCH_DIR="$(cd "$SCRATCH_DIR" && pwd)"
case "$SCRATCH_DIR" in
	*/steamapps/common/*)
		echo "error: refusing to deploy into '$SCRATCH_DIR' - it resolves under a real Steam library path." >&2
		exit 1
		;;
esac

DEST="$SCRATCH_DIR/$(basename "$SRC")"

# The one line that matters: unlink the destination PATH (never follows
# into whatever it might point at) before copying, so cp always creates a
# fresh regular file here rather than ever writing through an existing
# symlink.
rm -f "$DEST"
cp "$SRC" "$DEST"
chmod +x "$DEST"

echo "Deployed '$SRC' -> '$DEST' (destination unlinked first - never written through a symlink)." >&2
