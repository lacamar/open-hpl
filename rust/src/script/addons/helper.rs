//! `ExecuteString`: compiles a code fragment as a function of a module and runs it.

use super::sys::*;
use std::ffi::{c_int, c_void};

/// `module` null runs it in a scratch module named `ExecuteString`. Returns the engine's result code.
///
/// # Safety
/// `engine` is live and `module` is null or a live module of it.
pub unsafe fn execute_string(engine: *mut c_void, code: &[u8], module: *mut c_void) -> c_int {
    let wrapped: Vec<u8> = [b"void ExecuteString() {\n".as_slice(), code, b"\n;}"].concat();
    let Ok(src) = std::ffi::CString::new(wrapped) else {
        return -1;
    };
    // SAFETY: plain calls; `engine` is live and `module` is null or a live module.
    unsafe {
        let m = if module.is_null() {
            ohpl_as_get_module(engine, c"ExecuteString".as_ptr(), GM_ALWAYS_CREATE)
        } else {
            module
        };
        let mut func = std::ptr::null_mut();
        let r =
            ohpl_as3_compile_function(m, c"ExecuteString".as_ptr(), src.as_ptr(), &raw mut func);
        if r < 0 {
            return r;
        }
        let ctx = ohpl_as3_request_ctx(engine);
        let mut r = ohpl_as_ctx_prepare(ctx, func);
        if r >= 0 {
            r = ohpl_as_ctx_execute(ctx);
        }
        ohpl_as3_func_release(func);
        ohpl_as3_return_ctx(engine, ctx);
        r
    }
}
