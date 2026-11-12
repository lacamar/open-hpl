#!/usr/bin/env python3
"""usage: gen_script.py <out dir> [count per family]

Writes seeded random scripts for l3_trace.cpp: textures (all formats, sizes, types, compressed data,
cube maps, animations), vertex buffers (every element type and format, compile flags, transform,
tangents, copy), framebuffers, shader programs with random uniforms and create/update/destroy
lifetime sequences. Deterministic: the seed is the file name.
"""
import os
import random
import sys

out = sys.argv[1]
count = int(sys.argv[2]) if len(sys.argv) > 2 else 6
os.makedirs(out, exist_ok=True)

FORMATS = list(range(1, 30))
COMPRESSED = [8, 9, 10, 11, 12, 13, 27, 28, 29]
DEPTH = [14, 15, 16]
POW2 = [1, 2, 4, 8, 16, 32, 64]
ODD = [3, 5, 6, 7, 12, 24, 40, 48]
STATUS = [0x8CD5, 0x8CDD, 0x8CD6, 0x8CD7, 0x8CD9, 0x8CDA, 0x8CDB, 0x8CDC, 0]
FVAL = [0.0, 0.5, 1.0, 1.5, 2.0, 3.25, 16.0, 0.125]


def write(name, lines, sets=None):
    with open(f"{out}/{name}.txt", "w") as f:
        if sets:
            f.write(f"# sets: {sets}\n")
        f.write("\n".join(lines) + "\n")


def dims(r, ttype):
    pick = lambda: r.choice(POW2 if r.random() < 0.7 else ODD)
    w = pick()
    if ttype == 0:
        return w, 1, 1
    if ttype == 4:
        return w, pick(), r.choice([1, 2, 4, 8])
    return w, pick(), 1


class Tex:
    def __init__(self, i, r):
        self.i = i
        self.type = r.choice([0, 1, 1, 1, 2, 3, 4])
        self.usage = 1 if r.random() < 0.2 else 0
        self.fmt = r.choice(DEPTH if self.usage and r.random() < 0.4 else FORMATS)
        self.w, self.h, self.d = dims(r, self.type)
        self.mips = r.choice([0, 1, 1])
        self.down = r.choice([0, 0, 1, 2])
        self.created = False


def tex_ops(r, L, t, anim_ok=True):
    L.append(f"tex_new {t.i} {t.type} {t.usage} {t.mips} {r.choice([0, 0, 1])} {t.down}")
    if r.random() < 0.15:
        L.append(f"fail errors {r.choice([1, 3, 4, 5, 6, 9])}")
    mode = r.choice(["bmp", "bmp", "bmp", "raw", "anim", "cube"])
    nm = r.choice([1, 1, 2, 3, 4])
    if mode == "bmp":
        images = 6 if t.type == 3 and r.random() < 0.85 else r.choice([1, 2])
        L.append(f"tex_bmp {t.i} {t.fmt} {t.w} {t.h} {t.d} {images} {nm}")
    elif mode == "raw":
        L.append(f"tex_raw {t.i} {t.fmt} {t.w} {t.h} {t.d}")
    elif mode == "anim":
        L.append(f"tex_anim {t.i} {r.choice([1, 2, 3, 5])} {t.fmt} {t.w} {t.h} {t.d} {nm}")
    else:
        L.append(f"tex_cube {t.i} {r.choice([6, 6, 6, 4, 7])} {t.fmt} {t.w} {t.h} {nm}")
    L.append(f"fail errors 0")
    t.created = True


