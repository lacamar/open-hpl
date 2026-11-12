//! Bindings to `cshim/as_shim.h`; constants are asserted against `angelscript.h` in `cshim/layout_check.cpp`.

use std::ffi::{c_char, c_int, c_uint, c_void};

#[repr(C)]
pub struct AsMsg {
    pub section: *const c_char,
    pub row: i32,
    pub col: i32,
    pub kind: i32,
    pub message: *const c_char,
}

pub type MsgFn = unsafe extern "C" fn(*const AsMsg, *mut c_void);

pub const CALL_CDECL: c_int = 0;
pub const CALL_STDCALL: c_int = 1;
pub const CALL_CDECL_OBJLAST: c_int = 4;
pub const CALL_CDECL_OBJFIRST: c_int = 5;

pub const GM_ALWAYS_CREATE: c_int = 2;

pub const INVALID_ARG: c_int = -5;

pub const OBJ_REF: u64 = 1;
pub const OBJ_VALUE: u64 = 1 << 1;
pub const OBJ_POD: u64 = 1 << 3;
pub const OBJ_NOHANDLE: u64 = 1 << 4;
pub const OBJ_APP_CLASS: u64 = 1 << 8;
pub const OBJ_APP_CLASS_ALLINTS: u64 = 1 << 16;
pub const OBJ_APP_CLASS_ALLFLOATS: u64 = 1 << 17;

pub const BEHAVE_CONSTRUCT: c_int = 0;
pub const BEHAVE_DESTRUCT: c_int = 2;
pub const BEHAVE_FACTORY: c_int = 3;
pub const BEHAVE_ADDREF: c_int = 5;
pub const BEHAVE_RELEASE: c_int = 6;

pub const MSG_ERROR: i32 = 0;
pub const MSG_WARNING: i32 = 1;
pub const MSG_INFO: i32 = 2;

pub const EXECUTION_FINISHED: c_int = 0;

// SAFETY (all blocks below): plain calls into the shim; pointers are valid for the call.
unsafe extern "C" {
    pub fn ohpl_as_engine_new() -> *mut c_void;
    pub fn ohpl_as_engine_release(e: *mut c_void) -> c_int;
    pub fn ohpl_as_set_message_cb(e: *mut c_void, f: MsgFn, ctx: *mut c_void) -> c_int;
    pub fn ohpl_as_reg_global_function(
        e: *mut c_void,
        decl: *const c_char,
        f: *const c_void,
        conv: c_int,
    ) -> c_int;
    pub fn ohpl_as_reg_global_property(
        e: *mut c_void,
        decl: *const c_char,
        p: *mut c_void,
    ) -> c_int;
    pub fn ohpl_as_reg_object_type(
        e: *mut c_void,
        name: *const c_char,
        size: c_int,
        flags: u64,
    ) -> c_int;
    pub fn ohpl_as_reg_object_property(
        e: *mut c_void,
        obj: *const c_char,
        decl: *const c_char,
        offset: c_int,
    ) -> c_int;
    pub fn ohpl_as_reg_object_method(
        e: *mut c_void,
        obj: *const c_char,
        decl: *const c_char,
        f: *const c_void,
        conv: c_int,
    ) -> c_int;
    pub fn ohpl_as_reg_object_behaviour(
        e: *mut c_void,
        behaviour: c_int,
        obj: *const c_char,
        decl: *const c_char,
        f: *const c_void,
        conv: c_int,
    ) -> c_int;
    pub fn ohpl_as_reg_enum(e: *mut c_void, name: *const c_char) -> c_int;
    pub fn ohpl_as_reg_enum_value(
        e: *mut c_void,
        name: *const c_char,
        value: *const c_char,
        v: c_int,
    ) -> c_int;
    pub fn ohpl_as_reg_funcdef(e: *mut c_void, decl: *const c_char) -> c_int;
    pub fn ohpl_as_reg_typedef(e: *mut c_void, ty: *const c_char, decl: *const c_char) -> c_int;
    pub fn ohpl_as_set_default_namespace(e: *mut c_void, ns: *const c_char) -> c_int;
    pub fn ohpl_as_set_engine_property(e: *mut c_void, prop: c_int, value: usize) -> c_int;
    pub fn ohpl_as_get_module(e: *mut c_void, name: *const c_char, flag: c_int) -> *mut c_void;
    pub fn ohpl_as_discard_module(e: *mut c_void, name: *const c_char) -> c_int;
    pub fn ohpl_as_module_add_section(
        m: *mut c_void,
        name: *const c_char,
        code: *const c_char,
        len: usize,
    ) -> c_int;
    pub fn ohpl_as_module_build(m: *mut c_void) -> c_int;
    pub fn ohpl_as_module_function_by_name(m: *mut c_void, name: *const c_char) -> *mut c_void;
    pub fn ohpl_as_ctx_new(e: *mut c_void) -> *mut c_void;
    pub fn ohpl_as_ctx_release(c: *mut c_void) -> c_int;
    pub fn ohpl_as_ctx_prepare(c: *mut c_void, func: *mut c_void) -> c_int;
    pub fn ohpl_as_ctx_execute(c: *mut c_void) -> c_int;
    pub fn ohpl_as_register_addons(e: *mut c_void);
    pub fn ohpl_as_execute_string(
        e: *mut c_void,
        code: *const c_char,
        module: *mut c_void,
    ) -> c_int;
    pub fn ohpl_as_str_data(s: *const c_void) -> *const c_char;
    pub fn ohpl_as_str_len(s: *const c_void) -> usize;
    pub fn ohpl_as_str_new(p: *const c_char, n: usize) -> *mut c_void;
    pub fn ohpl_as_array_size(a: *const c_void) -> c_uint;
    pub fn ohpl_as_array_at(a: *mut c_void, i: c_uint) -> *mut c_void;
}
