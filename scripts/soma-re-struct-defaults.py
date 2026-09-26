#!/usr/bin/env python3
"""Recover default field values of script value structs from the official binary.

Symbolically runs hpl::Factory_<Type>_Default (and the constructors it calls) over objdump
output: immediate/constant stores into the new object become a byte image, std::string members
(COW pointers to the empty rep) become string slots. Emits a C++ table.

  scripts/soma-re-struct-defaults.py <objdump.asm> <binary> <out.h> Type...
"""
import re, struct, sys

ASM, BIN, OUT = sys.argv[1:4]
TYPES = sys.argv[4:]
data = open(BIN, 'rb').read()
BASE = 0x400000

funcs = {}
cur = None
for line in open(ASM, errors='replace'):
    m = re.match(r'^[0-9a-f]+ <(.*)>:$', line)
    if m:
        cur = m.group(1)
        funcs.setdefault(cur, [])
        continue
    if cur is not None and line.startswith(' '):
        funcs[cur].append(line.rstrip('\n'))

def rd(addr, n):
    o = addr - BASE
    return data[o:o + n]

EMPTY_STRING_REPS = set()

def find(name_prefix):
    for k in funcs:
        if k.startswith(name_prefix):
            return k
    return None

def run(fname, regs, img, strings, depth=0):
    if depth > 8 or fname not in funcs:
        return
    xmm = {}
    for line in funcs[fname]:
        parts = line.split('\t')
        if len(parts) < 2:
            continue
        ins = parts[-1].strip()
        m = re.match(r'(\w+)\s+(.*)$', ins)
        if not m:
            continue
        op, args = m.group(1), m.group(2)
        comment = ''
        if '#' in args:
            args, comment = args.split('#', 1)
        args = args.strip()
        if op == 'ret':
            break
        if op == 'mov' and re.match(r'^(r\w+),(r\w+)$', args):
            d, s = args.split(',')
            if s in regs:
                regs[d] = regs[s]
            continue
        m2 = re.match(r'^lea\s', ins)
        if op == 'lea':
            mm = re.match(r'^(r\w+),\[(r\w+)\+0x([0-9a-f]+)\]$', args)
            if mm and mm.group(2) in regs:
                regs[mm.group(1)] = regs[mm.group(2)] + int(mm.group(3), 16)
            continue
        mm = re.match(r'^(BYTE|WORD|DWORD|QWORD) PTR \[(r\w+)(?:\+0x([0-9a-f]+))?\],(0x[0-9a-f]+|\d+)$', args) if op == 'mov' else None
        if mm and mm.group(2) in regs:
            off = regs[mm.group(2)] + int(mm.group(3) or '0', 16)
            size = {'BYTE': 1, 'WORD': 2, 'DWORD': 4, 'QWORD': 8}[mm.group(1)]
            val = int(mm.group(4), 0)
            if size == 8 and 0x1f00000 < val < 0x2000000 and off != 0:
                strings.add(off)
                continue
            if off < 16 and size == 8:
                continue
            for i in range(size):
                img[off + i] = (val >> (8 * i)) & 0xff
            continue
        if op == 'xorps':
            r = args.split(',')[0]
            xmm[r] = b'\0' * 16
            continue
        if op in ('movss', 'movaps', 'movups', 'movsd', 'movq'):
            a, b = [x.strip() for x in args.split(',', 1)]
            if a.startswith('xmm'):
                if b.startswith('xmm'):
                    xmm[a] = xmm.get(b)
                elif 'rip' in b:
                    addr = int(comment.split()[0], 16)
                    n = 4 if op == 'movss' else 8 if op in ('movsd', 'movq') else 16
                    xmm[a] = rd(addr, n)
                else:
                    xmm[a] = None
            else:
                mm = re.match(r'^(?:\w+ PTR )?\[(r\w+)(?:\+0x([0-9a-f]+))?\]$', a)
                if mm and mm.group(1) in regs and xmm.get(b) is not None:
                    off = regs[mm.group(1)] + int(mm.group(2) or '0', 16)
                    n = 4 if op == 'movss' else 8 if op in ('movsd', 'movq') else 16
                    v = xmm[b][:n].ljust(n, b'\0')
                    for i in range(n):
                        img[off + i] = v[i]
            continue
        if op == 'call':
            mm = re.search(r'<(.*)>', args)
            if not mm:
                continue
            callee = mm.group(1)
            if 'rdi' not in regs:
                continue
            base = regs['rdi']
            if callee.startswith('hpl::cColor::cColor(float, float)'):
                v, a = xmm.get('xmm0'), xmm.get('xmm1')
                if v is not None and a is not None:
                    for k in range(3):
                        for i in range(4):
                            img[base + 4 * k + i] = v[i]
                    for i in range(4):
                        img[base + 12 + i] = a[i]
            elif callee.startswith('hpl::cColor::cColor(float, float, float, float)'):
                for k, r in enumerate(['xmm0', 'xmm1', 'xmm2', 'xmm3']):
                    v = xmm.get(r)
                    if v is not None:
                        for i in range(4):
                            img[base + 4 * k + i] = v[i]
            elif re.match(r'hpl::c\w+::c\w+\(\)', callee) or re.match(r'hpl::i\w+::i\w+\(\)', callee):
                run(callee, {'rdi': base}, img, strings, depth + 1)
            continue

out = ['// Generated by scripts/soma-re-struct-defaults.py from the official binary - do not edit by hand.',
       '#include "SomaImGuiDefaults.h"', '', 'const cSomaStructDefaults gvSomaStructDefaults[] = {']
for t in TYPES:
    f = find('hpl::Factory_%s_Default()' % t)
    if not f:
        print('no default factory for', t, file=sys.stderr)
        continue
    size = 0
    for line in funcs[f]:
        mm = re.search(r'mov\s+edi,0x([0-9a-f]+)', line)
        if mm:
            size = int(mm.group(1), 16)
            break
    img, strings = {}, set()
    run(f, {'rax': 0, 'rbx': 0}, img, strings)
    # object pointer comes from operator new in rax; map rbx/r14 lazily
    img = {k: v for k, v in img.items() if k >= 16 and k < size}
    bytes_ = ''.join('\\x%02x' % img.get(i, 0) for i in range(16, size))
    out.append('\t{"%s", %d, "%s", {%s}},' % (t, size, bytes_, ', '.join(str(s) for s in sorted(strings))))
out.append('};')
out.append('const int glSomaStructDefaultsNum = sizeof(gvSomaStructDefaults) / sizeof(gvSomaStructDefaults[0]);')
open(OUT, 'w').write('\n'.join(out) + '\n')
print('wrote', OUT)
