//! Engine services the Rust-owned gui reaches: resource managers, fonts, materials, the file
//! system and the C++ gui sets (until those are owned here too). One static table, registered at
//! start-up by `RustGuiG1.cpp`; a missing table makes every call a no-op.

use std::ffi::c_void;
use std::sync::atomic::{AtomicPtr, Ordering};

pub const LOG: i32 = 0;
pub const WARNING: i32 = 1;
pub const ERROR: i32 = 2;

pub type Sink = unsafe extern "C" fn(*mut c_void, *const u8, usize);
pub type SinkWide = unsafe extern "C" fn(*mut c_void, *const u32, usize);

#[repr(C)]
pub struct Host {
    pub log: unsafe extern "C" fn(i32, *const u8, usize),
    pub material_new: unsafe extern "C" fn(i32, *mut c_void) -> usize,
    pub material_delete: unsafe extern "C" fn(usize),
    pub image_create: unsafe extern "C" fn(*mut c_void, *const u8, usize) -> usize,
    pub texture_create_2d: unsafe extern "C" fn(*mut c_void, *const u8, usize, bool, i32) -> usize,
    pub texture_render_target: unsafe extern "C" fn(usize) -> bool,
    pub file_found: unsafe extern "C" fn(*mut c_void, *const u8, usize) -> bool,
    pub font_create: unsafe extern "C" fn(*mut c_void, *const u8, usize) -> usize,
    /// `cFileSearcher::GetFilePath`, delivered as wide characters.
    pub find_file: unsafe extern "C" fn(*mut c_void, *const u8, usize, SinkWide, *mut c_void),
    /// `cPlatform::OpenFile(path, "rb")` and read it whole into `sink`: 0 does not open, 1 read, 2 short read.
    pub read_file: unsafe extern "C" fn(*const u32, usize, Sink, *mut c_void) -> i32,
    pub screen_size: unsafe extern "C" fn(*mut c_void, *mut [f32; 2]),
}

static HOST: AtomicPtr<Host> = AtomicPtr::new(std::ptr::null_mut());

/// # Safety
/// `host` is a static table that outlives every call into the gui.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_set_host(host: *mut Host) {
    HOST.store(host, Ordering::Release);
}

pub fn host() -> Option<&'static Host> {
    // SAFETY: `ohpl_g1_set_host` contract.
    unsafe { HOST.load(Ordering::Acquire).as_ref() }
}

pub fn log(kind: i32, msg: &str) {
    if let Some(h) = host() {
        // SAFETY: the host copies the text before returning.
        unsafe { (h.log)(kind, msg.as_ptr(), msg.len()) };
    }
}
