//! The engine side of the add-ons: `Host` over the shim, registration of the `array<T>` and `string`
//! types in the generic calling convention, and the C entry points for code that still holds arrays.

use super::array::{Array, CACHE_KEY, free_cache, release as array_release};
use super::host::{CmpCtx, FuncInfo, Host, P};
use super::string::{self, ScriptString};
use super::sys::*;
use std::ffi::{CStr, CString, c_char, c_int, c_void};

pub struct ShimHost;

pub static SHIM: ShimHost = ShimHost;

fn vp(p: P) -> *mut c_void {
    p as *mut c_void
}

fn cs(s: &str) -> CString {
    CString::new(s.replace('\0', "")).unwrap_or_default()
}

fn text(p: *const c_char) -> Vec<u8> {
    if p.is_null() {
        return Vec::new();
    }
    // SAFETY: the engine returns NUL-terminated strings.
    unsafe { CStr::from_ptr(p) }.to_bytes().to_vec()
}

// SAFETY (all blocks in `impl Host`): plain calls into the shim with pointers the engine handed out.
impl Host for ShimHost {
    fn active_ctx(&self) -> P {
        unsafe { ohpl_as3_active_ctx() as P }
    }

    fn throw(&self, msg: &str) {
        let c = self.active_ctx();
        if c != 0 {
            unsafe { ohpl_as3_ctx_set_exception(vp(c), cs(msg).as_ptr()) };
        }
    }

    fn lock(&self, acquire: bool) {
        unsafe { ohpl_as3_lock(c_int::from(acquire)) };
    }

    fn type_engine(&self, ty: P) -> P {
        unsafe { ohpl_as3_type_engine(vp(ty)) as P }
    }

    fn type_flags(&self, ty: P) -> u64 {
        unsafe { ohpl_as3_type_flags(vp(ty)) }
    }

    fn type_id(&self, ty: P) -> i32 {
        unsafe { ohpl_as2_type_id(vp(ty)) }
    }

    fn type_name(&self, ty: P) -> String {
        if ty == 0 {
            return String::new();
        }
        String::from_utf8_lossy(&text(unsafe { ohpl_as2_type_name(vp(ty)) })).into_owned()
    }

    fn type_size(&self, ty: P) -> usize {
        usize::try_from(unsafe { ohpl_as2_type_size(vp(ty)) }).unwrap_or(0)
    }

    fn sub_type(&self, ty: P) -> P {
        unsafe { ohpl_as3_type_sub(vp(ty)) as P }
    }

    fn sub_type_id(&self, ty: P) -> i32 {
        unsafe { ohpl_as3_type_sub_id(vp(ty)) }
    }

    fn type_by_id(&self, engine: P, id: i32) -> P {
        unsafe { ohpl_as2_type_by_id(vp(engine), id) as P }
    }

    fn type_addref(&self, ty: P) {
        unsafe { ohpl_as3_type_addref(vp(ty)) };
    }

    fn type_release(&self, ty: P) {
        unsafe { ohpl_as3_type_release(vp(ty)) };
    }

    fn type_user(&self, ty: P) -> P {
        unsafe { ohpl_as3_type_user(vp(ty), CACHE_KEY) as P }
    }

    fn set_type_user(&self, ty: P, p: P) {
        unsafe { ohpl_as3_type_set_user(vp(ty), vp(p), CACHE_KEY) };
    }

    fn methods(&self, ty: P) -> Vec<FuncInfo> {
        if ty == 0 {
            return Vec::new();
        }
        let n = unsafe { ohpl_as3_type_method_count(vp(ty)) };
        (0..n)
            .map(|i| {
                let f = unsafe { ohpl_as3_type_method(vp(ty), i) };
                let mut ret_flags = 0u32;
                let ret_id = unsafe { ohpl_as2_func_return(f, &raw mut ret_flags) };
                let params = unsafe { ohpl_as2_func_param_count(f) };
                let (mut p0_id, mut p0_flags) = (0, 0u32);
                if params == 1 {
                    unsafe { ohpl_as2_func_param(f, 0, &raw mut p0_id, &raw mut p0_flags) };
                }
                FuncInfo {
                    f: f as P,
                    name: text(unsafe { ohpl_as2_func_name(f) }),
                    ret_id,
                    ret_flags,
                    params,
                    read_only: unsafe { ohpl_as3_func_read_only(f) } != 0,
                    p0_id,
                    p0_flags,
                }
            })
            .collect()
    }

