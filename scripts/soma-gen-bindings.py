#!/usr/bin/env python3
"""Generate native bindings for script types that map onto HPL2 classes of the same shape.

For every recovered method of a script type, emit a SOMA_METHOD binding that calls the same-named
method on the HPL2 class, then compile and drop every binding the compiler rejects (different
signature, missing method) until the file builds. What remains is verified by the compiler.

  scripts/soma-gen-bindings.py            # regenerates soma/src/game/SomaScriptGenBindings.cpp
"""
import os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'soma/src/game/SomaScriptGenBindings.cpp')
BUILD = os.path.join(ROOT, 'amnesia/src/build')

# script type -> HPL2 class (hpl namespace)
TYPES = {
    'iCharacterBody': 'iCharacterBody', 'cCamera': 'cCamera', 'iPhysicsWorld': 'iPhysicsWorld',
    'iPhysicsBody': 'iPhysicsBody', 'iPhysicsJoint': 'iPhysicsJoint', 'iCollideShape': 'iCollideShape',
    'iPhysicsJointHinge': 'iPhysicsJointHinge', 'iPhysicsJointSlider': 'iPhysicsJointSlider', 'iPhysicsJointBall': 'iPhysicsJointBall',
    'iPhysicsMaterial': 'iPhysicsMaterial', 'cSurfaceData': 'cSurfaceData',
    'cWorld': 'cWorld', 'iEntity3D': 'iEntity3D', 'cMeshEntity': 'cMeshEntity', 'cSubMeshEntity': 'cSubMeshEntity',
    'iLight': 'iLight', 'cLightPoint': 'cLightPoint', 'cLightSpot': 'cLightSpot', 'cLightBox': 'cLightBox', 'cLightDirectional': 'cLightDirectional',
    'cBillboard': 'cBillboard', 'cParticleSystem': 'cParticleSystem', 'cSoundEntity': 'cSoundEntity',
    'cBoundingVolume': 'cBoundingVolume', 'cResourceVarsObject': 'cResourceVarsObject',
    'cAnimationState': 'cAnimationState', 'cGuiSet': 'cGuiSet', 'iFontData': 'iFontData',
    'cGuiGfxElement': 'cGuiGfxElement', 'cViewport': 'cViewport', 'cNode3D': 'cNode3D', 'cBone': 'cBone',
    'cMesh': 'cMesh', 'cSubMesh': 'cSubMesh', 'cMaterial': 'cMaterial', 'iTexture': 'iTexture',
    'cAction': 'cAction', 'cBeam': 'cBeam', 'cRopeEntity': 'cRopeEntity', 'cFogArea': 'cFogArea', 'cEnvironmentParticles': 'cEnvironmentParticles',
    'iWidget': 'iWidget', 'cWidgetWindow': 'cWidgetWindow', 'cGuiSkin': 'cGuiSkin', 'cAINodeContainer': 'cAINodeContainer', 'cAINode': 'cAINode',
    'cColliderEntity': 'cColliderEntity', 'cCollideData': 'cCollideData', 'cSoundHandler': 'cSoundHandler',
    'cBoneState': 'cBoneState', 'iKeyboard': 'iKeyboard', 'iMouse': 'iMouse', 'iGamepad': 'iGamepad', 'cForceField': 'cForceField',
    'iLowLevelGraphics': 'iLowLevelGraphics', 'cPostEffectComposite': 'cPostEffectComposite',
}

# HPL3 names whose HPL2 spelling differs
RENAME = {'IsTriggered': 'IsTriggerd', 'WasTriggered': 'WasTriggerd', 'BecameTriggered': 'BecameTriggerd',
          'DoubleTriggered': 'DoubleTriggerd', 'SetMaxPushForce': 'SetPushForce', 'GetMaxPushForce': 'GetPushForce',
          'GetFogBrightness': 'GetSecondaryFogBrightness'}  # official binding

# global prefix -> HPL2 object the cFoo_Bar() globals call Bar() on
GLOBALS = {
    'cInput_': 'gpSomaBase->mpEngine->GetInput()', 'cScene_': 'gpSomaBase->mpEngine->GetScene()',
    'cPhysics_': 'gpSomaBase->mpEngine->GetPhysics()', 'cResources_': 'gpSomaBase->mpEngine->GetResources()',
    'cGraphics_': 'gpSomaBase->mpEngine->GetGraphics()', 'cGui_': 'gpSomaBase->mpEngine->GetGui()',
    'cSystem_': 'gpSomaBase->mpEngine->GetSystem()', 'cEngine_': 'gpSomaBase->mpEngine',
    'cSound_': 'gpSomaBase->mpEngine->GetSound()',
    'cMath_': 'cMath::', 'cString_': 'cString::',
}

