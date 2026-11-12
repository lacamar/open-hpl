#!/usr/bin/env python3
"""Generates the fake GL tables of the L2 differential from rust/src/sys/gl.rs.

usage: gen_fakegl.py <gl.rs> <out fakegl_gen.cpp> <out fakegl.rs>
Both fakes record `name(arg,...)` lines with the same formatting and return the same values, so a
C++ trace and a Rust trace of the same script compare byte for byte.
"""
import re
import sys

src = open(sys.argv[1]).read()
body = src[src.index("gl_table! {"):]
body = body[: body.index("\n}\n")]
funcs = []
for m in re.finditer(r"^\s+(gl\w+)\((.*?)\)(?: -> ([^;]+))?;", body, re.M):
    name, args, ret = m.group(1), m.group(2), m.group(3)
    arglist = []
    if args.strip():
        for a in re.split(r",\s*(?=\w+:)", args.strip()):
            n, t = a.split(":", 1)
            arglist.append((n.strip(), t.strip()))
    funcs.append((name, arglist, ret.strip() if ret else None))

CT = {
    "u32": "unsigned", "i32": "int", "u8": "unsigned char", "f32": "float", "f64": "double",
    "isize": "long", "*const c_void": "const void*", "*mut c_void": "void*",
    "*mut u32": "unsigned*", "*const u32": "const unsigned*", "*mut i32": "int*",
    "*const i32": "const int*", "*mut f32": "float*", "*const f32": "const float*",
    "*const f64": "const double*", "*mut i8": "char*", "*const i8": "const char*",
    "*const *const i8": "const char* const*", "*mut u64": "unsigned long long*",
    "Option<GLDEBUGPROC>": "void*", "*const u8": "const unsigned char*",
}

CPP_BEHAVE = {
    "glGetString": """
	switch(name) {
	case 0x1F00: return (const unsigned char*)"FakeVendor";
	case 0x1F01: return (const unsigned char*)"FakeRenderer";
	case 0x1F02: return (const unsigned char*)"4.5 (Compatibility Profile) Fake";
	case 0x8B8C: return (const unsigned char*)"4.60 Fake";
	case 0x1F03: return (const unsigned char*)FakeExtensions();
	}
	return (const unsigned char*)"";""",
    "glGetIntegerv": "\tif(pname == 0x821D) { *params = FakeExtCount(); return; }\n\t*params = (int)(pname % 31) + 1;",
    "glGetFloatv": "\t*params = 16.0f;",
    "glCreateShader": "\treturn 100 + ++gShaders;",
    "glCreateProgram": "\treturn 200 + ++gPrograms;",
    "glGenTextures": "\tfor(int i = 0; i < n; ++i) textures[i] = ++gGen;",
    "glGenBuffersARB": "\tfor(int i = 0; i < n; ++i) buffers[i] = ++gGen;",
    "glGenFramebuffersEXT": "\tfor(int i = 0; i < n; ++i) framebuffers[i] = ++gGen;",
    "glGenRenderbuffersEXT": "\tfor(int i = 0; i < n; ++i) renderbuffers[i] = ++gGen;",
    "glGenQueries": "\tfor(int i = 0; i < n; ++i) ids[i] = ++gGen;",
    "glGenQueriesARB": "\tfor(int i = 0; i < n; ++i) ids[i] = ++gGen;",
    "glGetUniformLocation": "\treturn 5;",
    "glIsQuery": "\treturn 1;",
    "glGetQueryObjectivARB": "\t*params = pname == 0x8867 ? 1 : 77;",
    "glCheckFramebufferStatusEXT": "\treturn 0x8CD5;",
    "glGetProgramiv": "\t*param = 1;",
    "glGetShaderiv": "\t*param = 1;",
}
CPP_EXTRA = {
    "glLoadMatrixf": "for(int i = 0; i < 16; ++i) extra += \" \" + A(m[i]);",
    "glClipPlane": "for(int i = 0; i < 4; ++i) extra += \" \" + A(equation[i]);",
    "glTexEnvfv": "for(int i = 0; i < 4; ++i) extra += \" \" + A(params[i]);",
    "glGetUniformLocation": "extra += std::string(\" \") + name;",
    "glShaderSource": "extra += \" \" + std::to_string(Fnv(string[0]));",
}

