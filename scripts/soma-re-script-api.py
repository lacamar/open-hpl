#!/usr/bin/env python3
"""Recover SOMA's AngelScript registration table from the official x86-64 binary.

Static analysis of `objdump -d` output: tracks string literals, asSFuncPtr structs and immediates
through registers/stack slots and records every call into HPL3's script-registration layer.
Output: JSON list of registrations in program order.

  scripts/soma-re-script-api.py <Soma_NoSteam.bin.x86_64> <out.json>
"""
import json, re, struct, subprocess, sys

BIN, OUT = sys.argv[1], sys.argv[2]
data = open(BIN, 'rb').read()

secs = []
for m in re.finditer(r'\]\s+(\S+)\s+\S+\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)',
                     subprocess.run(['readelf', '-S', '-W', BIN], capture_output=True, text=True).stdout):
    secs.append((int(m.group(2), 16), int(m.group(3), 16), int(m.group(4), 16)))

def fileoff(addr):
    for a, o, s in secs:
        if a and a <= addr < a + s:
            return o + addr - a
    return None

def cstr(addr):
    o = fileoff(addr)
    if o is None:
        return None
    e = data.index(b'\0', o)
    try:
        return data[o:e].decode('latin-1')
    except Exception:
        return None

def q(addr):
    o = fileoff(addr)
    return struct.unpack_from('<Q', data, o)[0] if o is not None else None

syms = {}
for line in subprocess.run(['nm', '-C', BIN], capture_output=True, text=True).stdout.splitlines():
    p = line.split(' ', 2)
    if len(p) == 3 and p[0]:
        syms.setdefault(int(p[0], 16), p[2])
by_name = {}
for a, n in syms.items():
    by_name.setdefault(n, a)

def vtable_slots(cls):
    a = by_name.get('vtable for ' + cls)
    if a is None:
        return {}
    out = {}
    for i in range(2, 200):
        f = q(a + 8 * i)
        if f is None or f not in syms:
            break
        out[(i - 2) * 8] = syms[f]
    return out

LOWLEVEL_VT = vtable_slots('hpl::cLowLevelScriptAS')

REG = {}
for full, parts in {'rax': 'eax ax al', 'rbx': 'ebx bx bl', 'rcx': 'ecx cx cl', 'rdx': 'edx dx dl',
                    'rsi': 'esi si sil', 'rdi': 'edi di dil', 'rbp': 'ebp bp bpl', 'rsp': 'esp sp spl'}.items():
    REG[full] = full
    for p in parts.split():
        REG[p] = full
for i in range(8, 16):
    for suf in ('', 'd', 'w', 'b'):
        REG[f'r{i}{suf}'] = f'r{i}'

ARGS = ['rdi', 'rsi', 'rdx', 'rcx', 'r8', 'r9']
CLOBBER = ['rax', 'rcx', 'rdx', 'rsi', 'rdi', 'r8', 'r9', 'r10', 'r11']

