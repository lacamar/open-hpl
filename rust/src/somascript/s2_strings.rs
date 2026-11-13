//! S2: `SomaScriptStrings.cpp` and `SomaScriptStringFuncs.cpp` on the Rust path: `tString`,
//! `tWString`, the concatenation operators, the `array<T>` STL names and the `cString_*` globals.

use super::s2_as::{self, Engine, Frame, Soma, str_bytes, wstr_units};
use super::s2_host;
use super::strings as st;
use crate::string as cstr;
use std::ffi::{c_char, c_void};

// the native-class traits only matter for native calling conventions; generic natives reject them
const STR_FLAGS: u64 = s2_as::OBJ_VALUE as u64;

fn bytes<'a>(p: *mut c_void) -> &'a [u8] {
    // SAFETY: the declaration passes a `const tString &in`.
    unsafe { str_bytes(p) }
}

fn units<'a>(p: *mut c_void) -> &'a [u32] {
    // SAFETY: the declaration passes a `const tWString &in`.
    unsafe { wstr_units(p) }
}

fn cmp<T: Ord>(a: &[T], b: &[T]) -> i32 {
    match a.cmp(b) {
        std::cmp::Ordering::Less => -1,
        std::cmp::Ordering::Equal => 0,
        std::cmp::Ordering::Greater => 1,
    }
}

fn set_str(dst: *mut c_void, b: &[u8]) {
    // SAFETY: `dst` is a live `std::string`.
    unsafe { s2_as::ohpl_as2_str_assign(dst, b.as_ptr(), b.len()) }
}

fn set_wstr(dst: *mut c_void, u: &[u32]) {
    // SAFETY: `dst` is a live `std::wstring`.
    unsafe { s2_as::ohpl_as2_wstr_assign(dst, u.as_ptr(), u.len()) }
}

fn cat(a: &[u8], b: &[u8]) -> Vec<u8> {
    [a, b].concat()
}

fn catw(a: &[u32], b: &[u32]) -> Vec<u32> {
    [a, b].concat()
}

