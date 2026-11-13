//! S2: `SomaScriptGlobals.cpp` natives on the Rust path: script global variables, entity-script
//! forwarding, secret codes and the debug/underwater/log functions. The natives that need engine
//! objects (resources, languages, the world) stay in C++ and are registered between the two halves.

use super::ffi_globals::with_variant;
use super::globals::{
    self, FIELD_B, FIELD_C, FIELD_F, FIELD_I, FIELD_ID, FIELD_M, FIELD_S, FIELD_V2, FIELD_V3,
    get_words, set_words,
};
use super::s2_as::{self, Engine, Frame, Func, Soma, str_bytes};
use super::s2_host;
use std::ffi::c_void;
use std::sync::OnceLock;

const FORWARD_KEY: usize = 0x50b1;
const SCOPE_VAR: u32 = 0;
const SCOPE_ARG: u32 = 1;
const SCOPE_RET: u32 = 2;

#[derive(Clone, Copy)]
enum Ty {
    Str,
    Bool,
    Int,
    Float,
    V2,
    V3,
    Mat,
    Col,
    Id,
}

const TYPES: [(Ty, &str, &str, u32); 9] = [
    (Ty::Str, "String", "const tString &in asVar", FIELD_S),
    (Ty::Bool, "Bool", "bool abX", FIELD_B),
    (Ty::Int, "Int", "int alX", FIELD_I),
    (Ty::Float, "Float", "float afX", FIELD_F),
    (Ty::V2, "Vector2f", "const cVector2f&in avX", FIELD_V2),
    (Ty::V3, "Vector3f", "const cVector3f&in avX", FIELD_V3),
    (Ty::Mat, "Matrix", "const cMatrixf&in a_mtxX", FIELD_M),
    (Ty::Col, "Color", "const cColor&in aX", FIELD_C),
    (Ty::Id, "ID", "tID alX", FIELD_ID),
];

fn bytes<'a>(p: *mut c_void) -> &'a [u8] {
    // SAFETY: the declaration passes a `const tString &in`.
    unsafe { str_bytes(p) }
}

fn words_of<T: Copy>(v: &T) -> [u32; 16] {
    let mut w = [0u32; 16];
    let n = size_of::<T>().min(64);
    // SAFETY: `v` is a plain-old-data value of `n` readable bytes; `w` has room for 64 bytes.
    unsafe {
        std::ptr::copy_nonoverlapping(
            std::ptr::from_ref(v).cast::<u8>(),
            w.as_mut_ptr().cast::<u8>(),
            n,
        )
    };
    w
}

/// The value argument `k` of a setter as the words `SetWords` copies.
fn value_words(ty: Ty, fr: &Frame, k: usize) -> [u32; 16] {
    match ty {
        Ty::Str => [0; 16],
        Ty::Bool => words_of(&u32::from(fr.arg::<bool>(k))),
        Ty::Int => words_of(&fr.arg::<i32>(k)),
        Ty::Float => words_of(&fr.arg::<f32>(k)),
        Ty::V2 => words_of(fr.rf::<[f32; 2]>(k)),
        Ty::V3 => words_of(fr.rf::<[f32; 3]>(k)),
        Ty::Mat => words_of(fr.rf::<[f32; 16]>(k)),
        Ty::Col => words_of(fr.rf::<[f32; 4]>(k)),
        Ty::Id => {
            // `cSomaID`: uint8 at 0, int32 at 4 and 8
            // SAFETY: a by-value `tID` argument is a 12-byte object.
            let p = unsafe { std::slice::from_raw_parts(fr.obj_arg(k).cast::<u8>(), 12) };
            let r = |o: usize| {
                i32::from_ne_bytes(std::array::from_fn(|i| p.get(o + i).copied().unwrap_or(0)))
            };
            words_of(&[i32::from(p.first().copied().unwrap_or(0)), r(4), r(8)])
        }
    }
}

fn set_value(ty: Ty, field: u32, scope: u32, name: &[u8], idx: i32, fr: &Frame, k: usize) {
    if let Ty::Str = ty {
        let s = bytes(fr.ptr_arg(k)).to_vec();
        with_variant(scope, name, idx, |v| v.s = s);
    } else {
        let w = value_words(ty, fr, k);
        with_variant(scope, name, idx, |v| set_words(v, field, &w));
    }
}