def tex_mutate(r, L, t):
    for _ in range(r.randint(2, 7)):
        k = r.choice(["filter", "aniso", "wrap", "cmpmode", "cmpfunc", "red", "mips", "pixels", "time", "update", "next", "prev", "settime", "setraw", "state"])
        if k == "filter": L.append(f"tex_filter {t.i} {r.randint(0, 2)}")
        elif k == "aniso": L.append(f"tex_aniso {t.i} {r.choice(FVAL)}")
        elif k == "wrap": L.append(f"tex_wrap {t.i} {r.choice(['s', 't', 'r', 'str'])} {r.randint(0, 3)}")
        elif k == "cmpmode": L.append(f"tex_cmpmode {t.i} {r.randint(0, 1)}")
        elif k == "cmpfunc": L.append(f"tex_cmpfunc {t.i} {r.randint(0, 1)}")
        elif k == "red": L.append(f"tex_red {t.i} {r.randint(0, 1)}")
        elif k == "mips": L.append(f"tex_mips {t.i}")
        elif k == "pixels": L.append(f"tex_pixels {t.i}")
        elif k == "time": L.append(f"tex_time {t.i} {r.choice(FVAL)} {r.randint(0, 2)}")
        elif k == "update": L.append(f"tex_update {t.i} {r.choice([0.1, 0.5, 1.5, 7.0])}")
        elif k == "next": L.append(f"tex_next {t.i}")
        elif k == "prev": L.append(f"tex_prev {t.i}")
        elif k == "settime": L.append(f"tex_settime {t.i} {r.choice([0.0, 1.0, 2.5, -1.0, 9.0])}")
        elif k == "setraw":
            L.append(f"tex_setraw {t.i} {r.randint(0, 2)} {r.randint(0, 3)} {r.randint(0, 3)} {r.randint(0, 1)} {r.choice(FORMATS)} {r.choice(POW2)} {r.choice(POW2)} {r.choice([1, 2])}")
        else: L.append(f"tex_state {t.i}")


def gen_textures(seed):
    r = random.Random(seed)
    L = []
    texs = [Tex(i, r) for i in range(r.randint(3, 7))]
    for t in texs:
        tex_ops(r, L, t)
        tex_mutate(r, L, t)
    r.shuffle(texs)
    for t in texs[: r.randint(0, len(texs))]:
        L.append(f"tex_free {t.i}")
    return L


def gen_lifetime(seed):
    r = random.Random(seed)
    L = []
    live = {"tex": [], "vb": [], "sh": [], "prog": [], "fb": [], "dsb": []}
    nxt = {k: 0 for k in live}
    for _ in range(r.randint(30, 50)):
        k = r.choice(list(live))
        if live[k] and r.random() < 0.45:
            if k == "sh" and any(True for _ in live["prog"]):
                continue
            if k == "tex" and live["fb"]:
                continue
            if k == "dsb" and live["fb"]:
                continue
            i = live[k].pop(r.randrange(len(live[k])))
            L.append(f"{k}_free {i}")
            continue
        i = nxt[k]
        nxt[k] += 1
        live[k].append(i)
        if k == "tex":
            t = Tex(i, r)
            t.usage = 0
            L.append(f"tex_new {i} {t.type} 0 {t.mips} 0 0")
            L.append(f"tex_raw {i} {r.choice([4, 5, 7])} {t.w} {t.h} {t.d}")
        elif k == "vb":
            L.append(f"vb_new {i} {r.randint(0, 1)} 0 {r.randint(0, 2)} 4 6")
            L.append(f"vb_elem {i} 1 0 3")
            L.append(f"vb_fill {i} 4 6")
            L.append(f"vb_compile {i} 0")
            if r.random() < 0.5:
                L.append(f"vb_update {i} 2 1")
        elif k == "sh":
            L.append(f"sh_new {i} {r.randint(0, 1)}")
            L.append(f"sh_src {i} {r.randint(1, 9)} {r.randint(1, 3)} 0")
        elif k == "prog":
            L.append(f"prog_new {i}")
            L.append(f"prog_link {i}")
        elif k == "fb":
            L.append(f"fb_new {i}")
            L.append(f"fb_compile {i}")
        else:
            L.append(f"dsb_new {i} 16 16 {r.choice([24, 16, 0])} {r.choice([8, 0])}")
    return L


