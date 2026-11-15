#!/usr/bin/env bash
# C/C++ lines still to port (tracked files; vendored HPL2/dependencies, tests and goldens excluded).
# Usage: scripts/cpp-lines.sh [rev]   (default: working tree)
set -euo pipefail
cd "$(dirname "$0")/.."
rev=${1:-}
dirs=(HPL2/core/include HPL2/core/sources amnesia/src/game amnesia/src/launcher amfp/src soma/src cshim)
total=0
for d in "${dirs[@]}"; do
	if [ -n "$rev" ]; then
		files=$(git ls-tree -r --name-only "$rev" -- "$d" | grep -E '\.(cpp|h|hpp|inl|c)$' || true)
		n=$(for f in $files; do git show "$rev:$f"; done | wc -l)
	else
		n=$(git ls-files -- "$d" | grep -E '\.(cpp|h|hpp|inl|c)$' | xargs -d '\n' -r cat | wc -l)
	fi
	printf '%7d %s\n' "$n" "$d"
	total=$((total + n))
done
printf '%7d total\n' "$total"