fn string_type(s: &Soma) {
    let e = s.0;
    e.reg_object_type(b"tString", 32, STR_FLAGS);
    e.reg_object_type(b"tWString", 32, STR_FLAGS);
    e.reg_string_factory(b"tString");

    s.construct("tString", "void f()", |fr| {
        // SAFETY: constructor storage.
        unsafe { s2_as::ohpl_as2_str_ctor(fr.this, b"".as_ptr(), 0) }
    });
    s.construct("tString", "void f(const tString &in)", |fr| {
        let b = bytes(fr.ptr_arg(0));
        // SAFETY: constructor storage.
        unsafe { s2_as::ohpl_as2_str_ctor(fr.this, b.as_ptr(), b.len()) }
    });
    e.reg_behaviour(
        s2_as::BEHAVE_DESTRUCT,
        b"tString",
        b"void f()",
        Box::new(|fr| {
            // SAFETY: the engine destroys a live string.
            unsafe { s2_as::ohpl_as2_str_dtor(fr.this) }
        }),
    );
    s.plain("tString", "tString &opAssign(const tString &in)", |fr| {
        set_str(fr.this, bytes(fr.ptr_arg(0)));
        fr.ret_ref(fr.this);
    });
    s.plain("tString", "tString &opAddAssign(const tString &in)", |fr| {
        let b = bytes(fr.ptr_arg(0));
        // SAFETY: live string.
        unsafe { s2_as::ohpl_as2_str_append(fr.this, b.as_ptr(), b.len()) };
        fr.ret_ref(fr.this);
    });
    s.plain("tString", "bool opEquals(const tString &in) const", |fr| {
        fr.ret::<bool>(bytes(fr.this) == bytes(fr.ptr_arg(0)))
    });
    s.plain("tString", "int opCmp(const tString &in) const", |fr| {
        fr.ret::<i32>(cmp(bytes(fr.this), bytes(fr.ptr_arg(0))))
    });
    s.plain("tString", "tString opAdd(const tString &in) const", |fr| {
        fr.ret_string(&cat(bytes(fr.this), bytes(fr.ptr_arg(0))))
    });
    s.plain("tString", "uint64 length() const", |fr| {
        fr.ret::<u64>(bytes(fr.this).len() as u64)
    });
    s.plain("tString", "void resize(uint64)", |fr| {
        // SAFETY: live string.
        unsafe { s2_as::ohpl_as2_str_resize(fr.this, fr.arg::<u64>(0) as usize) }
    });
    let index = |fr: &Frame| {
        static mut DUMMY8: u8 = 0;
        let b = bytes(fr.this);
        let i = fr.arg::<u32>(0) as usize;
        if i >= b.len() {
            fr.ret_ref(std::ptr::addr_of_mut!(DUMMY8));
        } else {
            fr.ret_ref(b.as_ptr().wrapping_add(i).cast_mut());
        }
    };
    s.plain("tString", "uint8 &opIndex(uint)", index);
    s.plain("tString", "const uint8 &opIndex(uint) const", index);

    s.construct("tWString", "void f()", |fr| {
        // SAFETY: constructor storage.
        unsafe { s2_as::ohpl_as2_wstr_ctor(fr.this, [].as_ptr(), 0) }
    });
    s.construct("tWString", "void f(const tWString &in)", |fr| {
        let u = units(fr.ptr_arg(0));
        // SAFETY: constructor storage.
        unsafe { s2_as::ohpl_as2_wstr_ctor(fr.this, u.as_ptr(), u.len()) }
    });
    s.construct("tWString", "void f(const tString &in)", |fr| {
        let w = st::widen(bytes(fr.ptr_arg(0)));
        // SAFETY: constructor storage.
        unsafe { s2_as::ohpl_as2_wstr_ctor(fr.this, w.as_ptr(), w.len()) }
    });
    e.reg_behaviour(
        s2_as::BEHAVE_DESTRUCT,
        b"tWString",
        b"void f()",
        Box::new(|fr| {
            // SAFETY: the engine destroys a live string.
            unsafe { s2_as::ohpl_as2_wstr_dtor(fr.this) }
        }),
    );
    s.plain("tWString", "tWString &opAssign(const tWString &in)", |fr| {
        set_wstr(fr.this, units(fr.ptr_arg(0)));
        fr.ret_ref(fr.this);
    });
    s.plain(
        "tWString",
        "tWString &opAddAssign(const tWString &in)",
        |fr| {
            let u = units(fr.ptr_arg(0));
            // SAFETY: live string.
            unsafe { s2_as::ohpl_as2_wstr_append(fr.this, u.as_ptr(), u.len()) };
            fr.ret_ref(fr.this);
        },
    );
    s.plain(
        "tWString",
        "bool opEquals(const tWString &in) const",
        |fr| fr.ret::<bool>(units(fr.this) == units(fr.ptr_arg(0))),
    );
    s.plain("tWString", "int opCmp(const tWString &in) const", |fr| {
        fr.ret::<i32>(cmp(units(fr.this), units(fr.ptr_arg(0))))
    });
    s.plain(
        "tWString",
        "tWString opAdd(const tWString &in) const",
        |fr| fr.ret_wstring(&catw(units(fr.this), units(fr.ptr_arg(0)))),
    );
    s.plain("tWString", "uint64 length() const", |fr| {
        fr.ret::<u64>(units(fr.this).len() as u64)
    });
    // the C++ native hands out the low half of each wide character
    let windex = |fr: &Frame| {
        static mut DUMMY16: u16 = 0;
        let u = units(fr.this);
        let i = fr.arg::<u32>(0) as usize;
        if i >= u.len() {
            fr.ret_ref(std::ptr::addr_of_mut!(DUMMY16));
        } else {
            fr.ret_ref(u.as_ptr().wrapping_add(i).cast_mut().cast::<u16>());
        }
    };
    s.plain("tWString", "uint16 &opIndex(uint)", windex);
    s.plain("tWString", "const uint16 &opIndex(uint) const", windex);
    s.plain("tWString", "void resize(uint64)", |fr| {
        // SAFETY: live string.
        unsafe { s2_as::ohpl_as2_wstr_resize(fr.this, fr.arg::<u64>(0) as usize) }
    });
    wconcat(s, "tString", Box::new(|fr| fr.ptr_arg(0)), |p| {
        st::widen(bytes(p))
    });
    concat(s, "float", |fr| {
        st::tostr(st::TOSTR_FLOAT, &[*fr.rf::<f32>(0)], &[])
    });
    concat(s, "int", |fr| {
        st::tostr(st::TOSTR_INT, &[], &[*fr.rf::<i32>(0)])
    });
    concat(s, "uint", |fr| {
        st::tostr(st::TOSTR_UINT, &[], &[*fr.rf::<u32>(0) as i32])
    });
    concat(s, "bool", |fr| {
        st::tostr(st::TOSTR_BOOL, &[], &[i32::from(*fr.rf::<bool>(0))])
    });
}

