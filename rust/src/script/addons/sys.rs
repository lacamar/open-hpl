//! Bindings to `cshim/as_shim_addon.h` and the parts of `as_shim.h` / `as_shim_s2.h` the add-ons use.
//! Constants are asserted against `angelscript.h` in `cshim/layout_check.cpp`.

pub use crate::script::sys::{
    ohpl_as_ctx_execute, ohpl_as_ctx_new, ohpl_as_ctx_prepare, ohpl_as_ctx_release,
    ohpl_as_get_module, ohpl_as_reg_object_type,
};
use std::ffi::{c_char, c_int, c_void};

pub const BEHAVE_FACTORY: c_int = 3;
pub const BEHAVE_LIST_FACTORY: c_int = 4;
pub const BEHAVE_ADDREF: c_int = 5;
pub const BEHAVE_RELEASE: c_int = 6;
pub const BEHAVE_TEMPLATE_CALLBACK: c_int = 8;
pub const BEHAVE_GETREFCOUNT: c_int = 9;
pub const BEHAVE_SETGCFLAG: c_int = 10;
pub const BEHAVE_GETGCFLAG: c_int = 11;
pub const BEHAVE_ENUMREFS: c_int = 12;
pub const BEHAVE_RELEASEREFS: c_int = 13;

pub const OBJ_REF: u64 = 1;
pub const OBJ_GC: u64 = 1 << 2;
pub const OBJ_TEMPLATE: u64 = 1 << 6;

pub const MSG_ERROR: c_int = 0;
pub const EXECUTION_FINISHED: c_int = 0;
pub const GM_ALWAYS_CREATE: c_int = 2;

#[repr(C)]
pub struct StringFactory {
    pub get: unsafe extern "C" fn(*const c_char, u32) -> *const c_void,
    pub release: unsafe extern "C" fn(*const c_void) -> c_int,
    pub raw: unsafe extern "C" fn(*const c_void, *mut c_char, *mut u32) -> c_int,
}

