#![allow(clippy::missing_safety_doc, clippy::too_many_arguments)]

use super::g2_popup::G2Popup;
use super::h21_log::{Ids, fmt_cmd, fmt_ret, hex, units};
use super::h21_popup::{self as pop, Cmd, Host, Hx, Popup, Ret};
use super::set::{Msg, W};
use std::cell::RefCell;
use std::ffi::{CStr, c_char};

pub(super) struct PopupHost(pub(super) *mut G2Popup);

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

impl Host for PopupHost {
    fn exec(&self, c: &Cmd) -> Ret {
        record(|r| fmt_cmd(c, &mut r.ids));
        let mut ret = Ret::default();
        super::h21_direct::exec(self.0.cast(), c, &mut ret);
        record(|r| fmt_ret(&ret, &mut r.ids));
        ret
    }
}

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

fn ptr_id(p: *const Popup) -> impl FnOnce(&mut Ids) -> usize {
    move |ids| ids.id(p as usize)
}

fn boxed(p: Popup) -> *mut Popup {
    Box::into_raw(Box::new(p))
}

pub(super) unsafe fn msgbox_new(
    vt: &PopupHost,
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
    let (l, t, a, b) = unsafe {
        (
            units(label, label_n),
            units(text, text_n),
            units(b1, b1_n),
            units(b2, b2_n),
        )
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

pub(super) fn color_new(vt: &PopupHost, dest: usize, cb_obj: usize, cb_fn: usize) -> *mut Popup {
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

pub(super) unsafe fn fp_new(
    vt: &PopupHost,
    kind: i32,
    path: *const u32,
    path_n: usize,
    name: *const u32,
    name_n: usize,
    cb_obj: usize,
    cb_fn: usize,
) -> *mut Popup {
    let (pa, na) = unsafe { (units(path, path_n), units(name, name_n)) };
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

pub(super) fn kb_new(vt: &PopupHost, target: W) -> *mut Popup {
    entry(
        |ids| format!("kb_new target={}", ids.id(target)),
        |p: &*mut Popup, ids| format!("p={}", ptr_id(*p)(ids)),
        || boxed(pop::kb_new(&Hx(vt), target)),
    )
}

pub(super) unsafe fn event(vt: &PopupHost, p: *const Popup, tag: i32, w: W, d: *const Msg) -> bool {
    let (Some(po), Some(d)) = (unsafe { p.as_ref() }, unsafe { d.as_ref() }) else {
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

pub(super) unsafe fn destroy(vt: &PopupHost, p: *mut Popup) {
    if p.is_null() {
        return;
    }
    entry(
        |ids| format!("destroy p={}", ids.id(p as usize)),
        |(): &(), _| String::from("ok"),
        || {
            let po = unsafe { Box::from_raw(p) };
            po.destroy(&Hx(vt));
        },
    );
}

pub(super) unsafe fn set_destroy_callback(p: *const Popup, obj: usize, f: usize) {
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

pub(super) unsafe fn set_next_focus(p: *const Popup, w: W) {
    if let Some(po) = unsafe { p.as_ref() } {
        entry(
            |ids| format!("set_next_focus p={} w={}", ids.id(p as usize), ids.id(w)),
            |(): &(), _| String::from("ok"),
            || po.set_next_focus(w),
        );
    }
}

pub(super) unsafe fn set_kill_on_escape(vt: &PopupHost, p: *const Popup, x: bool) {
    if let Some(po) = unsafe { p.as_ref() } {
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

pub(super) unsafe fn kill_on_escape(vt: &PopupHost, p: *const Popup) -> bool {
    match unsafe { p.as_ref() } {
        Some(po) => entry(
            |ids| format!("kill_on_escape p={}", ids.id(p as usize)),
            |r: &bool, _| format!("ret={}", u8::from(*r)),
            || po.kill_on_escape(&Hx(vt)),
        ),
        None => false,
    }
}

pub(super) unsafe fn fp_init(vt: &PopupHost, p: *const Popup) {
    if let Some(po) = unsafe { p.as_ref() } {
        entry(
            |ids| format!("fp_init p={}", ids.id(p as usize)),
            |(): &(), _| String::from("ok"),
            || po.fp_init(&Hx(vt)),
        );
    }
}

pub(super) unsafe fn fp_set_save_dest(
    vt: &PopupHost,
    p: *const Popup,
    name: *const u32,
    name_n: usize,
) {
    if let (Some(po), n) = (unsafe { p.as_ref() }, unsafe { units(name, name_n) }) {
        entry(
            |ids| format!("fp_set_save_dest p={} name={}", ids.id(p as usize), hex(n)),
            |(): &(), _| String::from("ok"),
            || po.fp_set_save_dest(&Hx(vt), n),
        );
    }
}

pub(super) unsafe fn fp_on_add_filter(vt: &PopupHost, p: *const Popup) {
    if let Some(po) = unsafe { p.as_ref() } {
        entry(
            |ids| format!("fp_on_add_filter p={}", ids.id(p as usize)),
            |(): &(), _| String::from("ok"),
            || po.fp_on_add_filter(&Hx(vt)),
        );
    }
}

pub(super) unsafe fn fp_on_navigate(vt: &PopupHost, p: *const Popup) {
    if let Some(po) = unsafe { p.as_ref() } {
        entry(
            |ids| format!("fp_on_navigate p={}", ids.id(p as usize)),
            |(): &(), _| String::from("ok"),
            || po.fp_on_navigate(&Hx(vt)),
        );
    }
}

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
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h21_log_write(path: *const c_char) -> bool {
    if path.is_null() {
        return false;
    }
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