type Ptr = Box<dyn Fn(&Frame) -> *mut c_void>;

/// `tWString` operators taking a narrow-string operand (`RegisterWConcat<std::string>`).
fn wconcat(s: &Soma, t: &'static str, arg: Ptr, to_wide: fn(*mut c_void) -> Vec<u32>) {
    let arg: std::rc::Rc<Ptr> = std::rc::Rc::new(arg);
    let a = arg.clone();
    s.plain(
        "tWString",
        &format!("tWString &opAssign(const {t} &in)"),
        move |fr| {
            set_wstr(fr.this, &to_wide(a(fr)));
            fr.ret_ref(fr.this);
        },
    );
    let a = arg.clone();
    s.plain(
        "tWString",
        &format!("tWString &opAddAssign(const {t} &in)"),
        move |fr| {
            let w = to_wide(a(fr));
            // SAFETY: live string.
            unsafe { s2_as::ohpl_as2_wstr_append(fr.this, w.as_ptr(), w.len()) };
            fr.ret_ref(fr.this);
        },
    );
    let a = arg.clone();
    s.plain(
        "tWString",
        &format!("tWString opAdd(const {t} &in) const"),
        move |fr| {
            fr.ret_wstring(&catw(units(fr.this), &to_wide(a(fr))));
        },
    );
    s.plain(
        "tWString",
        &format!("tWString opAdd_r(const {t} &in) const"),
        move |fr| {
            fr.ret_wstring(&catw(&to_wide(arg(fr)), units(fr.this)));
        },
    );
}

type ToStr = std::rc::Rc<dyn Fn(&Frame) -> Vec<u8>>;

fn concat_with(s: &Soma, t: &'static str, to_str: ToStr) {
    let w = to_str.clone();
    s.plain("tWString", &format!("tWString &opAssign(const {t} &in)"), {
        let w = w.clone();
        move |fr| {
            set_wstr(fr.this, &st::widen(&w(fr)));
            fr.ret_ref(fr.this);
        }
    });
    s.plain(
        "tWString",
        &format!("tWString &opAddAssign(const {t} &in)"),
        {
            let w = w.clone();
            move |fr| {
                let x = st::widen(&w(fr));
                // SAFETY: live string.
                unsafe { s2_as::ohpl_as2_wstr_append(fr.this, x.as_ptr(), x.len()) };
                fr.ret_ref(fr.this);
            }
        },
    );
    s.plain(
        "tWString",
        &format!("tWString opAdd(const {t} &in) const"),
        {
            let w = w.clone();
            move |fr| fr.ret_wstring(&catw(units(fr.this), &st::widen(&w(fr))))
        },
    );
    s.plain(
        "tWString",
        &format!("tWString opAdd_r(const {t} &in) const"),
        {
            let w = w.clone();
            move |fr| fr.ret_wstring(&catw(&st::widen(&w(fr)), units(fr.this)))
        },
    );
    s.plain("tString", &format!("tString &opAssign(const {t} &in)"), {
        let w = w.clone();
        move |fr| {
            set_str(fr.this, &w(fr));
            fr.ret_ref(fr.this);
        }
    });
    s.plain(
        "tString",
        &format!("tString &opAddAssign(const {t} &in)"),
        {
            let w = w.clone();
            move |fr| {
                let x = w(fr);
                // SAFETY: live string.
                unsafe { s2_as::ohpl_as2_str_append(fr.this, x.as_ptr(), x.len()) };
                fr.ret_ref(fr.this);
            }
        },
    );
    s.plain("tString", &format!("tString opAdd(const {t} &in) const"), {
        let w = w.clone();
        move |fr| fr.ret_string(&cat(bytes(fr.this), &w(fr)))
    });
    s.plain(
        "tString",
        &format!("tString opAdd_r(const {t} &in) const"),
        move |fr| {
            fr.ret_string(&cat(&w(fr), bytes(fr.this)));
        },
    );
}