fn f32s<const N: usize>(w: &[u32; 16]) -> [f32; N] {
    std::array::from_fn(|i| f32::from_bits(w.get(i).copied().unwrap_or(0)))
}

fn get_value(ty: Ty, field: u32, scope: u32, name: &[u8], idx: i32, fr: &Frame) {
    if let Ty::Str = ty {
        fr.ret_string(&with_variant(scope, name, idx, |v| v.s.clone()));
        return;
    }
    let w = with_variant(scope, name, idx, |v| get_words(v, field));
    let word = |i: usize| w.get(i).copied().unwrap_or(0);
    match ty {
        Ty::Str => {}
        Ty::Bool => fr.ret::<bool>(word(0) != 0),
        Ty::Int => fr.ret::<i32>(word(0) as i32),
        Ty::Float => fr.ret::<f32>(f32::from_bits(word(0))),
        Ty::V2 => fr.ret::<[f32; 2]>(f32s(&w)),
        Ty::V3 => fr.ret::<[f32; 3]>(f32s(&w)),
        Ty::Mat => fr.ret::<[f32; 16]>(f32s(&w)),
        Ty::Col => fr.ret::<[f32; 4]>(f32s(&w)),
        Ty::Id => {
            let mut id = [0u8; 12];
            id[0] = word(0) as u8;
            id[4..8].copy_from_slice(&word(1).to_ne_bytes());
            id[8..12].copy_from_slice(&word(2).to_ne_bytes());
            fr.ret::<[u8; 12]>(id);
        }
    }
}

fn type_name(e: Engine, id: i32) -> Vec<u8> {
    e.type_by_id(id).map(|t| t.name()).unwrap_or_default()
}

