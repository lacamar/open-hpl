#!/usr/bin/env python3
"""Compare two OHPL_A13A_DUMP files (cpp vs rust). Masked: CombinedObjects/CombinedMesh counters in body names
(allocation order) and the player position (physics settling); light levels agree within 0.05 (lights flicker with
real time, the same C++ build differs by up to 0.013 between runs). Masked differences are counted.
Exit 0 when equal after masking."""
import re
import sys

NAME = re.compile(r"Combined(Objects|Mesh)\d+")
LIGHT = re.compile(r"^(los \d+ \d \d light )(\S+) (\S+)(.*)$")
TOL = 0.05


def load(p):
    with open(p, encoding="latin-1") as f:
        return f.read().split("\n")


def main(a, b):
    la, lb = load(a), load(b)
    if len(la) != len(lb):
        print(f"DIFFER line counts {len(la)} vs {len(lb)}")
        return 1
    masked = 0
    for i, (x, y) in enumerate(zip(la, lb)):
        x, y = NAME.sub("CombinedX", x), NAME.sub("CombinedX", y)
        if x.startswith("== "):
            x, y = x.split(" player=")[0], y.split(" player=")[0]
        mx, my = LIGHT.match(x), LIGHT.match(y)
        if mx and my:
            if mx.group(1) != my.group(1) or mx.group(4) != my.group(4):
                print(f"DIFFER line {i + 1}: {x} | {y}")
                return 1
            for k in (2, 3):
                if abs(float(mx.group(k)) - float(my.group(k))) > TOL:
                    print(f"DIFFER line {i + 1} light: {x} | {y}")
                    return 1
            masked += 1
            continue
        if x != y:
            print(f"DIFFER line {i + 1}: {x} | {y}")
            return 1
    print(f"equal after masking ({masked} light lines within {TOL})")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1], sys.argv[2]))
