//! Process start (`main` of LowLevelSystemSDL.cpp) and `cSDLEngineSetup`'s SDL setup. The
//! read-only protection of the Steam libraries runs after the argument pass, as in C++, and
//! `hplMain` is only reached when it returns true.

use super::clib::{self, CLONE_NEWNS, CLONE_NEWUSER, MS_BIND, MS_PRIVATE, MS_REC, W_OK};
use crate::sdlsys::{
    Arg, EnvInputs, classify_arg, engine_env, remount_flags, steam_root, vdf_path,
};
use crate::sdlsys::{ENV_ALSOFT_NULL, ENV_DISABLE_SCREENSAVER, ENV_VIDEODRIVER_X11};
use crate::string_ffi::{Sink, emit};
use crate::sys::sdl2::{
    SDL_DisableScreenSaver, SDL_EnableScreenSaver, SDL_GetError,
    SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES, SDL_INIT_TIMER, SDL_INIT_VIDEO, SDL_Init, SDL_Quit,
    SDL_SetHint,
};
use core::ffi::{CStr, c_char, c_int, c_void};
use std::collections::BTreeSet;
use std::ffi::{CString, OsStr};
use std::os::unix::ffi::OsStrExt;

fn cstring(b: &[u8]) -> Option<CString> {
    CString::new(b).ok()
}

fn lossy(b: &[u8]) -> String {
    String::from_utf8_lossy(b).into_owned()
}

fn add_root(set: &mut BTreeSet<Vec<u8>>, path: &[u8]) {
    if let Some(root) = std::fs::canonicalize(OsStr::from_bytes(path))
        .ok()
        .and_then(|r| steam_root(r.as_os_str().as_bytes()))
    {
        set.insert(root);
    }
}

pub(super) fn add_library_folders(set: &mut BTreeSet<Vec<u8>>, vdf: &[u8]) {
    let Ok(text) = std::fs::read(OsStr::from_bytes(vdf)) else {
        return;
    };
    for line in text.split(|&b| b == b'\n') {
        if let Some(p) = vdf_path(line) {
            add_root(set, &[p, b"/steamapps/common"].concat());
        }
    }
}

fn collect_roots() -> BTreeSet<Vec<u8>> {
    let mut roots = BTreeSet::new();
    add_root(&mut roots, b".");
    add_root(&mut roots, b"..");
    if let Ok(rd) = std::fs::read_dir(".") {
        for e in rd.flatten() {
            add_root(&mut roots, e.file_name().as_bytes());
        }
    }
    let home = std::env::var_os("HOME");
    let data = std::env::var_os("XDG_DATA_HOME");
    if let Some(h) = home {
        let h = h.as_bytes();
        add_library_folders(
            &mut roots,
            &[h, b"/.steam/steam/steamapps/libraryfolders.vdf"].concat(),
        );
        add_library_folders(
            &mut roots,
            &[h, b"/.local/share/Steam/steamapps/libraryfolders.vdf"].concat(),
        );
    }
    if let Some(d) = data.filter(|d| !d.is_empty()) {
        add_library_folders(
            &mut roots,
            &[d.as_bytes(), b"/Steam/steamapps/libraryfolders.vdf"].concat(),
        );
    }
    roots
}

fn write_proc(path: &str, data: &str) -> bool {
    std::fs::write(path, data).is_ok()
}

fn mount_call(src: Option<&CString>, target: &CString, flags: u64) -> bool {
    // SAFETY: NUL-terminated strings; the filesystem type and data are null.
    unsafe {
        clib::mount(
            src.map_or(core::ptr::null(), |s| s.as_ptr()),
            target.as_ptr(),
            core::ptr::null(),
            flags,
            core::ptr::null(),
        ) == 0
    }
}