PRIM = {'void': 'void', 'bool': 'bool', 'int': 'int', 'uint': 'unsigned int', 'float': 'float', 'double': 'double',
        'int8': 'signed char', 'uint8': 'unsigned char', 'int16': 'short', 'uint16': 'unsigned short',
        'int64': 'long long', 'uint64': 'unsigned long long'}
VALUE = {'tString': 'tString', 'tWString': 'tWString', 'cVector2f': 'cVector2f', 'cVector3f': 'cVector3f',
         'cVector2l': 'cVector2l', 'cVector3l': 'cVector3l', 'cColor': 'cColor', 'cMatrixf': 'cMatrixf',
         'cQuaternion': 'cQuaternion', 'cPlanef': 'cPlanef', 'cRect2f': 'cRect2f', 'cRect2l': 'cRect2l'}


def enums():
    e = set()
    for line in open(os.path.join(ROOT, 'soma/data/script_api.txt')):
        f = line.rstrip('\n').split('\t')
        if f[0] == 'E':
            e.add(f[1])
    return e


ENUMS = enums()


def split_params(s):
    out, depth, cur = [], 0, ''
    for ch in s:
        if ch in '(<':
            depth += 1
        elif ch in ')>':
            depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur)
            cur = ''
        else:
            cur += ch
    if cur.strip():
        out.append(cur)
    return [p.strip() for p in out]


def cpp_type(t, is_ret=False):
    """AngelScript type -> C++ type, or None if not mappable."""
    t = re.sub(r'\s+', ' ', t).strip()
    t = re.sub(r'\s*=.*$', '', t)
    const = t.startswith('const ')
    if const:
        t = t[6:].strip()
    ref = ''
    m = re.match(r'^(.*?)\s*&\s*(in|out|inout)?$', t)
    if m:
        t, ref = m.group(1).strip(), '&'
    handle = t.endswith('@')
    if handle:
        t = t[:-1].strip()
    if t in PRIM:
        base = PRIM[t]
        if ref and not const:
            return base + ' &'
        return base
    if t in ENUMS:
        if ref:
            return None
        return 'int' if not is_ret else t  # enum returns cast below
    if t in VALUE:
        base = VALUE[t]
        if handle:
            return None
        if ref:
            return ('const ' if const else '') + base + ' &'
        return base
    if t in TYPES and (handle or ref):
        return TYPES[t] + ' *' if handle else TYPES[t] + ' &'
    return None


def parse(decl):
    d = re.sub(r'\s+', ' ', decl).strip()
    m = re.match(r'^(.*?)\s*([A-Za-z_]\w*)\s*\((.*)\)\s*(const)?\s*$', d)
    if not m:
        return None
    ret, name, params, const = m.group(1), m.group(2), m.group(3), m.group(4)
    if name.startswith('op') or not ret:
        return None
    ps = []
    for p in split_params(params):
        p = re.sub(r'\s*=.*$', '', p).strip()
        toks = p.replace('&', ' & ').replace('@', ' @ ').split()
        # drop the parameter name if present
        if len(toks) > 1 and re.fullmatch(r'[a-z_]\w*', toks[-1]) and toks[-1] not in ('in', 'out', 'inout', 'const'):
            toks = toks[:-1]
        ps.append(' '.join(toks).replace(' & ', '&').replace(' @', '@').replace(' &', '&'))
    return ret, name, ps, bool(const)