# registration entry point -> ordered arg kinds after `this` ('s' string, 'f' funcptr, 'i' int, 'p' pointer)
ENTRY = {
    'hpl::iScriptRegisterAS::RegisterType': 'siii',
    'hpl::iScriptRegisterAS::RegisterTemplate': 's',
    'hpl::iScriptRegisterAS::RegisterFactoryDefault': 'p',
    'hpl::iScriptRegisterAS::RegisterConstructDefault': 'p',
    'hpl::iScriptRegisterAS::RegisterFactory': 'sf',
    'hpl::iScriptRegisterAS::RegisterTemplateFactory': 'f',
    'hpl::iScriptRegisterAS::RegisterListFactory': 'f',
    'hpl::iScriptRegisterAS::RegisterStringFactory': 'f',
    'hpl::iScriptRegisterAS::RegisterConstruct': 'sf',
    'hpl::iScriptRegisterAS::RegisterDestruct': 'p',
    'hpl::iScriptRegisterAS::RegisterRefMangers': 'ff',
    'hpl::iScriptRegisterAS::RegisterRefMangersFunc': 'ff',
    'hpl::iScriptRegisterAS::RegisterCasting': 'sff',
    'hpl::iScriptRegisterAS::RegisterProperty': 'si',
    'hpl::iScriptRegisterAS::RegisterMethod': 'sf',
    'hpl::iScriptRegisterAS::RegisterOperator': 'sf',
    'hpl::iScriptRegisterAS::RegisterOperatorFunc': 'sfi',
    'hpl::iScriptRegisterAS::RegisterFunction': 'sfi',
    'hpl::iScriptRegisterAS::SetGlobalPrefix': 's',
    'hpl::iScriptRegisterAS::RegisterGlobalFunction': 'sf',
    'hpl::iScriptRegisterAS::RegisterGlobalProperty': 'sp',
    'hpl::iScriptUserClassSetup::ScriptSetupClass': 'spp',
    'hpl::iScriptUserClassSetup::ScriptSetupClassRefCount': 'sppff',
    'hpl::iScriptUserClassSetup::ScriptSetupInterface': 's',
    'hpl::iScriptUserClassSetup::ScriptInterfaceMethod': 's',
    'hpl::iScriptUserClassSetup::ScriptDefineClass': 's',
    'hpl::iScriptUserClassSetup::ScriptSetupFactoryDefault': 'p',
    'hpl::iScriptUserClassSetup::ScriptSetupFactory': 'sf',
    'hpl::iScriptUserClassSetup::ScriptSetupMethod': 'sf',
    'hpl::iScriptUserClassSetup::ScriptSetupFunction': 'sf',
    'hpl::iScriptUserClassSetup::ScriptSetupProperty': 'si',
    'hpl::iScriptUserClassSetup::ScriptSetupCast': 'sff',
    'cGlobalScriptFuncs::AddFunc': 'sf',
    'cGlobalScriptFuncs::AddFuncClean': 'sf',
}
# static (non-member) entry points take no `this`
STATIC = {'hpl::iScriptUserClassSetup::ScriptSetupClass', 'hpl::iScriptUserClassSetup::ScriptSetupClassRefCount',
          'hpl::iScriptUserClassSetup::ScriptSetupInterface', 'hpl::iScriptUserClassSetup::ScriptInterfaceMethod',
          'hpl::iScriptUserClassSetup::ScriptDefineClass', 'hpl::iScriptUserClassSetup::ScriptSetupFactoryDefault',
          'hpl::iScriptUserClassSetup::ScriptSetupFactory', 'hpl::iScriptUserClassSetup::ScriptSetupMethod',
          'hpl::iScriptUserClassSetup::ScriptSetupFunction', 'hpl::iScriptUserClassSetup::ScriptSetupProperty',
          'hpl::iScriptUserClassSetup::ScriptSetupCast', 'cGlobalScriptFuncs::AddFunc', 'cGlobalScriptFuncs::AddFuncClean'}
LOWLEVEL_ARGS = {  # iLowLevelScript virtuals (this = lowlevel)
    'RegisterGlobalFunction': 'sf', 'RegisterGlobalProperty': 'sp', 'RegisterInterface': 's',
    'RegisterInterfaceMethod': 'ss', 'RegisterEnum': 's', 'RegisterEnumValue': 'ssi',
    'RegisterClassType': 's', 'RegisterClassFactory': 'ssf', 'RegisterClassMethod': 'ssf',
    'RegisterClassFunction': 'ssf', 'RegisterClassProperty': 'ssi', 'RegisterClassCasting': 'ssff',
}

insn_re = re.compile(r'^\s*([0-9a-f]+):\s+(\S+)\s*(.*)$')
func_re = re.compile(r'^([0-9a-f]+) <(.*)>:$')
mem_re = re.compile(r'(?:(QWORD|DWORD|WORD|BYTE|XMMWORD) PTR )?\[rsp\+?(0x[0-9a-f]+)?\]')


def strip_args(n):
    return re.sub(r'\(.*$', '', n)


