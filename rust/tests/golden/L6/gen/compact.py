#!/usr/bin/env python3
"""gen output on stdin -> "<fnv32(path)> <low 32 bits of the mesh hash>" lines (FAIL: "x<log hash>")."""
import sys


def fnv32(s: bytes) -> int:
    h = 0x811C9DC5
    for b in s:
        h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
    return h


for line in sys.stdin.buffer:
    parts = line.rstrip(b"\n").split(b" ")
    if len(parts) >= 3 and parts[-2] == b"FAIL":
        name, val = b" ".join(parts[:-2]), "x" + parts[-1].decode()[-7:]
    else:
        name, val = b" ".join(parts[:-8]), parts[-8].decode()[-8:]
    print(f"{fnv32(name):08x} {val}")
