#!/usr/bin/env python3
"""Compare two OHPL_A10B_DUMP files (a10b-scenario.sh), old C++ against the Rust classes.

  dump-diff.py boot A B   strict: the pre-menu runs to the main menu, deterministic at the fixed dump step
  dump-diff.py play A B   `start_map` interrupts the pre-menu at a wall-clock frame, so pre-menu frame blocks
                          are dropped; the credits are cut by `quit` at a wall-clock frame, so one side may
                          end with more credits frames than the other

Exit 0 and one summary line when equal."""
import difflib
import re
import sys

PTR = re.compile(r"\bp(\d+)\b")


def renumber(lines):
    """Pointer ids by first appearance, so frames dropped from one side do not shift later ids."""
    ids = {}

    def sub(m):
        if m.group(1) == "0":
            return "p0"
        return "p%d" % ids.setdefault(m.group(1), len(ids) + 1)

    out = []
    for l in lines:
        parts = l.split('"')
        for i in range(0, len(parts), 2):
            parts[i] = PTR.sub(sub, parts[i])
        out.append('"'.join(parts))
    return out


def blocks(lines):
    """[(event line, [op lines])]; lines before the first event form block 0 with an empty event."""
    out = [("", [])]
    for l in lines:
        if l.startswith("ev "):
            out.append((l, []))
        else:
            out[-1][1].append(l)
    return out


def is_draw(blk):
    return blk[0].split(" ")[1:2] in (["pm_draw"], ["ls_draw"], ["cr_draw"], ["de_draw"])


def draws(bs):
    out = {}
    for ev, ops in bs:
        if is_draw((ev, ops)):
            out[ev] = ops
    return out


def tail_start(bs):
    """Index of the first credits update: the credits run is cut by `quit` at a wall-clock frame."""
    for i, blk in enumerate(bs):
        if blk[0].startswith("ev cr_update"):
            return i
    return len(bs)


def flat(bs):
    return [x for ev, ops in bs for x in ([ev] if ev else []) + ops]


def main():
    mode, a, b = sys.argv[1:4]
    la = [l.rstrip("\n") for l in open(a, errors="replace")]
    lb = [l.rstrip("\n") for l in open(b, errors="replace")]
    if mode == "play":
        def norm(ls):
            return [(ev, ops) for ev, ops in blocks(ls) if not ev.startswith(("ev pm_update", "ev pm_draw"))]
        ba, bb = norm(la), norm(lb)
        ba, bb = blocks(renumber(flat(ba))), blocks(renumber(flat(bb)))
    else:
        ba, bb = blocks(la), blocks(lb)
    ka = [(ev, tuple(ops)) for ev, ops in ba]
    kb = [(ev, tuple(ops)) for ev, ops in bb]
    na = [blk for blk in ba if not is_draw(blk)]
    nb = [blk for blk in bb if not is_draw(blk)]
    da, db = draws(ba), draws(bb)
    ka = [(ev, tuple(ops)) for ev, ops in na]
    kb = [(ev, tuple(ops)) for ev, ops in nb]
    sm = difflib.SequenceMatcher(None, ka, kb, autojunk=False)
    skipped = 0
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == "equal":
            continue
        extra = na[i1:i2] + nb[j1:j2]
        in_tail = i1 >= tail_start(na) and j1 >= tail_start(nb)
        credits_tail = mode == "play" and in_tail and tag != "replace" and all(
            ev.startswith(("ev cr_update", "ev cr_draw")) for ev, _ in extra
        )
        if not credits_tail:
            print(f"DIFF {tag} A[{i1}:{i2}] B[{j1}:{j2}]")
            for x, y in zip(flat(na[i1:i2]), flat(nb[j1:j2])):
                if x != y:
                    print(f"  A: {x}\n  B: {y}")
                    break
            else:
                print(f"  A: {flat(na[i1:i2])[:3]}\n  B: {flat(nb[j1:j2])[:3]}")
            return 1
        skipped += len(extra)
    common = 0
    for key in sorted(set(da) & set(db)):
        if da[key] != db[key]:
            print(f"DIFF in {key}")
            for x, y in zip(da[key], db[key]):
                if x != y:
                    print(f"  A: {x}\n  B: {y}")
                    break
            return 1
        common += 1
    print(f"equal: {len(na)} / {len(nb)} blocks, {common} draw frames in both ({len(da)} / {len(db)}), {skipped} trailing credits blocks")
    return 0


sys.exit(main())
