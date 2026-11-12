//! `cPlatform` file-system half of PlatformUnix.cpp. Paths cross as UTF-8 bytes; the C++ shell
//! keeps the wide-string conversions.

use super::clib::{self, S_IFDIR, S_IFMT};
use crate::string_ffi::{Sink, emit};
use core::ffi::{CStr, c_char, c_int, c_void};
use std::ffi::{OsStr, OsString};
use std::os::unix::ffi::{OsStrExt, OsStringExt};

fn cpath(p: &[u8]) -> Option<std::ffi::CString> {
    std::ffi::CString::new(p).ok()
}

/// `stat` of `p`; `None` on failure. `follow` false is `lstat`.
fn stat_of(p: &[u8], follow: bool) -> Option<clib::stat> {
    let c = cpath(p)?;
    let mut st = clib::stat::zeroed();
    // SAFETY: NUL-terminated path, valid out pointer.
    let r = unsafe {
        if follow {
            clib::stat(c.as_ptr(), &mut st)
        } else {
            clib::lstat(c.as_ptr(), &mut st)
        }
    };
    (r == 0).then_some(st)
}

pub fn file_size(p: &[u8]) -> u64 {
    stat_of(p, true).map_or(0, |s| s.st_size as u64)
}

pub fn file_exists(p: &[u8]) -> bool {
    stat_of(p, false).is_some()
}

pub fn folder_exists(p: &[u8]) -> bool {
    stat_of(p, true).is_some_and(|s| s.st_mode & S_IFMT == S_IFDIR)
}

pub fn remove_file(p: &[u8]) {
    if let Some(c) = cpath(p) {
        // SAFETY: NUL-terminated path.
        unsafe { clib::unlink(c.as_ptr()) };
    }
}

/// `mkdir -p` of every ancestor (failures ignored), then the folder itself.
pub fn create_folder(p: &[u8]) -> bool {
    let mut i = 1;
    while let Some(off) = p.get(i..).and_then(|t| t.iter().position(|&b| b == b'/')) {
        let at = i + off;
        if let Some(c) = p.get(..at).and_then(cpath) {
            // SAFETY: NUL-terminated path.
            unsafe { clib::mkdir(c.as_ptr(), 0o755) };
        }
        i = at + 1;
    }
    cpath(p).is_some_and(|c| {
        // SAFETY: NUL-terminated path.
        unsafe { clib::mkdir(c.as_ptr(), 0o755) == 0 }
    })
}

pub fn full_path(p: &[u8]) -> Option<Vec<u8>> {
    std::fs::canonicalize(OsStr::from_bytes(p))
        .ok()
        .map(|r| r.into_os_string().into_vec())
}

#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq, Eq)]
pub struct Date {
    pub seconds: c_int,
    pub minutes: c_int,
    pub hours: c_int,
    pub month_day: c_int,
    pub month: c_int,
    pub year: c_int,
    pub week_day: c_int,
    pub year_day: c_int,
}

fn date_of(t: &clib::tm) -> Date {
    Date {
        seconds: t.tm_sec,
        minutes: t.tm_min,
        hours: t.tm_hour,
        month_day: t.tm_mday,
        month: t.tm_mon,
        year: 1900 + t.tm_year,
        week_day: t.tm_wday,
        year_day: t.tm_yday,
    }
}

fn gm_date(t: i64) -> Date {
    let mut tm = clib::tm::zeroed();
    // SAFETY: valid in/out pointers.
    unsafe { clib::gmtime_r(&t, &mut tm) };
    date_of(&tm)
}

/// The C++ read an uninitialised `stat` on failure; this uses time 0.
pub fn file_date(p: &[u8], created: bool) -> Date {
    let t = stat_of(p, true).map_or(0, |s| if created { s.st_ctime } else { s.st_mtime });
    gm_date(t)
}

pub fn now_date() -> Date {
    let mut tm = clib::tm::zeroed();
    // SAFETY: valid in/out pointers.
    unsafe {
        let t = clib::time(core::ptr::null_mut());
        clib::localtime_r(&t, &mut tm);
    }
    date_of(&tm)
}

const STAR: u32 = b'*' as u32;
const QUESTION: u32 = b'?' as u32;

/// Case-insensitive `*` / `?` match (`towupper`), as `patiMatch`.
pub fn pati_match(pattern: &[u32], string: &[u32]) -> bool {
    match pattern.split_first() {
        None => string.is_empty(),
        Some((&STAR, rest)) => {
            pati_match(rest, string)
                || string
                    .split_first()
                    .is_some_and(|(_, tail)| pati_match(pattern, tail))
        }
        Some((&QUESTION, rest)) => string
            .split_first()
            .is_some_and(|(_, tail)| pati_match(rest, tail)),
        Some((&c, rest)) => string.split_first().is_some_and(|(&s, tail)| {
            // SAFETY: pure libc function.
            let same = unsafe { clib::towupper(c) == clib::towupper(s) };
            same && pati_match(rest, tail)
        }),
    }
}

/// `mbstowcs` through the process locale, as `cString::To16Char`.
pub fn to_wide(s: &[u8]) -> Vec<u32> {
    let Some(c) = cpath(s) else {
        return Vec::new();
    };
    // SAFETY: NUL-terminated input; a null destination only measures.
    let need = unsafe { clib::mbstowcs(core::ptr::null_mut(), c.as_ptr(), 0) };
    if need == usize::MAX {
        return Vec::new();
    }
    let mut out = vec![0u32; need + 1];
    // SAFETY: `out` holds need + 1 units.
    unsafe { clib::mbstowcs(out.as_mut_ptr(), c.as_ptr(), need) };
    out.truncate(need);
    out
}