    fn has_default_ctor(&self, ty: P) -> bool {
        (0..unsafe { ohpl_as3_type_behaviour_count(vp(ty)) }).any(|i| {
            let mut beh = 0;
            let f = unsafe { ohpl_as3_type_behaviour(vp(ty), i, &raw mut beh) };
            beh == 0 && unsafe { ohpl_as2_func_param_count(f) } == 0
        })
    }

    fn has_default_factory(&self, ty: P) -> bool {
        (0..unsafe { ohpl_as3_type_factory_count(vp(ty)) })
            .any(|i| unsafe { ohpl_as2_func_param_count(ohpl_as3_type_factory(vp(ty), i)) } == 0)
    }

    fn engine_prop(&self, engine: P, prop: i32) -> i32 {
        unsafe { ohpl_as3_engine_prop(vp(engine), prop) }
    }

    fn message(&self, engine: P, msg: &str) {
        unsafe {
            ohpl_as3_write_message(
                vp(engine),
                c"array".as_ptr(),
                0,
                0,
                MSG_ERROR,
                cs(msg).as_ptr(),
            );
        }
    }

    fn type_decl(&self, engine: P, id: i32) -> String {
        String::from_utf8_lossy(&text(unsafe { ohpl_as3_type_decl(vp(engine), id, 0) }))
            .into_owned()
    }

    fn create_object(&self, engine: P, ty: P) -> P {
        unsafe { ohpl_as2_create_object(vp(engine), vp(ty)) as P }
    }

    fn assign_object(&self, engine: P, dst: P, src: P, ty: P) {
        unsafe { ohpl_as3_assign_object(vp(engine), vp(dst), vp(src), vp(ty)) };
    }

    fn addref_object(&self, engine: P, obj: P, ty: P) {
        unsafe { ohpl_as3_addref_object(vp(engine), vp(obj), vp(ty)) };
    }

    fn release_object(&self, engine: P, obj: P, ty: P) {
        unsafe { ohpl_as3_release_object(vp(engine), vp(obj), vp(ty)) };
    }

    fn notify_gc(&self, engine: P, obj: P, ty: P) {
        unsafe { ohpl_as3_notify_gc(vp(engine), vp(obj), vp(ty)) };
    }

    fn gc_enum(&self, engine: P, obj: P) {
        unsafe { ohpl_as3_gc_enum(vp(engine), vp(obj)) };
    }

    fn gc_forward(&self, engine: P, obj: P, ty: P) {
        unsafe { ohpl_as3_gc_forward_enum(vp(engine), vp(obj), vp(ty)) };
    }

    fn hndl_assign(&self, engine: P, ty: P, dst: P, src: P) -> bool {
        let n = self.type_name(ty);
        let decl = cs(&format!("{n}& opHndlAssign(const {n}&in)"));
        let f = unsafe { ohpl_as2_type_method_by_decl(vp(ty), decl.as_ptr()) };
        if f.is_null() {
            return false;
        }
        unsafe {
            let c = ohpl_as3_request_ctx(vp(engine));
            ohpl_as_ctx_prepare(c, f);
            ohpl_as_ctx_set_object(c, vp(dst));
            ohpl_as_ctx_set_arg_address(c, 0, vp(src));
            ohpl_as_ctx_execute(c);
            ohpl_as3_return_ctx(vp(engine), c);
        }
        true
    }

    fn begin_ctx(&self, engine: P, pooled: bool) -> CmpCtx {
        let active = self.active_ctx();
        if active != 0
            && unsafe { ohpl_as3_ctx_engine(vp(active)) } as P == engine
            && unsafe { ohpl_as3_ctx_push(vp(active)) } >= 0
        {
            return CmpCtx {
                ctx: active,
                nested: true,
            };
        }
        let ctx = unsafe {
            if pooled {
                ohpl_as3_request_ctx(vp(engine))
            } else {
                ohpl_as_ctx_new(vp(engine))
            }
        };
        CmpCtx {
            ctx: ctx as P,
            nested: false,
        }
    }

    fn end_ctx(&self, engine: P, c: CmpCtx, pooled: bool) {
        unsafe {
            if c.nested {
                let state = ohpl_as_ctx_state(vp(c.ctx));
                ohpl_as3_ctx_pop(vp(c.ctx));
                if state == 2 {
                    ohpl_as_ctx_abort(vp(c.ctx));
                }
            } else if pooled {
                ohpl_as3_return_ctx(vp(engine), vp(c.ctx));
            } else {
                ohpl_as_ctx_release(vp(c.ctx));
            }
        }
    }

