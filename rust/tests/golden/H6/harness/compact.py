# usage: compact.py <census dir with <kind>.cpp> <out census.txt>
import sys

def fnv32(s):
    h = 0x811C9DC5
    for b in s:
        h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
    return h

out = []
total = 0
fails = 0
for kind in ["msh", "anm", "dae", "daeanim", "fbx", "fbxanim"]:
    for line in open(f"{sys.argv[1]}/{kind}.cpp", "rb"):
        total += 1
        p = line.rstrip(b"\n").split(b" ")
        if p[-2] == b"FAIL":
            fails += 1
            continue
        bones, tracks = int(p[-2]), int(p[-1])
        if bones == 0 and tracks == 0:
            continue
        name = b" ".join(p[:-3])
        out.append(f"{kind} {fnv32(name):08x} {p[-3].decode()[-8:]} {bones} {tracks}")
out.sort()
open(sys.argv[2], "w").write("\n".join(out) + "\n")
print(total, "files,", fails, "failed loads,", len(out), "kept")