/// The official natives that set the global args, run an entity script's `_Global_` function and
/// return the global return value.
unsafe extern "C" fn forward(g: *mut c_void) {
    // SAFETY: the engine passes the generic frame of a function registered below.
    unsafe {
        let func = Func(s2_as::ohpl_as2_g_func(g));
        let e = Engine(s2_as::ohpl_as2_g_engine(g));
        let Some((_, class, fname)) = func
            .user(FORWARD_KEY)
            .checked_sub(1)
            .and_then(s2_host::global_func)
        else {
            return;
        };
        let n = s2_as::ohpl_as2_g_arg_count(g);
        let slot = |i: u32| s2_as::ohpl_as2_g_arg_addr(g, i);
        let is_enum = |id: i32| {
            e.type_by_id(id)
                .is_some_and(|t| t.flags() & s2_as::OBJ_ENUM != 0)
        };
        for i in 1..n {
            let id = s2_as::ohpl_as2_g_arg_type_id(g, i);
            let arg = i as i32 - 1;
            let name = type_name(e, id);
            let obj = s2_as::ohpl_as2_g_arg_object(g, i).cast::<u8>();
            let (ty, field, w): (Ty, u32, [u32; 16]) = if id == s2_as::TYPEID_BOOL {
                (
                    Ty::Bool,
                    FIELD_B,
                    words_of(&u32::from(slot(i).cast::<u8>().read() != 0)),
                )
            } else if id == s2_as::TYPEID_FLOAT {
                (Ty::Float, FIELD_F, words_of(&slot(i).cast::<f32>().read()))
            } else if id <= s2_as::TYPEID_DOUBLE || is_enum(id) {
                (Ty::Int, FIELD_I, words_of(&slot(i).cast::<u32>().read()))
            } else if name == b"tString" {
                let s = str_bytes(obj.cast()).to_vec();
                with_variant(SCOPE_ARG, b"", arg, |v| v.s = s);
                continue;
            } else if name == b"cVector3f" {
                (
                    Ty::V3,
                    FIELD_V3,
                    words_of(&obj.cast::<[f32; 3]>().read_unaligned()),
                )
            } else if name == b"cVector2f" {
                (
                    Ty::V2,
                    FIELD_V2,
                    words_of(&obj.cast::<[f32; 2]>().read_unaligned()),
                )
            } else if name == b"cColor" {
                (
                    Ty::Col,
                    FIELD_C,
                    words_of(&obj.cast::<[f32; 4]>().read_unaligned()),
                )
            } else if name == b"tID" {
                let p = std::slice::from_raw_parts(obj, 12);
                let r = |o: usize| {
                    i32::from_ne_bytes(std::array::from_fn(|k| p.get(o + k).copied().unwrap_or(0)))
                };
                (
                    Ty::Id,
                    FIELD_ID,
                    words_of(&[i32::from(p.first().copied().unwrap_or(0)), r(4), r(8)]),
                )
            } else {
                continue;
            };
            let _ = ty;
            with_variant(SCOPE_ARG, b"", arg, |v| set_words(v, field, &w));
        }
        globals::lock().ret = globals::Variant::default();
        let target = str_bytes(slot(0).cast::<*mut c_void>().read());
        s2_host::run_global_func(target, &class, &fname);
        let ret_id = func.return_type().0;
        let ret_name = type_name(e, ret_id);
        let loc = s2_as::ohpl_as2_g_ret_loc(g);
        let w = with_variant(SCOPE_RET, b"", 0, |v| get_words(v, FIELD_I));
        let word = |i: usize| w.get(i).copied().unwrap_or(0);
        if ret_id == s2_as::TYPEID_VOID {
        } else if ret_id == s2_as::TYPEID_BOOL {
            loc.cast::<u8>().write(u8::from(
                with_variant(SCOPE_RET, b"", 0, |v| get_words(v, FIELD_B))[0] != 0,
            ));
        } else if ret_id == s2_as::TYPEID_FLOAT {
            loc.cast::<f32>().write(f32::from_bits(
                with_variant(SCOPE_RET, b"", 0, |v| get_words(v, FIELD_F))[0],
            ));
        } else if ret_id <= s2_as::TYPEID_DOUBLE || is_enum(ret_id) {
            loc.cast::<u32>().write(word(0));
        } else if ret_name == b"tString" {
            let s = with_variant(SCOPE_RET, b"", 0, |v| v.s.clone());
            s2_as::ohpl_as2_str_ctor(loc, s.as_ptr(), s.len());
        } else if ret_name == b"cVector3f" {
            let w = with_variant(SCOPE_RET, b"", 0, |v| get_words(v, FIELD_V3));
            loc.cast::<[f32; 3]>().write_unaligned(f32s(&w));
        } else if ret_name == b"cVector2f" {
            let w = with_variant(SCOPE_RET, b"", 0, |v| get_words(v, FIELD_V2));
            loc.cast::<[f32; 2]>().write_unaligned(f32s(&w));
        }
    }
}

fn secret(key: [u8; 3], x: &[u8]) {
    if globals::secret_code(&mut globals::lock(), key, x) {
        s2_host::run_global_func(b"DescriptionHandler", b"", b"_Global_Add");
    }
}

fn int_text(x: i32) -> Vec<u8> {
    vec![x as u8; 3]
}