    fn call_eq(&self, c: CmpCtx, f: P, obj: P, arg: P) -> bool {
        unsafe {
            ohpl_as_ctx_prepare(vp(c.ctx), vp(f));
            ohpl_as_ctx_set_object(vp(c.ctx), vp(obj));
            ohpl_as_ctx_set_arg_object(vp(c.ctx), 0, vp(arg));
            ohpl_as_ctx_execute(vp(c.ctx)) == EXECUTION_FINISHED
                && ohpl_as3_ctx_ret_byte(vp(c.ctx)) != 0
        }
    }

    fn call_cmp(&self, c: CmpCtx, f: P, obj: P, arg: P) -> Option<i32> {
        unsafe {
            ohpl_as_ctx_prepare(vp(c.ctx), vp(f));
            ohpl_as_ctx_set_object(vp(c.ctx), vp(obj));
            ohpl_as_ctx_set_arg_object(vp(c.ctx), 0, vp(arg));
            (ohpl_as_ctx_execute(vp(c.ctx)) == EXECUTION_FINISHED)
                .then(|| ohpl_as_ctx_ret_dword(vp(c.ctx)) as i32)
        }
    }

    fn call_less(&self, c: CmpCtx, f: P, a: P, b: P) -> Option<bool> {
        unsafe {
            ohpl_as_ctx_prepare(vp(c.ctx), vp(f));
            ohpl_as_ctx_set_arg_address(vp(c.ctx), 0, vp(a));
            ohpl_as_ctx_set_arg_address(vp(c.ctx), 1, vp(b));
            (ohpl_as_ctx_execute(vp(c.ctx)) == EXECUTION_FINISHED)
                .then(|| ohpl_as3_ctx_ret_addr_of(vp(c.ctx)).cast::<u8>().read() != 0)
        }
    }
}

type Gen = *mut c_void;

/// The object a method was called on.
///
/// # Safety
/// `g` is the generic frame of a method registered on the type `T`.
unsafe fn this<'a, T>(g: Gen) -> &'a T {
    // SAFETY: caller contract; the engine passes a live object.
    unsafe { &*ohpl_as2_g_object(g).cast::<T>() }
}

/// # Safety
/// Argument `i` of `g` is a 32-bit value.
unsafe fn arg_u32(g: Gen, i: u32) -> u32 {
    // SAFETY: caller contract.
    unsafe { ohpl_as2_g_arg_addr(g, i).cast::<u32>().read_unaligned() }
}

/// What `GetArgAddress` / `GetArgObject` return: the pointer a reference or handle argument holds.
///
/// # Safety
/// Argument `i` of `g` is a reference or handle.
unsafe fn arg_ptr(g: Gen, i: u32) -> usize {
    // SAFETY: caller contract.
    unsafe { ohpl_as2_g_arg_addr(g, i).cast::<usize>().read_unaligned() }
}

/// # Safety
/// `g` is a frame whose function returns a 64-bit-or-smaller value, a reference or a handle.
unsafe fn ret(g: Gen, v: u64) {
    // SAFETY: caller contract; the return slot is a full quadword.
    unsafe { ohpl_as2_g_ret_loc(g).cast::<u64>().write_unaligned(v) }
}

fn arr<'a>(g: Gen) -> &'a Array {
    // SAFETY (callers): registered only on `array<T>`.
    unsafe { this::<Array>(g) }
}

fn other<'a>(g: Gen, i: u32) -> Option<&'a Array> {
    // SAFETY: a `const array<T>&in` argument is a live array.
    unsafe { (arg_ptr(g, i) as *const Array).as_ref() }
}

macro_rules! gen_fn {
    ($name:ident, |$g:ident| $body:expr) => {
        unsafe extern "C" fn $name($g: Gen) {
            // SAFETY: the engine calls generic functions with a live frame matching the registered declaration.
            #[allow(unused_unsafe)]
            unsafe {
                $body
            }
        }
    };
}

