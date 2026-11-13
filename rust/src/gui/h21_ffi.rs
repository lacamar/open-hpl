//! C ABI of the H21 popups, declared in `gui/RustGuiH21.h`.
//!
//! `vt` is a valid host table and `p` a popup from one of the `*_new` functions; null pointers
//! make a call a no-op returning the zero value. A popup lives until `ohpl_h21_destroy`.

use super::h21_log::{Ids, fmt_cmd, fmt_ret, hex, units};
use super::h21_popup::{self as pop, Cmd, Host, Hx, Popup, Ret};
use super::set::{Msg, W};
use std::cell::RefCell;
use std::ffi::{CStr, c_char, c_void};

#[repr(C)]
pub struct HostVt {
    ctx: *mut c_void,
    exec: extern "C" fn(*mut c_void, *const Cmd, *mut Ret),
}

struct Recorder {
    text: String,
    ids: Ids,
}

thread_local! {
    static REC: RefCell<Option<Recorder>> = const { RefCell::new(None) };
}

fn record(f: impl FnOnce(&mut Recorder) -> String) {
    REC.with(|r| {
        if let Some(rec) = r
            .try_borrow_mut()
            .ok()
            .as_deref_mut()
            .and_then(Option::as_mut)
        {
            let line = f(rec);
            rec.text.push_str(&line);
            rec.text.push('\n');
        }
    });
}

fn recording() -> bool {
    REC.with(|r| r.try_borrow().is_ok_and(|r| r.is_some()))
}

impl Host for HostVt {
    fn exec(&self, c: &Cmd) -> Ret {
        record(|r| fmt_cmd(c, &mut r.ids));
        let mut ret = Ret::default();
        (self.exec)(self.ctx, c, &mut ret);
        record(|r| fmt_ret(&ret, &mut r.ids));
        ret
    }
}

/// Runs an entry point, logging `ev <head>` first and `= <tail>` after when recording.
fn entry<R>(
    head: impl FnOnce(&mut Ids) -> String,
    tail: impl FnOnce(&R, &mut Ids) -> String,
    f: impl FnOnce() -> R,
) -> R {
    if recording() {
        record(|r| format!("ev {}", head(&mut r.ids)));
    }
    let out = f();
    if recording() {
        record(|r| format!("= {}", tail(&out, &mut r.ids)));
    }
    out
}

/// # Safety
/// `vt` is null or valid.
unsafe fn host<'a>(vt: *const HostVt) -> Option<&'a HostVt> {
    // SAFETY: caller contract.
    unsafe { vt.as_ref() }
}

fn ptr_id(p: *const Popup) -> impl FnOnce(&mut Ids) -> usize {
    move |ids| ids.id(p as usize)
}

fn boxed(p: Popup) -> *mut Popup {
    Box::into_raw(Box::new(p))
}

/// # Safety
/// `vt` valid; string pointers valid for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h21_msgbox_new(
    vt: *const HostVt,
    label: *const u32,
    label_n: usize,
    text: *const u32,
    text_n: usize,
    b1: *const u32,
    b1_n: usize,
    b2: *const u32,
    b2_n: usize,
    cb_obj: usize,
    cb_fn: usize,
) -> *mut Popup {
    // SAFETY: caller contract.
    let (Some(vt), l, t, a, b) = (
        unsafe { host(vt) },
        unsafe { units(label, label_n) },
        unsafe { units(text, text_n) },
        unsafe { units(b1, b1_n) },
        unsafe { units(b2, b2_n) },
    ) else {
        return std::ptr::null_mut();
    };
    entry(
        |ids| {
            format!(
                "msgbox_new l={} t={} b1={} b2={} cb={},{}",
                hex(l),
                hex(t),
                hex(a),
                hex(b),
                ids.id(cb_obj),
                ids.id(cb_fn)
            )
        },
        |p: &*mut Popup, ids| format!("p={}", ptr_id(*p)(ids)),
        || boxed(pop::msgbox_new(&Hx(vt), l, t, a, b, [cb_obj, cb_fn])),
    )
}

