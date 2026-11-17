#!/usr/bin/env python3
"""Compare two OHPL_A13A_DUMP files (cpp vs rust). Masked: CombinedObjects/CombinedMesh counters in body names
(allocation order) and the player position (physics settling). Light levels agree within 0.05 (lights flicker with
real time, the same C++ build differs by up to 0.013 between runs); other decimals (ray distances, normals) within
1e-3 (the same C++ build differs by 3e-5 where physics bodies are still settling). Light levels at frame 3 are
not compared: whether the light containers are filled by then follows load timing (0.1 vs 0.55 on amfp 06_factory
with the same binary). Everything else is exact.
Exit 0 when equal after masking."""
import re
import sys

NAME = re.compile(r"Combined(Objects|Mesh)\d+")
SPLIT = re.compile(r"[ ,=]")
LIGHT_TOL, DIST_TOL = 0.05, 1e-3


def load(p):
    with open(p, encoding="latin-1") as f:
        return f.read().split("\n")


def isnum(t):
    return "." in t and re.fullmatch(r"-?\d+\.\d+", t) is not None


def same_line(x, y, light_tol=LIGHT_TOL):
    tx, ty = SPLIT.split(x), SPLIT.split(y)
    if len(tx) != len(ty):
        return False
    light = False
    for a, b in zip(tx, ty):
        if a == "light":
            light = True
        if isnum(a) and isnum(b):
            tol = light_tol if light and x.startswith("los") and a in tx[tx.index("light") + 1 : tx.index("light") + 3] else DIST_TOL
            if abs(float(a) - float(b)) > tol:
                return False
        elif a != b:
            return False
    return True


def main(a, b):
    la, lb = load(a), load(b)
    if len(la) != len(lb):
        print(f"DIFFER line counts {len(la)} vs {len(lb)}")
        return 1
    early = False
    for i, (x, y) in enumerate(zip(la, lb)):
        x, y = NAME.sub("CombinedX", x), NAME.sub("CombinedX", y)
        if x.startswith("== "):
            x, y = x.split(" player=")[0], y.split(" player=")[0]
            early = x.endswith(" frame 3")
        if not same_line(x, y, float("inf") if early else LIGHT_TOL):
            print(f"DIFFER line {i + 1}: {x} | {y}")
            return 1
    print(f"equal after masking ({len(la)} lines, lights within {LIGHT_TOL}, decimals within {DIST_TOL})")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1], sys.argv[2]))