/// Private user+mount namespace with every Steam root bind-remounted read-only. Fails closed:
/// false when a root is still writable.
fn protect(roots: &BTreeSet<Vec<u8>>) -> bool {
    if roots.is_empty() {
        return true;
    }
    // SAFETY: plain syscalls.
    let (uid, gid) = unsafe { (clib::getuid(), clib::getgid()) };
    let cwd = std::env::current_dir().ok();
    let root = c"/";
    let isolated = cwd.is_some()
        // SAFETY: plain syscall.
        && unsafe { clib::unshare(CLONE_NEWUSER | CLONE_NEWNS) } == 0
        && write_proc("/proc/self/setgroups", "deny")
        && write_proc("/proc/self/uid_map", &format!("{uid} {uid} 1"))
        && write_proc("/proc/self/gid_map", &format!("{gid} {gid} 1"))
        && mount_call(None, &root.to_owned(), MS_REC | MS_PRIVATE);
    if isolated {
        for r in roots {
            let Some(c) = cstring(r) else { continue };
            let mut st = clib::statvfs {
                f_bsize: 0,
                f_frsize: 0,
                f_blocks: 0,
                f_bfree: 0,
                f_bavail: 0,
                f_files: 0,
                f_ffree: 0,
                f_favail: 0,
                f_fsid: 0,
                f_flag: 0,
                f_namemax: 0,
                spare: [0; 6],
            };
            // SAFETY: NUL-terminated path, valid out pointer.
            let ok = mount_call(Some(&c), &c, MS_BIND | MS_REC)
                && unsafe { clib::statvfs(c.as_ptr(), &mut st) } == 0;
            let flags = if ok { remount_flags(st.f_flag) } else { 0 };
            if !ok || !mount_call(None, &c, flags) {
                clib::warn(&format!(
                    "open-hpl: could not make '{}' read-only: {}",
                    lossy(r),
                    clib::errno_text()
                ));
            }
        }
        if let Some(cwd) = cwd.as_ref().and_then(|c| cstring(c.as_os_str().as_bytes())) {
            // SAFETY: NUL-terminated path.
            if unsafe { clib::chdir(cwd.as_ptr()) } != 0 {
                clib::warn(&format!(
                    "open-hpl: chdir '{}': {}",
                    lossy(cwd.as_bytes()),
                    clib::errno_text()
                ));
            }
        }
    } else {
        clib::warn(&format!(
            "open-hpl: could not make Steam libraries read-only: {}",
            clib::errno_text()
        ));
    }
    let mut all_ro = true;
    for r in roots {
        let Some(c) = cstring(r) else { continue };
        // SAFETY: NUL-terminated path.
        if unsafe { clib::access(c.as_ptr(), W_OK) } == 0 {
            clib::warn(&format!(
                "open-hpl: refusing to run: '{}' is writable",
                lossy(r)
            ));
            all_ro = false;
        }
    }
    all_ro
}

/// Command line text and whether a working directory was chosen. `enter_dir` returns `None` when
/// the argument is not a directory, else whether the chdir succeeded.
pub fn parse_args(
    args: &[Vec<u8>],
    mut enter_dir: impl FnMut(&[u8]) -> Option<bool>,
) -> (Vec<u8>, bool) {
    let mut cwd = false;
    let mut got_dir = false;
    let mut cmd: Vec<u8> = Vec::new();
    let push = |cmd: &mut Vec<u8>, a: &[u8]| {
        if !cmd.is_empty() {
            cmd.push(b' ');
        }
        cmd.extend_from_slice(a);
    };
    for a in args {
        match classify_arg(a, got_dir) {
            Arg::Cwd => cwd = true,
            Arg::Skip => {}
            Arg::GameDirCandidate => {
                if let Some(ok) = enter_dir(a) {
                    got_dir = true;
                    cwd = ok;
                } else {
                    push(&mut cmd, a);
                }
            }
            Arg::Command => push(&mut cmd, a),
        }
    }
    (cmd, cwd)
}