pub fn register_before_engine(e: Engine) {
    let s = Soma(e);
    let mut i = 0;
    while let Some((decl, _, _)) = s2_host::global_func(i) {
        let d = super::s2_api::active_native_decl(None, &decl);
        if e.global_func_by_decl(&d).is_none() {
            let r = e.reg_func_raw(&d, forward as *const c_void);
            if r < 0 {
                let w = format!(
                    "SOMA script: could not register forwarding function {} ({r})\n",
                    String::from_utf8_lossy(&decl)
                );
                s2_host::misc(s2_host::MISC_LOG, 1, w.as_bytes());
            } else if let Some(f) = e.func_by_id(r) {
                f.set_user(i + 1, FORWARD_KEY);
            }
        }
        i += 1;
    }
    s.func(
        "void SlideDoor_SetClosed(const tString& in asName, bool abClosed, bool abInstant = false)",
        |fr| {
            let closed = fr.arg::<bool>(1);
            with_variant(SCOPE_ARG, b"", 0, |v| {
                set_words(v, FIELD_F, &words_of(&if closed { 0.0f32 } else { 1.0 }))
            });
            let inst = u32::from(fr.arg::<bool>(2));
            with_variant(SCOPE_ARG, b"", 1, |v| {
                set_words(v, FIELD_B, &words_of(&inst))
            });
            s2_host::run_global_func(
                bytes(fr.ptr_arg(0)),
                b"cScrPropSlideDoor",
                b"_Global_SetOpenAmount",
            );
        },
    );
    s.func("bool cScript_RunGlobalFunc(const tString&in asObjectName, const tString&in asClassName, const tString&in asFuncName)", |fr| {
        fr.ret::<bool>(s2_host::run_global_func(bytes(fr.ptr_arg(0)), bytes(fr.ptr_arg(1)), bytes(fr.ptr_arg(2))));
    });
    for (ty, name, param, field) in TYPES {
        s.func(
            &format!("void cScript_SetGlobalVar{name}(const tString &in asName, {param})"),
            move |fr| {
                set_value(ty, field, SCOPE_VAR, bytes(fr.ptr_arg(0)), 0, fr, 1);
            },
        );
        s.func(
            &format!("void cScript_SetGlobalArg{name}(int alIdx, {param})"),
            move |fr| {
                set_value(ty, field, SCOPE_ARG, b"", fr.arg(0), fr, 1);
            },
        );
        s.func(
            &format!("void cScript_SetGlobalReturn{name}({param})"),
            move |fr| {
                set_value(ty, field, SCOPE_RET, b"", 0, fr, 0);
            },
        );
    }
    let ret_decl = |name: &str| match name {
        "String" => "tString",
        "Bool" => "bool",
        "Int" => "int",
        "Float" => "float",
        "Vector2f" => "cVector2f",
        "Vector3f" => "cVector3f",
        "Matrix" => "cMatrixf",
        "Color" => "cColor",
        _ => "tID",
    };
    for (ty, name, _, field) in TYPES {
        s.func(
            &format!(
                "{} cScript_GetGlobalVar{name}(const tString &in asName)",
                ret_decl(name)
            ),
            move |fr| {
                get_value(ty, field, SCOPE_VAR, bytes(fr.ptr_arg(0)), 0, fr);
            },
        );
    }
    for (ty, name, _, field) in TYPES {
        s.func(
            &format!("{} cScript_GetGlobalArg{name}(int alIdx)", ret_decl(name)),
            move |fr| {
                get_value(ty, field, SCOPE_ARG, b"", fr.arg(0), fr);
            },
        );
    }
    s.func("void cLux_DropPageFocus(const tString&in asX)", |fr| {
        secret([0x1e, 0x28, 0x56], bytes(fr.ptr_arg(0)))
    });
    s.func("void cLux_PreloadWebpage(const tString &in asX)", |fr| {
        secret([0x53, 0x58, 0x15], bytes(fr.ptr_arg(0)))
    });
    s.func("void cLux_SetAreaOffline(int alX)", |fr| {
        secret([0x40, 0x48, 0x21], &int_text(fr.arg(0)))
    });
    s.func("void cLux_RegisterCollisionRadius(int alX)", |fr| {
        secret([0x62, 0x71, 0x60], &int_text(fr.arg(0)))
    });
    s.func("void cLux_ResetShudderEffects(int alX)", |fr| {
        secret([0x4e, 0x41, 0x53], &int_text(fr.arg(0)))
    });
    for (ty, name, _, field) in TYPES {
        let decl = if name == "String" {
            "const tString& cScript_GetGlobalReturnString()".to_string()
        } else {
            format!("{} cScript_GetGlobalReturn{name}()", ret_decl(name))
        };
        if name == "String" {
            s.func(&decl, |fr| {
                let s = with_variant(SCOPE_RET, b"", 0, |v| v.s.clone());
                let p = return_string();
                // SAFETY: `p` is the leaked, live `std::string` of this native.
                unsafe { s2_as::ohpl_as2_str_assign(p, s.as_ptr(), s.len()) };
                fr.ret_ref(p);
            });
        } else {
            s.func(&decl, move |fr| get_value(ty, field, SCOPE_RET, b"", 0, fr));
        }
    }
}