gen_fn!(a_factory, |g| {
    let ti = arg_ptr(g, 0);
    let a = Array::new(&SHIM, ti);
    (*a).init_len(0);
    ret(g, a as u64)
});
gen_fn!(a_factory_len, |g| {
    let a = Array::new(&SHIM, arg_ptr(g, 0));
    (*a).init_len(arg_u32(g, 1));
    ret(g, a as u64)
});
gen_fn!(a_factory_def, |g| {
    let a = Array::new(&SHIM, arg_ptr(g, 0));
    (*a).init_default(arg_u32(g, 1), arg_ptr(g, 2));
    ret(g, a as u64)
});
gen_fn!(a_factory_list, |g| {
    let a = Array::new(&SHIM, arg_ptr(g, 0));
    (*a).init_list(arg_ptr(g, 1) as *mut u8);
    ret(g, a as u64)
});
gen_fn!(a_template_cb, |g| {
    let ti = arg_ptr(g, 0);
    let dont_gc = arg_ptr(g, 1) as *mut bool;
    let mut skip = false;
    let ok = super::array::template_callback(&SHIM, ti, &mut skip);
    if let Some(d) = dont_gc.as_mut() {
        *d = skip;
    }
    ret(g, u64::from(ok))
});
gen_fn!(a_assign, |g| {
    let me = arr(g);
    if let Some(o) = other(g, 0) {
        me.copy_from(o);
    }
    ret(g, ohpl_as2_g_object(g) as u64)
});
gen_fn!(a_equals, |g| {
    let eq = other(g, 0).is_some_and(|o| arr(g).equals_array(o));
    ret(g, u64::from(eq))
});
gen_fn!(a_find, |g| ret(
    g,
    u64::from(arr(g).find(0, arg_ptr(g, 0)) as u32)
));
gen_fn!(a_find2, |g| ret(
    g,
    u64::from(arr(g).find(arg_u32(g, 0), arg_ptr(g, 1)) as u32)
));
gen_fn!(a_find_ref, |g| ret(
    g,
    u64::from(arr(g).find_by_ref(0, arg_ptr(g, 0)) as u32)
));
gen_fn!(a_find_ref2, |g| ret(
    g,
    u64::from(arr(g).find_by_ref(arg_u32(g, 0), arg_ptr(g, 1)) as u32)
));
gen_fn!(a_at, |g| ret(g, arr(g).at(arg_u32(g, 0)) as u64));
gen_fn!(a_insert_at, |g| arr(g)
    .insert_at(arg_u32(g, 0), arg_ptr(g, 1)));
gen_fn!(a_insert_at_arr, |g| {
    if let Some(o) = other(g, 1) {
        arr(g).insert_at_array(arg_u32(g, 0), o);
    }
});
gen_fn!(a_remove_at, |g| arr(g).remove_at(arg_u32(g, 0)));
gen_fn!(a_remove_range, |g| arr(g)
    .remove_range(arg_u32(g, 0), arg_u32(g, 1)));
gen_fn!(a_insert_last, |g| arr(g).insert_last(arg_ptr(g, 0)));
gen_fn!(a_remove_last, |g| arr(g).remove_last());
gen_fn!(a_length, |g| ret(g, u64::from(arr(g).size())));
gen_fn!(a_resize, |g| arr(g).resize(arg_u32(g, 0)));
gen_fn!(a_reserve, |g| arr(g).reserve(arg_u32(g, 0)));
gen_fn!(a_sort_asc, |g| {
    let a = arr(g);
    a.sort(0, a.size(), true)
});
gen_fn!(a_sort_asc2, |g| arr(g).sort(
    arg_u32(g, 0),
    arg_u32(g, 1),
    true
));
gen_fn!(a_sort_desc, |g| {
    let a = arr(g);
    a.sort(0, a.size(), false)
});
gen_fn!(a_sort_desc2, |g| arr(g).sort(
    arg_u32(g, 0),
    arg_u32(g, 1),
    false
));
gen_fn!(a_sort_cb, |g| arr(g).sort_callback(
    arg_ptr(g, 0),
    arg_u32(g, 1),
    arg_u32(g, 2)
));
gen_fn!(a_reverse, |g| arr(g).reverse());
gen_fn!(a_is_empty, |g| ret(g, u64::from(arr(g).is_empty())));
gen_fn!(a_add_ref, |g| arr(g).add_ref());
gen_fn!(a_release, |g| array_release(ohpl_as2_g_object(g).cast()));
gen_fn!(a_ref_count, |g| ret(
    g,
    u64::from(arr(g).ref_count() as u32)
));
gen_fn!(a_set_flag, |g| arr(g).set_flag());
gen_fn!(a_get_flag, |g| ret(g, u64::from(arr(g).flag())));
gen_fn!(a_enum_refs, |g| arr(g).enum_references(arg_ptr(g, 0)));
gen_fn!(a_release_refs, |g| arr(g).resize(0));
gen_fn!(a_for_begin, |g| ret(g, 0));
gen_fn!(a_for_end, |g| {
    let a = (ohpl_as2_g_object(g) as *const Array).as_ref();
    ret(g, u64::from(a.is_none_or(|a| a.size() <= arg_u32(g, 0))))
});
gen_fn!(a_for_next, |g| ret(
    g,
    u64::from(arg_u32(g, 0).wrapping_add(1))
));
gen_fn!(a_for_value1, |g| ret(g, u64::from(arg_u32(g, 0))));

