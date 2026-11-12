#!/usr/bin/env python3
"""Deterministic scenario script for gen.cpp / the Rust replay: python3 gen_script.py > ../script.txt"""
import random

rnd = random.Random(0x4817)
KIND_KEY, KIND_MOUSE_MOTION, KIND_MOUSE_BUTTON, KIND_WHEEL, KIND_PAD_AXIS, KIND_PAD_BUTTON, KIND_PAD_REMOVED = 1, 3, 4, 5, 6, 7, 8
KEY_A, KEY_SPACE = 44, 6
SYMS = [(ord("a") + i, KEY_A + i) for i in range(8)] + [(32, KEY_SPACE)]
RANGES = [0, 1, 2]
THRESH = [(0.0, 1.0), (0.25, 1.0), (0.5, 0.9), (0.1, 0.4)]
out = []


def ev(kind, sym=0, mod=0, state=0, button=0, av=0, x=0, y=0, xrel=0, yrel=0, wheel=0, which=0):
    out.append(f"ev {kind} {sym} {mod} {state} {button} {av} {x} {y} {xrel} {yrel} {wheel} {which}")


def random_events(npads):
    for _ in range(rnd.choice([0, 1, 1, 2, 3, 5])):
        r = rnd.random()
        pad = 100 + rnd.randrange(max(npads, 1) + 1)
        if r < 0.3:
            ev(KIND_KEY, sym=rnd.choice(SYMS)[0], state=rnd.choice([0, 1, 1]))
        elif r < 0.4:
            ev(KIND_MOUSE_BUTTON, state=rnd.choice([0, 1]), button=rnd.randrange(1, 7))
        elif r < 0.45:
            ev(KIND_WHEEL, wheel=rnd.choice([-1, 0, 1]))
        elif r < 0.5:
            ev(KIND_MOUSE_MOTION, x=rnd.randrange(800), y=rnd.randrange(600), xrel=rnd.randrange(-5, 5), yrel=rnd.randrange(-5, 5))
        elif r < 0.75:
            ev(KIND_PAD_AXIS, button=rnd.randrange(6), av=rnd.choice([0, 0, 16384, 32767, -32768, -20000, 9000, -9000, 12000]), which=pad)
        elif r < 0.97:
            ev(KIND_PAD_BUTTON, button=rnd.randrange(21), state=rnd.choice([0, 1]), which=pad)
        else:
            ev(KIND_PAD_REMOVED, which=pad)


def scenario(index):
    npads = rnd.choice([0, 1, 2])
    out.append(f"# scenario {index}")
    out.append(f"new {npads}")
    n_actions = rnd.randrange(3, 9)
    names = []
    for k in range(n_actions):
        name = rnd.choice(["Fwd", "Back", "Use", "Jump", "Lean", "Fwd", "Zoom", "Aim"]) if rnd.random() < 0.8 else f"A{k}"
        names.append(name)
        out.append(f"create {name} {rnd.choice([-1, -1, k, k % 3, 7])}")
    for k in range(n_actions):
        for _ in range(rnd.randrange(0, 4)):
            t = rnd.choice(["key", "key", "mouse", "padbtn", "padaxis", "padhat"])
            pad = rnd.randrange(3)
            if t == "key":
                out.append(f"bind {k} key {rnd.choice(SYMS)[1]}")
            elif t == "mouse":
                out.append(f"bind {k} mouse {rnd.randrange(9)}")
            elif t == "padbtn":
                out.append(f"bind {k} padbtn {pad} {rnd.randrange(21)}")
            elif t == "padaxis":
                lo, hi = rnd.choice(THRESH)
                out.append(f"bind {k} padaxis {pad} {rnd.randrange(6)} {rnd.choice(RANGES)} {lo} {hi}")
            else:
                out.append(f"bind {k} padhat {pad} {rnd.randrange(4)} {rnd.choice([0, 1, 2, 4, 8, 3])}")
        out.append(f"names {k}")
    alive = list(range(n_actions))
    for frame in range(rnd.randrange(30, 70)):
        random_events(npads)
        out.append(f"update {rnd.choice([0.016, 0.016, 0.033, 0.1, 0.4])}")
        r = rnd.random()
        if r < 0.5:
            out.append("qa")
        elif r < 0.7:
            out.append(f"q {rnd.choice(alive)}")
        elif r < 0.78:
            out.append(f"qn {rnd.choice(names + ['Nope'])}")
        elif r < 0.84:
            out.append(f"qi {rnd.choice([-1, 0, 1, 2, 7, 9])}")
        elif r < 0.88:
            out.append(f"getn {rnd.choice(names + ['Nope'])}")
        elif r < 0.9:
            out.append(f"getid {rnd.choice([-1, 0, 1, 2, 7, 9])}")
        elif r < 0.93:
            out.append("any")
            out.append("sub")
        elif r < 0.95:
            out.append(rnd.choice(["reset", f"resetone {rnd.choice(alive)}"]))
            out.append("qa")
        elif r < 0.97:
            out.append(f"clear {rnd.choice(alive)}")
        elif r < 0.99:
            k = rnd.choice(alive)
            out.append(f"destroy {k}")
            alive.remove(k)
            if not alive:
                break
        else:
            out.append(f"refresh {rnd.choice([0, 1, 2])}")
    out.append("qa")


for i in range(40):
    scenario(i)
print("\n".join(out))