/// The `static tString sReturn` of `cScript_GetGlobalReturnString`.
fn return_string() -> *mut c_void {
    static P: OnceLock<usize> = OnceLock::new();
    *P.get_or_init(|| {
        let mem = Box::into_raw(Box::new([0u64; 4])).cast::<c_void>();
        // SAFETY: `mem` is 32 aligned bytes, constructed once and never freed.
        unsafe { s2_as::ohpl_as2_str_ctor(mem, b"".as_ptr(), 0) };
        mem as usize
    }) as *mut c_void
}

fn log(level: i32, s: &[u8]) {
    let text: Vec<u8> = s.iter().copied().take_while(|&c| c != 0).collect();
    let tail: &[u8] = if s.last() != Some(&b'\n') { b"\n" } else { b"" };
    s2_host::misc(s2_host::MISC_LOG, level, &[text, tail.to_vec()].concat());
}

fn tagged(tag: &[u8], s: &[u8]) {
    let text: Vec<u8> = s.iter().copied().take_while(|&c| c != 0).collect();
    s2_host::misc(s2_host::MISC_LOG, 0, &[tag, &text, b"\n"].concat());
}

pub fn register_after_engine(e: Engine) {
    let s = Soma(e);
    s.func("bool cLux_ScriptDebugOn()", |fr| fr.ret::<bool>(false));
    s.func("bool cLux_DebugModeOn()", |fr| fr.ret::<bool>(false));
    s.func("bool cLux_GetGodModeActivated()", |fr| {
        static GOD: OnceLock<bool> = OnceLock::new();
        fr.ret::<bool>(*GOD.get_or_init(|| std::env::var_os("OPENHPL_SOMA_GODMODE").is_some()));
    });
    s.func("bool cLux_GetUnderwaterEffectsActive()", |fr| {
        fr.ret::<bool>(s2_host::misc(s2_host::MISC_UNDERWATER_GET, 0, b"") != 0);
    });
    s.func(
        "void cLux_SetUnderwaterEffectsActive(bool abX, bool abUseStartAndEndEffects)",
        |fr| {
            s2_host::misc(
                s2_host::MISC_UNDERWATER_SET,
                i32::from(fr.arg::<bool>(0)),
                b"",
            );
        },
    );
    s.func("void cLux_IncUnderwaterEffectUserCount()", |_| {
        s2_host::misc(s2_host::MISC_UNDERWATER_INC, 0, b"");
    });
    s.func("void cLux_DecUnderwaterEffectUserCount()", |_| {
        s2_host::misc(s2_host::MISC_UNDERWATER_DEC, 0, b"");
    });
    s.func("bool cLux_HasUnderwaterEffectUsers()", |fr| {
        fr.ret::<bool>(s2_host::misc(s2_host::MISC_UNDERWATER_USERS, 0, b"") > 0);
    });
    s.func("void __print(const tString&in asText)", |fr| {
        s2_host::misc(
            s2_host::MISC_EXEC_PRINT,
            0,
            &[bytes(fr.ptr_arg(0)), b"\n"].concat(),
        );
    });
    s.func("void Log(const tString&in asString)", |fr| {
        log(0, bytes(fr.ptr_arg(0)))
    });
    s.func("void Warning(const tString&in asString)", |fr| {
        log(1, bytes(fr.ptr_arg(0)))
    });
    s.func("void Error(const tString&in asString)", |fr| {
        log(2, bytes(fr.ptr_arg(0)))
    });
    s.func(
        "void cLux_AddDebugMessage(const tString&in asText, bool abCheckForDuplicates)",
        |fr| tagged(b"SOMA debug: ", bytes(fr.ptr_arg(0))),
    );
    s.func("void cLux_AddDebugMessage(const tString&in asText)", |fr| {
        tagged(b"SOMA debug: ", bytes(fr.ptr_arg(0)))
    });
    s.func(
        "void cLux_AddTodoMessage(const tString&in asText, bool abCheckForDuplicates)",
        |fr| tagged(b"SOMA todo: ", bytes(fr.ptr_arg(0))),
    );
    s.func("void cLux_AddTodoMessage(const tString&in asText)", |fr| {
        tagged(b"SOMA todo: ", bytes(fr.ptr_arg(0)))
    });
}
