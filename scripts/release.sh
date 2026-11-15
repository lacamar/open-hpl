#!/usr/bin/env bash
# Release: bump spec, commit it, fast-forward master, push branches, build on COPR, and tag only after COPR succeeds.
# Usage: scripts/release.sh <x.y.z> "<changelog bullet>" ["<bullet>" ...]
# Run it as a transient unit; the push hook builds and tests in this tree, so merge nothing until it returns.
set -euo pipefail
cd "$(dirname "$0")/.."
ver=${1:?version}; shift
[ $# -gt 0 ] || { echo "need at least one changelog bullet" >&2; exit 2; }
[[ $ver =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "version must be x.y.z" >&2; exit 2; }
git diff --quiet && git diff --cached --quiet || { echo "tracked changes pending; commit or restore them first" >&2; exit 1; }
git rev-parse -q --verify "refs/tags/v$ver" >/dev/null && { echo "tag v$ver exists" >&2; exit 1; }
branch=$(git rev-parse --abbrev-ref HEAD)
spec=packaging/open-hpl.spec
log=${XDG_CACHE_HOME:-$HOME/.cache}/open-hpl/review/release-$ver.log
mkdir -p "$(dirname "$log")"

entry="* $(LC_ALL=C date '+%a %b %d %Y') lacamar <lchlnm@pm.me> - $ver-1"
for b in "$@"; do entry+=$'\n'"- $b"; done
python3 - "$spec" "$ver" "$entry" <<'EOF'
import re, sys
p, ver, entry = sys.argv[1:]
s = open(p).read()
s = re.sub(r"^(Version:\s+)\S+", lambda m: m.group(1) + ver, s, count=1, flags=re.M)
s = s.replace("%changelog\n", "%changelog\n" + entry + "\n\n", 1)
open(p, "w").write(s)
EOF
git add "$spec"
git commit -q -m "Release $ver" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
git checkout -q master
git merge -q --ff-only "$branch"
git checkout -q "$branch"
echo "pushing master and $branch" | tee "$log"
git push origin master "$branch" >> "$log" 2>&1
echo "building on COPR" | tee -a "$log"
if ! packaging/build-rpm.sh -S --copr lacamar/arm64-misc --copr-wait >> "$log" 2>&1; then
	echo "COPR build failed; no tag created. See $log" >&2
	grep -E 'succeeded|failed|Summary' "$log" | tail -5 >&2
	exit 1
fi
git tag "v$ver"
git push origin "v$ver" >> "$log" 2>&1
grep -E 'succeeded|Summary' "$log" | tail -4
echo "released v$ver; install with: pkexec dnf upgrade -y --refresh open-hpl, then scripts/launcher-smoke.sh <game>"
