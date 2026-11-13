#!/usr/bin/env python3
"""Compare two OHPL_A7_MAPDUMP files. Random draws are masked: the first check interval of
Insanity/SlimeDamage areas, and timer counts after the frames (scripts re-arm timers randomly).
Exit 0 when equal."""
import re
import sys

MASK = re.compile(r'(name="mfCheckCollisionCount" val=")[-0-9.e+]*"')


def load(p):
    with open(p, encoding="latin-1") as f:
        out = []
        frames = False
        for line in f.read().split("\n"):
            if line.startswith("== map"):
                frames = line.endswith(" frames")
            if frames and line.startswith("timer "):
                line = re.sub(r"^(timer \S+ \S+ )\S+", r"\1X", line)
            if re.search(r"\|area(5|8)\|", line):
                line = MASK.sub(r'\1X"', line)
            out.append(line)
        return out


a, b = load(sys.argv[1]), load(sys.argv[2])
if a == b:
    print("same lines=%d areas=%d" % (len(a), sum(1 for l in a if "|area" in l)))
    sys.exit(0)
for i, (x, y) in enumerate(zip(a, b)):
    if x != y:
        j = next((k for k in range(min(len(x), len(y))) if x[k] != y[k]), 0)
        print("first diff line %d: cpp ...%s | rust ...%s" % (i, x[max(0, j - 60):j + 60], y[max(0, j - 60):j + 60]))
        break
else:
    print("length differs: %d vs %d" % (len(a), len(b)))
sys.exit(1)
