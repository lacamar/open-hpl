#!/usr/bin/env python3
"""usage: record.py <out dir> <golden dir>

Golden trace = the C++ path trace restricted to the calls the Rust path makes inside ohpl_glctx_*
(Rust-path markers), so the replay compares against what the C++ emitted for those calls.
"""
import sys

out, gold = sys.argv[1], sys.argv[2]
for s in "AB":
    cpp = open(f"{out}/cpp{s}.txt").read().splitlines()
    rust = open(f"{out}/rust{s}.txt").read().splitlines()
    keep, inside, ci = [], False, 0
    for line in rust:
        if line == "<":
            inside = True
        elif line == ">":
            inside = False
        elif line == "glGetString(7939)":
            continue
        else:
            assert cpp[ci] == line, (ci, cpp[ci], line)
            if inside:
                keep.append(line)
            ci += 1
    assert ci == len(cpp)
    open(f"{gold}/trace_{s}.txt", "w").write("\n".join(keep) + "\n")
    open(f"{gold}/ffi_{s}.txt", "w").write(open(f"{out}/rust{s}.ffi").read())
    # caps lines come from the harness, not from GL calls
    caps = [l for l in cpp if l.startswith("CAPS ") or (l.startswith("cap") and "=" in l and l[3].isnumeric())]
    open(f"{gold}/caps_{s}.txt", "w").write("\n".join(caps) + "\n")
    print(s, len(keep), "golden lines")
