//! Stage H1: the serialize-class registry fed at static-init time, and the process and file
//! helpers `cPlatform` kept in C++. Selected with `OHPL_IMPL=rust`.

use super::ffi::ohpl_ser_register;
use super::serialize::CreateFn;
use crate::string_ffi::{Sink, emit};
use std::ffi::{CStr, CString, c_char, c_int, c_void};
use std::io;
use std::os::unix::ffi::OsStrExt;
use std::os::unix::fs::OpenOptionsExt;
use std::path::Path;
use std::sync::{Mutex, PoisonError};

struct Pending {
    name: usize,
    parent: usize,
    size: usize,
    create: Option<CreateFn>,
}

static PENDING: Mutex<Vec<Pending>> = Mutex::new(Vec::new());

/// `cSerializeClass` constructor: remembers the class until the first save or load.
///
/// # Safety
/// `name` and `parent` are NUL-terminated and stay valid for the life of the process.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h1_ser_defer(
    name: *const c_char,
    parent: *const c_char,
    size: usize,
    create: Option<CreateFn>,
) {
    PENDING
        .lock()
        .unwrap_or_else(PoisonError::into_inner)
        .push(Pending {
            name: name as usize,
            parent: parent as usize,
            size,
            create,
        });
}

/// Registers every deferred class in construction order.
pub fn ohpl_h1_ser_flush() {
    let pending = std::mem::take(&mut *PENDING.lock().unwrap_or_else(PoisonError::into_inner));
    for p in pending {
        let name = p.name as *const c_char;
        // SAFETY: `ohpl_h1_ser_defer` contract: `name` is NUL-terminated.
        let bytes = unsafe { CStr::from_ptr(name) }.to_bytes();
        let table = crate::luxsave::a8_table::layout_table(bytes);
        let size = crate::luxsave::layout::size(bytes)
            .or_else(|| crate::luxsave::layout_ar::size(bytes))
            .or_else(|| crate::luxsave::layout_gcc::size(bytes));
        let (Some(table), true) = (table, size == Some(p.size)) else {
            eprintln!(
                "SV: class {} has no matching Rust layout",
                String::from_utf8_lossy(bytes)
            );
            continue;
        };
        // SAFETY: the table ends in T_NULL and its strings are 'static.
        unsafe {
            ohpl_ser_register(
                name,
                p.parent as *const c_char,
                table.as_ptr(),
                table.len(),
                p.size,
                p.create,
            );
        }
    }
}

unsafe extern "C" {
    fn fork() -> c_int;
    fn execvp(file: *const c_char, argv: *const *const c_char) -> c_int;
    fn exit(code: c_int) -> !;
}

fn clone_file(src: &Path, dst: &Path) {
    let input = std::fs::File::open(src);
    let output = std::fs::OpenOptions::new()
        .write(true)
        .create(true)
        .truncate(true)
        .mode(0o666)
        .open(dst);
    if let (Ok(mut i), Ok(mut o)) = (input, output) {
        let _ = io::copy(&mut i, &mut o);
    }
}

/// `CloneFile`: the destination is created and truncated even when the source cannot be read.
///
/// # Safety
/// Both strings are NUL-terminated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h1_clone_file(src: *const c_char, dst: *const c_char) {
    // SAFETY: caller contract.
    let (s, d) = unsafe { (CStr::from_ptr(src), CStr::from_ptr(dst)) };
    clone_file(
        Path::new(std::ffi::OsStr::from_bytes(s.to_bytes())),
        Path::new(std::ffi::OsStr::from_bytes(d.to_bytes())),
    );
}

/// Directory of the running executable; false when it cannot be found.
///
/// # Safety
/// `sink` accepts the bytes with `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h1_data_dir(sink: Sink<u8>, ctx: *mut c_void) -> bool {
    let Some(dir) = std::env::current_exe()
        .ok()
        .and_then(|p| p.parent().map(Path::to_path_buf))
    else {
        return false;
    };
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, dir.as_os_str().as_bytes()) };
    true
}

/// `fork` + `execvp(file, argv)`; the child calls `exit(1)` when the exec fails. Returns the pid, or
/// a negative value when the fork fails.
///
/// # Safety
/// `file` is NUL-terminated; `argv` points to `n` NUL-terminated strings.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h1_spawn(
    file: *const c_char,
    argv: *const *const c_char,
    n: usize,
) -> i32 {
    // SAFETY: caller contract.
    let (file, args) = unsafe {
        (
            CStr::from_ptr(file).to_owned(),
            std::slice::from_raw_parts(argv, n)
                .iter()
                .map(|a| CStr::from_ptr(*a).to_owned())
                .collect::<Vec<CString>>(),
        )
    };
    let mut ptrs: Vec<*const c_char> = args.iter().map(|a| a.as_ptr()).collect();
    ptrs.push(std::ptr::null());
    // SAFETY: the child only execs or exits; everything it needs was built before the fork.
    unsafe {
        let pid = fork();
        if pid == 0 {
            execvp(file.as_ptr(), ptrs.as_ptr());
            exit(1);
        }
        pid
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn clone_truncates_destination_when_source_is_missing() {
        let dir = std::env::temp_dir().join(format!("ohpl-h1-{}", std::process::id()));
        std::fs::create_dir_all(&dir).expect("dir");
        let (a, b) = (dir.join("a"), dir.join("b"));
        std::fs::write(&b, b"old").expect("write");
        clone_file(&a, &b);
        assert_eq!(std::fs::read(&b).expect("read"), b"");
        std::fs::write(&a, b"new data").expect("write");
        clone_file(&a, &b);
        assert_eq!(std::fs::read(&b).expect("read"), b"new data");
        std::fs::remove_dir_all(&dir).expect("rm");
    }

    #[test]
    fn defer_then_flush_registers_in_order() {
        let size = crate::luxsave::layout_gcc::size(b"cScriptVar").expect("layout");
        // SAFETY: literals.
        unsafe { ohpl_h1_ser_defer(c"cScriptVar".as_ptr(), c"".as_ptr(), size, None) };
        ohpl_h1_ser_flush();
        let class = crate::system::serialize::lookup(b"cScriptVar").expect("registered");
        assert_eq!(class.size, size);
        let got: Vec<_> = class
            .fields
            .iter()
            .map(|f| (f.name.clone(), f.ty))
            .collect();
        assert_eq!(got, vec![(b"msName".to_vec(), 4), (b"mlVal".to_vec(), 2)]);
    }
}
