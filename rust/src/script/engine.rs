//! AngelScript engine setup and the registration API.
//!
//! Rust `extern "C"` functions register directly as `Cdecl` (globals) or `CdeclObjFirst`/`CdeclObjLast`
//! (methods, behaviours); the L7 probe in `cshim/probe` verifies every convention. Rules it found:
//! - value types passed or returned by value must not carry the constructor/assignment/copy flags;
//! - a handle parameter transfers one reference to the callee, which must release it;
//! - a handle return transfers one reference to the engine.

use super::sys;
use crate::sdlsys::{SCRIPT_MSG_ERROR, SCRIPT_MSG_INFO, SCRIPT_MSG_WARNING, script_message};
use std::cell::RefCell;
use std::ffi::{CStr, CString, c_int, c_void};
use std::ptr::NonNull;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct AsError(pub i32);

impl AsError {
    const BAD_ARG: Self = Self(sys::INVALID_ARG);
}

pub type AsResult = Result<(), AsError>;

fn check(r: c_int) -> AsResult {
    if r < 0 { Err(AsError(r)) } else { Ok(()) }
}

fn cs(s: &str) -> Result<CString, AsError> {
    CString::new(s).map_err(|_| AsError::BAD_ARG)
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(i32)]
pub enum CallConv {
    Cdecl = sys::CALL_CDECL,
    Stdcall = sys::CALL_STDCALL,
    CdeclObjLast = sys::CALL_CDECL_OBJLAST,
    CdeclObjFirst = sys::CALL_CDECL_OBJFIRST,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(i32)]
pub enum Behaviour {
    Construct = sys::BEHAVE_CONSTRUCT,
    Destruct = sys::BEHAVE_DESTRUCT,
    Factory = sys::BEHAVE_FACTORY,
    AddRef = sys::BEHAVE_ADDREF,
    Release = sys::BEHAVE_RELEASE,
}

/// `asOBJ_*` flags for [`EngineRef::object_type`].
pub mod obj {
    pub use super::sys::{
        OBJ_APP_CLASS as APP_CLASS, OBJ_APP_CLASS_ALLFLOATS as APP_CLASS_ALLFLOATS,
        OBJ_APP_CLASS_ALLINTS as APP_CLASS_ALLINTS, OBJ_NOHANDLE as NOHANDLE, OBJ_POD as POD,
        OBJ_REF as REF, OBJ_VALUE as VALUE,
    };
}

/// Borrowed `asIScriptEngine*`; the owner is a [`ScriptEngine`] or C++.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct EngineRef(NonNull<c_void>);

impl EngineRef {
    pub fn from_raw(p: *mut c_void) -> Option<Self> {
        NonNull::new(p).map(Self)
    }

    pub fn as_raw(self) -> *mut c_void {
        self.0.as_ptr()
    }

    /// # Safety
    /// The pointer must match the declaration and calling convention, and outlive the engine.
    pub unsafe fn global_function(self, decl: &str, f: *const c_void, conv: CallConv) -> AsResult {
        let d = cs(decl)?;
        // SAFETY: `d` outlives the call; `f` is an `extern "C"` function matching `decl`.
        check(unsafe {
            sys::ohpl_as_reg_global_function(self.as_raw(), d.as_ptr(), f, conv as c_int)
        })
    }

    /// # Safety
    /// The pointer must match the declaration and calling convention, and outlive the engine.
    pub unsafe fn global_property(self, decl: &str, p: *mut c_void) -> AsResult {
        let d = cs(decl)?;
        // SAFETY: `d` outlives the call; `p` must stay valid while the engine lives.
        check(unsafe { sys::ohpl_as_reg_global_property(self.as_raw(), d.as_ptr(), p) })
    }

    pub fn object_type(self, name: &str, size: usize, flags: u64) -> AsResult {
        let n = cs(name)?;
        let size = c_int::try_from(size).map_err(|_| AsError::BAD_ARG)?;
        // SAFETY: `n` outlives the call.
        check(unsafe { sys::ohpl_as_reg_object_type(self.as_raw(), n.as_ptr(), size, flags) })
    }

    pub fn object_property(self, obj: &str, decl: &str, offset: usize) -> AsResult {
        let (o, d) = (cs(obj)?, cs(decl)?);
        let offset = c_int::try_from(offset).map_err(|_| AsError::BAD_ARG)?;
        // SAFETY: strings outlive the call.
        check(unsafe {
            sys::ohpl_as_reg_object_property(self.as_raw(), o.as_ptr(), d.as_ptr(), offset)
        })
    }

