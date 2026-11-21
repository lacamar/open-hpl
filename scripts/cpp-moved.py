#!/usr/bin/env python3
"""Share of removed C/C++ lines that reappear verbatim as added C/C++ lines (moved, not ported).

Usage: scripts/cpp-moved.py <base> [<rev>]   (rev defaults to HEAD; exit 1 above 20 %)
"""
import collections
import subprocess
import sys

base = sys.argv[1]
rev = sys.argv[2] if len(sys.argv) > 2 else "HEAD"
diff = subprocess.run(
    ["git", "diff", "-U0", "--no-color", base, rev, "--", "*.cpp", "*.h", "*.inl", "*.c", ":!HPL2/dependencies"],
    capture_output=True, text=True, errors="replace", check=True,
).stdout


def substantive(line):
    s = line[1:].strip()
    return len(s) > 25 and not s.startswith(("//", "*", "/*", "#include"))


removed, added = collections.Counter(), collections.Counter()
for line in diff.splitlines():
    if line.startswith(("---", "+++")):
        continue
    if line.startswith("-") and substantive(line):
        removed[line[1:].strip()] += 1
    elif line.startswith("+") and substantive(line):
        added[line[1:].strip()] += 1

total = sum(removed.values())
moved = sum(min(n, added[l]) for l, n in removed.items())
share = moved / total if total else 0.0
print(f"{moved}/{total} removed substantive lines reappear as added C/C++ ({share:.0%})")
sys.exit(1 if share > 0.20 else 0)
