//! S2: the C++ side of the script natives: recovered struct defaults, the std containers behind
//! `cScriptStringSet` and `array<T>`, the entity-script runner, and the generated engine bindings.

use crate::string_ffi::Sink;
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
    pub misc: unsafe extern "C" fn(u32, i32, *const u8, usize) -> i32,
    pub game_id: unsafe extern "C" fn(Sink<u8>, *mut c_void),
    pub register_array: unsafe extern "C" fn(*mut c_void),
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

pub const MISC_UNDERWATER_GET: u32 = 0;
pub const MISC_UNDERWATER_SET: u32 = 1;
pub const MISC_UNDERWATER_INC: u32 = 2;
pub const MISC_UNDERWATER_DEC: u32 = 3;
pub const MISC_UNDERWATER_USERS: u32 = 4;
pub const MISC_EXEC_PRINT: u32 = 5;
pub const MISC_LOG: u32 = 6;

pub fn misc(kind: u32, arg: i32, text: &[u8]) -> i32 {
    // SAFETY: the text is valid for the call.
    ops().map_or(0, |h| unsafe {
        (h.misc)(kind, arg, text.as_ptr(), text.len())
    })
}

pub fn run_global_func(obj: &[u8], class: &[u8], func: &[u8]) -> bool {
    // SAFETY: the strings are valid for the call.
    ops().is_some_and(|h| unsafe {
        (h.run_global_func)(
            obj.as_ptr(),
            obj.len(),
            class.as_ptr(),
            class.len(),
            func.as_ptr(),
            func.len(),
        )
    })
}

pub fn global_func(i: usize) -> Option<(Vec<u8>, Vec<u8>, Vec<u8>)> {
    let h = ops()?;
    let (mut d, mut c, mut f) = (std::ptr::null(), std::ptr::null(), std::ptr::null());
    // SAFETY: the host returns NUL-terminated strings of a static table.
    unsafe {
        if !(h.global_funcs)(i, &mut d, &mut c, &mut f) {
            return None;
        }
        let s = |p: *const u8| std::ffi::CStr::from_ptr(p.cast()).to_bytes().to_vec();
        Some((s(d), s(c), s(f)))
    }
}

pub fn game_id() -> Vec<u8> {
    unsafe extern "C" fn sink(ctx: *mut c_void, p: *const u8, n: usize) {
        // SAFETY: `ctx` is the `Vec` below; the host passes `n` valid bytes.
        unsafe { (*ctx.cast::<Vec<u8>>()).extend_from_slice(std::slice::from_raw_parts(p, n)) }
    }
    let mut v = Vec::new();
    if let Some(h) = ops() {
        // SAFETY: `v` outlives the call.
        unsafe { (h.game_id)(sink, std::ptr::from_mut(&mut v).cast()) };
    }
    v
}

pub fn register_array(engine: *mut c_void) {
    // SAFETY: plain call into the host.
    if let Some(h) = ops() {
        unsafe { (h.register_array)(engine) };
    }
}