// SAFETY (all blocks below): plain calls into the shim; pointers are valid for the call.
unsafe extern "C" {
    pub fn ohpl_as2_g_object(g: *mut c_void) -> *mut c_void;
    pub fn ohpl_as2_g_arg_addr(g: *mut c_void, i: u32) -> *mut c_void;
    pub fn ohpl_as2_g_ret_loc(g: *mut c_void) -> *mut c_void;
    pub fn ohpl_as2_reg_object_method(
        e: *mut c_void,
        ty: *const c_char,
        decl: *const c_char,
        f: *const c_void,
        aux: *mut c_void,
    ) -> c_int;
    pub fn ohpl_as2_reg_object_behaviour(
        e: *mut c_void,
        b: c_int,
        ty: *const c_char,
        decl: *const c_char,
        f: *const c_void,
        aux: *mut c_void,
    ) -> c_int;
    pub fn ohpl_as2_type_by_id(e: *mut c_void, id: c_int) -> *mut c_void;
    pub fn ohpl_as2_type_id(t: *mut c_void) -> c_int;
    pub fn ohpl_as2_type_name(t: *mut c_void) -> *const c_char;
    pub fn ohpl_as2_type_size(t: *mut c_void) -> c_int;
    pub fn ohpl_as2_type_method_by_decl(t: *mut c_void, d: *const c_char) -> *mut c_void;
    pub fn ohpl_as2_func_name(f: *mut c_void) -> *const c_char;
    pub fn ohpl_as2_func_return(f: *mut c_void, flags: *mut u32) -> c_int;
    pub fn ohpl_as2_func_param_count(f: *mut c_void) -> u32;
    pub fn ohpl_as2_func_param(f: *mut c_void, i: u32, id: *mut c_int, fl: *mut u32) -> c_int;
    pub fn ohpl_as2_create_object(e: *mut c_void, t: *mut c_void) -> *mut c_void;

    pub fn ohpl_as_reg_funcdef(e: *mut c_void, decl: *const c_char) -> c_int;
    pub fn ohpl_as_ctx_state(c: *mut c_void) -> c_int;
    pub fn ohpl_as_ctx_abort(c: *mut c_void) -> c_int;
    pub fn ohpl_as_ctx_set_object(c: *mut c_void, obj: *mut c_void) -> c_int;
    pub fn ohpl_as_ctx_set_arg_object(c: *mut c_void, arg: u32, v: *mut c_void) -> c_int;
    pub fn ohpl_as_ctx_set_arg_address(c: *mut c_void, arg: u32, v: *mut c_void) -> c_int;
    pub fn ohpl_as_ctx_ret_dword(c: *mut c_void) -> u32;

    pub fn ohpl_as3_active_ctx() -> *mut c_void;
    pub fn ohpl_as3_lock(acquire: c_int);
    pub fn ohpl_as3_type_engine(t: *mut c_void) -> *mut c_void;
    pub fn ohpl_as3_type_sub_id(t: *mut c_void) -> c_int;
    pub fn ohpl_as3_type_sub(t: *mut c_void) -> *mut c_void;
    pub fn ohpl_as3_type_flags(t: *mut c_void) -> u64;
    pub fn ohpl_as3_type_addref(t: *mut c_void);
    pub fn ohpl_as3_type_release(t: *mut c_void);
    pub fn ohpl_as3_type_user(t: *mut c_void, key: usize) -> *mut c_void;
    pub fn ohpl_as3_type_set_user(t: *mut c_void, p: *mut c_void, key: usize);
    pub fn ohpl_as3_type_behaviour_count(t: *mut c_void) -> u32;
    pub fn ohpl_as3_type_behaviour(t: *mut c_void, i: u32, beh: *mut c_int) -> *mut c_void;
    pub fn ohpl_as3_type_factory_count(t: *mut c_void) -> u32;
    pub fn ohpl_as3_type_factory(t: *mut c_void, i: u32) -> *mut c_void;
    pub fn ohpl_as3_type_method_count(t: *mut c_void) -> u32;
    pub fn ohpl_as3_type_method(t: *mut c_void, i: u32) -> *mut c_void;
    pub fn ohpl_as3_func_read_only(f: *mut c_void) -> c_int;
    pub fn ohpl_as3_engine_prop(e: *mut c_void, prop: c_int) -> c_int;
    pub fn ohpl_as3_write_message(
        e: *mut c_void,
        section: *const c_char,
        row: c_int,
        col: c_int,
        ty: c_int,
        msg: *const c_char,
    );
    pub fn ohpl_as3_type_decl(e: *mut c_void, id: c_int, ns: c_int) -> *const c_char;
    pub fn ohpl_as3_assign_object(
        e: *mut c_void,
        dst: *mut c_void,
        src: *mut c_void,
        t: *mut c_void,
    ) -> c_int;
    pub fn ohpl_as3_addref_object(e: *mut c_void, obj: *mut c_void, t: *mut c_void);
    pub fn ohpl_as3_release_object(e: *mut c_void, obj: *mut c_void, t: *mut c_void);
    pub fn ohpl_as3_notify_gc(e: *mut c_void, obj: *mut c_void, t: *mut c_void);
    pub fn ohpl_as3_gc_enum(e: *mut c_void, obj: *mut c_void);
    pub fn ohpl_as3_gc_forward_enum(e: *mut c_void, obj: *mut c_void, t: *mut c_void);
    pub fn ohpl_as3_request_ctx(e: *mut c_void) -> *mut c_void;
    pub fn ohpl_as3_return_ctx(e: *mut c_void, c: *mut c_void);
    pub fn ohpl_as3_type_cleanup_cb(
        e: *mut c_void,
        cb: unsafe extern "C" fn(*mut c_void),
        key: usize,
    );
    pub fn ohpl_as3_reg_default_array(e: *mut c_void, decl: *const c_char) -> c_int;
    pub fn ohpl_as3_reg_string_factory(
        e: *mut c_void,
        ty: *const c_char,
        f: *const StringFactory,
    ) -> c_int;
    pub fn ohpl_as3_ctx_engine(c: *mut c_void) -> *mut c_void;
    pub fn ohpl_as3_ctx_set_exception(c: *mut c_void, info: *const c_char) -> c_int;
    pub fn ohpl_as3_ctx_push(c: *mut c_void) -> c_int;
    pub fn ohpl_as3_ctx_pop(c: *mut c_void) -> c_int;
    pub fn ohpl_as3_ctx_ret_byte(c: *mut c_void) -> u8;
    pub fn ohpl_as3_ctx_ret_addr_of(c: *mut c_void) -> *mut c_void;
    pub fn ohpl_as3_compile_function(
        m: *mut c_void,
        name: *const c_char,
        code: *const c_char,
        out: *mut *mut c_void,
    ) -> c_int;
    pub fn ohpl_as3_func_release(f: *mut c_void);
}