fn entries(dir: &[u8]) -> Option<Vec<OsString>> {
    let rd = std::fs::read_dir(OsStr::from_bytes(dir)).ok()?;
    let mut names: Vec<OsString> = vec![".".into(), "..".into()];
    names.extend(rd.flatten().map(|e| e.file_name()));
    Some(names)
}

/// Non-directory entries of `dir` matching `mask`, sorted by wide code point.
/// A path of 256 or more wide units fails the original `swprintf`; such entries are skipped.
pub fn find_files(dir: &[u8], mask: &[u32]) -> Vec<Vec<u32>> {
    let Some(names) = entries(dir) else {
        return Vec::new();
    };
    let slash = usize::from(dir.last() != Some(&b'/'));
    let mut out: Vec<Vec<u32>> = Vec::new();
    for n in names {
        let name = n.as_bytes();
        let mut full = dir.to_vec();
        if slash == 1 {
            full.push(b'/');
        }
        full.extend_from_slice(name);
        if to_wide(&full).len() >= 256 {
            continue;
        }
        let Some(st) = stat_of(&full, true) else {
            continue;
        };
        if st.st_mode & S_IFMT == S_IFDIR {
            continue;
        }
        let wide = to_wide(name);
        if pati_match(mask, &wide) {
            out.push(wide);
        }
    }
    out.sort();
    out
}

/// Sub-folders of `dir` (not followed through symlinks), sorted. The path is cut at 255 bytes as
/// the original `snprintf` buffer did.
pub fn find_folders(dir: &[u8], add_hidden: bool, add_up: bool) -> Vec<Vec<u32>> {
    let Some(names) = entries(dir) else {
        return Vec::new();
    };
    let mut base = dir.to_vec();
    if base.last() != Some(&b'/') {
        base.push(b'/');
    }
    let mut out: Vec<Vec<u32>> = Vec::new();
    for n in names {
        let name = n.as_bytes();
        let mut full = base.clone();
        full.extend_from_slice(name);
        full.truncate(255);
        let Some(st) = stat_of(&full, false) else {
            continue;
        };
        if st.st_mode & S_IFMT != S_IFDIR {
            continue;
        }
        if !add_up && name == b".." {
            continue;
        }
        if !add_hidden && name.first() == Some(&b'.') {
            continue;
        }
        if name == b"." {
            continue;
        }
        out.push(to_wide(name));
    }
    out.sort();
    out
}

/// # Safety
/// `p` is NUL-terminated.
unsafe fn path<'a>(p: *const c_char) -> &'a [u8] {
    if p.is_null() {
        return &[];
    }
    // SAFETY: caller contract.
    unsafe { CStr::from_ptr(p) }.to_bytes()
}

/// # Safety
/// `p` is NUL-terminated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_file_size(p: *const c_char) -> u64 {
    // SAFETY: caller contract.
    file_size(unsafe { path(p) })
}

/// # Safety
/// `p` is NUL-terminated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_file_exists(p: *const c_char) -> bool {
    // SAFETY: caller contract.
    file_exists(unsafe { path(p) })
}

/// # Safety
/// `p` is NUL-terminated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_folder_exists(p: *const c_char) -> bool {
    // SAFETY: caller contract.
    folder_exists(unsafe { path(p) })
}

/// # Safety
/// `p` is NUL-terminated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_remove_file(p: *const c_char) {
    // SAFETY: caller contract.
    remove_file(unsafe { path(p) });
}

/// # Safety
/// `p` is NUL-terminated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_create_folder(p: *const c_char) -> bool {
    // SAFETY: caller contract.
    create_folder(unsafe { path(p) })
}

/// Emits the real path; false when it does not resolve.
///
/// # Safety
/// `p` NUL-terminated; `sink` accepts `ctx` and a slice valid for the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_full_path(
    p: *const c_char,
    sink: Sink<u8>,
    ctx: *mut c_void,
) -> bool {
    // SAFETY: caller contract.
    let Some(r) = full_path(unsafe { path(p) }) else {
        return false;
    };
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &r) };
    true
}

/// # Safety
/// `p` is NUL-terminated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_file_date(p: *const c_char, created: bool) -> Date {
    // SAFETY: caller contract.
    file_date(unsafe { path(p) }, created)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_l1_date_now() -> Date {
    now_date()
}

/// Emits the working directory; false when it cannot be read.
///
/// # Safety
/// `sink` accepts `ctx` and a slice valid for the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_working_dir(sink: Sink<u8>, ctx: *mut c_void) -> bool {
    let Ok(d) = std::env::current_dir() else {
        return false;
    };
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, d.as_os_str().as_bytes()) };
    true
}

type NameSink = unsafe extern "C" fn(*mut c_void, *const u32, usize);

/// Emits each match as wide units; `mask` is `mask_len` wide units.
///
/// # Safety
/// `dir` NUL-terminated; `mask` valid for `mask_len`; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_find_files(
    dir: *const c_char,
    mask: *const u32,
    mask_len: usize,
    sink: NameSink,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    let (dir, mask) = unsafe { (path(dir), crate::string_ffi::units(mask, mask_len)) };
    for n in find_files(dir, mask) {
        // SAFETY: caller contract.
        unsafe { sink(ctx, n.as_ptr(), n.len()) };
    }
}

/// # Safety
/// `dir` NUL-terminated; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_find_folders(
    dir: *const c_char,
    add_hidden: bool,
    add_up: bool,
    sink: NameSink,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    for n in find_folders(unsafe { path(dir) }, add_hidden, add_up) {
        // SAFETY: caller contract.
        unsafe { sink(ctx, n.as_ptr(), n.len()) };
    }
}
