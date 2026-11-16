#!/usr/bin/env python3
"""Compare two a1b-trace.sh output dirs map by map: a1b-compare.py <cpp-dir> <rust-dir>.
RandFloat/RandInt results are masked (the engine seeds from the clock); a map whose script branches on a
draw is reported as 'diverges after a random draw' with the first differing line."""
import os
import re
import sys

a_dir, b_dir = sys.argv[1:3]
RAND = re.compile(r"^(Rand(?:Float|Int)\(.*\))=.*$")


def load(p):
    # AmfpStub_* calls were not traced by the first (C++-era) recording
    return [RAND.sub(r"\1=?", l.rstrip("\n")) for l in open(p, errors="replace") if not l.startswith("AmfpStub_")]


bad = 0
for name in sorted(f for f in os.listdir(b_dir) if f.endswith(".txt")):
    pa, pb = os.path.join(a_dir, name), os.path.join(b_dir, name)
    if not os.path.exists(pa):
        continue
    la, lb = load(pa), load(pb)
    if la == lb:
        print(f"same      {name} ({len(la)} calls)")
        continue
    k = next((i for i in range(min(len(la), len(lb))) if la[i] != lb[i]), min(len(la), len(lb)))
    if k == min(len(la), len(lb)):
        print(f"prefix    {name} (runs stopped at different times: {len(la)}/{len(lb)} calls, equal over {k})")
        continue
    rand_before = any(l.startswith("Rand") for l in la[:k])
    print(f"DIFFERENT {name}: first at call {k} ({'after a random draw' if rand_before else 'NO random draw before it'}): "
          f"cpp={la[k] if k < len(la) else '<end>'!r} rust={lb[k] if k < len(lb) else '<end>'!r}")
    bad += 1
sys.exit(1 if bad else 0)