def gen_vb(seed):
    r = random.Random(seed)
    L = []
    use_c = r.random() < 0.4
    nvb = r.randint(2, 4)
    for i in range(nvb):
        L.append(f"vb_new {i} {r.randint(0, 1)} {r.randint(0, 7)} {r.randint(0, 2)} {r.choice([0, 4, 16])} {r.choice([0, 6, 24])}")
        L.append(f"vb_elem {i} 1 0 {r.choice([3, 3, 4])} 0")
        elems = [e for e in (0, 2, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13) if r.random() < 0.5 and (e != 3 or use_c)]
        if 0 in elems and 5 in elems and r.random() < 0.6:
            pass
        for e in elems:
            if e in (2, 3):
                fmt, n = r.choice([(0, 4), (2, 4), (0, 4), (1, 4)])
            elif e == 0:
                fmt, n = 0, 3
            elif 5 <= e <= 9:
                fmt, n = 0, r.choice([2, 3, 4])
            else:
                fmt, n = r.choice([(0, r.randint(1, 4)), (1, r.randint(1, 4)), (2, r.randint(1, 4))])
            L.append(f"vb_elem {i} {e} {fmt} {n} {r.randint(0, 3)}")
        nv = r.choice([3, 4, 8, 12])
        L.append(f"vb_fill {i} {nv} {r.choice([3, 6, 12, 24])}")
        L.append(f"vb_num {i} {r.choice([-1, 3, 6, 12])}")
        L.append(f"vb_compile {i} {r.choice([0, 1])}")
        for _ in range(r.randint(2, 6)):
            k = r.choice(["update", "bind", "draw", "drawidx", "transform", "shadow", "bv", "unbind"])
            if k == "update": L.append(f"vb_update {i} {r.choice([0, 1, 2, 3, 0x10, 0x20, 0xffff, 0xffffffff])} {r.randint(0, 1)}")
            elif k == "bind": L.append(f"vb_bind {i}")
            elif k == "draw": L.append(f"vb_draw {i} {r.randint(0, 8)}")
            elif k == "drawidx": L.append(f"vb_drawidx {i} {r.choice([0, 3, 6, 12])} {r.randint(0, 8)}")
            elif k == "transform": L.append(f"vb_transform {i}")
            elif k == "shadow": L.append(f"vb_shadow {i} {r.randint(0, 1)}")
            elif k == "bv": L.append(f"vb_bv {i}")
            else: L.append(f"vb_unbind {i}")
        if r.random() < 0.7:
            L.append(f"vb_copy {i} {nvb + i} {r.randint(0, 1)} {r.randint(0, 2)} {r.choice([0xffffffff, 2, 0x12, 0x32, 3])}")
            L.append(f"vb_bind {nvb + i}")
            L.append(f"vb_draw {nvb + i} 0")
            L.append(f"vb_unbind {nvb + i}")
            if r.random() < 0.5:
                L.append(f"vb_free {nvb + i}")
    for i in range(nvb):
        if r.random() < 0.6:
            L.append(f"vb_free {i}")
    return L, ("C" if use_c else None)


def gen_fbo(seed):
    r = random.Random(seed)
    L = []
    nt = r.randint(3, 6)
    for i in range(nt):
        t = r.choice([1, 1, 2, 3, 4, 0])
        w, h, d = dims(r, t)
        L.append(f"tex_new {i} {t} 1 {r.randint(0, 1)} 0 0")
        L.append(f"tex_raw {i} {r.choice([4, 5, 7, 14, 15, 16, 22, 26])} {w} {h} {d}")
    for i in range(r.randint(1, 3)):
        L.append(f"dsb_new {i} {r.choice(POW2)} {r.choice(POW2)} {r.choice([24, 24, 16, 32, 0, 8])} {r.choice([8, 0, 0, 1, 4, 24])}")
    for f in range(r.randint(1, 3)):
        L.append(f"fb_new {f}")
        for _ in range(r.randint(2, 6)):
            k = r.randint(0, 7)
            t = r.randrange(nt)
            if k <= 2: L.append(f"fb_tex {f} {k} {r.randint(0, 5)} {t} {r.randint(0, 2)} {r.randint(0, 5)}")
            elif k <= 4: L.append(f"fb_tex {f} {k} 0 {t} {r.randint(0, 2)} {r.randint(0, 5)}")
            elif k == 5: L.append(f"fb_tex {f} {r.randint(0, 4)} {r.randint(0, 5)} -1 0 0")
            elif k == 6: L.append(f"fb_dsb {f} {r.randint(-1, 0)}")
            else: L.append(f"fb_post {f}")
        L.append(f"fail fbstatus {r.choice(STATUS)}")
        L.append(f"fb_compile {f}")
        L.append("fail fbstatus 36053")
        L.append(f"fb_post {f}")
    for f in range(3):
        if any(l.startswith(f"fb_new {f}") for l in L):
            L.append(f"fb_free {f}")
    for i in range(3):
        if any(l.startswith(f"dsb_new {i} ") for l in L):
            L.append(f"dsb_free {i}")
    return L