/// # Safety
/// `vt` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h21_color_new(
    vt: *const HostVt,
    dest: usize,
    cb_obj: usize,
    cb_fn: usize,
) -> *mut Popup {
    // SAFETY: caller contract.
    let Some(vt) = (unsafe { host(vt) }) else {
        return std::ptr::null_mut();
    };
    entry(
        |ids| {
            format!(
                "color_new dest={} cb={},{}",
                ids.id(dest),
                ids.id(cb_obj),
                ids.id(cb_fn)
            )
        },
        |p: &*mut Popup, ids| format!("p={}", ptr_id(*p)(ids)),
        || boxed(pop::color_new(&Hx(vt), dest, [cb_obj, cb_fn])),
    )
}

/// # Safety
/// `vt` valid; string pointers valid for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h21_fp_new(
    vt: *const HostVt,
    kind: i32,
    path: *const u32,
    path_n: usize,
    name: *const u32,
    name_n: usize,
    cb_obj: usize,
    cb_fn: usize,
) -> *mut Popup {
    // SAFETY: caller contract.
    let (Some(vt), pa, na) = (
        unsafe { host(vt) },
        unsafe { units(path, path_n) },
        unsafe { units(name, name_n) },
    ) else {
        return std::ptr::null_mut();
    };
    entry(
        |ids| {
            format!(
                "fp_new kind={kind} path={} name={} cb={},{}",
                hex(pa),
                hex(na),
                ids.id(cb_obj),
                ids.id(cb_fn)
            )
        },
        |p: &*mut Popup, ids| format!("p={}", ptr_id(*p)(ids)),
        || boxed(pop::fp_new(&Hx(vt), kind, pa, na, [cb_obj, cb_fn])),
    )
}

/// # Safety
/// `vt` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h21_kb_new(vt: *const HostVt, target: W) -> *mut Popup {
    // SAFETY: caller contract.
    let Some(vt) = (unsafe { host(vt) }) else {
        return std::ptr::null_mut();
    };
    entry(
        |ids| format!("kb_new target={}", ids.id(target)),
        |p: &*mut Popup, ids| format!("p={}", ptr_id(*p)(ids)),
        || boxed(pop::kb_new(&Hx(vt), target)),
    )
}

/// # Safety
/// `vt` and `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h21_event(
    vt: *const HostVt,
    p: *const Popup,
    tag: i32,
    w: W,
    d: *const Msg,
) -> bool {
    // SAFETY: caller contract.
    let (Some(vt), Some(po), Some(d)) = (unsafe { host(vt) }, unsafe { p.as_ref() }, unsafe {
        d.as_ref()
    }) else {
        return false;
    };
    entry(
        |ids| {
            format!(
                "event p={} tag={tag} w={} val={}",
                ids.id(p as usize),
                ids.id(w),
                d.val()
            )
        },
        |r: &bool, _| format!("ret={}", u8::from(*r)),
        || po.event(&Hx(vt), tag, w, d),
    )
}

/// # Safety
/// `vt` valid; `p` from a `*_new` function and not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h21_destroy(vt: *const HostVt, p: *mut Popup) {
    // SAFETY: caller contract.
    let (Some(vt), false) = (unsafe { host(vt) }, p.is_null()) else {
        return;
    };
    entry(
        |ids| format!("destroy p={}", ids.id(p as usize)),
        |(): &(), _| String::from("ok"),
        || {
            // SAFETY: `p` is a live `Box<Popup>` pointer given up by the caller.
            let po = unsafe { Box::from_raw(p) };
            po.destroy(&Hx(vt));
        },
    );
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h21_window(p: *const Popup) -> W {
    // SAFETY: caller contract.
    unsafe { p.as_ref() }.map_or(0, Popup::window)
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h21_set_destroy_callback(p: *const Popup, obj: usize, f: usize) {
    // SAFETY: caller contract.
    if let Some(po) = unsafe { p.as_ref() } {
        entry(
            |ids| {
                format!(
                    "set_destroy_callback p={} cb={},{}",
                    ids.id(p as usize),
                    ids.id(obj),
                    ids.id(f)
                )
            },
            |(): &(), _| String::from("ok"),
            || po.set_destroy_callback(obj, f),
        );
    }
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h21_set_next_focus(p: *const Popup, w: W) {
    // SAFETY: caller contract.
    if let Some(po) = unsafe { p.as_ref() } {
        entry(
            |ids| format!("set_next_focus p={} w={}", ids.id(p as usize), ids.id(w)),
            |(): &(), _| String::from("ok"),
            || po.set_next_focus(w),
        );
    }
}

/// # Safety
/// `vt` and `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h21_set_kill_on_escape(vt: *const HostVt, p: *const Popup, x: bool) {
    // SAFETY: caller contract.
    if let (Some(vt), Some(po)) = (unsafe { host(vt) }, unsafe { p.as_ref() }) {
        entry(
            |ids| {
                format!(
                    "set_kill_on_escape p={} x={}",
                    ids.id(p as usize),
                    u8::from(x)
                )
            },
            |(): &(), _| String::from("ok"),
            || po.set_kill_on_escape(&Hx(vt), x),
        );
    }
}

/// # Safety
/// `vt` and `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h21_kill_on_escape(vt: *const HostVt, p: *const Popup) -> bool {
    // SAFETY: caller contract.
    match (unsafe { host(vt) }, unsafe { p.as_ref() }) {
        (Some(vt), Some(po)) => entry(
            |ids| format!("kill_on_escape p={}", ids.id(p as usize)),
            |r: &bool, _| format!("ret={}", u8::from(*r)),
            || po.kill_on_escape(&Hx(vt)),
        ),
        _ => false,
    }
}

