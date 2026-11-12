//! Engine log lines (`Log`, `Warning`, `Error`, `FatalError`) as a level and a message; the C++
//! shell installs the callback that prints them.

use core::ffi::{CStr, c_char};
use std::sync::Mutex;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(i32)]
pub enum Level {
    Log = 0,
    Warning = 1,
    Error = 2,
    Fatal = 3,
}

pub type Sink<'a> = &'a mut dyn FnMut(Level, &[u8]);

type CallbackFn = unsafe extern "C" fn(i32, *const c_char);

static CALLBACK: Mutex<Option<CallbackFn>> = Mutex::new(None);

/// # Safety
/// `f` stays valid for the rest of the process (or is null).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_set_log(f: Option<CallbackFn>) {
    if let Ok(mut g) = CALLBACK.lock() {
        *g = f;
    }
}

/// Forwards to the installed callback; drops the line when there is none.
pub fn to_callback(level: Level, msg: &[u8]) {
    let f = CALLBACK.lock().ok().and_then(|g| *g);
    if let Some(f) = f {
        let mut line: Vec<u8> = msg.iter().copied().filter(|&b| b != 0).collect();
        line.push(0);
        // SAFETY: NUL-terminated line, callback contract of `ohpl_l5_set_log`.
        unsafe { f(level as i32, line.as_ptr().cast()) };
    }
}

pub fn lossy(b: &[u8]) -> String {
    String::from_utf8_lossy(b).into_owned()
}

/// # Safety
/// `p` is NUL-terminated or null.
pub unsafe fn c_bytes<'a>(p: *const c_char) -> &'a [u8] {
    if p.is_null() {
        &[]
    } else {
        // SAFETY: caller contract.
        unsafe { CStr::from_ptr(p) }.to_bytes()
    }
}