def run():
    records = []
    fn = None
    regs = {}
    mem = {}
    asm = subprocess.Popen(['objdump', '-d', '-M', 'intel', '--no-show-raw-insn', '-C', BIN],
                           stdout=subprocess.PIPE, text=True, bufsize=1 << 20)

    def val(op):
        op = op.strip()
        if op in REG:
            return regs.get(REG[op])
        if re.fullmatch(r'0x[0-9a-f]+|\d+', op):
            return ('imm', int(op, 0))
        m = mem_re.fullmatch(op)
        if m:
            return mem.get(int(m.group(2) or '0', 16))
        return None

    def arg(kind, v):
        if v is None:
            return None
        if kind == 's':
            if v[0] == 'stack':
                s = mem.get(v[1])
                return s[1] if s and s[0] == 'str' else None
            if v[0] == 'imm':
                return cstr(v[1])
        if kind == 'f':
            if v[0] == 'stack':
                p = mem.get(v[1])
                kind_b = mem.get(v[1] + 0x20)
                if p and p[0] == 'imm':
                    a = p[1]
                    if a & 1 and a < 0x10000:
                        return {'virtual_offset': a - 1, 'flag': kind_b[1] if kind_b else None}
                    return {'addr': hex(a), 'sym': syms.get(a), 'flag': kind_b[1] if kind_b else None}
            return None
        if kind in 'ip':
            if v[0] == 'imm':
                return v[1] if kind == 'i' else (syms.get(v[1]) or hex(v[1]))
        return None

    for line in asm.stdout:
        line = line.rstrip('\n')
        fm = func_re.match(line)
        if fm:
            fn = fm.group(2)
            regs, mem = {}, {}
            continue
        im = insn_re.match(line)
        if not im or fn is None:
            continue
        op, rest = im.group(2), im.group(3)
        comment = ''
        if '#' in rest:
            rest, comment = rest.split('#', 1)
        ops = [o.strip() for o in re.split(r',(?![^\[]*\])', rest)] if rest.strip() else []
        if op in ('mov', 'movabs') and len(ops) == 2:
            dst, src = ops
            if dst in REG:
                m = re.fullmatch(r'QWORD PTR \[(r\w+)\+?(0x[0-9a-f]+)?\]', src)
                base = regs.get(REG[m.group(1)]) if m and m.group(1) in REG else None
                if m and base and base[0] == 'vtbl':
                    regs[REG[dst]] = ('vfn', base[1], int(m.group(2) or '0', 16))
                elif m and base and base[0] == 'obj':
                    regs[REG[dst]] = ('vtbl', base[1])
                elif (m and m.group(2) == '0x18') or 'mpLowLevelScript' in comment:
                    regs[REG[dst]] = ('obj', 'lowlevel')
                else:
                    regs[REG[dst]] = val(src)
            else:
                mm = mem_re.fullmatch(dst)
                if mm:
                    off = int(mm.group(2) or '0', 16)
                    mem[off] = val(src)
        elif op == 'lea' and len(ops) == 2 and ops[0] in REG:
            mm = re.fullmatch(r'\[rsp\+?(0x[0-9a-f]+)?\]', ops[1])
            if mm:
                regs[REG[ops[0]]] = ('stack', int(mm.group(1) or '0', 16))
            elif 'rip' in ops[1] and comment.strip():
                regs[REG[ops[0]]] = ('imm', int(comment.split()[0], 16))
            else:
                regs[REG[ops[0]]] = None
        elif op in ('movaps', 'movups') and len(ops) == 2 and ops[1].startswith('xmm'):
            mm = mem_re.fullmatch(ops[0])
            if mm:
                off = int(mm.group(2) or '0', 16)
                for k in range(off, off + 16):
                    mem.pop(k, None)
        elif op == 'xor' and len(ops) == 2 and ops[0] == ops[1] and ops[0] in REG:
            regs[REG[ops[0]]] = ('imm', 0)
        elif op == 'call':
            target = rest.strip()
            tm = re.match(r'[0-9a-f]+ <(.*)>', target)
            name = strip_args(tm.group(1)) if tm else None
            argv = [regs.get(r) for r in ARGS]
            if name and 'basic_string' in name and 'char const*' in tm.group(1):
                d = argv[0]
                s = arg('s', argv[1]) if argv[1] and argv[1][0] == 'imm' else None
                if d and d[0] == 'stack' and s is not None:
                    mem[d[1]] = ('str', s)
            elif name in ENTRY:
                kinds = ENTRY[name]
                a = argv if (name in STATIC or 'iScriptRegisterAS' in name) else argv[1:]
                records.append({'fn': fn, 'at': im.group(1), 'call': name.split('::')[-1],
                                'via': name.split('::')[-2],
                                'args': [arg(k, a[i]) for i, k in enumerate(kinds)]})
            elif target in REG and regs.get(REG[target]) and regs[REG[target]][0] == 'vfn':
                slot = LOWLEVEL_VT.get(regs[REG[target]][2], '?')
                meth = strip_args(slot).split('::')[-1]
                if meth in LOWLEVEL_ARGS:
                    kinds = LOWLEVEL_ARGS[meth]
                    records.append({'fn': fn, 'at': im.group(1), 'call': meth, 'via': 'iLowLevelScript',
                                    'args': [arg(k, argv[1 + i]) for i, k in enumerate(kinds)]})
            for r in CLOBBER:
                regs.pop(r, None)
        elif op.startswith('j') or op == 'ret':
            pass
        elif ops and ops[0] in REG:
            regs[REG[ops[0]]] = None
    asm.wait()
    return records