/// Locale check, then the argument pass (which may `chdir`). Returns 0, or 1 when the locale cannot
/// be set. `cwd` receives whether a working directory was chosen.
///
/// # Safety
/// `argv` has `argc` NUL-terminated entries; `cwd` is a valid out pointer; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_main_args(
    argc: c_int,
    argv: *const *const c_char,
    sink: Sink<u8>,
    ctx: *mut c_void,
    cwd: *mut bool,
) -> c_int {
    // SAFETY: libc calls with NUL-terminated arguments.
    unsafe {
        if clib::setlocale(clib::LC_CTYPE, c"".as_ptr()).is_null() {
            clib::warn("Can't set the specified locale! Check LANG, LC_CTYPE, LC_ALL.");
            return 1;
        }
        let charset = clib::nl_langinfo(clib::CODESET);
        if clib::strcasecmp(charset, c"UTF-8".as_ptr()) != 0 {
            let lang = std::env::var("LANG").unwrap_or_else(|_| "(null)".into());
            let cs = CStr::from_ptr(charset).to_string_lossy();
            clib::warn(&format!(
                "UTF-8 Charset not available.\nCurrent LANG is {lang}\nCharset: {cs}"
            ));
        }
    }
    let args: Vec<Vec<u8>> = (1..usize::try_from(argc).unwrap_or(0))
        .filter_map(|i| {
            // SAFETY: caller contract.
            let p = unsafe { argv.add(i).read() };
            // SAFETY: caller contract.
            (!p.is_null()).then(|| unsafe { CStr::from_ptr(p) }.to_bytes().to_vec())
        })
        .collect();
    let (cmd, chosen) = parse_args(&args, |a| {
        let c = cstring(a)?;
        std::fs::metadata(OsStr::from_bytes(a))
            .ok()
            .filter(|m| m.is_dir())?;
        // SAFETY: NUL-terminated path.
        Some(unsafe { clib::chdir(c.as_ptr()) } == 0)
    });
    // SAFETY: caller contract.
    unsafe {
        *cwd = chosen;
        emit(sink, ctx, &cmd);
    }
    0
}

/// The Steam-library read-only protection. False means the engine must not start.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_l1_protect_steam() -> bool {
    protect(&collect_roots())
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_l1_sdl_hints() {
    // SAFETY: NUL-terminated strings.
    unsafe { SDL_SetHint(SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES.as_ptr(), c"0".as_ptr()) };
}

/// Env plan, `SDL_Init`, screensaver. False when the video init failed (the C++ raises FatalError).
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_l1_sdl_init(video: bool) -> bool {
    let set = |n: &str| std::env::var_os(n).is_some();
    let plan = engine_env(&EnvInputs {
        headless: set("OPENHPL_HEADLESS_SOCKET"),
        alsoft_set: set("ALSOFT_DRIVERS"),
        video,
        videodriver_set: set("SDL_VIDEODRIVER"),
        display_set: set("DISPLAY"),
    });
    if plan & ENV_ALSOFT_NULL != 0 {
        // SAFETY: NUL-terminated strings.
        unsafe { clib::setenv(c"ALSOFT_DRIVERS".as_ptr(), c"null".as_ptr(), 1) };
    }
    if !video {
        // SAFETY: plain SDL call; the result is ignored as in C++.
        unsafe { SDL_Init(SDL_INIT_TIMER) };
        return true;
    }
    if plan & ENV_VIDEODRIVER_X11 != 0 {
        // SAFETY: NUL-terminated strings.
        unsafe { clib::setenv(c"SDL_VIDEODRIVER".as_ptr(), c"x11".as_ptr(), 1) };
    }
    // SAFETY: plain SDL call.
    if unsafe { SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) } < 0 {
        return false;
    }
    if plan & ENV_DISABLE_SCREENSAVER != 0 {
        // SAFETY: plain SDL call.
        unsafe { SDL_DisableScreenSaver() };
    }
    true
}

/// Emits `SDL_GetError()`.
///
/// # Safety
/// `sink` accepts `ctx` and a slice valid for the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_sdl_error(sink: Sink<u8>, ctx: *mut c_void) {
    // SAFETY: SDL returns a NUL-terminated string.
    unsafe { emit(sink, ctx, CStr::from_ptr(SDL_GetError()).to_bytes()) };
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_l1_sdl_shutdown() {
    // SAFETY: plain SDL calls.
    unsafe {
        SDL_EnableScreenSaver();
        SDL_Quit();
    }
}