/// # Safety
/// `vt` and `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h21_fp_init(vt: *const HostVt, p: *const Popup) {
    // SAFETY: caller contract.
    if let (Some(vt), Some(po)) = (unsafe { host(vt) }, unsafe { p.as_ref() }) {
        entry(
            |ids| format!("fp_init p={}", ids.id(p as usize)),
            |(): &(), _| String::from("ok"),
            || po.fp_init(&Hx(vt)),
        );
    }
}

/// # Safety
/// `vt` and `p` valid; the string is valid for its length.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h21_fp_set_save_dest(
    vt: *const HostVt,
    p: *const Popup,
    name: *const u32,
    name_n: usize,
) {
    // SAFETY: caller contract.
    if let (Some(vt), Some(po), n) = (unsafe { host(vt) }, unsafe { p.as_ref() }, unsafe {
        units(name, name_n)
    }) {
        entry(
            |ids| format!("fp_set_save_dest p={} name={}", ids.id(p as usize), hex(n)),
            |(): &(), _| String::from("ok"),
            || po.fp_set_save_dest(&Hx(vt), n),
        );
    }
}

/// # Safety
/// `vt` and `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h21_fp_on_add_filter(vt: *const HostVt, p: *const Popup) {
    // SAFETY: caller contract.
    if let (Some(vt), Some(po)) = (unsafe { host(vt) }, unsafe { p.as_ref() }) {
        entry(
            |ids| format!("fp_on_add_filter p={}", ids.id(p as usize)),
            |(): &(), _| String::from("ok"),
            || po.fp_on_add_filter(&Hx(vt)),
        );
    }
}

/// # Safety
/// `vt` and `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h21_fp_on_navigate(vt: *const HostVt, p: *const Popup) {
    // SAFETY: caller contract.
    if let (Some(vt), Some(po)) = (unsafe { host(vt) }, unsafe { p.as_ref() }) {
        entry(
            |ids| format!("fp_on_navigate p={}", ids.id(p as usize)),
            |(): &(), _| String::from("ok"),
            || po.fp_on_navigate(&Hx(vt)),
        );
    }
}

/// Starts recording the host traffic of this thread (cassette golden vectors).
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h21_log_start() {
    REC.with(|r| {
        if let Ok(mut r) = r.try_borrow_mut() {
            *r = Some(Recorder {
                text: String::new(),
                ids: Ids::default(),
            });
        }
    });
}

/// Writes the recording to `path` and stops recording.
///
/// # Safety
/// `path` is a valid C string.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h21_log_write(path: *const c_char) -> bool {
    if path.is_null() {
        return false;
    }
    // SAFETY: caller contract.
    let Ok(path) = unsafe { CStr::from_ptr(path) }.to_str() else {
        return false;
    };
    let text = REC.with(|r| {
        r.try_borrow_mut()
            .ok()
            .and_then(|mut r| r.take())
            .map(|r| r.text)
    });
    text.is_some_and(|t| std::fs::write(path, t).is_ok())
}