fn concat(s: &Soma, t: &'static str, to_str: impl Fn(&Frame) -> Vec<u8> + 'static) {
    concat_with(s, t, std::rc::Rc::new(to_str));
}

fn floats_of<const N: usize>(fr: &Frame) -> [f32; N] {
    *fr.rf::<[f32; N]>(0)
}

fn ints_of<const N: usize>(fr: &Frame) -> [i32; N] {
    *fr.rf::<[i32; N]>(0)
}

pub fn concats(s: &Soma) {
    use st::{
        TOSTR_FLOATS as FS, TOSTR_ID, TOSTR_INTS as IS, TOSTR_MATRIX, TOSTR_PLANE, TOSTR_RECT_F,
        TOSTR_RECT_L,
    };
    concat(s, "cVector2f", |fr| st::tostr(FS, &floats_of::<2>(fr), &[]));
    concat(s, "cVector2l", |fr| st::tostr(IS, &[], &ints_of::<2>(fr)));
    concat(s, "cVector3f", |fr| st::tostr(FS, &floats_of::<3>(fr), &[]));
    concat(s, "cVector3l", |fr| st::tostr(IS, &[], &ints_of::<3>(fr)));
    concat(s, "cVector4f", |fr| st::tostr(FS, &floats_of::<4>(fr), &[]));
    concat(s, "cMatrixf", |fr| {
        let m = fr.rf::<[f32; 16]>(0);
        st::tostr(TOSTR_MATRIX, m, &[])
    });
    concat(s, "cQuaternion", |fr| {
        st::tostr(FS, &floats_of::<4>(fr), &[])
    });
    concat(s, "cRect2f", |fr| {
        st::tostr(TOSTR_RECT_F, &floats_of::<4>(fr), &[])
    });
    concat(s, "cRect2l", |fr| {
        st::tostr(TOSTR_RECT_L, &[], &ints_of::<4>(fr))
    });
    concat(s, "cPlanef", |fr| {
        st::tostr(TOSTR_PLANE, &floats_of::<4>(fr), &[])
    });
    concat(s, "cColor", |fr| st::tostr(FS, &floats_of::<4>(fr), &[]));
    concat(s, "cDate", |fr| {
        let d = fr.rf::<[i32; 8]>(0);
        format!(
            "({}/{}/{} {}:{:02}:{:02})",
            d[3], d[4], d[5], d[2], d[1], d[0]
        )
        .into_bytes()
    });
    concat(s, "tID", |fr| {
        // `cSomaIDLayout`: uint8 at 0, int32 at 4 and 8
        let p = fr.rf::<[u8; 12]>(0);
        let r = |o: usize| {
            i32::from_ne_bytes(std::array::from_fn(|k| p.get(o + k).copied().unwrap_or(0)))
        };
        st::tostr(TOSTR_ID, &[], &[i32::from(p[0]), r(4), r(8)])
    });
}

fn array_extras(s: &Soma) {
    let Some(h) = s2_host::ops() else { return };
    let (ins, ins_at, pop, pop_at, size) = (
        h.arr_insert_last,
        h.arr_insert_at,
        h.arr_remove_last,
        h.arr_remove_at,
        h.arr_size,
    );
    // SAFETY (all closures): the host ops take the live `CScriptArray` and the element pointer the engine passes.
    s.plain("array<T>", "void push_back(const T&in)", move |fr| unsafe {
        ins(fr.this, fr.ptr_arg(0))
    });
    s.plain(
        "array<T>",
        "void push_front(const T&in)",
        move |fr| unsafe { ins_at(fr.this, 0, fr.ptr_arg(0)) },
    );
    s.plain(
        "array<T>",
        "void insertBack(const T&in)",
        move |fr| unsafe { ins(fr.this, fr.ptr_arg(0)) },
    );
    s.plain("array<T>", "void pop_back()", move |fr| unsafe {
        if size(fr.this) != 0 {
            pop(fr.this);
        }
    });
    s.plain("array<T>", "void pop_front()", move |fr| unsafe {
        if size(fr.this) != 0 {
            pop_at(fr.this, 0);
        }
    });
    s.plain(
        "array<T>",
        "void insert(uint, const T&in)",
        move |fr| unsafe { ins_at(fr.this, fr.arg::<u32>(0), fr.ptr_arg(1)) },
    );
    s.plain("array<T>", "void erase(uint)", move |fr| unsafe {
        pop_at(fr.this, fr.arg::<u32>(0))
    });
    s.plain("array<T>", "uint size() const", move |fr| {
        fr.ret::<u32>(unsafe { size(fr.this) })
    });
    s.plain("array<T>", "bool empty() const", move |fr| {
        fr.ret::<bool>(unsafe { size(fr.this) } == 0)
    });
}