def gen():
    api = {}
    cur = None
    for line in open(os.path.join(ROOT, 'soma/data/script_api.txt')):
        f = line.rstrip('\n').split('\t')
        if f[0] == 'O':
            cur = f[1]
        elif f[0] == 'M' and cur in TYPES:
            api.setdefault(cur, []).append(f[1])
    lines = []
    for st, methods in sorted(api.items()):
        cls = TYPES[st]
        seen = set()
        for decl in methods:
            p = parse(decl)
            if not p:
                continue
            ret, name, ps, is_const = p
            rt = cpp_type(ret, True)
            if rt is None:
                continue
            args = [cpp_type(x) for x in ps]
            if any(a is None for a in args):
                continue
            key = (name, tuple(args))
            if key in seen:
                continue
            seen.add(key)
            enum_ret = ret.strip() in ENUMS
            params = ', '.join([f'{cls} *o'] + [f'{a} a{i}' for i, a in enumerate(args)])
            call_args = []
            for i, (a, x) in enumerate(zip(args, ps)):
                xt = re.sub(r'\s*&.*$', '', x.replace('const ', '')).strip()
                call_args.append(f'({xt})a{i}' if xt in ENUMS else f'a{i}')
            call = f'o->{RENAME.get(name, name)}({", ".join(call_args)})'
            if rt == 'void':
                body = f'{{ {call}; }}'
                rdecl = ''
            elif enum_ret:
                body = f'{{ return (int){call}; }}'
                rdecl = ' -> int'
            elif rt.startswith('const ') and rt.endswith('&') and rt[6:-1].strip() in VALUE.values():
                # HPL3 returns a reference where HPL2 may return by value
                body = f'{{ static thread_local {rt[6:-1].strip()} r; r = {call}; return r; }}'
                rdecl = f' -> {rt}'
            else:
                body = f'{{ return {call}; }}'
                rdecl = f' -> {rt}'
            esc = decl.replace('\\', '\\\\').replace('"', '\\"')
            lines.append(f'\tSOMA_METHOD_NEW(e, "{st}", "{esc}", +[]({params}){rdecl} {body});')
    for line in open(os.path.join(ROOT, 'soma/data/script_api.txt')):
        f = line.rstrip('\n').split('\t')
        if f[0] != 'G':
            continue
        p = parse(f[1])
        if not p:
            continue
        ret, name, ps, _ = p
        pre = next((k for k in GLOBALS if name.startswith(k)), None)
        if not pre:
            continue
        rt = cpp_type(ret, True)
        args = [cpp_type(x) for x in ps]
        if rt is None or any(a is None for a in args):
            continue
        call_args = []
        for i, (a, x) in enumerate(zip(args, ps)):
            xt = re.sub(r'\s*&.*$', '', x.replace('const ', '')).strip()
            call_args.append(f'({xt})a{i}' if xt in ENUMS else f'a{i}')
        obj = GLOBALS[pre]
        call = f'{obj}{"" if obj.endswith("::") else "->"}{RENAME.get(name[len(pre):], name[len(pre):])}({", ".join(call_args)})'
        params = ', '.join(f'{a} a{i}' for i, a in enumerate(args))
        if rt == 'void':
            body, rdecl = f'{{ {call}; }}', ''
        elif ret.strip() in ENUMS:
            body, rdecl = f'{{ return (int){call}; }}', ' -> int'
        else:
            body, rdecl = f'{{ return {call}; }}', f' -> {rt}'
        esc = f[1].replace('\\', '\\\\').replace('"', '\\"')
        lines.append(f'\tSOMA_FUNC_NEW(e, "{esc}", +[]({params}){rdecl} {body});')
    # Casts between mapped types: implicit up (static_cast), explicit down (dynamic_cast)
    cur = None
    for line in open(os.path.join(ROOT, 'soma/data/script_api.txt')):
        f = line.rstrip('\n').split('\t')
        if f[0] == 'O':
            cur = f[1]
        elif f[0] == 'C' and cur in TYPES and f[1] in TYPES:
            a, b = TYPES[cur], TYPES[f[1]]
            lines.append(f'\tSOMA_METHOD_NEW(e, "{cur}", "{f[1]}@ opImplCast()", +[]({a} *o) -> {b} * {{ return static_cast<{b} *>(o); }});')
            lines.append(f'\tSOMA_METHOD_NEW(e, "{f[1]}", "{cur}@ opCast()", +[]({b} *o) -> {a} * {{ return dynamic_cast<{a} *>(o); }});')
    return lines


HEADER = '''// Generated by scripts/soma-gen-bindings.py - do not edit by hand.
// Recovered SOMA script methods bound to the same-named HPL2 methods; every line compiles.
#include "SomaScriptBind.h"
#include "SomaScriptNatives.h"
#include "SomaBase.h"

#include "hpl.h"

using namespace hpl;

void RegisterSomaScriptGenBindings(asIScriptEngine *e)
{
'''


def write(lines):
    with open(OUT, 'w') as f:
        f.write(HEADER + '\n'.join(lines) + '\n}\n')


def compile_errors():
    flags = open(os.path.join(BUILD, 'soma_game/CMakeFiles/Soma.dir/flags.make')).read()
    get = lambda k: re.search(rf'^{k} = (.*)$', flags, re.M).group(1)
    cmd = f'g++ -std=c++20 {get("CXX_DEFINES")} {get("CXX_INCLUDES")} -fno-strict-aliasing -O0 -fsyntax-only -fmax-errors=0 {OUT}'
    r = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    bad = set()
    for m in re.finditer(re.escape(os.path.basename(OUT)) + r':(\d+):\d+: error', r.stderr):
        bad.add(int(m.group(1)))
    # errors inside templates point at SomaScriptBind.h; attribute them to the instantiating line
    for m in re.finditer(re.escape(os.path.basename(OUT)) + r':(\d+):\d+:\s+required from here', r.stderr):
        bad.add(int(m.group(1)))
    return bad, r.returncode, r.stderr


lines = gen()
print(len(lines), 'candidate bindings')
first = HEADER.count('\n') + 1
for it in range(12):
    write(lines)
    bad, rc, err = compile_errors()
    if rc == 0:
        break
    idx = {n - first for n in bad if n >= first}
    if not idx:
        print(err[-3000:])
        sys.exit(1)
    lines = [l for i, l in enumerate(lines) if i not in idx]
    print(f'pass {it}: dropped {len(idx)}, {len(lines)} left')
write(lines)
print(len(lines), 'bindings written to', OUT)
