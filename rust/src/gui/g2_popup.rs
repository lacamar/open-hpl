//! Owner of the gui popups (`iGuiPopUp` and its four subclasses). `this` of the C++ facade
//! (`gui/GuiPopUp*.h`) is the [`G2Popup`] allocation. The popup logic is `h21_popup`; widget
//! access is `h21_direct` with the popup as context, the file picker's browser `h21_fb`.

use super::g1_set::ohpl_g1_set_destroy_popup;
use super::h21_fb::FileBrowser;
use super::h21_ffi::{
    HostVt, ohpl_h21_color_new, ohpl_h21_destroy, ohpl_h21_event, ohpl_h21_fp_init,
    ohpl_h21_fp_new, ohpl_h21_fp_on_add_filter, ohpl_h21_fp_on_navigate, ohpl_h21_fp_set_save_dest,
    ohpl_h21_kb_new, ohpl_h21_kill_on_escape, ohpl_h21_msgbox_new, ohpl_h21_set_destroy_callback,
    ohpl_h21_set_kill_on_escape, ohpl_h21_set_next_focus,
};
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

fn vt(p: *mut G2Popup) -> Option<HostVt> {
    Some(HostVt {
        ctx: p.cast(),
        exec: super::h21_direct::exec,
    })
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

/// # Safety
/// `p` is a live `G2Popup`.
unsafe fn state(p: *mut G2Popup) -> *mut Popup {
    // SAFETY: caller contract.
    unsafe { (*p).state }
}

/// # Safety
/// `p` is a live `G2Popup`.
unsafe fn fbof(p: *mut G2Popup) -> usize {
    // SAFETY: caller contract.
    unsafe { (*p).fb }
}

fn build(
    set: usize,
    skin: usize,
    make: impl FnOnce(*mut G2Popup, &HostVt) -> *mut Popup,
) -> *mut G2Popup {
    let p = alloc(set, skin);
    if let Some(v) = vt(p) {
        let st = make(p, &v);
        // SAFETY: `p` is the allocation made above.
        unsafe { (*p).state = st };
    }
    p
}

/// # Safety
/// String pointers valid for their lengths.
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
    build(set, skin, |_, v| {
        // SAFETY: caller contract.
        unsafe {
            ohpl_h21_msgbox_new(
                v, label, label_n, text, text_n, b1, b1_n, b2, b2_n, cb_obj, cb_fn,
            )
        }
    })
}

/// # Safety
/// `dest` is a `cColor*` or null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_color(
    set: usize,
    skin: usize,
    dest: usize,
    cb_obj: usize,
    cb_fn: usize,
) -> *mut G2Popup {
    // SAFETY: caller contract.
    build(set, skin, |_, v| unsafe {
        ohpl_h21_color_new(v, dest, cb_obj, cb_fn)
    })
}

/// # Safety
/// String pointers valid for their lengths.
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
    if let Some(v) = vt(p) {
        // SAFETY: caller contract; `p` is the allocation made above.
        unsafe {
            let start = std::slice::from_raw_parts(path, path_n);
            (*p).fb = Box::into_raw(Box::new(FileBrowser::new(p, start, hidden))) as usize;
            (*p).state = ohpl_h21_fp_new(&v, kind, path, path_n, name, name_n, cb_obj, cb_fn);
        }
    }
    p
}

/// # Safety
/// `target` is a live text box of a live set.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_kb(set: usize, skin: usize, target: W) -> *mut G2Popup {
    // SAFETY: caller contract.
    build(set, skin, |_, v| unsafe { ohpl_h21_kb_new(v, target) })
}

/// `cGuiSet::CreatePopUpUIKeyboard`: the popup for `target`'s own set.
///
/// # Safety
/// `target` is a live text box of a live set.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_kb_for(target: W) -> *mut G2Popup {
    let set = super::gw::set_of(target);
    // SAFETY: caller contract.
    let skin = unsafe { super::g1_set::ohpl_g1_set_skin(set) } as usize;
    // SAFETY: caller contract.
    unsafe { ohpl_g2_popup_kb(set as usize, skin, target) }
}

/// # Safety
/// `p` is a popup from this module, not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_free(p: *mut G2Popup) {
    if p.is_null() {
        return;
    }
    if let Some(v) = vt(p) {
        let st = unsafe { state(p) };
        // SAFETY: `st` is the popup state given up here.
        unsafe { ohpl_h21_destroy(&v, st) };
    }
    let fb = unsafe { fbof(p) };
    if fb != 0 {
        // SAFETY: made by `Box::into_raw` in `ohpl_g2_popup_filepicker`, freed once.
        drop(unsafe { Box::from_raw(fb as *mut FileBrowser) });
    }
    // SAFETY: `p` came from `alloc` and is not used afterwards.
    drop(unsafe { Box::from_raw(p) });
}

/// # Safety
/// `p` is a live popup.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_add_destroy_cb(p: *mut G2Popup, obj: usize, f: usize) {
    // SAFETY: caller contract.
    unsafe { ohpl_h21_set_destroy_callback(state(p), obj, f) };
}

/// # Safety
/// `p` is a live popup.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_set_kill_on_escape(p: *mut G2Popup, x: bool) {
    if let Some(v) = vt(p) {
        // SAFETY: caller contract.
        unsafe { ohpl_h21_set_kill_on_escape(&v, state(p), x) };
    }
}