pub fn register_types(e: Engine) {
    let s = Soma(e);
    string_type(&s);
}

pub fn register_array_extras(e: Engine) {
    array_extras(&Soma(e));
}

unsafe extern "C" {
    fn mbstowcs(dst: *mut u32, src: *const c_char, n: usize) -> usize;
}

/// `cString::To16Char`: the locale's multibyte decoding of the text up to its first NUL.
fn to_16_char(s: &[u8]) -> Vec<u32> {
    let mut z: Vec<u8> = s.iter().copied().take_while(|&c| c != 0).collect();
    if z.is_empty() {
        return Vec::new();
    }
    z.push(0);
    // SAFETY: `z` is NUL-terminated; the second call writes at most `needed` units into `w`.
    unsafe {
        let needed = mbstowcs(std::ptr::null_mut(), z.as_ptr().cast(), 0);
        if needed == usize::MAX {
            return Vec::new();
        }
        let mut w = vec![0u32; needed];
        mbstowcs(w.as_mut_ptr(), z.as_ptr().cast(), needed);
        w
    }
}

fn cbytes(b: &[u8]) -> &[u8] {
    b.split(|&c| c == 0).next().unwrap_or_default()
}

fn with_string<R>(b: &[u8], f: impl FnOnce(*mut c_void) -> R) -> R {
    let mut mem = [0u64; 4];
    let p = mem.as_mut_ptr().cast::<c_void>();
    // SAFETY: `mem` is 32 aligned bytes, enough for a `std::string`, constructed and destroyed here.
    unsafe {
        s2_as::ohpl_as2_str_ctor(p, b.as_ptr(), b.len());
        let r = f(p);
        s2_as::ohpl_as2_str_dtor(p);
        r
    }
}

fn with_wstring<R>(u: &[u32], f: impl FnOnce(*mut c_void) -> R) -> R {
    let mut mem = [0u64; 4];
    let p = mem.as_mut_ptr().cast::<c_void>();
    // SAFETY: as `with_string`.
    unsafe {
        s2_as::ohpl_as2_wstr_ctor(p, u.as_ptr(), u.len());
        let r = f(p);
        s2_as::ohpl_as2_wstr_dtor(p);
        r
    }
}

