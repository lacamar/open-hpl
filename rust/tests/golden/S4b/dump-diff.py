#!/usr/bin/env python3
"""Compare OHPL_S4B_DUMP files (C++ vs Rust, same scenario) or a dump against a committed per-tick digest.
Run length varies with load, so ticks are compared over the shorter run minus its last (possibly cut) tick;
every byte is exact except the MASKS below.
  dump-diff.py <a.txt|a.digest> <b.txt>   exit 0 when equal
  dump-diff.py --record <dump.txt>        prints the per-tick digest"""
import hashlib
import os
import re
import sys

# Masked fields hold unseeded RNG draws taken outside agent updates (frame-count dependent): the same C++
# build differs between two runs. Positions of physics-settled bodies differ in the last float digit.
MASKS = {
    "soma-00_02_subway": [r"anim=\S*"],
    "soma-00_03_laboratory": [r"(?<= )(?:pos|vel)=\S+", r"anim=\S*"],
    "rebirth-03_06_ghouls_nest": [r"idle_extra=\S+"],
}


def ticks(path):
    out, cur = [], None
    for line in open(path, encoding="latin-1").read().splitlines():
        if line.startswith("T "):
            cur = []
            out.append(cur)
        elif cur is not None:
            cur.append(line)
    return out[:-1]


def masks_for(path):
    key = re.sub(r"^s4b-|(-(cpp|rust))?(_r\d|_o\d?)?\.(txt|digest)$", "", os.path.basename(path))
    return [re.compile(m) for m in MASKS.get(key, [])]


def masked(tick, masks):
    for m in masks:
        tick = [m.sub("", line) for line in tick]
    return tick


def digest(tick):
    return hashlib.sha1("\n".join(tick).encode("latin-1")).hexdigest()[:16]


def digests(path, masks):
    if path.endswith(".digest"):
        return open(path).read().split()
    return [digest(masked(t, masks)) for t in ticks(path)]


def main(a, b):
    masks = masks_for(b)
    da, db = digests(a, masks), digests(b, masks)
    n = min(len(da), len(db))
    if n < 1:
        print(f"too short ({len(da)} vs {len(db)} ticks)")
        return 1
    for i in range(n):
        if da[i] == db[i]:
            continue
        msg = f"tick {i + 1} differs"
        if not a.endswith(".digest"):
            x, y = masked(ticks(a)[i], masks), masked(ticks(b)[i], masks)
            j = next((k for k, (p, q) in enumerate(zip(x, y)) if p != q), min(len(x), len(y)))
            msg += f" line {j}: {(x[j] if j < len(x) else '-')[:120]!r} != {(y[j] if j < len(y) else '-')[:120]!r}"
        print(msg)
        return 1
    print(f"equal over {n} ticks" + (" (masked)" if masks else ""))
    return 0


if __name__ == "__main__":
    if sys.argv[1] == "--record":
        print("\n".join(digests(sys.argv[2], masks_for(sys.argv[2]))))
        sys.exit(0)
    sys.exit(main(*sys.argv[1:3]))