RUST_BEHAVE = {
    "glGetString": """
    EXT.with(|e| match name {
        0x1F00 => c"FakeVendor".as_ptr().cast(),
        0x1F01 => c"FakeRenderer".as_ptr().cast(),
        0x1F02 => c"4.5 (Compatibility Profile) Fake".as_ptr().cast(),
        0x8B8C => c"4.60 Fake".as_ptr().cast(),
        0x1F03 => e.borrow().as_ptr().cast(),
        _ => c"".as_ptr().cast(),
    })""",
    "glGetIntegerv": "    unsafe { *params = (pname % 31) as i32 + 1 };",
    "glGetFloatv": "    unsafe { *params = 16.0 };",
    "glCreateShader": "    100 + bump(&SHADERS)",
    "glCreateProgram": "    200 + bump(&PROGRAMS)",
    "glGenTextures": "    unsafe { gen_ids(n, textures) };",
    "glGenBuffersARB": "    unsafe { gen_ids(n, buffers) };",
    "glGenFramebuffersEXT": "    unsafe { gen_ids(n, framebuffers) };",
    "glGenRenderbuffersEXT": "    unsafe { gen_ids(n, renderbuffers) };",
    "glGenQueries": "    unsafe { gen_ids(n, ids) };",
    "glGenQueriesARB": "    unsafe { gen_ids(n, ids) };",
    "glGetUniformLocation": "    5",
    "glIsQuery": "    1",
    "glGetQueryObjectivARB": "    unsafe { *params = if pname == 0x8867 { 1 } else { 77 } };",
    "glCheckFramebufferStatusEXT": "    0x8CD5",
    "glGetProgramiv": "    unsafe { *param = 1 };",
    "glGetShaderiv": "    unsafe { *param = 1 };",
}
RUST_EXTRA = {
    "glLoadMatrixf": "for i in 0..16 { extra += &format!(\" {}\", unsafe { *m.add(i) }.fmt_arg()); }",
    "glClipPlane": "for i in 0..4 { extra += &format!(\" {}\", unsafe { *equation.add(i) }.fmt_arg()); }",
    "glTexEnvfv": "for i in 0..4 { extra += &format!(\" {}\", unsafe { *params.add(i) }.fmt_arg()); }",
    "glGetUniformLocation": "extra += &format!(\" {}\", unsafe { std::ffi::CStr::from_ptr(name.cast()) }.to_string_lossy());",
    "glShaderSource": "extra += &format!(\" {}\", fnv(unsafe { std::ffi::CStr::from_ptr((*string).cast()) }.to_bytes()));",
}

RRET = {None: "", "u32": " -> u32", "i32": " -> i32", "u8": " -> u8", "*const u8": " -> *const u8"}
RDEF = {None: "", "u32": "0", "i32": "0", "u8": "0", "*const u8": "core::ptr::null()"}
CRDEF = {None: "", "u32": "return 0;", "i32": "return 0;", "u8": "return 0;", "*const u8": "return 0;"}

# ---- C++ ----
cpp = ["// generated by gen_fakegl.py; do not edit", '#include "fakegl.h"', "", 'extern "C" {', ""]
tab = []
for name, args, ret in funcs:
    cargs = ", ".join(f"{CT[t]} {n}" for n, t in args)
    cret = "void" if ret is None else CT[ret]
    anames = ", ".join(f"A({n})" for n, _ in args)
    extra = CPP_EXTRA.get(name, "")
    cpp.append(f"{cret} {name}({cargs})")
    cpp.append("{")
    cpp.append("\tstd::string extra;")
    if extra:
        cpp.append("\t" + extra)
    cpp.append(f'\tRec("{name}", {{{anames}}}, extra);')
    if name in CPP_BEHAVE:
        cpp.append(CPP_BEHAVE[name])
    elif ret is not None:
        cpp.append("\t" + CRDEF[ret])
    cpp.append("}")
    cpp.append("")
    tab.append(f'\t{{"{name}", (void*)&{name}}},')
cpp.append("}")
cpp.append("")
cpp.append("const FakeEntry gFakeGlTable[] = {")
cpp += tab
cpp.append("\t{NULL, NULL}")
cpp.append("};")
open(sys.argv[2], "w").write("\n".join(cpp) + "\n")

# ---- Rust ----
rs = ["// generated by gen_fakegl.py; do not edit", "#![allow(non_snake_case, clippy::missing_safety_doc, clippy::too_many_arguments)]", "use super::fake::*;", "use core::ffi::c_void;", "use crate::sys::gl::{GLDEBUGPROC, GlFns};", ""]
for name, args, ret in funcs:
    rargs = ", ".join(f"{n}: {t}" for n, t in args)
    anames = ", ".join(f"{n}.fmt_arg()" for n, _ in args)
    extra = RUST_EXTRA.get(name, "")
    rs.append(f"pub unsafe extern \"C\" fn {name}({rargs}){RRET[ret]} {{")
    rs.append("    let mut extra = String::new();" if extra else "    let extra = String::new();")
    if extra:
        rs.append("    " + extra)
    rs.append(f'    rec("{name}", &[{anames}], &extra);')
    if name in RUST_BEHAVE:
        rs.append(RUST_BEHAVE[name])
    elif ret is not None:
        rs.append("    " + RDEF[ret])
    rs.append("}")
    rs.append("")
rs.append("pub fn table() -> GlFns {")
rs.append("    GlFns {")
for name, _, _ in funcs:
    rs.append(f"        {name}: Some({name}),")
rs.append("    }")
rs.append("}")
rs.append("")
rs.append("pub fn lookup(name: &str) -> *mut c_void {")
rs.append("    match name {")
for name, _, _ in funcs:
    rs.append(f'        "{name}" => {name} as *const () as *mut c_void,')
rs.append("        _ => core::ptr::null_mut(),")
rs.append("    }")
rs.append("}")
open(sys.argv[3], "w").write("\n".join(rs) + "\n")
print(len(funcs), "functions")