unsafe extern "C" fn array_cache_cleanup(t: *mut c_void) {
    // SAFETY: the engine calls this for types whose user data at `CACHE_KEY` we set.
    unsafe { free_cache(ohpl_as3_type_user(t, CACHE_KEY) as P) };
}

fn reg_beh(e: *mut c_void, beh: c_int, decl: &str, f: unsafe extern "C" fn(Gen)) -> bool {
    // SAFETY: plain call; `f` is a generic-convention function.
    unsafe {
        ohpl_as2_reg_object_behaviour(
            e,
            beh,
            c"array<T>".as_ptr(),
            cs(decl).as_ptr(),
            f as *const c_void,
            std::ptr::null_mut(),
        ) >= 0
    }
}

fn reg_method(e: *mut c_void, decl: &str, f: unsafe extern "C" fn(Gen)) -> bool {
    // SAFETY: as `reg_beh`.
    unsafe {
        ohpl_as2_reg_object_method(
            e,
            c"array<T>".as_ptr(),
            cs(decl).as_ptr(),
            f as *const c_void,
            std::ptr::null_mut(),
        ) >= 0
    }
}

/// `RegisterScriptArray(engine, defaultArray)`; true when every declaration was accepted.
///
/// # Safety
/// `e` is a live engine.
pub unsafe fn register_array(e: *mut c_void, default_array: bool) -> bool {
    // SAFETY: plain calls; `e` is a live engine.
    let mut ok = unsafe {
        ohpl_as3_type_cleanup_cb(e, array_cache_cleanup, CACHE_KEY);
        ohpl_as_reg_object_type(
            e,
            c"array<class T>".as_ptr(),
            0,
            OBJ_REF | OBJ_GC | OBJ_TEMPLATE,
        ) >= 0
    };
    let t = [
        (
            BEHAVE_TEMPLATE_CALLBACK,
            "bool f(int&in, bool&out)",
            a_template_cb as unsafe extern "C" fn(Gen),
        ),
        (BEHAVE_FACTORY, "array<T>@ f(int&in)", a_factory),
        (
            BEHAVE_FACTORY,
            "array<T>@ f(int&in, uint length) explicit",
            a_factory_len,
        ),
        (
            BEHAVE_FACTORY,
            "array<T>@ f(int&in, uint length, const T &in value)",
            a_factory_def,
        ),
        (
            BEHAVE_LIST_FACTORY,
            "array<T>@ f(int&in type, int&in list) {repeat T}",
            a_factory_list,
        ),
        (BEHAVE_ADDREF, "void f()", a_add_ref),
        (BEHAVE_RELEASE, "void f()", a_release),
    ];
    for (b, d, f) in t {
        ok &= reg_beh(e, b, d, f);
    }
    let m: [(&str, unsafe extern "C" fn(Gen)); 28] = [
        ("T &opIndex(uint index)", a_at),
        ("const T &opIndex(uint index) const", a_at),
        ("uint opForBegin() const", a_for_begin),
        ("bool opForEnd(uint) const", a_for_end),
        ("uint opForNext(uint) const", a_for_next),
        ("const T &opForValue0(uint index) const", a_at),
        ("uint opForValue1(uint index) const", a_for_value1),
        ("array<T> &opAssign(const array<T>&in)", a_assign),
        ("void insertAt(uint index, const T&in value)", a_insert_at),
        (
            "void insertAt(uint index, const array<T>& arr)",
            a_insert_at_arr,
        ),
        ("void insertLast(const T&in value)", a_insert_last),
        ("void removeAt(uint index)", a_remove_at),
        ("void removeLast()", a_remove_last),
        ("void removeRange(uint start, uint count)", a_remove_range),
        ("uint length() const", a_length),
        ("void reserve(uint length)", a_reserve),
        ("void resize(uint length)", a_resize),
        ("void sortAsc()", a_sort_asc),
        ("void sortAsc(uint startAt, uint count)", a_sort_asc2),
        ("void sortDesc()", a_sort_desc),
        ("void sortDesc(uint startAt, uint count)", a_sort_desc2),
        ("void reverse()", a_reverse),
        (
            "int find(const T&in if_handle_then_const value) const",
            a_find,
        ),
        (
            "int find(uint startAt, const T&in if_handle_then_const value) const",
            a_find2,
        ),
        (
            "int findByRef(const T&in if_handle_then_const value) const",
            a_find_ref,
        ),
        (
            "int findByRef(uint startAt, const T&in if_handle_then_const value) const",
            a_find_ref2,
        ),
        ("bool opEquals(const array<T>&in) const", a_equals),
        ("bool isEmpty() const", a_is_empty),
    ];
    for (d, f) in m {
        ok &= reg_method(e, d, f);
    }
    // SAFETY: plain call.
    ok &= unsafe {
        ohpl_as_reg_funcdef(
            e,
            c"bool array<T>::less(const T&in if_handle_then_const a, const T&in if_handle_then_const b)".as_ptr(),
        )
    } >= 0;
    ok &= reg_method(
        e,
        "void sort(const less &in, uint startAt = 0, uint count = uint(-1))",
        a_sort_cb,
    );
    let gc = [
        (
            BEHAVE_GETREFCOUNT,
            "int f()",
            a_ref_count as unsafe extern "C" fn(Gen),
        ),
        (BEHAVE_SETGCFLAG, "void f()", a_set_flag),
        (BEHAVE_GETGCFLAG, "bool f()", a_get_flag),
        (BEHAVE_ENUMREFS, "void f(int&in)", a_enum_refs),
        (BEHAVE_RELEASEREFS, "void f(int&in)", a_release_refs),
    ];
    for (b, d, f) in gc {
        ok &= reg_beh(e, b, d, f);
    }
    if default_array {
        // SAFETY: plain call.
        ok &= unsafe { ohpl_as3_reg_default_array(e, c"array<T>".as_ptr()) } >= 0;
    }
    ok
}

