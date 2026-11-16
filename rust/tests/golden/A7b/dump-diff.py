#!/usr/bin/env python3
"""Compare two OHPL_A7B_DUMP files. Masked: Combined* counters in body names (allocation order, ov lists re-sorted) and
the slime area check counter (pod bytes 12..16, a loader/attack random draw). Everything else is exact.
Exit 0 when equal after masking; prints a summary line."""
import re
import sys

COMB = re.compile(r"Combined(Objects|Mesh)\d+")


def norm(line):
    m = re.match(r"(\d+) (\S+) t=(\d+) act=(\d) pod=([0-9a-f]*) ov=(\S*) (.*)", line)
    if not m:
        return COMB.sub("Combined", line)
    fr, name, t, act, pod, ov, rest = m.groups()
    if t == "8":
        pod = pod[:24] + "xxxxxxxx" + pod[32:]
    items = sorted(COMB.sub("Combined", i) for i in ov.split(",") if i)
    return f"{fr} {name} t={t} act={act} pod={pod} ov={','.join(items)} {rest}"


def load(p):
    with open(p, encoding="latin-1") as f:
        return [norm(l) for l in f.read().split("\n")]


a, b = load(sys.argv[1]), load(sys.argv[2])
bad = [i for i in range(min(len(a), len(b))) if a[i] != b[i]]
if len(a) != len(b) or bad:
    print(f"{len(bad)} differing lines, lengths {len(a)} {len(b)}; first: {a[bad[0]][:200] if bad else ''} | {b[bad[0]][:200] if bad else ''}")
    sys.exit(1)
print(f"lines={len(a)} equal after masking")