/// # Safety
/// `p` is a live popup.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_kill_on_escape(p: *mut G2Popup) -> bool {
    match vt(p) {
        // SAFETY: caller contract.
        Some(v) => unsafe { ohpl_h21_kill_on_escape(&v, state(p)) },
        None => false,
    }
}

/// # Safety
/// `p` is a live popup.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_set_next_focus(p: *mut G2Popup, w: W) {
    // SAFETY: caller contract.
    unsafe { ohpl_h21_set_next_focus(state(p), w) };
}

/// # Safety
/// `p` is a live popup.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_set(p: *mut G2Popup) -> usize {
    unsafe { (*p).set }
}

/// Queues the popup for deletion by its set, once.
///
/// # Safety
/// `p` is a live popup.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_self_destruct(p: *mut G2Popup) {
    // SAFETY: caller contract.
    unsafe {
        if (*p).queued {
            return;
        }
        (*p).queued = true;
        ohpl_g1_set_destroy_popup((*p).set as *mut _, p as usize);
    }
}

/// Keeps `s` as the host answer string `idx` and points `ret` at it.
///
/// # Safety
/// `p` live, `ret` writable, `s` valid for `n` units.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_str(
    p: *mut G2Popup,
    idx: usize,
    s: *const u32,
    n: usize,
    ret: *mut Ret,
) {
    // SAFETY: caller contract.
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

/// Callback object registered on a widget: `tag` selects the popup's reaction.
///
/// # Safety
/// `p` is a live popup.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_binding(p: *mut G2Popup, tag: i32) -> *mut c_void {
    // SAFETY: caller contract.
    unsafe {
        (*p).bindings.push_back(Binding { popup: p, tag });
        match (*p).bindings.back_mut() {
            Some(b) => std::ptr::from_mut(b).cast(),
            None => std::ptr::null_mut(),
        }
    }
}

/// `tGuiCallbackFunc` of every popup binding.
///
/// # Safety
/// `obj` came from [`ohpl_g2_popup_binding`] of a live popup; `d` is a `cGuiMessageData`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_dispatch(obj: *mut c_void, w: W, d: *const Msg) -> bool {
    // SAFETY: caller contract.
    let (popup, tag) = unsafe {
        let b = obj.cast::<Binding>();
        ((*b).popup, (*b).tag)
    };
    match vt(popup) {
        // SAFETY: as above.
        Some(v) => unsafe { ohpl_h21_event(&v, state(popup), tag, w, d) },
        None => false,
    }
}

/// # Safety
/// `p` is a live file picker.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_fp_init(p: *mut G2Popup) {
    if let Some(v) = vt(p) {
        // SAFETY: caller contract.
        unsafe { ohpl_h21_fp_init(&v, state(p)) };
    }
}

/// # Safety
/// `p` is a live file picker; `dest` the caller's `tWString`, `name` its content.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_fp_save_dest(
    p: *mut G2Popup,
    dest: *mut c_void,
    name: *const u32,
    name_n: usize,
) {
    if let Some(v) = vt(p) {
        // SAFETY: caller contract.
        if let Some(b) = unsafe { (fbof(p) as *mut FileBrowser).as_mut() } {
            b.save_dest = dest as usize;
        }
        // SAFETY: caller contract.
        unsafe { ohpl_h21_fp_set_save_dest(&v, state(p), name, name_n) };
    }
}

/// # Safety
/// `p` is a live file picker; `dest` the caller's `tWStringVec`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_fp_load_dest(p: *mut G2Popup, dest: *mut c_void) {
    // SAFETY: caller contract.
    if let Some(b) = unsafe { (fbof(p) as *mut FileBrowser).as_mut() } {
        b.load_dest = dest as usize;
    }
}

/// # Safety
/// `p` is a live file picker; strings valid for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_fp_add_category(
    p: *mut G2Popup,
    cat: *const u32,
    cat_n: usize,
    filter: *const u32,
    filter_n: usize,
) -> i32 {
    // SAFETY: caller contract.
    unsafe {
        let name = std::slice::from_raw_parts(cat, cat_n);
        let filter = std::slice::from_raw_parts(filter, filter_n);
        super::h21_fb::add_category(fbof(p) as *mut FileBrowser, name, filter)
    }
}

/// # Safety
/// `p` is a live file picker; `filter` valid for its length.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_fp_add_filter(
    p: *mut G2Popup,
    cat: i32,
    filter: *const u32,
    filter_n: usize,
) {
    // SAFETY: caller contract.
    unsafe {
        let filter = std::slice::from_raw_parts(filter, filter_n);
        super::h21_fb::add_filter(fbof(p) as *mut FileBrowser, cat, filter);
    }
}

/// `iFileBrowser::OnAddFilter` of the picker's browser.
///
/// # Safety
/// `p` is a live file picker.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_on_add_filter(p: *mut G2Popup) {
    if let Some(v) = vt(p) {
        // SAFETY: caller contract.
        unsafe { ohpl_h21_fp_on_add_filter(&v, state(p)) };
    }
}

/// `iFileBrowser::OnNavigate` of the picker's browser.
///
/// # Safety
/// `p` is a live file picker.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2_popup_on_navigate(p: *mut G2Popup) {
    if let Some(v) = vt(p) {
        // SAFETY: caller contract.
        unsafe { ohpl_h21_fp_on_navigate(&v, state(p)) };
    }
}