recs = run()
json.dump(recs, open(OUT, 'w'), indent=0)
from collections import Counter
print(len(recs), 'registrations')
for k, v in Counter(r['call'] for r in recs).most_common():
    print(f'{v:6} {k}')
unresolved = sum(1 for r in recs if any(a is None for a in r['args']))
print('with unresolved args:', unresolved)


def group(recs):
    api = {'types': {}, 'enums': {}, 'globals': [], 'global_props': [], 'funcdefs': [], 'orphans': []}
    cur = None
    prefix = ''
    last_fn = None

    def T(name, kind):
        t = api['types'].setdefault(name, {'kind': kind, 'size': 0, 'flags': 0, 'behaviours': [],
                                           'methods': [], 'props': [], 'casts': []})
        if kind != 'ref' or t['kind'] == 'ref':
            t['kind'] = kind
        return t

    def sym(f):
        return (f or {}).get('sym') if isinstance(f, dict) else None

    for r in recs:
        c, a = r['call'], r['args']
        if c in ('RegisterType', 'RegisterTemplate', 'ScriptSetupClass', 'ScriptSetupClassRefCount',
                 'ScriptDefineClass', 'RegisterClassType', 'ScriptSetupInterface', 'RegisterInterface',
                 'RegisterEnum', 'RegisterEnumValue', 'RegisterInterfaceMethod', 'RegisterClassMethod',
                 'RegisterClassFunction', 'RegisterClassProperty', 'RegisterClassFactory') and a[0] is None:
            api['orphans'].append(r)
            continue
        if r['fn'] != last_fn:
            last_fn, cur, prefix = r['fn'], None, ''
        if c == 'RegisterType':
            cur = a[0]
            t = T(cur, 'value' if (a[2] or 0) & 2 else 'ref')
            t['size'], t['flags'] = a[1], a[2]
        elif c == 'RegisterTemplate':
            cur = a[0]
            T(cur, 'template')
        elif c in ('ScriptSetupClass', 'ScriptSetupClassRefCount', 'RegisterClassType'):
            cur = a[0]
            T(cur, 'ref')
        elif c == 'ScriptDefineClass':  # forward declaration; the current class is unchanged
            T(a[0], 'ref')
        elif c in ('ScriptSetupInterface', 'RegisterInterface'):
            cur = a[0]
            T(cur, 'interface')
        elif c == 'ScriptInterfaceMethod':
            api['types'][cur]['methods'].append({'decl': a[0]})
        elif c == 'RegisterInterfaceMethod':
            T(a[0], 'interface')['methods'].append({'decl': a[1]})
        elif c in ('RegisterMethod', 'ScriptSetupMethod', 'RegisterOperator'):
            if cur is None:
                api['orphans'].append(r)
                continue
            api['types'][cur]['methods'].append({'decl': a[0], 'native': sym(a[1])})
        elif c in ('RegisterFunction', 'RegisterOperatorFunc', 'ScriptSetupFunction'):
            if cur is None:
                api['orphans'].append(r)
                continue
            api['types'][cur]['methods'].append({'decl': a[0], 'native': sym(a[1]),
                                                 'objpos': a[2] if len(a) > 2 else None})
        elif c in ('RegisterClassMethod', 'RegisterClassFunction'):
            T(a[0], 'ref')['methods'].append({'decl': a[1], 'native': sym(a[2])})
        elif c in ('RegisterProperty', 'ScriptSetupProperty'):
            if cur is None:
                api['orphans'].append(r)
                continue
            api['types'][cur]['props'].append({'decl': a[0], 'offset': a[1]})
        elif c == 'RegisterClassProperty':
            T(a[0], 'ref')['props'].append({'decl': a[1], 'offset': a[2]})
        elif c in ('RegisterConstruct', 'RegisterFactory', 'ScriptSetupFactory'):
            beh = 'construct' if c == 'RegisterConstruct' else 'factory'
            api['types'][cur]['behaviours'].append({'beh': beh, 'params': a[0], 'native': sym(a[1])})
        elif c == 'RegisterClassFactory':
            T(a[0], 'ref')['behaviours'].append({'beh': 'factory', 'params': a[1], 'native': sym(a[2])})
        elif c in ('RegisterConstructDefault', 'RegisterFactoryDefault', 'ScriptSetupFactoryDefault',
                   'RegisterDestruct', 'RegisterTemplateFactory', 'RegisterListFactory', 'RegisterStringFactory',
                   'RegisterRefMangers', 'RegisterRefMangersFunc'):
            if cur is not None:
                api['types'][cur]['behaviours'].append({'beh': c.replace('Register', '').replace('ScriptSetup', '')})
        elif c in ('RegisterCasting', 'ScriptSetupCast'):
            if cur is not None:
                api['types'][cur]['casts'].append({'other': a[0], 'to_cur': sym(a[1]), 'from_cur': sym(a[2])})
        elif c == 'SetGlobalPrefix':
            prefix = a[0] or ''
        elif c == 'RegisterGlobalFunction':
            d = a[0]
            if prefix and d:
                d = re.sub(r'(\b[A-Za-z_]\w*)\s*\(', lambda m: prefix + '_' + m.group(1) + '(', d, count=1)
            api['globals'].append({'decl': d, 'native': sym(a[1])})
        elif c in ('AddFunc', 'AddFuncClean'):
            d = a[0]
            if c == 'AddFunc' and d:  # cGlobalScriptFuncs::AddFunc registers under the cLux_ prefix
                d = re.sub(r'(\b[A-Za-z_]\w*)\s*\(', lambda m: 'cLux_' + m.group(1) + '(', d, count=1)
            api['globals'].append({'decl': d, 'native': sym(a[1])})
        elif c == 'RegisterGlobalProperty':
            d = a[0]
            if prefix and d and not re.search(r'\b' + prefix + r'_\w+\s*$', d.strip()):
                d = re.sub(r'(\b[A-Za-z_]\w*)\s*$', lambda m: prefix + '_' + m.group(1), d.strip(), count=1)
            api['global_props'].append({'decl': d, 'ptr': a[1]})
        elif c == 'RegisterEnum':
            api['enums'].setdefault(a[0], [])
        elif c == 'RegisterEnumValue':
            api['enums'].setdefault(a[0], []).append([a[1], a[2]])
    return api