fn sref<'a>(g: Gen, i: u32) -> &'a ScriptString {
    // SAFETY: a `const string &in` argument is a live string; an engine never passes null.
    unsafe { &*(arg_ptr(g, i) as *const ScriptString) }
}

fn me<'a>(g: Gen) -> &'a ScriptString {
    // SAFETY (callers): registered only on `string`.
    unsafe { this::<ScriptString>(g) }
}

fn new_ret(g: Gen, s: *mut ScriptString) {
    // SAFETY: the function returns a `string@` or reference; the slot takes the pointer as is.
    unsafe { ret(g, s as u64) }
}

gen_fn!(s_factory, |g| new_ret(g, ScriptString::new_raw(b"")));
gen_fn!(s_factory_copy, |g| new_ret(
    g,
    ScriptString::new_raw(sref(g, 0).bytes())
));
gen_fn!(s_add_ref, |g| me(g).add_ref());
gen_fn!(s_release, |g| string::release(ohpl_as2_g_object(g).cast()));
gen_fn!(s_assign, |g| {
    me(g).assign(sref(g, 0).bytes());
    new_ret(g, ohpl_as2_g_object(g).cast())
});
gen_fn!(s_add_assign, |g| {
    me(g).append(sref(g, 0).bytes());
    new_ret(g, ohpl_as2_g_object(g).cast())
});
gen_fn!(s_equals, |g| ret(
    g,
    u64::from(me(g).bytes() == sref(g, 0).bytes())
));
gen_fn!(s_cmp, |g| ret(
    g,
    u64::from(string::cmp(me(g).bytes(), sref(g, 0).bytes()) as u32)
));
gen_fn!(s_add, |g| new_ret(
    g,
    ScriptString::concat(me(g).bytes(), sref(g, 0).bytes())
));
gen_fn!(s_char_at, |g| {
    let (i, s) = (arg_u32(g, 0), me(g));
    let p = s.byte_addr(i as usize);
    if p.is_null() {
        SHIM.throw("Out of range");
    }
    ret(g, p as u64)
});
gen_fn!(s_length, |g| ret(g, me(g).bytes().len() as u64));
gen_fn!(s_resize, |g| me(g).resize(
    ohpl_as2_g_arg_addr(g, 0).cast::<usize>().read_unaligned()
));

/// The scalar a conversion operator was called with, printed as the add-on did.
///
/// # Safety
/// Argument 0 of `g` has the type selected by `K`.
unsafe fn scalar<const K: u8>(g: Gen) -> Vec<u8> {
    // SAFETY: caller contract.
    unsafe {
        let a = ohpl_as2_g_arg_addr(g, 0);
        match K {
            0 => string::fmt_g(a.cast::<f64>().read_unaligned()),
            1 => string::fmt_g(f64::from(a.cast::<f32>().read_unaligned())),
            2 => a.cast::<i32>().read_unaligned().to_string().into_bytes(),
            3 => a.cast::<u32>().read_unaligned().to_string().into_bytes(),
            _ => string::fmt_bool(a.cast::<u8>().read() != 0),
        }
    }
}

