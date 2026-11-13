#!/usr/bin/env python3
"""Compare two OHPL_A7_MAPDUMP files (cpp vs rust). Scripts and loaders draw from the random
generator, so masks apply: the first check interval of Insanity/SlimeDamage areas, timer counts
(scripts arm timers with random times), which random-sound timers are armed after the frames, and the active flag of entities named Rand* (switched at random by map scripts).
Masked differences are counted, not hidden.
Exit 0 when equal after masking."""
import re
import sys

MASK = re.compile(r'(name="mfCheckCollisionCount" val=")[-0-9.e+]*"')
TIMER = re.compile(r"^(timer \S+ \S+ )\S+")


def load(p):
    with open(p, encoding="latin-1") as f:
        lines = f.read().split("\n")
    frames = False
    out = []
    for l in lines:
        if l.startswith("== map"):
            frames = l.endswith(" frames")
        out.append(("F" if frames else "L") + l)
    return out


def norm(line):
    if line.startswith("Ftimer "):
        return "Ftimer"
    if line.startswith("Ltimer "):
        return "L" + TIMER.sub(r"\1X", line[1:])
    if re.search(r"\|area(5|8)\|", line):
        return MASK.sub(r'\1X"', line)
    # entities a script switches at random (RandMask*, RandSound*, ...): active flag
    return re.sub(r"^([LF]\d+\|\d+\|[Rr]and[^|]*\|-?\d+\|)[01]\|", r"\1X|", line)


a, b = load(sys.argv[1]), load(sys.argv[2])
if len(a) != len(b):
    print("length differs: %d vs %d" % (len(a), len(b)))
    sys.exit(1)
soft = 0
for i, (x, y) in enumerate(zip(a, b)):
    if x == y:
        continue
    if norm(x) == norm(y):
        soft += 1
        continue
    j = next((k for k in range(min(len(x), len(y))) if x[k] != y[k]), 0)
    print("first diff line %d: cpp ...%s | rust ...%s" % (i, x[max(0, j - 60):j + 60], y[max(0, j - 60):j + 60]))
    sys.exit(1)
print("same lines=%d areas=%d random-masked=%d" % (len(a), sum(1 for l in a if "|area" in l), soft))