def gen_glsl(seed):
    r = random.Random(seed)
    L = []
    names = ["uColor", "uMtx", "uArr", "xmissing", "uTex", "uA", "uB"]
    L.append(f"sh_debug {r.randint(0, 1)}")
    for p in range(r.randint(1, 3)):
        sv, sf = 2 * p, 2 * p + 1
        L.append(f"sh_new {sv} 0")
        L.append(f"sh_new {sf} 1")
        if r.random() < 0.3:
            L.append(f"fail compile {r.randint(1, 2)}")
        L.append(f"sh_src {sv} {r.randint(1, 20)} {r.randint(1, 5)} {r.randint(0, 1)}")
        L.append(f"sh_src {sf} {r.randint(1, 20)} {r.randint(1, 5)} {r.randint(0, 1)}")
        for s in range(r.randint(0, 3)):
            L.append(f"sh_sampler {sf} {r.choice(names)} {s}")
        L.append(f"prog_new {p}")
        if r.random() < 0.9:
            L.append(f"prog_shader {p} 0 {sv}")
        if r.random() < 0.9:
            L.append(f"prog_shader {p} 1 {sf}")
        if r.random() < 0.3:
            L.append(f"fail link {r.randint(1, 2)}")
        L.append(f"prog_link {p}")
        L.append(f"prog_link {p}")
        for slot, n in enumerate(names):
            L.append(f"prog_var {p} {slot} {n}")
        for _ in range(r.randint(3, 10)):
            k = r.choice(["set", "set", "set", "bind", "unbind", "var", "asid", "sampler", "dump"])
            if k == "set":
                L.append(f"prog_set {p} {r.randrange(len(names))} {r.choice(['i', 'f', 'v2', 'v3', 'v4', 'arr', 'mtx'])} {r.choice(FVAL)} {r.choice(FVAL)} {r.choice(FVAL)} {r.choice(FVAL)}")
            elif k == "bind": L.append(f"prog_bind {p}")
            elif k == "unbind": L.append(f"prog_unbind {p}")
            elif k == "var": L.append(f"prog_var {p} {r.randrange(len(names))} {r.choice(names)}")
            elif k == "asid": L.append(f"prog_asid {p} {r.randrange(len(names))} {r.choice(names)}")
            elif k == "sampler": L.append(f"prog_sampler {p} {r.choice(names)} {r.randint(0, 7)}")
            else: L.append(f"prog_dump {p}")
        if r.random() < 0.6:
            L.append(f"prog_free {p}")
            L.append(f"sh_free {sv}")
            L.append(f"sh_free {sf}")
    return L


for k in range(count):
    write(f"tex_{k:02d}", gen_textures(1000 + k))
    vb, vb_sets = gen_vb(2000 + k)
    write(f"vb_{k:02d}", vb, vb_sets)
    write(f"fbo_{k:02d}", gen_fbo(3000 + k))
    write(f"glsl_{k:02d}", gen_glsl(4000 + k))
    write(f"life_{k:02d}", gen_lifetime(5000 + k))