unsafe extern "C" fn s_assign_k<const K: u8>(g: Gen) {
    // SAFETY: registered with a `K`-typed argument.
    unsafe {
        me(g).assign(&scalar::<K>(g));
        new_ret(g, ohpl_as2_g_object(g).cast())
    }
}

unsafe extern "C" fn s_add_assign_k<const K: u8>(g: Gen) {
    // SAFETY: as `s_assign_k`.
    unsafe {
        me(g).append(&scalar::<K>(g));
        new_ret(g, ohpl_as2_g_object(g).cast())
    }
}

unsafe extern "C" fn s_add_k<const K: u8>(g: Gen) {
    // SAFETY: as `s_assign_k`.
    unsafe { new_ret(g, ScriptString::concat(me(g).bytes(), &scalar::<K>(g))) }
}

unsafe extern "C" fn s_add_r_k<const K: u8>(g: Gen) {
    // SAFETY: as `s_assign_k`.
    unsafe { new_ret(g, ScriptString::concat(&scalar::<K>(g), me(g).bytes())) }
}

unsafe extern "C" fn factory_get(data: *const c_char, len: u32) -> *const c_void {
    let b = if data.is_null() {
        &[][..]
    } else {
        // SAFETY: the engine passes `len` bytes.
        unsafe { std::slice::from_raw_parts(data.cast::<u8>(), len as usize) }
    };
    ScriptString::new_raw(b).cast_const().cast()
}

unsafe extern "C" fn factory_release(s: *const c_void) -> c_int {
    if s.is_null() {
        return -1;
    }
    // SAFETY: the engine returns constants made by `factory_get`.
    unsafe { string::release(s.cast()) };
    0
}

unsafe extern "C" fn factory_raw(s: *const c_void, data: *mut c_char, len: *mut u32) -> c_int {
    // SAFETY: the engine passes a constant from `factory_get`, and a buffer of the size it asked for.
    unsafe {
        let Some(s) = s.cast::<ScriptString>().as_ref() else {
            return -1;
        };
        let b = s.bytes();
        if let Some(l) = len.as_mut() {
            *l = u32::try_from(b.len()).unwrap_or(u32::MAX);
        }
        if !data.is_null() {
            std::ptr::copy_nonoverlapping(b.as_ptr(), data.cast::<u8>(), b.len());
        }
    }
    0
}

/// `RegisterScriptString`; true when every declaration was accepted.
///
/// # Safety
/// `e` is a live engine.
pub unsafe fn register_string(e: *mut c_void) -> bool {
    static FACTORY: StringFactory = StringFactory {
        get: factory_get,
        release: factory_release,
        raw: factory_raw,
    };
    let beh = |b: c_int, d: &str, f: unsafe extern "C" fn(Gen)| {
        // SAFETY: plain call with a generic-convention function.
        unsafe {
            ohpl_as2_reg_object_behaviour(
                e,
                b,
                c"string".as_ptr(),
                cs(d).as_ptr(),
                f as *const c_void,
                std::ptr::null_mut(),
            ) >= 0
        }
    };
    let meth = |d: &str, f: unsafe extern "C" fn(Gen)| {
        // SAFETY: as above.
        unsafe {
            ohpl_as2_reg_object_method(
                e,
                c"string".as_ptr(),
                cs(d).as_ptr(),
                f as *const c_void,
                std::ptr::null_mut(),
            ) >= 0
        }
    };
    // SAFETY: plain call.
    let mut ok = unsafe { ohpl_as_reg_object_type(e, c"string".as_ptr(), 0, OBJ_REF) } >= 0;
    ok &= beh(BEHAVE_FACTORY, "string @f()", s_factory);
    ok &= beh(
        BEHAVE_FACTORY,
        "string @f(const string &in)",
        s_factory_copy,
    );
    ok &= beh(BEHAVE_ADDREF, "void f()", s_add_ref);
    ok &= beh(BEHAVE_RELEASE, "void f()", s_release);
    ok &= meth("string &opAssign(const string &in)", s_assign);
    ok &= meth("string &opAddAssign(const string &in)", s_add_assign);
    // SAFETY: plain call; the factory is static.
    ok &= unsafe { ohpl_as3_reg_string_factory(e, c"string".as_ptr(), &raw const FACTORY) } >= 0;
    ok &= meth("bool opEquals(const string &in) const", s_equals);
    ok &= meth("int opCmp(const string &in) const", s_cmp);
    ok &= meth("string@ opAdd(const string &in) const", s_add);
    ok &= meth("uint8 &opIndex(uint)", s_char_at);
    ok &= meth("const uint8 &opIndex(uint) const", s_char_at);
    ok &= meth("uint64 length() const", s_length);
    ok &= meth("void resize(uint64)", s_resize);
    macro_rules! conv {
        ($k:literal, $ty:literal) => {
            ok &= meth(concat!("string &opAssign(", $ty, ")"), s_assign_k::<$k>);
            ok &= meth(
                concat!("string &opAddAssign(", $ty, ")"),
                s_add_assign_k::<$k>,
            );
            ok &= meth(concat!("string@ opAdd(", $ty, ") const"), s_add_k::<$k>);
            ok &= meth(concat!("string@ opAdd_r(", $ty, ") const"), s_add_r_k::<$k>);
        };
    }
    conv!(0, "double");
    conv!(1, "float");
    conv!(2, "int");
    conv!(3, "uint");
    conv!(4, "bool");
    ok
}

