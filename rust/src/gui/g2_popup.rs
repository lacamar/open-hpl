#![allow(clippy::missing_safety_doc, clippy::too_many_arguments)]

use super::g1_set::ohpl_g1_set_destroy_popup;
use super::h21_fb::FileBrowser;
use super::h21_ffi::{self as ffi, PopupHost};
use super::h21_popup::{Popup, Ret};
use super::set::{Msg, W};
use std::collections::LinkedList;
use std::ffi::c_void;

pub struct Binding {
    popup: *mut G2Popup,
    tag: i32,
}

pub struct G2Popup {
    state: *mut Popup,
    pub(super) set: usize,
    pub(super) skin: usize,
    queued: bool,
    bindings: LinkedList<Binding>,
    strs: [Vec<u32>; 3],
    pub(super) fb: usize,
}

fn vt(p: *mut G2Popup) -> PopupHost {
    PopupHost(p)
}

fn alloc(set: usize, skin: usize) -> *mut G2Popup {
    Box::into_raw(Box::new(G2Popup {
        state: std::ptr::null_mut(),
        set,
        skin,
        queued: false,
        bindings: LinkedList::new(),
        strs: [Vec::new(), Vec::new(), Vec::new()],
        fb: 0,
    }))
}

unsafe fn state(p: *mut G2Popup) -> *mut Popup {
    unsafe { (*p).state }
}

unsafe fn fbof(p: *mut G2Popup) -> usize {
    unsafe { (*p).fb }
}

fn build(
    set: usize,
    skin: usize,
    make: impl FnOnce(*mut G2Popup, &PopupHost) -> *mut Popup,
) -> *mut G2Popup {
    let p = alloc(set, skin);
    let st = make(p, &vt(p));
    unsafe { (*p).state = st };
    p
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_msgbox(
    set: usize,
    skin: usize,
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
) -> *mut G2Popup {
    build(set, skin, |_, v| unsafe {
        ffi::msgbox_new(
            v, label, label_n, text, text_n, b1, b1_n, b2, b2_n, cb_obj, cb_fn,
        )
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_color(
    set: usize,
    skin: usize,
    dest: usize,
    cb_obj: usize,
    cb_fn: usize,
) -> *mut G2Popup {
    build(set, skin, |_, v| ffi::color_new(v, dest, cb_obj, cb_fn))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_filepicker(
    set: usize,
    skin: usize,
    kind: i32,
    path: *const u32,
    path_n: usize,
    name: *const u32,
    name_n: usize,
    hidden: bool,
    cb_obj: usize,
    cb_fn: usize,
) -> *mut G2Popup {
    let p = alloc(set, skin);
    unsafe {
        let start = std::slice::from_raw_parts(path, path_n);
        (*p).fb = Box::into_raw(Box::new(FileBrowser::new(p, start, hidden))) as usize;
        (*p).state = ffi::fp_new(&vt(p), kind, path, path_n, name, name_n, cb_obj, cb_fn);
    }
    p
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_kb(set: usize, skin: usize, target: W) -> *mut G2Popup {
    build(set, skin, |_, v| ffi::kb_new(v, target))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_kb_for(target: W) -> *mut G2Popup {
    let set = super::gw::set_of(target);
    let skin = unsafe { super::g1_set::ohpl_g1_set_skin(set) } as usize;
    unsafe { ohpl_g2_popup_kb(set as usize, skin, target) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_free(p: *mut G2Popup) {
    if p.is_null() {
        return;
    }
    let st = unsafe { state(p) };
    unsafe { ffi::destroy(&vt(p), st) };
    let fb = unsafe { fbof(p) };
    if fb != 0 {
        drop(unsafe { Box::from_raw(fb as *mut FileBrowser) });
    }
    drop(unsafe { Box::from_raw(p) });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_add_destroy_cb(p: *mut G2Popup, obj: usize, f: usize) {
    unsafe { ffi::set_destroy_callback(state(p), obj, f) };
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_set_kill_on_escape(p: *mut G2Popup, x: bool) {
    unsafe { ffi::set_kill_on_escape(&vt(p), state(p), x) };
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_kill_on_escape(p: *mut G2Popup) -> bool {
    unsafe { ffi::kill_on_escape(&vt(p), state(p)) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_set_next_focus(p: *mut G2Popup, w: W) {
    unsafe { ffi::set_next_focus(state(p), w) };
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_set(p: *mut G2Popup) -> usize {
    unsafe { (*p).set }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_self_destruct(p: *mut G2Popup) {
    unsafe {
        if (*p).queued {
            return;
        }
        (*p).queued = true;
        ohpl_g1_set_destroy_popup((*p).set as *mut _, p as usize);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_str(
    p: *mut G2Popup,
    idx: usize,
    s: *const u32,
    n: usize,
    ret: *mut Ret,
) {
    unsafe {
        let Some(slot) = (*p).strs.get_mut(idx) else {
            return;
        };
        slot.clear();
        if !s.is_null() {
            slot.extend_from_slice(std::slice::from_raw_parts(s, n));
        }
        let n = slot.len();
        if let Some((sp, np)) = ret
            .as_mut()
            .and_then(|r| r.s.get_mut(idx).zip(r.n.get_mut(idx)))
        {
            *sp = slot.as_ptr();
            *np = n;
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_binding(p: *mut G2Popup, tag: i32) -> *mut c_void {
    unsafe {
        (*p).bindings.push_back(Binding { popup: p, tag });
        match (*p).bindings.back_mut() {
            Some(b) => std::ptr::from_mut(b).cast(),
            None => std::ptr::null_mut(),
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_dispatch(obj: *mut c_void, w: W, d: *const Msg) -> bool {
    let (popup, tag) = unsafe {
        let b = obj.cast::<Binding>();
        ((*b).popup, (*b).tag)
    };
    unsafe { ffi::event(&vt(popup), state(popup), tag, w, d) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_fp_init(p: *mut G2Popup) {
    unsafe { ffi::fp_init(&vt(p), state(p)) };
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_fp_save_dest(
    p: *mut G2Popup,
    dest: *mut c_void,
    name: *const u32,
    name_n: usize,
) {
    if let Some(b) = unsafe { (fbof(p) as *mut FileBrowser).as_mut() } {
        b.save_dest = dest as usize;
    }
    unsafe { ffi::fp_set_save_dest(&vt(p), state(p), name, name_n) };
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_fp_load_dest(p: *mut G2Popup, dest: *mut c_void) {
    if let Some(b) = unsafe { (fbof(p) as *mut FileBrowser).as_mut() } {
        b.load_dest = dest as usize;
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_fp_add_category(
    p: *mut G2Popup,
    cat: *const u32,
    cat_n: usize,
    filter: *const u32,
    filter_n: usize,
) -> i32 {
    unsafe {
        let name = std::slice::from_raw_parts(cat, cat_n);
        let filter = std::slice::from_raw_parts(filter, filter_n);
        super::h21_fb::add_category(fbof(p) as *mut FileBrowser, name, filter)
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_fp_add_filter(
    p: *mut G2Popup,
    cat: i32,
    filter: *const u32,
    filter_n: usize,
) {
    unsafe {
        let filter = std::slice::from_raw_parts(filter, filter_n);
        super::h21_fb::add_filter(fbof(p) as *mut FileBrowser, cat, filter);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_on_add_filter(p: *mut G2Popup) {
    unsafe { ffi::fp_on_add_filter(&vt(p), state(p)) };
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_on_navigate(p: *mut G2Popup) {
    unsafe { ffi::fp_on_navigate(&vt(p), state(p)) };
}