    /// # Safety
    /// The pointer must match the declaration and calling convention, and outlive the engine.
    pub unsafe fn object_method(
        self,
        obj: &str,
        decl: &str,
        f: *const c_void,
        conv: CallConv,
    ) -> AsResult {
        let (o, d) = (cs(obj)?, cs(decl)?);
        // SAFETY: strings outlive the call; `f` is an `extern "C"` function matching `decl` and `conv`.
        check(unsafe {
            sys::ohpl_as_reg_object_method(self.as_raw(), o.as_ptr(), d.as_ptr(), f, conv as c_int)
        })
    }

    /// # Safety
    /// The pointer must match the declaration and calling convention, and outlive the engine.
    pub unsafe fn object_behaviour(
        self,
        b: Behaviour,
        obj: &str,
        decl: &str,
        f: *const c_void,
        conv: CallConv,
    ) -> AsResult {
        let (o, d) = (cs(obj)?, cs(decl)?);
        // SAFETY: as `object_method`.
        check(unsafe {
            sys::ohpl_as_reg_object_behaviour(
                self.as_raw(),
                b as c_int,
                o.as_ptr(),
                d.as_ptr(),
                f,
                conv as c_int,
            )
        })
    }

    pub fn enum_type(self, name: &str) -> AsResult {
        let n = cs(name)?;
        // SAFETY: `n` outlives the call.
        check(unsafe { sys::ohpl_as_reg_enum(self.as_raw(), n.as_ptr()) })
    }

    pub fn enum_value(self, name: &str, value: &str, v: i32) -> AsResult {
        let (n, s) = (cs(name)?, cs(value)?);
        // SAFETY: strings outlive the call.
        check(unsafe { sys::ohpl_as_reg_enum_value(self.as_raw(), n.as_ptr(), s.as_ptr(), v) })
    }

    pub fn funcdef(self, decl: &str) -> AsResult {
        let d = cs(decl)?;
        // SAFETY: `d` outlives the call.
        check(unsafe { sys::ohpl_as_reg_funcdef(self.as_raw(), d.as_ptr()) })
    }

    pub fn typedef(self, ty: &str, decl: &str) -> AsResult {
        let (t, d) = (cs(ty)?, cs(decl)?);
        // SAFETY: strings outlive the call.
        check(unsafe { sys::ohpl_as_reg_typedef(self.as_raw(), t.as_ptr(), d.as_ptr()) })
    }

    pub fn default_namespace(self, ns: &str) -> AsResult {
        let n = cs(ns)?;
        // SAFETY: `n` outlives the call.
        check(unsafe { sys::ohpl_as_set_default_namespace(self.as_raw(), n.as_ptr()) })
    }

    pub fn engine_property(self, prop: i32, value: usize) -> AsResult {
        // SAFETY: plain call.
        check(unsafe { sys::ohpl_as_set_engine_property(self.as_raw(), prop, value) })
    }
}

/// Bytes of a script `string` handed to a registered function as `const string &in`.
///
/// # Safety
/// `s` is the pointer the engine passed for a `string` parameter and stays valid for `'a`.
pub unsafe fn script_str<'a>(s: *const c_void) -> &'a [u8] {
    // SAFETY: caller contract; the accessor reads the `std::string` at offset 0 of the script string.
    unsafe {
        let n = sys::ohpl_as_str_len(s);
        if n == 0 {
            return &[];
        }
        std::slice::from_raw_parts(sys::ohpl_as_str_data(s).cast::<u8>(), n)
    }
}

/// Returns a new script string for a registered function declared `string@`.
pub fn new_script_str(b: &[u8]) -> *mut c_void {
    // SAFETY: the slice is valid for the call; the shim copies it.
    unsafe { sys::ohpl_as_str_new(b.as_ptr().cast(), b.len()) }
}

/// `cScriptOutput`: engine messages formatted as the C++ class does, until a build reads them.
#[derive(Default)]
pub struct Output {
    msg: RefCell<Vec<u8>>,
}

impl Output {
    pub fn take(&self) -> Vec<u8> {
        self.msg
            .try_borrow_mut()
            .map(|mut m| std::mem::take(&mut *m))
            .unwrap_or_default()
    }

