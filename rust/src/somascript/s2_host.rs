//! S2: the C++ side of the script natives: recovered struct defaults, the std containers behind
//! `cScriptStringSet` and `array<T>`, the entity-script runner, and the generated engine bindings.

use std::ffi::c_void;
use std::sync::OnceLock;

#[repr(C)]
pub struct OhplS2Defaults {
    pub size: usize,
    pub bytes: *const u8,
    pub nstr: usize,
    pub str_offsets: *const i32,
}

#[repr(C)]
pub struct OhplS2HostOps {
    pub struct_defaults: unsafe extern "C" fn(*const u8, usize, *mut OhplS2Defaults) -> i32,
    pub intern_empty: unsafe extern "C" fn() -> *const c_void,
    pub native_offset: unsafe extern "C" fn(*const u8, usize, *const u8, usize, i32) -> i32,
    pub register_step: unsafe extern "C" fn(*mut c_void, u32),
    pub sset_reset: unsafe extern "C" fn(*mut c_void),
    pub sset_assign: unsafe extern "C" fn(*mut c_void, *mut c_void),
    pub sset_add: unsafe extern "C" fn(*mut c_void, *const c_void),
    pub sset_erase: unsafe extern "C" fn(*mut c_void, *const c_void),
    pub sset_count: unsafe extern "C" fn(*mut c_void, *const c_void) -> i32,
    pub sset_size: unsafe extern "C" fn(*mut c_void) -> i32,
    pub sset_to_array: unsafe extern "C" fn(*mut c_void, *mut c_void),
    pub arr_insert_last: unsafe extern "C" fn(*mut c_void, *mut c_void),
    pub arr_insert_at: unsafe extern "C" fn(*mut c_void, u32, *mut c_void),
    pub arr_remove_last: unsafe extern "C" fn(*mut c_void),
    pub arr_remove_at: unsafe extern "C" fn(*mut c_void, u32),
    pub arr_size: unsafe extern "C" fn(*const c_void) -> u32,
    pub arr_resize: unsafe extern "C" fn(*mut c_void, u32),
    pub run_global_func:
        unsafe extern "C" fn(*const u8, usize, *const u8, usize, *const u8, usize) -> bool,
    pub gen_count: unsafe extern "C" fn() -> usize,
    pub gen_op: unsafe extern "C" fn(usize) -> *const c_void,
    pub global_funcs:
        unsafe extern "C" fn(usize, *mut *const u8, *mut *const u8, *mut *const u8) -> bool,
    pub easing: unsafe extern "C" fn(i32, f32) -> f32,
}

static HOST: OnceLock<&'static OhplS2HostOps> = OnceLock::new();

/// # Safety
/// `ops` points at a table that lives for the whole process.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s2_install_host(ops: *const OhplS2HostOps) {
    // SAFETY: caller contract.
    if let Some(o) = unsafe { ops.as_ref() } {
        let _ = HOST.set(o);
    }
}

pub(crate) fn ops() -> Option<&'static OhplS2HostOps> {
    HOST.get().copied()
}

pub struct Defaults {
    pub size: usize,
    pub bytes: &'static [u8],
    pub string_offsets: &'static [i32],
}

pub fn struct_defaults(ty: &[u8]) -> Option<Defaults> {
    let h = ops()?;
    let mut d = OhplS2Defaults {
        size: 0,
        bytes: std::ptr::null(),
        nstr: 0,
        str_offsets: std::ptr::null(),
    };
    // SAFETY: `ty` is valid for the call; the table the host returns is static.
    unsafe {
        if (h.struct_defaults)(ty.as_ptr(), ty.len(), &mut d) == 0 {
            return None;
        }
        let len = d.size.saturating_sub(16);
        Some(Defaults {
            size: d.size,
            bytes: if len == 0 {
                &[]
            } else {
                std::slice::from_raw_parts(d.bytes, len)
            },
            string_offsets: if d.nstr == 0 {
                &[]
            } else {
                std::slice::from_raw_parts(d.str_offsets, d.nstr)
            },
        })
    }
}

pub fn intern_empty() -> *const c_void {
    // SAFETY: the host returns a pointer to its interned empty string.
    ops().map_or(std::ptr::null(), |h| unsafe { (h.intern_empty)() })
}

pub fn native_offset(ty: &[u8], decl: &[u8], offset: i32) -> i32 {
    // SAFETY: strings are valid for the call.
    ops().map_or(offset, |h| unsafe {
        (h.native_offset)(ty.as_ptr(), ty.len(), decl.as_ptr(), decl.len(), offset)
    })
}

/// Types whose constructors all come from natives (the API's recorded ones are skipped).
pub fn has_native_behaviours(ty: &[u8]) -> bool {
    const TYPES: [&[u8]; 19] = [
        b"cImGuiGfx",
        b"cImGuiFont",
        b"cVector2f",
        b"cVector3f",
        b"cVector4f",
        b"cVector2l",
        b"cVector3l",
        b"cColor",
        b"cMatrixf",
        b"cQuaternion",
        b"cPidControllerVec3",
        b"cPidControllerf",
        b"cRect2f",
        b"cRect2l",
        b"cPlanef",
        b"cDate",
        b"cScriptStringSet",
        b"cCollideData",
        b"cBoundingVolume",
    ];
    TYPES.contains(&ty)
}

pub fn easing(t: i32, x: f32) -> f32 {
    // SAFETY: plain call into the host.
    ops().map_or(0.0, |h| unsafe { (h.easing)(t, x) })
}

pub fn gen_op(i: usize) -> Option<*const c_void> {
    let h = ops()?;
    // SAFETY: plain call into the host.
    let p = unsafe { (h.gen_op)(i) };
    (!p.is_null()).then_some(p)
}