pub fn host() -> &'static dyn Host {
    &SHIM
}

/// C entry points for the code that holds `array<T>` objects: the SOMA natives and host tables.
pub mod c_api {
    use super::*;

    unsafe fn a<'x>(p: *const c_void) -> Option<&'x Array> {
        // SAFETY: caller passes an array the engine handed to the native.
        unsafe { p.cast::<Array>().as_ref() }
    }

    /// # Safety
    /// `p` is a live `array<T>`.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_asx_arr_size(p: *const c_void) -> u32 {
        // SAFETY: caller contract.
        unsafe { a(p) }.map_or(0, Array::size)
    }

    /// # Safety
    /// `p` is a live `array<T>`.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_asx_arr_at(p: *mut c_void, i: u32) -> *mut c_void {
        // SAFETY: caller contract.
        unsafe { a(p) }.map_or(std::ptr::null_mut(), |r| r.at(i).cast())
    }

    /// # Safety
    /// `p` is a live `array<T>`.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_asx_arr_resize(p: *mut c_void, n: u32) {
        // SAFETY: caller contract.
        if let Some(r) = unsafe { a(p) } {
            r.resize(n);
        }
    }

    /// # Safety
    /// `p` is a live `array<T>`; `v` points at a value of its element type.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_asx_arr_set(p: *mut c_void, i: u32, v: *mut c_void) {
        // SAFETY: caller contract.
        if let Some(r) = unsafe { a(p) } {
            r.set_value(i, v as P);
        }
    }

    /// # Safety
    /// `p` is a live `array<T>`; `v` points at a value of its element type.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_asx_arr_insert_last(p: *mut c_void, v: *mut c_void) {
        // SAFETY: caller contract.
        if let Some(r) = unsafe { a(p) } {
            r.insert_last(v as P);
        }
    }

    /// # Safety
    /// `p` is a live `array<T>`.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_asx_arr_insert_at(p: *mut c_void, i: u32, v: *mut c_void) {
        // SAFETY: caller contract.
        if let Some(r) = unsafe { a(p) } {
            r.insert_at(i, v as P);
        }
    }

    /// # Safety
    /// `p` is a live `array<T>`.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_asx_arr_remove_at(p: *mut c_void, i: u32) {
        // SAFETY: caller contract.
        if let Some(r) = unsafe { a(p) } {
            r.remove_at(i);
        }
    }

    /// # Safety
    /// `p` is a live `array<T>`.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_asx_arr_remove_last(p: *mut c_void) {
        // SAFETY: caller contract.
        if let Some(r) = unsafe { a(p) } {
            r.remove_last();
        }
    }

    /// # Safety
    /// `p` is a live `array<T>`.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_asx_arr_elem_type(p: *const c_void) -> i32 {
        // SAFETY: caller contract.
        unsafe { a(p) }.map_or(0, Array::elem_type_id)
    }

    /// Bytes of a script `string` (the Amnesia reference type).
    ///
    /// # Safety
    /// `s` is a live `string`.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_asx_str_data(s: *const c_void) -> *const c_char {
        // SAFETY: caller contract.
        unsafe { s.cast::<ScriptString>().as_ref() }
            .map_or(c"".as_ptr(), |s| s.bytes().as_ptr().cast())
    }

    /// # Safety
    /// `s` is a live `string`.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_asx_str_len(s: *const c_void) -> usize {
        // SAFETY: caller contract.
        unsafe { s.cast::<ScriptString>().as_ref() }.map_or(0, |s| s.bytes().len())
    }

    /// `RegisterScriptArray(engine, defaultArray)`.
    ///
    /// # Safety
    /// `e` is a live engine.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_asx_register_array(
        e: *mut c_void,
        default_array: c_int,
    ) -> c_int {
        c_int::from(unsafe { register_array(e, default_array != 0) })
    }
}
