#!/usr/bin/env python3
"""Compare two OHPL_A7B_DUMP files. Masked: Combined* bodies (their number depends on the map cache state) and
the sticky pod past byte 84 (attached/last body pointers) and the insanity (bytes 8..12) and slime (12..16) check counters (random draws). Rope positions agree within 0.1 (the rope settles differently between runs of the same binary: 0.05 C++ vs C++; digests skip rope lines). Everything else is exact.
Exit 0 when equal after masking; prints a summary line.
`--digest FILE...` prints sha256 of the masked dump (digests.txt holds the C++ baseline's)."""
import re
import sys

COMB = re.compile(r"Combined(Objects|Mesh)\d+")
# critters (LuxCritter_Bug/Spider, rats) walk by rand draws: their bodies enter and leave areas at random frames
CRITTER = re.compile(r"(cockroach|spider|rat)_\d+_Body_\d+:")


NUM = re.compile(r"-?\d+\.\d+")
ROPE_TOL = 0.1


def same(x, y):
    if x == y:
        return True
    if " rope " not in x:
        return False
    fx, fy = NUM.findall(x), NUM.findall(y)
    return NUM.sub("#", x) == NUM.sub("#", y) and len(fx) == len(fy) and all(abs(float(p) - float(q)) <= ROPE_TOL for p, q in zip(fx, fy))


def norm(line):
    m = re.match(r"(\d+) (\S+) t=(\d+) act=(\d) pod=([0-9a-f]*) ov=(\S*) (.*)", line)
    if not m:
        return COMB.sub("Combined", line)
    fr, name, t, act, pod, ov, rest = m.groups()
    if t == "4":
        pod = pod[:168]
    if t == "5":
        pod = pod[:16] + "xxxxxxxx" + pod[24:]
    if t == "8":
        pod = pod[:24] + "xxxxxxxx" + pod[32:]
    items = sorted(i for i in ov.split(",") if i and not COMB.match(i) and not CRITTER.match(i))
    return f"{fr} {name} t={t} act={act} pod={pod} ov={','.join(items)} {rest}"


def load(p):
    with open(p, encoding="latin-1") as f:
        return [norm(l) for l in f.read().split("\n")]


if sys.argv[1] == "--digest":
    import hashlib
    for f in sys.argv[2:]:
        n = [l for l in load(f) if " rope " not in l]
        print(hashlib.sha256("\n".join(n).encode()).hexdigest(), len(n), f.rsplit("/", 1)[-1])
    sys.exit(0)
a, b = load(sys.argv[1]), load(sys.argv[2])
bad = [i for i in range(min(len(a), len(b))) if not same(a[i], b[i])]
if len(a) != len(b) or bad:
    print(f"{len(bad)} differing lines, lengths {len(a)} {len(b)}; first: {a[bad[0]][:200] if bad else ''} | {b[bad[0]][:200] if bad else ''}")
    sys.exit(1)
print(f"lines={len(a)} equal after masking")