    pub fn peek(&self) -> Vec<u8> {
        self.msg.try_borrow().map(|m| m.clone()).unwrap_or_default()
    }

    pub fn clear(&self) {
        if let Ok(mut m) = self.msg.try_borrow_mut() {
            m.clear();
        }
    }

    pub fn set(&self, v: Vec<u8>) {
        if let Ok(mut m) = self.msg.try_borrow_mut() {
            *m = v;
        }
    }

    fn add(&self, section: &[u8], row: i32, col: i32, kind: u32, message: &[u8]) {
        if let Ok(mut m) = self.msg.try_borrow_mut() {
            m.extend_from_slice(&script_message(section, row, col, kind, message));
        }
    }
}

fn bytes<'a>(p: *const std::ffi::c_char) -> &'a [u8] {
    if p.is_null() {
        return b"(null)";
    }
    // SAFETY: the engine passes NUL-terminated strings.
    unsafe { CStr::from_ptr(p) }.to_bytes()
}

/// # Safety
/// `m` is the engine's message and `ctx` the `Output` registered by [`ScriptEngine::new`].
unsafe extern "C" fn on_message(m: *const sys::AsMsg, ctx: *mut c_void) {
    // SAFETY: caller contract; only shared access, the interior is a RefCell.
    let (Some(m), Some(out)) = (unsafe { m.as_ref() }, unsafe {
        ctx.cast::<Output>().as_ref()
    }) else {
        return;
    };
    let kind = match m.kind {
        sys::MSG_WARNING => SCRIPT_MSG_WARNING,
        sys::MSG_INFO => SCRIPT_MSG_INFO,
        _ => SCRIPT_MSG_ERROR,
    };
    out.add(bytes(m.section), m.row, m.col, kind, bytes(m.message));
}

/// `cLowLevelSystemSDL`'s engine: created with the message sink and the string and array add-ons.
pub struct ScriptEngine {
    raw: EngineRef,
    pub output: Box<Output>,
}

impl ScriptEngine {
    pub fn new() -> Option<Self> {
        // SAFETY: plain call.
        let raw = EngineRef::from_raw(unsafe { sys::ohpl_as_engine_new() })?;
        let output = Box::<Output>::default();
        // SAFETY: `output` is heap-pinned and outlives the engine (dropped after `Release` in `Drop`).
        unsafe {
            sys::ohpl_as_set_message_cb(
                raw.as_raw(),
                on_message,
                std::ptr::from_ref::<Output>(&output).cast_mut().cast(),
            );
            sys::ohpl_as_register_addons(raw.as_raw());
        }
        Some(Self { raw, output })
    }

    pub fn engine(&self) -> EngineRef {
        self.raw
    }

    /// `AddScriptFunc`: always `asCALL_STDCALL`, which is cdecl on aarch64.
    /// # Safety
    /// `f` matches `decl` and outlives the engine.
    pub unsafe fn add_func(&self, decl: &str, f: *const c_void) -> bool {
        // SAFETY: caller contract.
        unsafe { self.raw.global_function(decl, f, CallConv::Stdcall) }.is_ok()
    }

    /// # Safety
    /// `p` outlives the engine.
    pub unsafe fn add_var(&self, decl: &str, p: *mut c_void) -> bool {
        // SAFETY: caller contract.
        unsafe { self.raw.global_property(decl, p) }.is_ok()
    }
}

impl Drop for ScriptEngine {
    fn drop(&mut self) {
        // SAFETY: the engine is released once; scripts must be gone, as in the C++ order.
        unsafe { sys::ohpl_as_engine_release(self.raw.as_raw()) };
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn output_formats_like_cscriptoutput() {
        let o = Output::default();
        o.add(b"main", 3, 4, SCRIPT_MSG_ERROR, b"boom");
        o.add(b"main", 5, 1, SCRIPT_MSG_WARNING, b"hm");
        assert_eq!(
            o.peek(),
            b"main (3, 4) : ERR  : boom\nmain (5, 1) : WARN : hm\n"
        );
        assert_eq!(o.take().len(), 50);
        assert!(o.peek().is_empty());
    }

    #[test]
    fn nul_in_decl_is_rejected() {
        assert_eq!(cs("a\0b").err(), Some(AsError::BAD_ARG));
    }
}