if len(sys.argv) > 3:
    api = group(recs)
    json.dump(api, open(sys.argv[3], 'w'), indent=1, sort_keys=True)
    print('types', len(api['types']), 'enums', len(api['enums']), 'globals', len(api['globals']),
          'global props', len(api['global_props']), 'orphans', len(api['orphans']))


def emit_text(api, path):
    """One record per line: <tag>\t<fields...>; the engine's cSomaScriptApi parses this."""
    def clean(x):
        return re.sub(r'\s+', ' ', x).strip() if isinstance(x, str) else x
    for t in api['types'].values():
        for k in ('methods', 'props'):
            for m in t[k]:
                m['decl'] = clean(m['decl'])
        for b in t['behaviours']:
            b['params'] = clean(b.get('params'))
    for g in api['globals'] + api['global_props']:
        g['decl'] = clean(g['decl'])
    with open(path, 'w') as f:
        f.write('# SOMA script API recovered by scripts/soma-re-script-api.py from Soma_NoSteam.bin.x86_64\n')
        for name in sorted(api['enums']):
            f.write(f'E\t{name}\n')
            for v, n in api['enums'][name]:
                f.write(f'V\t{v}\t{n}\n')
        for name in sorted(api['types']):
            t = api['types'][name]
            f.write(f"T\t{name}\t{t['kind']}\t{t['size'] or 0}\t{t['flags'] or 0}\n")
        for name in sorted(api['types']):
            t = api['types'][name]
            f.write(f'O\t{name}\n')
            for b in t['behaviours']:
                if b['beh'] in ('construct', 'factory') and b.get('params') is not None:
                    f.write(f"B\t{b['beh']}\t{b['params']}\t{b.get('native') or ''}\n")
                elif b['beh'] in ('construct', 'factory'):
                    continue
                else:
                    f.write(f"B\t{b['beh']}\t\t\n")
            for m in t['methods']:
                if m['decl']:
                    f.write(f"M\t{m['decl']}\t{m.get('native') or ''}\n")
            for p in t['props']:
                if p['decl']:
                    f.write(f"P\t{p['decl']}\t{p['offset'] if p['offset'] is not None else -1}\n")
            for c in t['casts']:
                if c['other']:
                    f.write(f"C\t{c['other']}\n")
        for g in api['globals']:
            if g['decl']:
                f.write(f"G\t{g['decl']}\t{g.get('native') or ''}\n")
        for g in api['global_props']:
            if g['decl']:
                f.write(f"GP\t{g['decl']}\n")


if len(sys.argv) > 4:
    emit_text(api, sys.argv[4])
