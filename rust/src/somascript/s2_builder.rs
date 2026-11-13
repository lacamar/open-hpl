//! S2: `cSomaScriptBuilder::Build` on the Rust path: collect the sections, reuse or write the
//! bytecode cache, build the module.

use super::builder::{self, Builder};
use super::s2_as::Engine;
use super::s2_host;
use std::ffi::{CString, c_char, c_int, c_void};
use std::os::unix::ffi::OsStrExt;
use std::os::unix::fs::MetadataExt;
use std::path::Path;

const GM_ALWAYS_CREATE: c_int = 2;
const AS_ERROR: c_int = -1;

unsafe extern "C" {
    fn ohpl_as_get_module(e: *mut c_void, name: *const c_char, flag: c_int) -> *mut c_void;
    fn ohpl_as_module_add_section(
        m: *mut c_void,
        name: *const c_char,
        code: *const c_char,
        len: usize,
    ) -> c_int;
    fn ohpl_as_module_build(m: *mut c_void) -> c_int;
    fn ohpl_as2_module_load_bytecode(m: *mut c_void, p: *const u8, n: usize) -> c_int;
    fn ohpl_as2_module_save_bytecode(
        m: *mut c_void,
        sink: unsafe extern "C" fn(*mut c_void, *const u8, usize),
        ctx: *mut c_void,
    ) -> c_int;
}

fn module(e: Engine, name: &CString) -> *mut c_void {
    // SAFETY: plain call; `name` is NUL-terminated.
    unsafe { ohpl_as_get_module(e.0, name.as_ptr(), GM_ALWAYS_CREATE) }
}

unsafe extern "C" fn collect_bytes(ctx: *mut c_void, p: *const u8, n: usize) {
    // SAFETY: `ctx` is the `Vec` of `save_bytecode`; the engine passes `n` valid bytes.
    unsafe { (*ctx.cast::<Vec<u8>>()).extend_from_slice(std::slice::from_raw_parts(p, n)) }
}

/// `Build`: `missing` receives the first missing include when it is still empty.
pub(crate) fn build(
    b: &Builder,
    cache_dir: &[u8],
    e: Engine,
    module_name: &[u8],
    entry: &[u8],
    missing: Option<&mut Vec<u8>>,
) -> c_int {
    let Some(c) = b.collect(entry) else {
        return AS_ERROR;
    };
    for w in &c.warnings {
        s2_host::misc(s2_host::MISC_LOG, 1, w);
    }
    if let Some(m) = missing
        && m.is_empty()
    {
        m.clone_from(&c.missing);
    }
    let name = super::s2_as::cs(module_name);
    let mut m = module(e, &name);

    let (mut key, mut cache_file) = (Vec::new(), Vec::new());
    if !cache_dir.is_empty()
        && let Ok(st) = std::fs::metadata("/proc/self/exe")
    {
        key = builder::cache_key(st.size(), st.mtime(), st.mtime_nsec(), &c.sections).into_bytes();
        cache_file = [cache_dir, b"/", builder::cache_file_name(entry).as_bytes()].concat();
        let data =
            std::fs::read(Path::new(std::ffi::OsStr::from_bytes(&cache_file))).unwrap_or_default();
        if let Some(body) = data.strip_prefix(key.as_slice()) {
            // SAFETY: `body` is valid for the call.
            if unsafe { ohpl_as2_module_load_bytecode(m, body.as_ptr(), body.len()) } >= 0 {
                return 0;
            }
            m = module(e, &name);
        }
    }

    for (sname, code) in &c.sections {
        let n = super::s2_as::cs(sname);
        // SAFETY: the name outlives the call; the engine copies `code.len()` bytes.
        unsafe { ohpl_as_module_add_section(m, n.as_ptr(), code.as_ptr().cast(), code.len()) };
    }
    // SAFETY: plain call.
    let r = unsafe { ohpl_as_module_build(m) };
    if r >= 0 && !cache_file.is_empty() {
        let mut out = key;
        // SAFETY: `out` outlives the call.
        let saved = unsafe {
            ohpl_as2_module_save_bytecode(m, collect_bytes, std::ptr::from_mut(&mut out).cast())
        };
        if saved >= 0 {
            let tmp = [
                cache_file.as_slice(),
                b".",
                std::process::id().to_string().as_bytes(),
            ]
            .concat();
            let (tmp_p, dst) = (
                Path::new(std::ffi::OsStr::from_bytes(&tmp)),
                Path::new(std::ffi::OsStr::from_bytes(&cache_file)),
            );
            if std::fs::write(tmp_p, &out).is_ok() {
                let _ = std::fs::rename(tmp_p, dst);
            }
        }
    }
    r
}