pub fn register_natives(e: Engine) {
    let s = Soma(e);
    concats(&s);
    let h = s2_host::ops();
    s.func(
        "tString cString_ToString(float afX, int alNumOfDecimals=-1, bool abRemoveZeros=false)",
        |fr| {
            fr.ret_string(&st::float_to_string(fr.arg(0), fr.arg(1), fr.arg(2)));
        },
    );
    s.func(
        "tString cString_ToString(int alX, int alPaddingZeros)",
        |fr| fr.ret_string(&st::int_padded(fr.arg(0), fr.arg(1))),
    );
    s.func(
        "tWString cString_ToStringW(float afX, int alNumOfDecimals=-1, bool abRemoveZeros=false)",
        |fr| {
            fr.ret_wstring(&to_16_char(&st::float_to_string(
                fr.arg(0),
                fr.arg(1),
                fr.arg(2),
            )));
        },
    );
    s.func(
        "tWString cString_ToStringW(int alX, int alPaddingZeros)",
        |fr| {
            fr.ret_wstring(&to_16_char(&st::int_padded(fr.arg(0), fr.arg(1))));
        },
    );
    s.func(
        "tWString cString_ReplaceStringToW(const tWString&in aString, const tWString&in asOldString,const tWString&in asNewString)",
        |fr| fr.ret_wstring(&st::replace_all_wide(units(fr.ptr_arg(0)), units(fr.ptr_arg(1)), units(fr.ptr_arg(2)))),
    );
    // SAFETY (the `c_ptr` uses): a `std::string`'s data is NUL-terminated.
    let c_ptr = |p: *mut c_void| -> *const c_char { bytes_ptr(p) };
    s.func(
        "float cString_ToFloat(const tString&in asStr, float afDefault)",
        move |fr| {
            // SAFETY: NUL-terminated string data.
            fr.ret::<f32>(unsafe { cstr::to_float(c_ptr(fr.ptr_arg(0))) });
        },
    );
    s.func(
        "int cString_ToInt(const tString&in asStr, int alDefault)",
        move |fr| {
            // SAFETY: NUL-terminated string data.
            fr.ret::<i32>(unsafe { cstr::to_int(c_ptr(fr.ptr_arg(0))) });
        },
    );
    s.func(
        "bool cString_ToBool(const tString&in asStr, bool abDefault)",
        |fr| fr.ret::<bool>(cstr::to_bool(cbytes(bytes(fr.ptr_arg(0))))),
    );
    s.func(
        "cVector2f cString_ToVector2f(const tString&in asStr, const cVector2f&in avDefault)",
        |fr| {
            let mut v = [0.0f32; 2];
            fr.ret::<[f32; 2]>(
                if cstr::floats_exact(cbytes(bytes(fr.ptr_arg(0))), &mut v) {
                    v
                } else {
                    *fr.rf::<[f32; 2]>(1)
                },
            );
        },
    );
    s.func(
        "cVector3f cString_ToVector3f(const tString&in asStr, const cVector3f&in avDefault)",
        |fr| {
            let mut v = [0.0f32; 3];
            fr.ret::<[f32; 3]>(
                if cstr::floats_exact(cbytes(bytes(fr.ptr_arg(0))), &mut v) {
                    v
                } else {
                    *fr.rf::<[f32; 3]>(1)
                },
            );
        },
    );
    s.func(
        "cColor cString_ToColor(const tString&in asStr, const cColor&in aDefault)",
        |fr| {
            let mut v = [0.0f32; 4];
            fr.ret::<[f32; 4]>(
                if cstr::floats_exact(cbytes(bytes(fr.ptr_arg(0))), &mut v) {
                    v
                } else {
                    *fr.rf::<[f32; 4]>(1)
                },
            );
        },
    );
    s.func(
        "tString cString_GetLastChar(const tString&in aString)",
        |fr| fr.ret_string(st::last_char(bytes(fr.ptr_arg(0)))),
    );
    s.func(
        "int cString_GetFirstCharPos(const tString&in aString, int8 alChar)",
        |fr| {
            fr.ret(st::find_char(
                bytes(fr.ptr_arg(0)),
                fr.arg::<i8>(1) as u8,
                false,
            ));
        },
    );
    s.func(
        "int cString_GetLastCharPos(const tString&in aString, int8 alChar)",
        |fr| {
            fr.ret(st::find_char(
                bytes(fr.ptr_arg(0)),
                fr.arg::<i8>(1) as u8,
                true,
            ));
        },
    );
    s.func(
        "int cString_GetFirstStringPos(const tString&in aString, const tString&in aChar)",
        |fr| {
            fr.ret(st::find_str(
                bytes(fr.ptr_arg(0)),
                bytes(fr.ptr_arg(1)),
                false,
            ));
        },
    );
    s.func(
        "int cString_GetLastStringPos(const tString&in aString, const tString&in aChar)",
        |fr| {
            fr.ret(st::find_str(
                bytes(fr.ptr_arg(0)),
                bytes(fr.ptr_arg(1)),
                true,
            ));
        },
    );
    s.func(
        "int cString_GetFirstStringPosW(const tWString&in aString, const tWString&in aChar)",
        |fr| {
            fr.ret(st::find_str(
                units(fr.ptr_arg(0)),
                units(fr.ptr_arg(1)),
                false,
            ));
        },
    );
    s.func(
        "int cString_GetLastStringPosW(const tWString&in aString, const tWString&in aChar)",
        |fr| {
            fr.ret(st::find_str(
                units(fr.ptr_arg(0)),
                units(fr.ptr_arg(1)),
                true,
            ));
        },
    );
    s.func(
        "int cString_CountCharsInString(const tString&in aString, const tString&in aChar)",
        |fr| {
            fr.ret(st::count_chars(bytes(fr.ptr_arg(0)), bytes(fr.ptr_arg(1))));
        },
    );
    s.func(
        "tWString cString_GetDecodedString(const tString&in asStr)",
        |fr| {
            let d: Vec<u32> = crate::xml::lang::decode_string(bytes(fr.ptr_arg(0)))
                .into_iter()
                .map(|c| c as u32)
                .collect();
            fr.ret_wstring(&d);
        },
    );
    s.func("uint cString_GetHash(const tString&in asStr)", |fr| {
        fr.ret(st::hash(bytes(fr.ptr_arg(0))))
    });
    s.func(
        "tString cString_GetNumericSuffix(const tString&in asStr)",
        |fr| fr.ret_string(st::numeric_suffix(bytes(fr.ptr_arg(0)))),
    );
    s.func(
        "tWString cString_GetNumericSuffixW(const tWString&in asStr)",
        |fr| fr.ret_wstring(st::numeric_suffix(units(fr.ptr_arg(0)))),
    );
    s.func(
        "int cString_GetNumericSuffixInt(const tString&in aString, int alDefault=0)",
        |fr| {
            fr.ret(st::numeric_suffix_int(bytes(fr.ptr_arg(0)), fr.arg(1)));
        },
    );
    s.func(
        "int cString_GetNumericSuffixIntW(const tWString&in aString, int alDefault=0)",
        |fr| {
            fr.ret(st::numeric_suffix_int_wide(units(fr.ptr_arg(0)), fr.arg(1)));
        },
    );
    let Some(h) = h else { return };
    let ins = h.arr_insert_last;
    s.func("void cString_GetStringVecW(const tWString&in asData, array<tWString> &inout avOutStrings, const tWString&in asSepp)", move |fr| {
        let arr = fr.ptr_arg(1);
        for part in st::split(units(fr.ptr_arg(0)), units(fr.ptr_arg(2))) {
            // SAFETY: the array copies the element during the call.
            with_wstring(part, |p| unsafe { ins(arr, p) });
        }
    });
    s.func("void cString_GetStringVec(const tString&in asData, array<tString> &inout avOutStrings, const tString&in asSepp)", move |fr| {
        let arr = fr.ptr_arg(1);
        for part in st::split(bytes(fr.ptr_arg(0)), bytes(fr.ptr_arg(2))) {
            // SAFETY: the array copies the element during the call.
            with_string(part, |p| unsafe { ins(arr, p) });
        }
    });
    s.func("void cString_GetFloatVec(const tString&in asData, array<float> &inout avOutFloats, const tString&in asSepp)", move |fr| {
        let arr = fr.ptr_arg(1);
        for t in st::split(bytes(fr.ptr_arg(0)), bytes(fr.ptr_arg(2))) {
            let mut f = super::cfmt::c_atof(t) as f32;
            // SAFETY: the array copies the element during the call.
            unsafe { ins(arr, std::ptr::from_mut(&mut f).cast()) };
        }
    });
    s.func("void cString_GetIntVec(const tString&in asData, array<int> &inout avOutInts, const tString&in asSepp)", move |fr| {
        let arr = fr.ptr_arg(1);
        for t in st::split(bytes(fr.ptr_arg(0)), bytes(fr.ptr_arg(2))) {
            let mut l = super::cfmt::c_atoi(t);
            // SAFETY: the array copies the element during the call.
            unsafe { ins(arr, std::ptr::from_mut(&mut l).cast()) };
        }
    });
    s.func(
        "tString String_SecondsToClockDisplay(float afSeconds, bool abShowHours, bool abShowMinutes, bool abShowSeconds, bool abShowHundredths)",
        |fr| fr.ret_string(&st::seconds_to_clock(fr.arg(0), fr.arg(1), fr.arg(2), fr.arg(3), fr.arg(4))),
    );
}

fn bytes_ptr(p: *mut c_void) -> *const c_char {
    // SAFETY: `p` is a live `std::string`.
    unsafe { s2_as::str_cptr(p) }
}
